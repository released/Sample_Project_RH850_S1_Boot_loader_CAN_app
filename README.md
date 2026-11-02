# RH850 Classical CAN device App

Application for R7F701684,paired with the same-parent
`Sample_Project_RH850_S1_Boot_loader_CAN` project.
This project includes its own HAL sources, configuration and PowerShell build tools.

## Editable inputs

| Header | Purpose |
| --- | --- |
| `app_image_config.h` | Six release fields:version,build,target,role,transport,version control |
| `app_irq_handlers.h` | App-local `X(EIINT_channel, global_ISR)` mapping |
| `app_runtime_log.h` | UART demonstration tag,such as V1/V2 |

Version is packed0x00MMmmpp. Default version control0 permits any App version.
Policy1 on a valid installed App accepts same/newer on normal START.
The Boot minimum version is independent of the App release version.
Generated identity/vector files are outputs,not authoring inputs.
The paired Boot's `can_port_config.h` enables CAN1/CAN4;the App uses
that same selection. Port/pin/buffer configuration is described in the Boot README.

The paired Boot also supplies `hal/can`: the shared v4 service/driver,
with EVM-specific board/config adapters. App RX remains IRQ-owned; foreground
work consumes complete frames and TX completion events. The ISP reply wrapper
reports success only after matching hardware completion,not queue admission.
See the Boot README's CAN service section for ownership and explicit controls.

Use no-channel global interrupt handlers with EIRET. Channel23 is the shared CAN RX FIFO IRQ;
84 is OSTM0 tick. Other channel indexes come from this MCU's hardware manual
and SC interrupt table. After adding a source file,register it in this App.
The full512-entry interface and linker layout do not grow with the used IRQ
count. Boot and App may use the same function names in separate images.

## Build and artifacts

