/*
 * Packet Protocol Implementation - Phase 3
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "packet.h"
#include "crc16.h"
#include "imu.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* Current system state */
static uint8_t current_mode = MODE_IDLE;
static bool emergency_stop_active = false;
static uint16_t error_count = 0;
static uint8_t last_error = ERROR_NO_ERROR;

/* Segment ID (configured at runtime, default 0) */
static uint8_t my_segment_id = 0;

void packet_set_segment_id(uint8_t id)
{
	my_segment_id = id;
}

int packet_parse_command(const uint8_t *data, size_t length)
{
	if (length < 6) {
		printk("[Packet] Error: Packet too short (%zu bytes)\n", length);
		error_count++;
		last_error = ERROR_CRC_ERROR;
		return -1;
	}

	/* Check magic header */
	uint16_t magic = data[0] | (data[1] << 8);
	if (magic != PACKET_MAGIC_MASTER_TO_STM32) {
		printk("[Packet] Error: Invalid magic header 0x%04X\n", magic);
		error_count++;
		last_error = ERROR_CRC_ERROR;
		return -1;
	}

	/* Verify CRC */
	if (!crc16_verify(data, length)) {
		printk("[Packet] Error: CRC check failed\n");
		error_count++;
		last_error = ERROR_CRC_ERROR;
		return -1;
	}

	/* Get packet type */
	uint8_t packet_type = data[2];

	printk("[Packet] Received valid packet: type=0x%02X, length=%zu\n",
	       packet_type, length);

	/* Handle based on packet type */
	switch (packet_type) {
	case CMD_TRAJECTORY:
		if (length == sizeof(trajectory_packet_t)) {
			packet_handle_trajectory((const trajectory_packet_t *)data);
		} else {
			printk("[Packet] Error: TRAJECTORY size mismatch\n");
			return -1;
		}
		break;

	case CMD_EMERGENCY_STOP:
		if (length == sizeof(emergency_stop_packet_t)) {
			packet_handle_emergency_stop((const emergency_stop_packet_t *)data);
		} else {
			printk("[Packet] Error: EMERGENCY_STOP size mismatch\n");
			return -1;
		}
		break;

	case CMD_START_HOMING:
		if (length == sizeof(start_homing_packet_t)) {
			const start_homing_packet_t *pkt = (const start_homing_packet_t *)data;
			printk("[Packet] START_HOMING: mode=%d\n", pkt->homing_mode);
			current_mode = MODE_HOMING;
		}
		break;

	case CMD_JOG_MOTOR:
		if (length == sizeof(jog_motor_packet_t)) {
			const jog_motor_packet_t *pkt = (const jog_motor_packet_t *)data;
			printk("[Packet] JOG_MOTOR: motor=%d, value=%.2f, speed=%d%%\n",
			       pkt->motor_id, pkt->value, pkt->speed_percent);
		}
		break;

	case CMD_SET_MODE:
		if (length == sizeof(set_mode_packet_t)) {
			packet_handle_set_mode((const set_mode_packet_t *)data);
		}
		break;

	case CMD_SET_ZERO_OFFSET:
		if (length == sizeof(set_zero_offset_packet_t)) {
			printk("[Packet] SET_ZERO_OFFSET command received\n");
			/* Phase 9: Save zero offsets to flash */
		}
		break;

	default:
		printk("[Packet] Warning: Unknown packet type 0x%02X\n", packet_type);
		return -1;
	}

	return packet_type;
}

void packet_handle_emergency_stop(const emergency_stop_packet_t *pkt)
{
	printk("\n");
	printk("╔═══════════════════════════════╗\n");
	printk("║   EMERGENCY STOP ACTIVATED    ║\n");
	printk("╚═══════════════════════════════╝\n");
	printk("Reason: 0x%02X\n", pkt->stop_reason);
	printk("Target: segment %d\n", pkt->segment_id);

	/* Check if broadcast or targeted to us */
	if (pkt->segment_id == 0xFF || pkt->segment_id == my_segment_id) {
		emergency_stop_active = true;
		current_mode = MODE_IDLE;

		printk(">>> Motors DISABLED <<<\n");
		printk("\n");

		/* Phase 7: Actually stop motors here */
	}
}

void packet_handle_set_mode(const set_mode_packet_t *pkt)
{
	const char *mode_name;

	switch (pkt->mode) {
	case MODE_IDLE:
		mode_name = "IDLE";
		break;
	case MODE_HOMING:
		mode_name = "HOMING";
		break;
	case MODE_OPERATION:
		mode_name = "OPERATION";
		break;
	default:
		mode_name = "UNKNOWN";
		break;
	}

	printk("[Packet] SET_MODE: %s (0x%02X)\n", mode_name, pkt->mode);

	current_mode = pkt->mode;

	if (pkt->mode == MODE_OPERATION) {
		emergency_stop_active = false;
	}
}

