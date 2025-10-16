/*
 * IMU (LSM6DSO) Driver and Sensor Fusion
 * Phase 4: IMU Integration
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include <stdbool.h>

/* IMU data structure */
typedef struct {
	/* Raw sensor data */
	float accel_x;  /* m/s² */
	float accel_y;
	float accel_z;

	float gyro_x;   /* rad/s */
	float gyro_y;
	float gyro_z;

	/* Fused orientation (Madgwick filter output) */
	float roll;     /* radians */
	float pitch;    /* radians */
	float yaw;      /* radians */

	/* Status */
	bool valid;     /* True if IMU is working */
	uint32_t last_update_ms;  /* Timestamp of last update */
} imu_data_t;

/**
 * Initialize IMU subsystem
 *
 * @return 0 on success, negative on error
 */
int imu_init(void);

/**
 * Update IMU readings (call at 100 Hz from control loop)
 * Reads accelerometer and gyroscope, runs Madgwick filter
 *
 * @return 0 on success, negative on error
 */
int imu_update(void);

/**
 * Get current IMU data
 *
 * @param data Pointer to structure to fill with current data
 */
void imu_get_data(imu_data_t *data);

/**
 * Get orientation in radians
 *
 * @param roll Output roll angle
 * @param pitch Output pitch angle
 * @param yaw Output yaw angle
 */
void imu_get_orientation(float *roll, float *pitch, float *yaw);

/**
 * Check if IMU is working
 *
 * @return true if IMU initialized and responding
 */
bool imu_is_valid(void);

#endif /* IMU_H */