From this project directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .uild.ps1
```

CS+ Rebuild then uses local pre/post hooks. Requires CS+/CC-RH and PowerShell,
not Python. App-only builds do not rebuild or rewrite Boot images.

| `DefaultBuild` artifact | Use |
| --- | --- |
| `Sample_Project_RH850_S1_Boot_loader_CAN_app_update.bin` | Load into Tool CAN Upgrade |
| `flash_app_variable_CRC.hex` | Programmer App-only image with absolute addresses |
| `app_build_report.html` | Actual BIN identity,CRC/hash,metadata bytes and IRQ destinations |
| Raw linker MOT/ABS/MAP/ASM | Engineering inputs;raw MOT lacks final size/CRC packaging |

App entry10000,metadata10100,EBASE10200,INTBP10400,code11000 onward.
Capacity960KiB;image length includes the final four CRC32 bytes.
Post-build checks compiled metadata and all512 actual vector destinations
before creating the BIN. It rejects stale compiled inputs.

## Tool operation and runtime

Open + Probe Host,select Classical500K,load the App BIN,Discover Target.
If discovered in App,Enter Boot;otherwise start the update from Boot.
An idle valid-App ENTER session returns to App after30seconds before START.
Tool confirms APP_READY and matching live identity after END/reset.
UART0 TX P0_2/115200 shows image tag and `log(timer)-V1` with an increasing counter.
The App inherits Boot clocks and selects its independent full interrupt table.

Offline build/image/model tests are available;physical CAN/Flash/IRQ/reset
and interruption tests are NOT RUN.
See the [RH850 guide](https://released.github.io/slide_boot_loader_for_RL78/#rh850-can-device)
and [Tool operation](https://released.github.io/slide_simple_renesas_isp_tool/#tool-operation).

## App timing test images

A successful App build also produces FF-filled test BINs under `test_bins/auto/`.
Open `test_bins/auto/index.html` to select an image by actual size, CRC and hash.
The set includes supported Flash-budget tiers and the declared App capacity;
tiers smaller than the executable or larger than physical Flash are skipped.
The packaged `DefaultBuild/*_update.bin`, code, IRQ tables and release settings
remain unchanged. Only test-image length, FF padding and final CRC are updated.
Automatic rebuilds refresh this one owned set; changed or unknown files stop
replacement. Manual `generate_test_bins.bat` creates a separate batch.
Use the matching ISP workflow and keep its JSON/HTML operation report for timing.
See the [test-image guide](https://released.github.io/slide_boot_loader_for_RL78/#device-test-images).

## User code entry

`custom_func.c` provides `hardware_init()` for initialization and `loop()` for
foreground service. Add application initialization and foreground work through
these callbacks, retaining the existing communication service calls.
The selected main/startup adapter owns entry and handoff. Generated peripheral
files remain under `src/`; retain the project's MTPJ source selection when using
Code Generator or Smart Configurator, then rebuild to validate the image and vectors.

## Project layout and standalone build

This project includes its own source files, configuration headers and build tools.
A sibling Boot, App or Transfer checkout is not required to compile it.
Keep the project directory name aligned with its `.mtpj` filename; remove a ZIP download suffix when necessary.
Build with CS+ Rebuild, or run the local PowerShell entry:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

| Location | Responsibility |
| --- | --- |
| `custom_func.c` | Project initialization in `hardware_init()` and recurring work in `loop()`; runtime orchestration stays here |
| `hal/can` | CAN service, controller driver and CAN adapter, when used by this project |
| `hal/device` | Device, clock, peripheral and CPU handoff APIs |
| `hal/image` | Metadata parser/profile, image validation and software CRC |
| `hal/flash` | Flash access/geometry APIs, when required by the selected image |
| `hal/isp` | Update/Transfer protocol and transport processing APIs |
| Root configuration headers | Local MCU/Flash, handoff and communication contract |
| `tools` and root PowerShell helpers | Local metadata/IRQ generation and linked-image checks |
| `src` | Code Generator or Smart Configurator peripheral sources |

Only the sources registered in this project's CS+ configuration are linked.
Generated main/vector implementations must not become a second active entry.
Retain the `custom_func.c` callbacks and the selected startup adapter after code generation.

Boot and App each carry a local contract. Keep their MCU, Flash layout, image format,
IRQ ABI, handoff and communication settings compatible when installing a pair.
App releases are edited in `app_image_config.h` and `app_irq_handlers.h` in the App project.
Rebuild the selected App to regenerate its metadata, vector table, update BIN and timing BINs.
The App build does not read or rewrite a Boot checkout.

## Diagnostic text output

The project-local `retarget_config.h` selects the formatter at compile time:

```c
#define TINY_PRINTF_FULL 1 /* 1: Full C99-format mode; 0: Current mode. */
```

Rebuild after changing the mode. Full is the default and supports C99 integer,
string/character, pointer, `f/F/e/E/g/G/a/A`, `n`, `hh/h/l/ll/j/z/t/L`, flags,
numeric/dynamic width and precision. Current retains the existing format subset,
finite rounding and attempted-character counts. Both retain streamed precision
and the project's finite UART0 wait; ISP data uses its independent interface.

```c
#include "retarget.h"
/* Foreground, after UART0 initialization. Use types matching each format. */
tiny_printf("counter=%lu status=0x%08lX\r\n", counter, status);
tiny_printf("voltage=%*.*f size=%zu\r\n", 12, 3, voltage, size);
```

Full uses fixed C locale and nearest/ties-to-even rounding. Its exact decimal
conversion does not delegate to the vendor `printf`. Double/long-double precision
follows the selected CC-RH `-Xdbl_size` setting (both types must match); extra
printed decimals do not add floating-point precision. `%lc`/`%ls` accept C-locale
ASCII wide characters; unrepresentable characters return a negative result.
Ordinary `%s` copies encoded bytes. `%p` uses hexadecimal `0x...`, including
`0x0` for null; pointer notation is implementation-defined by the C standard.

Full returns the count accepted by UART, or a negative result on format/count/
encoding errors or UART timeout. This is not receiver/wire acknowledgement.
`SendChar()` returns the accepted unsigned byte or -1 on its existing wait bound.
Current mode ignores the hook error and retains attempted-output counts.
Format in foreground code, not in an ISR or Flash P/E path.

Format examples and the mode table are described in the
[RH850 Log guide](https://released.github.io/slide_boot_loader_for_RH850/app-development.html#rh850-runtime-log).