void packet_handle_trajectory(const trajectory_packet_t *pkt)
{
	printk("[Packet] TRAJECTORY: id=%u, start=%u, duration=%u ms\n",
	       pkt->trajectory_id, pkt->start_timestamp, pkt->duration_ms);

	/* Phase 6: Add to trajectory buffer */
	/* For now, just print first coefficient of each motor */
	printk("  Motor 1 a0=%.3f, Motor 2 a0=%.3f, Motor 3 a0=%.3f\n",
	       pkt->motor_1_coeffs[0],
	       pkt->motor_2_coeffs[0],
	       pkt->motor_3_coeffs[0]);
}

void packet_build_motor_state(motor_state_packet_t *pkt, uint8_t segment_id)
{
	memset(pkt, 0, sizeof(*pkt));

	pkt->magic_header = PACKET_MAGIC_STM32_TO_MASTER;
	pkt->packet_type = FEEDBACK_MOTOR_STATE;
	pkt->segment_id = segment_id;
	pkt->timestamp = k_uptime_get_32();

	/* Phase 4: Get real IMU data */
	/* Phase 5: Get real motor positions/velocities */
	/* Phase 7: Get real currents and calculated values */

	/* For now, fill with zeros (placeholders) */
	pkt->motor_1_position = 0.0f;
	pkt->motor_1_velocity = 0.0f;
	pkt->motor_1_acceleration = 0.0f;
	pkt->motor_1_jerk = 0.0f;
	pkt->motor_1_current = 0.0f;

	pkt->motor_2_position = 0.0f;
	pkt->motor_2_velocity = 0.0f;
	pkt->motor_2_acceleration = 0.0f;
	pkt->motor_2_jerk = 0.0f;
	pkt->motor_2_current = 0.0f;

	pkt->motor_3_position = 0.0f;
	pkt->motor_3_velocity = 0.0f;
	pkt->motor_3_acceleration = 0.0f;
	pkt->motor_3_jerk = 0.0f;
	pkt->motor_3_current = 0.0f;

	/* Phase 4: Get real IMU orientation */
	if (imu_is_valid()) {
		imu_get_orientation(&pkt->imu_roll, &pkt->imu_pitch, &pkt->imu_yaw);
	} else {
		pkt->imu_roll = 0.0f;
		pkt->imu_pitch = 0.0f;
		pkt->imu_yaw = 0.0f;
	}

	pkt->status_flags = packet_get_status_flags();

	/* Calculate CRC */
	pkt->crc16 = crc16_ccitt(0xFFFF, (uint8_t *)pkt, sizeof(*pkt) - 2);
}

void packet_build_diagnostics(diagnostics_packet_t *pkt, uint8_t segment_id)
{
	memset(pkt, 0, sizeof(*pkt));

	pkt->magic_header = PACKET_MAGIC_STM32_TO_MASTER;
	pkt->packet_type = FEEDBACK_DIAGNOSTICS;
	pkt->segment_id = segment_id;
	pkt->timestamp = k_uptime_get_32();

	/* Phase 5: Get real TMC9660 temperatures */
	pkt->tmc9660_temp_avg = 25.0f;  /* Placeholder */

	/* STM32 internal temperature (could implement in Phase 3) */
	pkt->stm32_temp = 30.0f;  /* Placeholder */

	pkt->error_count = error_count;
	pkt->last_error_code = last_error;

	/* CPU usage - Zephyr can provide this */
	pkt->cpu_usage = 10;  /* Placeholder */

	/* Calculate CRC */
	pkt->crc16 = crc16_ccitt(0xFFFF, (uint8_t *)pkt, sizeof(*pkt) - 2);
}

uint8_t packet_get_status_flags(void)
{
	uint8_t flags = 0;

	if (emergency_stop_active) {
		flags |= STATUS_E_STOP_ACTIVE;
	}

	if (current_mode == MODE_HOMING) {
		flags |= STATUS_HOMING_IN_PROGRESS;
	}

	if (current_mode == MODE_OPERATION) {
		flags |= STATUS_TRAJECTORY_EXECUTING;
	}

	/* Phase 9: Add calibration valid flag */
	/* Phase 6: Add buffer empty flag */
	/* Phase 7: Add position/force limit flags */

	if (last_error != ERROR_NO_ERROR) {
		flags |= STATUS_ERROR_PRESENT;
	}

	return flags;
}
