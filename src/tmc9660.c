/*
 * TMC9660 UART Driver Implementation - Multi-Motor Support
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

/* Per-motor instance data */
typedef struct {
	const struct device *uart_dev;
	tmc9660_state_t state;
	struct k_mutex mutex;
	const char *name;
} tmc9660_instance_t;

/* Array of motor instances */
static tmc9660_instance_t motors[TMC9660_NUM_MOTORS] = {
	[TMC9660_MOTOR_A] = {
		.name = "Motor A",
		.state = {
			.device_addr = TMC9660_DEFAULT_DEVICE_ADDR,
			.host_addr = TMC9660_DEFAULT_HOST_ADDR,
			.current_bank = 0xFF,
			.initialized = false,
		},
	},
	[TMC9660_MOTOR_B] = {
		.name = "Motor B",
		.state = {
			.device_addr = TMC9660_DEFAULT_DEVICE_ADDR,
			.host_addr = TMC9660_DEFAULT_HOST_ADDR,
			.current_bank = 0xFF,
			.initialized = false,
		},
	},
	[TMC9660_MOTOR_C] = {
		.name = "Motor C",
		.state = {
			.device_addr = TMC9660_DEFAULT_DEVICE_ADDR,
			.host_addr = TMC9660_DEFAULT_HOST_ADDR,
			.current_bank = 0xFF,
			.initialized = false,
		},
	},
};

/* Timeouts */
#define TMC9660_REPLY_TIMEOUT_MS  100
#define TMC9660_BYTE_TIMEOUT_MS   10

/**
 * Calculate CRC8 checksum for TMC9660 protocol
 * Polynomial: x^8 + x^2 + x + 1 (0x07)
 * Initial value: 0x00
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
 * Send UART data
 */
static int uart_send(const struct device *uart, const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart, data[i]);
	}
	return 0;
}

/**
 * Receive UART data with timeout
 */
static int uart_recv(const struct device *uart, uint8_t *data, size_t len, k_timeout_t timeout)
{
	int64_t end_time = k_uptime_get() + timeout.ticks;

	for (size_t i = 0; i < len; i++) {
		int ret;
		do {
			ret = uart_poll_in(uart, &data[i]);
			if (ret == 0) {
				break;  /* Byte received */
			}
			if (k_uptime_get() >= end_time) {
				return -ETIMEDOUT;
			}
			k_sleep(K_USEC(100));
		} while (ret != 0);
	}

	return 0;
}

/**
 * Send command and receive reply
 */
static int tmc9660_transact(tmc9660_instance_t *inst, uint8_t cmd, uint32_t req_value,
			     uint32_t *reply_value, uint8_t *reply_status)
{
	tmc9660_msg_t req, reply;
	int ret;

	/* Build request message */
	req.sync_or_host = TMC9660_SYNC_BYTE;
	req.device_addr = inst->state.device_addr;
	req.cmd_or_status = cmd;
	pack_u32_msb(req.data, req_value);
	req.crc8 = tmc9660_crc8((uint8_t *)&req, TMC9660_MSG_SIZE - 1);

	/* Send request */
	ret = uart_send(inst->uart_dev, (uint8_t *)&req, TMC9660_MSG_SIZE);
	if (ret < 0) {
		return ret;
	}

	/* Receive reply */
	ret = uart_recv(inst->uart_dev, (uint8_t *)&reply, TMC9660_MSG_SIZE,
			K_MSEC(TMC9660_REPLY_TIMEOUT_MS));
	if (ret < 0) {
		return ret;
	}

	/* Verify CRC */
	uint8_t expected_crc = tmc9660_crc8((uint8_t *)&reply, TMC9660_MSG_SIZE - 1);
	if (reply.crc8 != expected_crc) {
		return -EBADMSG;
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
		return -EIO;
	}

	return 0;
}

