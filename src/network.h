/*
 * Network Layer - Phase 2: Ethernet Networking
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>

/* Network configuration from packet-protocol-specification.yaml */
#define TCP_LISTEN_PORT 5000
#define UDP_LISTEN_PORT 6000

/* DHCP configuration */
#define EXPECTED_IP_RANGE_START "192.168.1.100"
#define EXPECTED_IP_RANGE_END   "192.168.1.107"

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

#endif /* NETWORK_H */
