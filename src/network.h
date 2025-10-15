/*
 * Network Layer - Phase 3: TCP/UDP Sockets
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <stdint.h>
#include <stddef.h>

/* Network configuration from packet-protocol-specification.yaml */
#define TCP_LISTEN_PORT 5000
#define UDP_LISTEN_PORT 6000
#define MASTER_TCP_PORT 5000

/* DHCP configuration */
#define EXPECTED_IP_RANGE_START "192.168.1.100"
#define EXPECTED_IP_RANGE_END   "192.168.1.107"

/* Receive buffer size */
#define NETWORK_RX_BUFFER_SIZE 512

/**
 * Initialize network subsystem
 * - Brings up Ethernet interface
 * - Starts DHCP client
 * - Creates UDP and TCP sockets
 *
 * @return 0 on success, negative errno on failure
 */
int network_init(void);

/**
 * Start TCP/UDP servers (call after DHCP succeeds)
 *
 * @return 0 on success, negative errno on failure
 */
int network_start_servers(void);

/**
 * Get current IP address as string
 *
 * @param buf Buffer to store IP address string
 * @param buflen Length of buffer
 * @return 0 on success, negative errno on failure
 */
int network_get_ip_address(char *buf, size_t buflen);

/**
 * Check if network is ready (IP address assigned)
 *
 * @return true if network is ready, false otherwise
 */
bool network_is_ready(void);

/**
 * Send UDP packet to master
 *
 * @param data Pointer to packet data
 * @param length Length of data in bytes
 * @return Number of bytes sent, or negative errno on failure
 */
int network_send_udp(const uint8_t *data, size_t length);

/**
 * Send TCP packet to master
 *
 * @param data Pointer to packet data
 * @param length Length of data in bytes
 * @return Number of bytes sent, or negative errno on failure
 */
int network_send_tcp(const uint8_t *data, size_t length);

#endif /* NETWORK_H */