int tmc9660_init(tmc9660_motor_id_t motor)
{
	int ret;
	uint32_t value;
	tmc9660_instance_t *inst;

	if (motor >= TMC9660_NUM_MOTORS) {
		return -EINVAL;
	}

	inst = &motors[motor];

	k_mutex_init(&inst->mutex);
	k_mutex_lock(&inst->mutex, K_FOREVER);

	/* Get UART device */
	switch (motor) {
	case TMC9660_MOTOR_A:
		inst->uart_dev = DEVICE_DT_GET(DT_ALIAS(tmc9660a));
		break;
	case TMC9660_MOTOR_B:
		inst->uart_dev = DEVICE_DT_GET(DT_ALIAS(tmc9660b));
		break;
	case TMC9660_MOTOR_C:
		inst->uart_dev = DEVICE_DT_GET(DT_ALIAS(tmc9660c));
		break;
	default:
		LOG_ERR("%s: Invalid motor ID", inst->name);
		k_mutex_unlock(&inst->mutex);
		return -EINVAL;
	}

	if (!inst->uart_dev || !device_is_ready(inst->uart_dev)) {
		LOG_ERR("%s: UART device not ready", inst->name);
		k_mutex_unlock(&inst->mutex);
		return -ENODEV;
	}

	LOG_INF("%s: UART initialized", inst->name);

	/* Small delay for chip startup */
	k_sleep(K_MSEC(10));

	/* Verify chip type */
	ret = tmc9660_transact(inst, TMC9660_CMD_GET_INFO, TMC9660_INFO_CHIP_TYPE,
			       &value, NULL);
	if (ret < 0) {
		LOG_WRN("%s: Failed to read chip type: %d (chip may not be connected)",
			inst->name, ret);
		k_mutex_unlock(&inst->mutex);
		return ret;
	}

	inst->state.chip_type = value;
	if (value != TMC9660_CHIP_TYPE_EXPECTED) {
		LOG_ERR("%s: Unexpected chip type: 0x%08X (expected 0x%08X)",
			inst->name, value, TMC9660_CHIP_TYPE_EXPECTED);
		k_mutex_unlock(&inst->mutex);
		return -ENODEV;
	}

	LOG_INF("%s: Chip type verified: 0x%08X", inst->name, value);

	/* Read chip version */
	ret = tmc9660_transact(inst, TMC9660_CMD_GET_INFO, TMC9660_INFO_CHIP_VERSION,
			       &value, NULL);
	if (ret == 0) {
		inst->state.chip_version = value;
		LOG_INF("%s: Chip version: %u", inst->name, value);
	}

	/* Read bootloader version */
	ret = tmc9660_transact(inst, TMC9660_CMD_GET_INFO, TMC9660_INFO_BL_VERSION,
			       &value, NULL);
	if (ret == 0) {
		inst->state.bootloader_version = value;
		uint16_t major = (value >> 16) & 0xFFFF;
		uint16_t minor = value & 0xFFFF;
		LOG_INF("%s: Bootloader version: %u.%u", inst->name, major, minor);
	}

	inst->state.initialized = true;
	k_mutex_unlock(&inst->mutex);

	LOG_INF("%s: initialized successfully", inst->name);
	return 0;
}

int tmc9660_init_all(void)
{
	int ret;
	int success_count = 0;

	LOG_INF("Initializing all TMC9660 motors...");

	for (int i = 0; i < TMC9660_NUM_MOTORS; i++) {
		ret = tmc9660_init((tmc9660_motor_id_t)i);
		if (ret == 0) {
			success_count++;
		}
	}

	if (success_count == 0) {
		LOG_ERR("No TMC9660 motors initialized");
		return -ENODEV;
	}

	LOG_INF("TMC9660 initialization complete: %d/%d motors ready",
		success_count, TMC9660_NUM_MOTORS);

	return (success_count == TMC9660_NUM_MOTORS) ? 0 : -ENODEV;
}

bool tmc9660_is_ready(tmc9660_motor_id_t motor)
{
	if (motor >= TMC9660_NUM_MOTORS) {
		return false;
	}
	return motors[motor].state.initialized;
}

int tmc9660_get_info(tmc9660_motor_id_t motor, uint8_t info_selector, uint32_t *value)
{
	tmc9660_instance_t *inst;
	int ret;

	if (motor >= TMC9660_NUM_MOTORS || !value) {
		return -EINVAL;
	}

	inst = &motors[motor];
	k_mutex_lock(&inst->mutex, K_FOREVER);
	ret = tmc9660_transact(inst, TMC9660_CMD_GET_INFO, info_selector, value, NULL);
	k_mutex_unlock(&inst->mutex);

	return ret;
}

int tmc9660_set_bank(tmc9660_motor_id_t motor, uint8_t bank)
{
	tmc9660_instance_t *inst;
	int ret;
	uint32_t reply_value;

	if (motor >= TMC9660_NUM_MOTORS) {
		return -EINVAL;
	}

	inst = &motors[motor];
	k_mutex_lock(&inst->mutex, K_FOREVER);

	/* Only send if bank is different */
	if (inst->state.current_bank == bank) {
		k_mutex_unlock(&inst->mutex);
		return 0;
	}

	ret = tmc9660_transact(inst, TMC9660_CMD_SET_BANK, bank, &reply_value, NULL);
	if (ret == 0) {
		inst->state.current_bank = bank;
	}

	k_mutex_unlock(&inst->mutex);
	return ret;
}

