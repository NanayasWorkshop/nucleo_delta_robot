/*
 * TMC9660 UART Driver
 * Smart gate driver with FOC controller
 * Communication via UART bootloader protocol
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMC9660_H
#define TMC9660_H

#include <stdint.h>
#include <stdbool.h>

/* TMC9660 UART Protocol Constants */
#define TMC9660_SYNC_BYTE           0x55
#define TMC9660_DEFAULT_DEVICE_ADDR 0x01
#define TMC9660_DEFAULT_HOST_ADDR   0xFF
#define TMC9660_MSG_SIZE            8

/* TMC9660 Bootloader Commands */
#define TMC9660_CMD_GET_INFO        0x00
#define TMC9660_CMD_GET_BANK        0x08
#define TMC9660_CMD_SET_BANK        0x09
#define TMC9660_CMD_GET_ADDRESS     0x0A
#define TMC9660_CMD_SET_ADDRESS     0x0B
#define TMC9660_CMD_READ_32         0x0C
#define TMC9660_CMD_READ_32_INC     0x0D
#define TMC9660_CMD_WRITE_32        0x12
#define TMC9660_CMD_WRITE_32_INC    0x13
#define TMC9660_CMD_NO_OP           0x1D
#define TMC9660_CMD_BOOTSTRAP_RS485 0xFF

/* TMC9660 Memory Banks */
#define TMC9660_BANK_RESERVED       0
#define TMC9660_BANK_SPI            1
#define TMC9660_BANK_I2C            2
#define TMC9660_BANK_OTP            3
#define TMC9660_BANK_CONFIG         5

/* TMC9660 Status Codes */
#define TMC9660_STATUS_OK           0x00
#define TMC9660_STATUS_CMD_NOT_FOUND 0x01
#define TMC9660_STATUS_INVALID_ADDR 0x03
#define TMC9660_STATUS_INVALID_VALUE 0x04
#define TMC9660_STATUS_INVALID_BANK 0x0E
#define TMC9660_STATUS_BUSY         0x0F
#define TMC9660_STATUS_MEM_UNCONFIGURED 0x11
#define TMC9660_STATUS_OTP_ERROR    0x12

/* TMC9660 Info Selectors for GET_INFO command */
#define TMC9660_INFO_CHIP_TYPE      0
#define TMC9660_INFO_BL_VERSION     1
#define TMC9660_INFO_FEATURES       2
#define TMC9660_INFO_CHIP_VERSION   13
#define TMC9660_INFO_CHIP_FREQUENCY 14
#define TMC9660_INFO_CONFIG_MEM_START 17
#define TMC9660_INFO_CONFIG_MEM_SIZE 18
#define TMC9660_INFO_CHIP_VARIANT   28

/* TMC9660 Configuration Memory */
#define TMC9660_CONFIG_BASE_ADDR    0x00020000
#define TMC9660_CONFIG_SIZE         64

/* Expected chip identification */
#define TMC9660_CHIP_TYPE_EXPECTED  0x544D0001
#define TMC9660_CHIP_VARIANT_EXPECTED 2

/* TMC9660 Message Structure */
typedef struct __attribute__((packed)) {
	uint8_t sync_or_host;    /* Request: sync (0x55), Reply: host address */
	uint8_t device_addr;     /* Device address */
	uint8_t cmd_or_status;   /* Request: command, Reply: status */
	uint8_t data[4];         /* 32-bit data (MSB first) */
	uint8_t crc8;            /* CRC8 checksum */
} tmc9660_msg_t;

/* TMC9660 Device State */
typedef struct {
	uint8_t device_addr;     /* Current device address */
	uint8_t host_addr;       /* Current host address */
	uint8_t current_bank;    /* Currently selected memory bank */
	uint32_t current_addr;   /* Current memory address */
	bool initialized;        /* Initialization status */
	uint32_t chip_type;      /* Chip type ID */
	uint32_t chip_version;   /* Silicon revision */
	uint32_t bootloader_version; /* Bootloader version */
} tmc9660_state_t;

/**
 * Initialize TMC9660 UART driver
 *
 * @return 0 on success, negative errno on error
 */
int tmc9660_init(void);

/**
 * Check if TMC9660 is initialized and responding
 *
 * @return true if initialized, false otherwise
 */
bool tmc9660_is_ready(void);

/**
 * Send GET_INFO command to read chip information
 *
 * @param info_selector Info selector (see TMC9660_INFO_* defines)
 * @param value Output: 32-bit info value
 * @return 0 on success, negative errno on error
 */
int tmc9660_get_info(uint8_t info_selector, uint32_t *value);

/**
 * Set memory bank for subsequent read/write operations
 *
 * @param bank Memory bank number (see TMC9660_BANK_* defines)
 * @return 0 on success, negative errno on error
 */
int tmc9660_set_bank(uint8_t bank);

/**
 * Set memory address for subsequent read/write operations
 *
 * @param addr 32-bit memory address
 * @return 0 on success, negative errno on error
 */
int tmc9660_set_address(uint32_t addr);

/**
 * Read 32-bit value from current memory address
 *
 * @param value Output: 32-bit value read
 * @return 0 on success, negative errno on error
 */
int tmc9660_read_32(uint32_t *value);

/**
 * Write 32-bit value to current memory address
 *
 * @param value 32-bit value to write
 * @return 0 on success, negative errno on error
 */
int tmc9660_write_32(uint32_t value);

/**
 * Read register from CONFIG memory
 * Automatically handles bank selection and address setting
 *
 * @param offset Offset within CONFIG memory (0-63)
 * @param value Output: 32-bit value read
 * @return 0 on success, negative errno on error
 */
int tmc9660_read_config(uint8_t offset, uint32_t *value);

/**
 * Write register to CONFIG memory
 * Automatically handles bank selection and address setting
 * Note: Writing to CONFIG triggers runtime reconfiguration
 *
 * @param offset Offset within CONFIG memory (0-63)
 * @param value 32-bit value to write
 * @return 0 on success, negative errno on error
 */
int tmc9660_write_config(uint8_t offset, uint32_t value);

/**
 * Get current TMC9660 state information
 *
 * @param state Output: Current device state
 */
void tmc9660_get_state(tmc9660_state_t *state);

/**
 * Send NO_OP command (useful for testing communication)
 *
 * @return 0 on success, negative errno on error
 */
int tmc9660_no_op(void);

#endif /* TMC9660_H */
