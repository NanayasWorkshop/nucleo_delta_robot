/*
 * Packet Protocol Definitions - Phase 3
 * Based on packet-protocol-specification.yaml
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Magic headers */
#define PACKET_MAGIC_MASTER_TO_STM32  0xAA55
#define PACKET_MAGIC_STM32_TO_MASTER  0xBB55

/* Command packet types (Master → STM32) */
#define CMD_TRAJECTORY        0x01
#define CMD_EMERGENCY_STOP    0x02
#define CMD_START_HOMING      0x03
#define CMD_JOG_MOTOR         0x07
#define CMD_SET_MODE          0x08
#define CMD_SET_ZERO_OFFSET   0x09

/* Feedback packet types (STM32 → Master) */
#define FEEDBACK_MOTOR_STATE     0x01
#define FEEDBACK_CAPACITIVE_GRID 0x02
#define FEEDBACK_DIAGNOSTICS     0x03

/* Operating modes */
#define MODE_IDLE       0x01
#define MODE_HOMING     0x02
#define MODE_OPERATION  0x03

/* Status flags (bit field in motor_state packet) */
#define STATUS_E_STOP_ACTIVE          (1 << 0)
#define STATUS_HOMING_IN_PROGRESS     (1 << 1)
#define STATUS_POSITION_LIMIT_HIT     (1 << 2)
#define STATUS_FORCE_LIMIT_EXCEEDED   (1 << 3)
#define STATUS_BUFFER_EMPTY           (1 << 4)
#define STATUS_TRAJECTORY_EXECUTING   (1 << 5)
#define STATUS_CALIBRATION_VALID      (1 << 6)
#define STATUS_ERROR_PRESENT          (1 << 7)

/* Error codes */
#define ERROR_NO_ERROR              0x00
#define ERROR_E_STOP_TRIGGERED      0x01
#define ERROR_MOTOR_OVERCURRENT     0x02
#define ERROR_POSITION_LIMIT        0x03
#define ERROR_COMMUNICATION_TIMEOUT 0x04
#define ERROR_CRC_ERROR             0x05
#define ERROR_INVALID_TRAJECTORY    0x06
#define ERROR_ENCODER_FAULT         0x07
#define ERROR_TMC9660_FAULT         0x08
#define ERROR_IMU_FAULT             0x09
#define ERROR_CAPACITIVE_FAULT      0x0A
#define ERROR_FLASH_ERROR           0x0B
#define ERROR_BUFFER_OVERRUN        0x0C
#define ERROR_TEMPERATURE_HIGH      0x0D

/* ========================================
 * COMMAND PACKETS (Master → STM32)
 * ======================================== */

/**
 * Trajectory Command (0x01) - 112 bytes
 * TCP, 5 Hz
 */
typedef struct __attribute__((packed)) {
	uint16_t magic_header;           /* 0xAA55 */
	uint8_t  packet_type;            /* 0x01 */
	uint8_t  segment_id;             /* 1-8 */
	uint32_t trajectory_id;          /* Unique ID */
	uint32_t start_timestamp;        /* ms since boot */
	uint16_t duration_ms;            /* Trajectory duration */
	float    motor_1_coeffs[8];      /* Septic polynomial a0-a7 */
	float    motor_2_coeffs[8];
	float    motor_3_coeffs[8];
	uint16_t crc16;
} trajectory_packet_t;

/**
 * Emergency Stop (0x02) - 7 bytes
 * UDP, emergency only
 */
typedef struct __attribute__((packed)) {
	uint16_t magic_header;           /* 0xAA55 */
	uint8_t  packet_type;            /* 0x02 */
	uint8_t  segment_id;             /* Target segment (0xFF = broadcast) */
	uint8_t  stop_reason;            /* Why stopping */
	uint16_t crc16;
} emergency_stop_packet_t;

/**
 * Start Homing (0x03) - 7 bytes
 * TCP, once per startup
 */
typedef struct __attribute__((packed)) {
	uint16_t magic_header;           /* 0xAA55 */
	uint8_t  packet_type;            /* 0x03 */
	uint8_t  segment_id;
	uint8_t  homing_mode;            /* 0x01=full, 0x02=quick verify */
	uint16_t crc16;
} start_homing_packet_t;

/**
 * Jog Motor (0x07) - 13 bytes
 * TCP, manual calibration
 */
typedef struct __attribute__((packed)) {
	uint16_t magic_header;           /* 0xAA55 */
	uint8_t  packet_type;            /* 0x07 */
	uint8_t  segment_id;
	uint8_t  motor_id;               /* 1, 2, or 3 */
	uint8_t  mode;                   /* 0x01=mm, 0x02=encoder ticks */
	float    value;                  /* Distance to move */
	uint8_t  speed_percent;          /* 0-100% */
	uint16_t crc16;
} jog_motor_packet_t;