int tmc9660_set_address(tmc9660_motor_id_t motor, uint32_t addr)
{
	tmc9660_instance_t *inst;
	int ret;
	uint32_t reply_value;

	if (motor >= TMC9660_NUM_MOTORS) {
		return -EINVAL;
	}

	inst = &motors[motor];
	k_mutex_lock(&inst->mutex, K_FOREVER);

	ret = tmc9660_transact(inst, TMC9660_CMD_SET_ADDRESS, addr, &reply_value, NULL);
	if (ret == 0) {
		inst->state.current_addr = addr;
	}

	k_mutex_unlock(&inst->mutex);
	return ret;
}

int tmc9660_read_32(tmc9660_motor_id_t motor, uint32_t *value)
{
	tmc9660_instance_t *inst;
	int ret;

	if (motor >= TMC9660_NUM_MOTORS || !value) {
		return -EINVAL;
	}

	inst = &motors[motor];
	k_mutex_lock(&inst->mutex, K_FOREVER);
	ret = tmc9660_transact(inst, TMC9660_CMD_READ_32, 0, value, NULL);
	k_mutex_unlock(&inst->mutex);

	return ret;
}

int tmc9660_write_32(tmc9660_motor_id_t motor, uint32_t value)
{
	tmc9660_instance_t *inst;
	int ret;
	uint32_t reply_value;

	if (motor >= TMC9660_NUM_MOTORS) {
		return -EINVAL;
	}

	inst = &motors[motor];
	k_mutex_lock(&inst->mutex, K_FOREVER);
	ret = tmc9660_transact(inst, TMC9660_CMD_WRITE_32, value, &reply_value, NULL);
	k_mutex_unlock(&inst->mutex);

	return ret;
}

int tmc9660_read_config(tmc9660_motor_id_t motor, uint8_t offset, uint32_t *value)
{
	int ret;

	if (motor >= TMC9660_NUM_MOTORS || !value || offset >= TMC9660_CONFIG_SIZE) {
		return -EINVAL;
	}

	/* Ensure offset is 4-byte aligned */
	if (offset & 0x03) {
		return -EINVAL;
	}

	/* Set bank to CONFIG */
	ret = tmc9660_set_bank(motor, TMC9660_BANK_CONFIG);
	if (ret < 0) {
		return ret;
	}

	/* Set address */
	ret = tmc9660_set_address(motor, TMC9660_CONFIG_BASE_ADDR + offset);
	if (ret < 0) {
		return ret;
	}

	/* Read value */
	return tmc9660_read_32(motor, value);
}

int tmc9660_write_config(tmc9660_motor_id_t motor, uint8_t offset, uint32_t value)
{
	int ret;

	if (motor >= TMC9660_NUM_MOTORS || offset >= TMC9660_CONFIG_SIZE) {
		return -EINVAL;
	}

	/* Ensure offset is 4-byte aligned */
	if (offset & 0x03) {
		return -EINVAL;
	}

	/* Set bank to CONFIG */
	ret = tmc9660_set_bank(motor, TMC9660_BANK_CONFIG);
	if (ret < 0) {
		return ret;
	}

	/* Set address */
	ret = tmc9660_set_address(motor, TMC9660_CONFIG_BASE_ADDR + offset);
	if (ret < 0) {
		return ret;
	}

	/* Write value */
	return tmc9660_write_32(motor, value);
}

void tmc9660_get_state(tmc9660_motor_id_t motor, tmc9660_state_t *state_out)
{
	tmc9660_instance_t *inst;

	if (motor >= TMC9660_NUM_MOTORS || !state_out) {
		return;
	}

	inst = &motors[motor];
	k_mutex_lock(&inst->mutex, K_FOREVER);
	memcpy(state_out, &inst->state, sizeof(tmc9660_state_t));
	k_mutex_unlock(&inst->mutex);
}

int tmc9660_no_op(tmc9660_motor_id_t motor)
{
	tmc9660_instance_t *inst;
	int ret;
	uint32_t reply_value;

	if (motor >= TMC9660_NUM_MOTORS) {
		return -EINVAL;
	}

	inst = &motors[motor];
	k_mutex_lock(&inst->mutex, K_FOREVER);
	ret = tmc9660_transact(inst, TMC9660_CMD_NO_OP, 0, &reply_value, NULL);
	k_mutex_unlock(&inst->mutex);

	return ret;
}
