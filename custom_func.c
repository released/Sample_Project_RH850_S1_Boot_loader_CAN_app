/* Project/API reading notes: Project-owned initialization and foreground work.
 * The selected main/startup adapter calls hardware_init() once and loop() repeatedly.
 * Keep application changes here so generated entry sources remain replaceable; lower-level APIs
 * live in this project's hal directories.
 */
#include "r_smc_entry.h"
#include "custom_func.h"
#include "retarget.h"
#include "device_platform.h"
#include "can_boot_config.h"
#ifdef RH850_DEVICE_APP
#include "app_identity.h"
#include "app_runtime_log.h"
/* SC peripheral initializers use this synchronization readback. The App
 * inherits clocks and therefore does not link the SC clock initializer.
 */
volatile uint32_t g_cg_sync_read;
#endif

static volatile unsigned long g_millis;
volatile UART_MANAGER_T UART0Manager;
#ifdef RH850_DEVICE_BOOT
static unsigned long g_reset_cause;
#endif

unsigned long Device_Millis(void)
{
    return g_millis;
}
/* Advance the monotonic timebase only.
 * Protocol timeout, application work and text output are handled outside this ISR.
 */
void ostmr_1ms_IRQ(void)
{
    g_millis++;
}
void tmr_1ms_IRQ(void)
{
}
void UARTx_Process(unsigned char value)
{
    (void)value;
}
void UARTx_ErrorCheckProcess(unsigned char value)
{
    (void)value;
}

/* Foreground-only diagnostic output used by retarget.c.
 * Preserve a finite wait when UART0 is busy or stopped; ISP data uses its own link.
 */
int SendChar(unsigned char value)
{
    unsigned long guard;
    guard = 100000UL;
    while ((RLN30.LST & _UART_TRANSMISSION_OPERATED) && guard)
    {
        guard--;
    }
    if (guard)
    {
        RLN30.LUTDR.UINT16 = value;
        return (int)value;
    }
    return -1;
}
/* Write the validated shared-RAM request/owner contract, then request a hardware reset.
 * Call only after the relevant response completed transmission.
 */
void Device_RequestReset(unsigned long request)
{
    volatile unsigned long *cookie;
    unsigned char guard;
    DI();
    cookie = (volatile unsigned long *)BOOT_APP_SHARED_RAM_ADDRESS;
    cookie[0] = BOOT_APP_SIGNATURE;
    cookie[1] = request;
    cookie[2] = ~request;
    cookie[3] = BOOT_APP_OWNER_BOOT;
    __syncp();
    for (guard = 0U; guard < 4U; guard++)
    {
        WPROTR.PROTCMD0 = 0xA5U;
        RESCTL.SWRESA = 1UL;
        RESCTL.SWRESA = ~1UL;
        RESCTL.SWRESA = 1UL;
        __syncp();
    }
    for (;;)
    {
        NOP();
    }
}
unsigned char Device_ClockInherited(void)
{
    return (unsigned char)(CLKCTL.PLL1C == 0x0001031DUL && (CLKCTL.PLL1S & 4UL) &&
                           CLKCTL.CKSC_CPUCLKS_ACT == 3UL && CLKCTL.CKSC_CPUCLKD_ACT == 0x11UL &&
                           CLKCTL.CKSC_PPLLCLKS_ACT == 3UL && CLKCTL.CKSC_IPERI2S_ACT == 1UL);
}
/* Prepare role-specific clocks, timebase, text Log and the RAM Flash reader before accepting
 * normal protocol work.
 */
static unsigned char RuntimeStart(void)
{
    DI();
    g_millis = 0UL;
#ifdef RH850_DEVICE_BOOT
    g_reset_cause = RESCTL.RESF;
#endif
#ifdef RH850_DEVICE_APP
    if (!Device_ClockInherited())
    {
        Device_RequestReset(BOOT_APP_ENTER_REQUEST);
    }
#endif
    R_Systeminit();
    R_Config_UART0_Start();
    INTC2.ICRLIN30UR0.BIT.MKRLIN30UR0 = 1U;
    INTC2.ICRLIN30UR1.BIT.MKRLIN30UR1 = 1U;
    INTC2.ICRLIN30UR2.BIT.MKRLIN30UR2 = 1U;
    R_Config_OSTM0_Start();
    EI();
    if (!Device_ReadPrepare())
    {
        tiny_printf("RAM reader init failed\r\n");
        return 0U;
    }
    return 1U;
}

#ifdef RH850_DEVICE_BOOT
static boot_engine_t g_boot;
/* Quiesce Boot peripherals/IRQs and commit the App handoff marker before the assembly jump.
 * The destination image has already passed complete validation.
 */