/**
 * Set Mode (0x08) - 7 bytes
 * TCP
 */
typedef struct __attribute__((packed)) {
	uint16_t magic_header;           /* 0xAA55 */
	uint8_t  packet_type;            /* 0x08 */
	uint8_t  segment_id;
	uint8_t  mode;                   /* 0x01=IDLE, 0x02=HOMING, 0x03=OPERATION */
	uint16_t crc16;
} set_mode_packet_t;

/**
 * Set Zero Offset (0x09) - 6 bytes
 * TCP, calibration
 */
typedef struct __attribute__((packed)) {
	uint16_t magic_header;           /* 0xAA55 */
	uint8_t  packet_type;            /* 0x09 */
	uint8_t  segment_id;
	uint16_t crc16;
} set_zero_offset_packet_t;

/* ========================================
 * FEEDBACK PACKETS (STM32 → Master)
 * ======================================== */

/**
 * Motor State (0x01) - 83 bytes
 * UDP, 100 Hz
 */
typedef struct __attribute__((packed)) {
	uint16_t magic_header;           /* 0xBB55 */
	uint8_t  packet_type;            /* 0x01 */
	uint8_t  segment_id;
	uint32_t timestamp;              /* ms since boot */

	/* Motor 1 */
	float    motor_1_position;       /* mm */
	float    motor_1_velocity;       /* mm/s */
	float    motor_1_acceleration;   /* mm/s² */
	float    motor_1_jerk;           /* mm/s³ */
	float    motor_1_current;        /* Amps */

	/* Motor 2 */
	float    motor_2_position;
	float    motor_2_velocity;
	float    motor_2_acceleration;
	float    motor_2_jerk;
	float    motor_2_current;

	/* Motor 3 */
	float    motor_3_position;
	float    motor_3_velocity;
	float    motor_3_acceleration;
	float    motor_3_jerk;
	float    motor_3_current;

	/* IMU */
	float    imu_roll;               /* radians */
	float    imu_pitch;              /* radians */
	float    imu_yaw;                /* radians */

	uint8_t  status_flags;           /* Status bits */
	uint16_t crc16;
} motor_state_packet_t;

/**
 * Diagnostics (0x03) - 26 bytes
 * TCP, 1 Hz
 */
typedef struct __attribute__((packed)) {
	uint16_t magic_header;           /* 0xBB55 */
	uint8_t  packet_type;            /* 0x03 */
	uint8_t  segment_id;
	uint32_t timestamp;              /* ms since boot */
	float    tmc9660_temp_avg;       /* °C */
	float    stm32_temp;             /* °C */
	uint16_t error_count;
	uint8_t  last_error_code;
	uint8_t  cpu_usage;              /* 0-100% */
	uint16_t crc16;
} diagnostics_packet_t;

/* ========================================
 * PACKET HANDLING FUNCTIONS
 * ======================================== */

/**
 * Set this segment's ID
 *
 * @param id Segment ID (0-8, 0=unconfigured)
 */
void packet_set_segment_id(uint8_t id);

/**
 * Parse and validate received command packet
 *
 * @param data Pointer to received packet data
 * @param length Length of received data
 * @return Packet type on success, -1 on error
 */
int packet_parse_command(const uint8_t *data, size_t length);

/**
 * Build motor state feedback packet
 *
 * @param pkt Pointer to packet structure to fill
 * @param segment_id This segment's ID
 */
void packet_build_motor_state(motor_state_packet_t *pkt, uint8_t segment_id);

/**
 * Build diagnostics feedback packet
 *
 * @param pkt Pointer to packet structure to fill
 * @param segment_id This segment's ID
 */
void packet_build_diagnostics(diagnostics_packet_t *pkt, uint8_t segment_id);

/**
 * Handle EMERGENCY_STOP command
 *
 * @param pkt Pointer to parsed packet
 */
void packet_handle_emergency_stop(const emergency_stop_packet_t *pkt);

/**
 * Handle SET_MODE command
 *
 * @param pkt Pointer to parsed packet
 */
void packet_handle_set_mode(const set_mode_packet_t *pkt);

/**
 * Handle TRAJECTORY command
 *
 * @param pkt Pointer to parsed packet
 */
void packet_handle_trajectory(const trajectory_packet_t *pkt);

/**
 * Get current system status flags
 *
 * @return Status flags byte
 */
uint8_t packet_get_status_flags(void);

#endif /* PACKET_H */
