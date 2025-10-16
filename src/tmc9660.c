/*
 * TMC9660 UART Driver Implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tmc9660.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(tmc9660, LOG_LEVEL_INF);

/* UART device */
static const struct device *uart_dev;

/* Device state */
static tmc9660_state_t state = {
	.device_addr = TMC9660_DEFAULT_DEVICE_ADDR,
	.host_addr = TMC9660_DEFAULT_HOST_ADDR,
	.current_bank = 0xFF,  /* Invalid, force set on first access */
	.current_addr = 0,
	.initialized = false,
	.chip_type = 0,
	.chip_version = 0,
	.bootloader_version = 0,
};

/* Timeouts */
#define TMC9660_REPLY_TIMEOUT_MS  100
#define TMC9660_BYTE_TIMEOUT_MS   10

/* Mutex for thread-safe access */
K_MUTEX_DEFINE(tmc9660_mutex);

/**
 * Calculate CRC8 checksum for TMC9660 protocol
 * Polynomial: x^8 + x^2 + x + 1 (0x07)
 * Initial value: 0x00
 *
 * @param data Pointer to data buffer
 * @param len Length of data
 * @return CRC8 checksum
 */
static uint8_t tmc9660_crc8(const uint8_t *data, size_t len)
{
	uint8_t crc = 0;

	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ 0x07;
			} else {
				crc <<= 1;
			}
		}
	}

	return crc;
}

/**
 * Pack 32-bit value into message data field (MSB first)
 */
static void pack_u32_msb(uint8_t *dest, uint32_t value)
{
	dest[0] = (value >> 24) & 0xFF;
	dest[1] = (value >> 16) & 0xFF;
	dest[2] = (value >> 8) & 0xFF;
	dest[3] = value & 0xFF;
}

/**
 * Unpack 32-bit value from message data field (MSB first)
 */
static uint32_t unpack_u32_msb(const uint8_t *src)
{
	return ((uint32_t)src[0] << 24) |
	       ((uint32_t)src[1] << 16) |
	       ((uint32_t)src[2] << 8) |
	       ((uint32_t)src[3]);
}

/**
 * Send UART data with timeout
 */
static int uart_send(const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart_dev, data[i]);
	}
	return 0;
}

/**
 * Receive UART data with timeout
 */
static int uart_recv(uint8_t *data, size_t len, k_timeout_t timeout)
{
	int64_t end_time = k_uptime_get() + timeout.ticks;

	for (size_t i = 0; i < len; i++) {
		int ret;
		do {
			ret = uart_poll_in(uart_dev, &data[i]);
			if (ret == 0) {
				break;  /* Byte received */
			}
			if (k_uptime_get() >= end_time) {
				LOG_ERR("UART receive timeout at byte %zu/%zu", i, len);
				return -ETIMEDOUT;
			}
			k_sleep(K_USEC(100));  /* Small delay between polls */
		} while (ret != 0);
	}

	return 0;
}

/**
 * Send command and receive reply
 */
