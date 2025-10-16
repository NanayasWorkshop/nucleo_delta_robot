/*
 * CRC16-CCITT Header - Wrapper for Zephyr's Built-in CRC16
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/sys/crc.h>  /* Zephyr's built-in CRC functions */

/**
 * Calculate CRC16-CCITT checksum
 * Wrapper for Zephyr's crc16_ccitt with standard initial value
 *
 * @param data Pointer to data buffer
 * @param length Length of data in bytes
 * @return CRC16-CCITT value
 */
static inline uint16_t crc16_ccitt_calc(const uint8_t *data, size_t length)
{
	return crc16_ccitt(0xFFFF, data, length);
}

/**
 * Verify CRC16 checksum
 *
 * @param data Pointer to data buffer (including 2-byte CRC at end)
 * @param length Total length including CRC
 * @return true if CRC is valid, false otherwise
 */
static inline bool crc16_verify(const uint8_t *data, size_t length)
{
	if (length < 2) {
		return false;
	}

	/* Calculate CRC of data (excluding last 2 bytes which are the CRC) */
	uint16_t calculated_crc = crc16_ccitt(0xFFFF, data, length - 2);

	/* Extract CRC from packet (little-endian) */
	uint16_t packet_crc = data[length - 2] | (data[length - 1] << 8);

	return calculated_crc == packet_crc;
}

#endif /* CRC16_H */