static void RunValidatedApp(unsigned long flags)
{
    volatile unsigned long *cookie;
    DI();
    Device_CanStop();
    R_Config_OSTM0_Stop();
    R_Config_TAUJ0_0_Stop();
    R_Config_UART0_Stop();
    cookie = (volatile unsigned long *)BOOT_APP_SHARED_RAM_ADDRESS;
    cookie[0] = BOOT_APP_SIGNATURE;
    cookie[1] = 0UL;
    cookie[2] = 0xFFFFFFFFUL;
    cookie[3] = BOOT_APP_OWNER_APP | flags;
    __syncp();
    Device_JumpAsm();
    for (;;)
    {
        NOP();
    }
}

static const boot_io_t g_custom_io = {Device_FlashRead, Device_FlashErase, Device_FlashWritePage};
static volatile unsigned long *g_custom_cookie;
static unsigned long g_custom_request, g_custom_updated, g_custom_last;
static unsigned char g_custom_timed, g_custom_action, g_custom_length, g_custom_reply[8];
static boot_frame_t g_custom_frame;

/* Initialize the role selected by this project before foreground processing.
 * Preserve clock, image/handoff and peripheral/IRQ enable order when adding application
 * initialization.
 */
void hardware_init(void)
{
    g_custom_cookie = (volatile unsigned long *)BOOT_APP_SHARED_RAM_ADDRESS;
    g_custom_request = 0UL;
    if (g_custom_cookie[0] == BOOT_APP_SIGNATURE && g_custom_cookie[2] == ~g_custom_cookie[1])
    {
        g_custom_request = g_custom_cookie[1];
    }
    g_custom_cookie[0] = BOOT_APP_SIGNATURE;
    g_custom_cookie[1] = 0UL;
    g_custom_cookie[2] = 0xFFFFFFFFUL;
    g_custom_cookie[3] = BOOT_APP_OWNER_BOOT;
    if (!RuntimeStart())
    {
        for (;;)
        {
            NOP();
        }
    }
    RESCTL.RESFC = 0x7FFUL;
    tiny_printf("RH850 CAN DEVICE BOOT: EVM; X1=16M CPU120 CAN40\r\n");
    tiny_printf("reset cause: 0x%08lX\r\n", g_reset_cause);
    Boot_Init(&g_boot, &g_custom_io, 0U);
    g_custom_updated = (g_custom_request == BOOT_APP_UPDATE_REQUEST) ? BOOT_APP_UPDATED_FLAG : 0UL;
    if (g_boot.installed.valid && g_custom_request != BOOT_APP_ENTER_REQUEST)
    {
        RunValidatedApp(g_custom_updated);
    }
    if (!Device_CanInit(CAN_BOOT_REQUEST_ID, 1U))
    {
        tiny_printf("CAN init failed; Flash is not initialized\r\n");
        for (;;)
        {
            NOP();
        }
    }
    g_boot.flash_ready = Device_FlashInit();
    tiny_printf("%s", g_boot.flash_ready ? "FCL geometry: OK\r\n"
                                  : "FCL init/geometry failed; update blocked\r\n");
    g_custom_timed =
        (unsigned char)(g_boot.installed.valid && g_custom_request == BOOT_APP_ENTER_REQUEST);
    g_custom_last = Device_Millis();
}

/* Foreground service point called repeatedly by the selected main adapter.
 * Keep protocol/application work and text logging bounded so pending traffic and deferred actions
 * can continue.
 */
void loop(void)
{

    if (Device_CanRead(&g_custom_frame))
    {
        if (!g_boot.owner_valid || g_custom_frame.channel == g_boot.owner_channel)
        {
            g_custom_last = Device_Millis();
        }
        g_boot.diagnostics |= Device_CanDiagnostics() ? 4UL : 0UL;
        g_custom_length = Boot_Process(&g_boot, &g_custom_frame, g_custom_reply, &g_custom_action);
        if (g_custom_length &&
            Device_CanSend(CAN_BOOT_RESPONSE_ID, g_custom_reply, g_custom_length) &&
            g_custom_action == 1U)
        {
            tiny_printf("CAN update: END CRC/vector validation OK\r\n");
            Device_RequestReset(BOOT_APP_UPDATE_REQUEST);
        }
    }
    if (g_boot.owner_valid && !g_boot.active &&
        Device_Millis() - g_custom_last >= CAN_BOOT_INACTIVITY_MS)
    {
        Boot_ReleasePort(&g_boot);
        tiny_printf("CAN metadata staging expired; update port released\r\n");
    }
    if (g_custom_timed && !g_boot.ever_started &&
        Device_Millis() - g_custom_last >= CAN_BOOT_INACTIVITY_MS)
    {
        g_custom_timed = 0U;
        if (Device_ImageValidate(Device_FlashRead, &g_boot.installed))
        {
            RunValidatedApp(0UL);
        }
    }
}

