/*
 * IMU (LSM6DSO) Driver and Sensor Fusion
 * Phase 4: IMU Integration
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "imu.h"
#include "madgwick.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <math.h>

/* LSM6DSO device */
static const struct device *lsm6dso_dev = NULL;

/* Current IMU data */
static imu_data_t current_data = {0};

/* Madgwick filter instance */
static madgwick_t madgwick;

/* IMU configuration */
#define IMU_SAMPLE_FREQ 100.0f   /* 100 Hz update rate */
#define MADGWICK_BETA   0.1f     /* Filter gain */

/* Conversion factors */
#define ACCEL_SENSITIVITY_2G  0.000061f  /* LSM6DSO: 0.061 mg/LSB for ±2g */
#define GYRO_SENSITIVITY_2000DPS  0.070f  /* LSM6DSO: 70 mdps/LSB for ±2000dps */
#define DEG_TO_RAD  (M_PI / 180.0f)

int imu_init(void)
{
	printk("\n[Phase 4] Initializing IMU (LSM6DSO)...\n");

	/* Get LSM6DSO device from device tree */
	lsm6dso_dev = DEVICE_DT_GET_ONE(st_lsm6dso);

	if (!lsm6dso_dev) {
		printk("ERROR: LSM6DSO device not found in device tree\n");
		return -ENODEV;
	}

	if (!device_is_ready(lsm6dso_dev)) {
		printk("ERROR: LSM6DSO device is not ready\n");
		return -ENODEV;
	}

	printk("LSM6DSO device found: %s\n", lsm6dso_dev->name);

	/* Initialize Madgwick filter */
	madgwick_init(&madgwick, IMU_SAMPLE_FREQ, MADGWICK_BETA);
	printk("Madgwick filter initialized (beta=%.2f, freq=%.0f Hz)\n",
	       MADGWICK_BETA, IMU_SAMPLE_FREQ);

	/* Set up sensor attributes if needed */
	struct sensor_value odr;
	odr.val1 = 104;  /* 104 Hz ODR */
	odr.val2 = 0;

	int ret = sensor_attr_set(lsm6dso_dev, SENSOR_CHAN_ACCEL_XYZ,
	                          SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	if (ret < 0) {
		printk("Warning: Could not set accelerometer ODR: %d\n", ret);
	}

	ret = sensor_attr_set(lsm6dso_dev, SENSOR_CHAN_GYRO_XYZ,
	                      SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	if (ret < 0) {
		printk("Warning: Could not set gyroscope ODR: %d\n", ret);
	}

	/* Test reading to verify communication */
	ret = sensor_sample_fetch(lsm6dso_dev);
	if (ret < 0) {
		printk("ERROR: Failed to fetch initial sample: %d\n", ret);
		current_data.valid = false;
		return ret;
	}

	printk("[Phase 4] IMU initialized successfully\n");
	current_data.valid = true;
	current_data.last_update_ms = k_uptime_get_32();

	return 0;
}

int imu_update(void)
{
	if (!lsm6dso_dev || !current_data.valid) {
		return -ENODEV;
	}

	/* Fetch new sensor data */
	int ret = sensor_sample_fetch(lsm6dso_dev);
	if (ret < 0) {
		printk("IMU: Sample fetch failed: %d\n", ret);
		current_data.valid = false;
		return ret;
	}

	/* Read accelerometer */
	struct sensor_value accel[3];
	ret = sensor_channel_get(lsm6dso_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
	if (ret < 0) {
		printk("IMU: Accel read failed: %d\n", ret);
		return ret;
	}

	/* Read gyroscope */
	struct sensor_value gyro[3];
	ret = sensor_channel_get(lsm6dso_dev, SENSOR_CHAN_GYRO_XYZ, gyro);
	if (ret < 0) {
		printk("IMU: Gyro read failed: %d\n", ret);
		return ret;
	}

	/* Convert to SI units */
	/* Accelerometer: sensor_value is in m/s² */
	current_data.accel_x = sensor_value_to_float(&accel[0]);
	current_data.accel_y = sensor_value_to_float(&accel[1]);
	current_data.accel_z = sensor_value_to_float(&accel[2]);

	/* Gyroscope: sensor_value is in rad/s */
	current_data.gyro_x = sensor_value_to_float(&gyro[0]);
	current_data.gyro_y = sensor_value_to_float(&gyro[1]);
	current_data.gyro_z = sensor_value_to_float(&gyro[2]);

	/* Update Madgwick filter */
	madgwick_update(&madgwick,
	                current_data.gyro_x, current_data.gyro_y, current_data.gyro_z,
	                current_data.accel_x, current_data.accel_y, current_data.accel_z);

	/* Get orientation from quaternion */
	madgwick_get_euler(&madgwick,
	                   &current_data.roll,
	                   &current_data.pitch,
	                   &current_data.yaw);

	current_data.last_update_ms = k_uptime_get_32();

	return 0;
}

void imu_get_data(imu_data_t *data)
{
	if (data) {
		memcpy(data, &current_data, sizeof(imu_data_t));
	}
}

void imu_get_orientation(float *roll, float *pitch, float *yaw)
{
	if (roll) {
		*roll = current_data.roll;
	}
	if (pitch) {
		*pitch = current_data.pitch;
	}
	if (yaw) {
		*yaw = current_data.yaw;
	}
}

bool imu_is_valid(void)
{
	return current_data.valid;
}