static int tmc9660_transact(uint8_t cmd, uint32_t req_value, uint32_t *reply_value,
			     uint8_t *reply_status)
{
	tmc9660_msg_t req, reply;
	int ret;

	/* Build request message */
	req.sync_or_host = TMC9660_SYNC_BYTE;
	req.device_addr = state.device_addr;
	req.cmd_or_status = cmd;
	pack_u32_msb(req.data, req_value);
	req.crc8 = tmc9660_crc8((uint8_t *)&req, TMC9660_MSG_SIZE - 1);

	/* Send request */
	ret = uart_send((uint8_t *)&req, TMC9660_MSG_SIZE);
	if (ret < 0) {
		LOG_ERR("Failed to send request: %d", ret);
		return ret;
	}

	/* Receive reply */
	ret = uart_recv((uint8_t *)&reply, TMC9660_MSG_SIZE,
			K_MSEC(TMC9660_REPLY_TIMEOUT_MS));
	if (ret < 0) {
		LOG_ERR("Failed to receive reply: %d", ret);
		return ret;
	}

	/* Verify CRC */
	uint8_t expected_crc = tmc9660_crc8((uint8_t *)&reply, TMC9660_MSG_SIZE - 1);
	if (reply.crc8 != expected_crc) {
		LOG_ERR("CRC mismatch: got 0x%02X, expected 0x%02X",
			reply.crc8, expected_crc);
		return -EBADMSG;
	}

	/* Verify device address */
	if (reply.device_addr != state.device_addr) {
		LOG_WRN("Device address mismatch: got 0x%02X, expected 0x%02X",
			reply.device_addr, state.device_addr);
	}

	/* Extract reply data */
	if (reply_value) {
		*reply_value = unpack_u32_msb(reply.data);
	}
	if (reply_status) {
		*reply_status = reply.cmd_or_status;
	}

	/* Check status */
	if (reply.cmd_or_status != TMC9660_STATUS_OK) {
		LOG_WRN("Command 0x%02X returned status 0x%02X",
			cmd, reply.cmd_or_status);
		return -EIO;
	}

	return 0;
}

int tmc9660_init(void)
{
	int ret;
	uint32_t value;

	k_mutex_lock(&tmc9660_mutex, K_FOREVER);

	/* Get UART device via alias */
	uart_dev = DEVICE_DT_GET(DT_ALIAS(tmc9660uart));
	if (!uart_dev || !device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready");
		k_mutex_unlock(&tmc9660_mutex);
		return -ENODEV;
	}

	LOG_INF("TMC9660 UART initialized");

	/* Small delay for chip startup */
	k_sleep(K_MSEC(10));

	/* Verify chip type */
	ret = tmc9660_get_info(TMC9660_INFO_CHIP_TYPE, &value);
	if (ret < 0) {
		LOG_ERR("Failed to read chip type: %d", ret);
		LOG_WRN("Continuing without verification (chip may not be connected)");
		k_mutex_unlock(&tmc9660_mutex);
		return ret;
	}

	state.chip_type = value;
	if (value != TMC9660_CHIP_TYPE_EXPECTED) {
		LOG_ERR("Unexpected chip type: 0x%08X (expected 0x%08X)",
			value, TMC9660_CHIP_TYPE_EXPECTED);
		k_mutex_unlock(&tmc9660_mutex);
		return -ENODEV;
	}

	LOG_INF("Chip type verified: 0x%08X", value);

	/* Read chip version */
	ret = tmc9660_get_info(TMC9660_INFO_CHIP_VERSION, &value);
	if (ret == 0) {
		state.chip_version = value;
		LOG_INF("Chip version: %u", value);
	}

	/* Read bootloader version */
	ret = tmc9660_get_info(TMC9660_INFO_BL_VERSION, &value);
	if (ret == 0) {
		state.bootloader_version = value;
		uint16_t major = (value >> 16) & 0xFFFF;
		uint16_t minor = value & 0xFFFF;
		LOG_INF("Bootloader version: %u.%u", major, minor);
	}

	state.initialized = true;
	k_mutex_unlock(&tmc9660_mutex);

	LOG_INF("TMC9660 initialized successfully");
	return 0;
}

bool tmc9660_is_ready(void)
{
	return state.initialized;
}

int tmc9660_get_info(uint8_t info_selector, uint32_t *value)
{
	int ret;
	uint8_t status;

	if (!value) {
		return -EINVAL;
	}

	k_mutex_lock(&tmc9660_mutex, K_FOREVER);
	ret = tmc9660_transact(TMC9660_CMD_GET_INFO, info_selector, value, &status);
	k_mutex_unlock(&tmc9660_mutex);

	return ret;
}