#else

static volatile unsigned long *g_custom_cookie;
static device_image_info_t g_custom_image;
static boot_frame_t g_custom_frame;
static unsigned long g_custom_flags, g_custom_last, g_custom_counter, g_custom_health,
    g_custom_value;
static unsigned char g_custom_reply[8], g_custom_ok, g_custom_page;

/* Initialize the role selected by this project before foreground processing.
 * Preserve clock, image/handoff and peripheral/IRQ enable order when adding application
 * initialization.
 */
void hardware_init(void)
{
    g_custom_cookie = (volatile unsigned long *)BOOT_APP_SHARED_RAM_ADDRESS;
    if (g_custom_cookie[0] != BOOT_APP_SIGNATURE || g_custom_cookie[1] ||
        g_custom_cookie[2] != 0xFFFFFFFFUL || (g_custom_cookie[3] & 255UL) != BOOT_APP_OWNER_APP)
    {
        Device_RequestReset(BOOT_APP_ENTER_REQUEST);
    }
    g_custom_flags = g_custom_cookie[3];
    if (!RuntimeStart() || !Device_ImageValidate(Device_FlashRead, &g_custom_image) ||
        g_custom_image.version != DEVICE_APP_VERSION || g_custom_image.build != DEVICE_APP_BUILD)
    {
        Device_RequestReset(BOOT_APP_ENTER_REQUEST);
    }
    if (!Device_CanInit(CAN_BOOT_REQUEST_ID, 0U))
    {
        Device_RequestReset(BOOT_APP_ENTER_REQUEST);
    }
    tiny_printf("RH850 CAN DEVICE APP: boot-validated; X1=16M CPU120 CAN40\r\n");
    tiny_printf("app image tag: " APP_RUNTIME_LOG_TAG "\r\n");
    if (g_custom_flags & BOOT_APP_UPDATED_FLAG)
    {
        tiny_printf("CAN update: completed\r\n");
    }
    g_custom_cookie[3] = BOOT_APP_OWNER_APP;
    g_custom_health = 0x10300UL | ((g_custom_flags & BOOT_APP_UPDATED_FLAG) ? 0x20000UL : 0UL);
    g_custom_last = Device_Millis();
    g_custom_counter = 0UL;
}

/* Foreground service point called repeatedly by the selected main adapter.
 * Keep protocol/application work and text logging bounded so pending traffic and deferred actions
 * can continue.
 */
void loop(void)
{

    if (Device_Millis())
    {
        g_custom_health |= 0x401UL;
    }
    if (Device_CanRead(&g_custom_frame) && g_custom_frame.id == CAN_BOOT_REQUEST_ID &&
        g_custom_frame.length)
    {
        g_custom_reply[0] = g_custom_frame.data[0];
        g_custom_reply[1] = 0U;
        g_custom_reply[2] = 0U;
        g_custom_reply[3] = 1U;
        if (g_custom_frame.data[0] == 6U)
        {
            if (g_custom_frame.length != 1U)
            {
                g_custom_reply[1] = 4U;
            }
            if (Device_CanSend(CAN_BOOT_RESPONSE_ID, g_custom_reply, 2U) && !g_custom_reply[1])
            {
                Device_RequestReset(BOOT_APP_ENTER_REQUEST);
            }
        }
        else
        {
            g_custom_value = 0UL;
            g_custom_page = (g_custom_frame.length == 2U) ? g_custom_frame.data[1] : 0U;
            g_custom_reply[2] = g_custom_page;
            if (g_custom_frame.data[0] != 7U)
            {
                g_custom_reply[1] = 1U;
            }
            else if (g_custom_frame.length != 2U)
            {
                g_custom_reply[1] = 4U;
            }
            else
            {
                g_custom_value =
                    Device_ImageInfo(&g_custom_image, g_custom_page, 1U, g_custom_health,
                                     Device_CanDiagnostics(), &g_custom_ok);
                if (!g_custom_ok)
                {
                    g_custom_reply[1] = 4U;
                }
            }
            Device_Write32(g_custom_reply + 4, g_custom_value);
            (void)Device_CanSend(CAN_BOOT_RESPONSE_ID, g_custom_reply, 8U);
        }
    }
    if (Device_Millis() - g_custom_last >= 1000UL)
    {
        g_custom_last = Device_Millis();
        tiny_printf("log(timer)-" APP_RUNTIME_LOG_TAG ": %lu\r\n", g_custom_counter++);
    }
}

#endif
