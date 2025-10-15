/*
 * CRC16-CCITT Implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * Calculate CRC16-CCITT checksum
 *
 * Polynomial: 0x1021
 * Initial value: 0xFFFF
 *
 * @param data Pointer to data buffer
 * @param length Length of data in bytes
 * @return CRC16 checksum
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t length);

/**
 * Verify CRC16 checksum of a packet
 *
 * @param data Pointer to packet data (including CRC at end)
 * @param length Total length including CRC bytes
 * @return true if CRC is valid, false otherwise
 */
bool crc16_verify(const uint8_t *data, size_t length);

#endif /* CRC16_H */
