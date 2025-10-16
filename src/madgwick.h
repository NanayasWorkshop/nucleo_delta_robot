/*
 * Madgwick AHRS Algorithm
 * Based on: https://x-io.co.uk/open-source-imu-and-ahrs-algorithms/
 *
 * Simplified single-file implementation for embedded systems
 */

#ifndef MADGWICK_H
#define MADGWICK_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
	float q0, q1, q2, q3;  /* Quaternion elements */
	float beta;             /* Filter gain */
	float sample_freq;      /* Sample frequency in Hz */
} madgwick_t;

/**
 * Initialize Madgwick filter
 *
 * @param m Pointer to Madgwick structure
 * @param sample_freq Sampling frequency in Hz
 * @param beta Filter gain (typically 0.1 to 0.5, higher = faster convergence but more noise)
 */
static inline void madgwick_init(madgwick_t *m, float sample_freq, float beta)
{
	m->q0 = 1.0f;
	m->q1 = 0.0f;
	m->q2 = 0.0f;
	m->q3 = 0.0f;
	m->beta = beta;
	m->sample_freq = sample_freq;
}

/**
 * Update filter with gyroscope and accelerometer data
 *
 * @param m Pointer to Madgwick structure
 * @param gx Gyroscope x-axis (rad/s)
 * @param gy Gyroscope y-axis (rad/s)
 * @param gz Gyroscope z-axis (rad/s)
 * @param ax Accelerometer x-axis (any units, will be normalized)
 * @param ay Accelerometer y-axis
 * @param az Accelerometer z-axis
 */
static inline void madgwick_update(madgwick_t *m, float gx, float gy, float gz,
                                    float ax, float ay, float az)
{
	float recip_norm;
	float s0, s1, s2, s3;
	float q_dot1, q_dot2, q_dot3, q_dot4;
	float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

	/* Rate of change of quaternion from gyroscope */
	q_dot1 = 0.5f * (-m->q1 * gx - m->q2 * gy - m->q3 * gz);
	q_dot2 = 0.5f * (m->q0 * gx + m->q2 * gz - m->q3 * gy);
	q_dot3 = 0.5f * (m->q0 * gy - m->q1 * gz + m->q3 * gx);
	q_dot4 = 0.5f * (m->q0 * gz + m->q1 * gy - m->q2 * gx);

	/* Compute feedback only if accelerometer measurement valid */
	if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
		/* Normalize accelerometer measurement */
		recip_norm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
		ax *= recip_norm;
		ay *= recip_norm;
		az *= recip_norm;

		/* Auxiliary variables to avoid repeated arithmetic */
		_2q0 = 2.0f * m->q0;
		_2q1 = 2.0f * m->q1;
		_2q2 = 2.0f * m->q2;
		_2q3 = 2.0f * m->q3;
		_4q0 = 4.0f * m->q0;
		_4q1 = 4.0f * m->q1;
		_4q2 = 4.0f * m->q2;
		_8q1 = 8.0f * m->q1;
		_8q2 = 8.0f * m->q2;
		q0q0 = m->q0 * m->q0;
		q1q1 = m->q1 * m->q1;
		q2q2 = m->q2 * m->q2;
		q3q3 = m->q3 * m->q3;

		/* Gradient descent algorithm corrective step */
		s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
		s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * m->q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 +
		     _8q1 * q2q2 + _4q1 * az;
		s2 = 4.0f * q0q0 * m->q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 +
		     _8q2 * q2q2 + _4q2 * az;
		s3 = 4.0f * q1q1 * m->q3 - _2q1 * ax + 4.0f * q2q2 * m->q3 - _2q2 * ay;

		/* Normalize step magnitude */
		recip_norm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
		s0 *= recip_norm;
		s1 *= recip_norm;
		s2 *= recip_norm;
		s3 *= recip_norm;

		/* Apply feedback step */
		q_dot1 -= m->beta * s0;
		q_dot2 -= m->beta * s1;
		q_dot3 -= m->beta * s2;
		q_dot4 -= m->beta * s3;
	}

	/* Integrate rate of change of quaternion to yield quaternion */
	m->q0 += q_dot1 * (1.0f / m->sample_freq);
	m->q1 += q_dot2 * (1.0f / m->sample_freq);
	m->q2 += q_dot3 * (1.0f / m->sample_freq);
	m->q3 += q_dot4 * (1.0f / m->sample_freq);

	/* Normalize quaternion */
	recip_norm = 1.0f / sqrtf(m->q0 * m->q0 + m->q1 * m->q1 + m->q2 * m->q2 + m->q3 * m->q3);
	m->q0 *= recip_norm;
	m->q1 *= recip_norm;
	m->q2 *= recip_norm;
	m->q3 *= recip_norm;
}

/**
 * Get Euler angles from quaternion
 *
 * @param m Pointer to Madgwick structure
 * @param roll Output roll angle (radians)
 * @param pitch Output pitch angle (radians)
 * @param yaw Output yaw angle (radians)
 */
static inline void madgwick_get_euler(madgwick_t *m, float *roll, float *pitch, float *yaw)
{
	/* Roll (x-axis rotation) */
	float sinr_cosp = 2.0f * (m->q0 * m->q1 + m->q2 * m->q3);
	float cosr_cosp = 1.0f - 2.0f * (m->q1 * m->q1 + m->q2 * m->q2);
	*roll = atan2f(sinr_cosp, cosr_cosp);

	/* Pitch (y-axis rotation) */
	float sinp = 2.0f * (m->q0 * m->q2 - m->q3 * m->q1);
	if (fabsf(sinp) >= 1.0f) {
		*pitch = copysignf(M_PI / 2.0f, sinp);  /* Use 90 degrees if out of range */
	} else {
		*pitch = asinf(sinp);
	}

	/* Yaw (z-axis rotation) */
	float siny_cosp = 2.0f * (m->q0 * m->q3 + m->q1 * m->q2);
	float cosy_cosp = 1.0f - 2.0f * (m->q2 * m->q2 + m->q3 * m->q3);
	*yaw = atan2f(siny_cosp, cosy_cosp);
}

#endif /* MADGWICK_H */
