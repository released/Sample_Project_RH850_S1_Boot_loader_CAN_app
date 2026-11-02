/* Project/API reading notes: Editable App release identity.
 * Version/build identify this App, target/role/transports must match the installed Boot contract,
 * and version control is disabled by default.
 * Generated identity/metadata outputs are rebuilt from these inputs; do not edit the BIN by hand.
 */
#ifndef APP_IMAGE_CONFIG_H
#define APP_IMAGE_CONFIG_H

/* Release version, packed0x00MMmmpp (one byte each). Example1.2.3=0x00010203.
 * Boot version is independent. Same/newer ordering is used only when the
 * valid installed App enables version control below.
 */
#define APP_IMAGE_VERSION (0x00010000UL)
/* Build identity. YYYYMMDD is a convention,not the downgrade comparison key. */
#define APP_IMAGE_BUILD (20261021UL)
/* Must match the paired Boot engineering profile. 8501 identifies this
 * R7F701684 EVM contract. Changing it alone does not port an MCU; mismatch
 * is rejected by generation,Tool live checks and Boot before erase.
 */
#define APP_IMAGE_TARGET_ID (0x8501U)
/* DEVICE_APP=1. Other values(e.g.5) are invalid and generation stops. */
#define APP_IMAGE_ROLE (1U)
/* Update-permission mask: CAN=01,I2C=02,UART=04. This Boot implements CAN;
 * generation requires01. Changing it to02/04 does not create that bootloader.
 * Runtime peripherals and PC Host Link baud are independent of this field.
 */
#define APP_IMAGE_UPDATE_TRANSPORTS (0x01U)
/* 0=allow any version(default);1=allow same/newer when this App is valid.
 * The Tool's explicit force-old-version transaction skips ordering only;
 * identity,layout,IRQ ABI,minimum Boot and CRC gates remain active.
 */
#define APP_IMAGE_VERSION_CONTROL (0U)

#endif