int tmc9660_set_bank(uint8_t bank)
{
	int ret;
	uint8_t status;
	uint32_t reply_value;

	k_mutex_lock(&tmc9660_mutex, K_FOREVER);

	/* Only send if bank is different */
	if (state.current_bank == bank) {
		k_mutex_unlock(&tmc9660_mutex);
		return 0;
	}

	ret = tmc9660_transact(TMC9660_CMD_SET_BANK, bank, &reply_value, &status);
	if (ret == 0) {
		state.current_bank = bank;
		LOG_DBG("Set bank to %u", bank);
	}

	k_mutex_unlock(&tmc9660_mutex);
	return ret;
}

int tmc9660_set_address(uint32_t addr)
{
	int ret;
	uint8_t status;
	uint32_t reply_value;

	k_mutex_lock(&tmc9660_mutex, K_FOREVER);

	/* Always send address (some commands auto-increment) */
	ret = tmc9660_transact(TMC9660_CMD_SET_ADDRESS, addr, &reply_value, &status);
	if (ret == 0) {
		state.current_addr = addr;
		LOG_DBG("Set address to 0x%08X", addr);
	}

	k_mutex_unlock(&tmc9660_mutex);
	return ret;
}

int tmc9660_read_32(uint32_t *value)
{
	int ret;
	uint8_t status;

	if (!value) {
		return -EINVAL;
	}

	k_mutex_lock(&tmc9660_mutex, K_FOREVER);
	ret = tmc9660_transact(TMC9660_CMD_READ_32, 0, value, &status);
	k_mutex_unlock(&tmc9660_mutex);

	return ret;
}

int tmc9660_write_32(uint32_t value)
{
	int ret;
	uint8_t status;
	uint32_t reply_value;

	k_mutex_lock(&tmc9660_mutex, K_FOREVER);
	ret = tmc9660_transact(TMC9660_CMD_WRITE_32, value, &reply_value, &status);
	k_mutex_unlock(&tmc9660_mutex);

	return ret;
}

int tmc9660_read_config(uint8_t offset, uint32_t *value)
{
	int ret;

	if (!value || offset >= TMC9660_CONFIG_SIZE) {
		return -EINVAL;
	}

	/* Ensure offset is 4-byte aligned */
	if (offset & 0x03) {
		LOG_ERR("Config offset must be 4-byte aligned");
		return -EINVAL;
	}

	/* Set bank to CONFIG */
	ret = tmc9660_set_bank(TMC9660_BANK_CONFIG);
	if (ret < 0) {
		return ret;
	}

	/* Set address */
	ret = tmc9660_set_address(TMC9660_CONFIG_BASE_ADDR + offset);
	if (ret < 0) {
		return ret;
	}

	/* Read value */
	return tmc9660_read_32(value);
}

int tmc9660_write_config(uint8_t offset, uint32_t value)
{
	int ret;

	if (offset >= TMC9660_CONFIG_SIZE) {
		return -EINVAL;
	}

	/* Ensure offset is 4-byte aligned */
	if (offset & 0x03) {
		LOG_ERR("Config offset must be 4-byte aligned");
		return -EINVAL;
	}

	/* Set bank to CONFIG */
	ret = tmc9660_set_bank(TMC9660_BANK_CONFIG);
	if (ret < 0) {
		return ret;
	}

	/* Set address */
	ret = tmc9660_set_address(TMC9660_CONFIG_BASE_ADDR + offset);
	if (ret < 0) {
		return ret;
	}

	/* Write value */
	LOG_INF("Writing CONFIG[0x%02X] = 0x%08X", offset, value);
	return tmc9660_write_32(value);
}

void tmc9660_get_state(tmc9660_state_t *state_out)
{
	if (state_out) {
		k_mutex_lock(&tmc9660_mutex, K_FOREVER);
		memcpy(state_out, &state, sizeof(tmc9660_state_t));
		k_mutex_unlock(&tmc9660_mutex);
	}
}

int tmc9660_no_op(void)
{
	int ret;
	uint8_t status;
	uint32_t reply_value;

	k_mutex_lock(&tmc9660_mutex, K_FOREVER);
	ret = tmc9660_transact(TMC9660_CMD_NO_OP, 0, &reply_value, &status);
	k_mutex_unlock(&tmc9660_mutex);

	return ret;
}
