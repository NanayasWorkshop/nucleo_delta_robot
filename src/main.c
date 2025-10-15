/*
 * Segment Controller Firmware - Phase 3: Packet Protocol
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/version.h>
#include "network.h"
#include "packet.h"

/* Segment ID - default 0 (unconfigured) */
#define MY_SEGMENT_ID 0

/* Feedback thread timing */
#define DIAGNOSTICS_INTERVAL_MS 1000  /* 1 Hz */

static bool servers_started = false;

int main(void)
{
	int ret;
	char ip_addr[32];
	uint32_t last_diag_time = 0;

	printk("\n");
	printk("========================================\n");
	printk("  Segment Controller Firmware\n");
	printk("========================================\n");
	printk("Board: %s\n", CONFIG_BOARD);
	printk("Zephyr Version: %s\n", KERNEL_VERSION_STRING);
	printk("========================================\n\n");

	printk("[Phase 1] Basic Bringup - SUCCESS\n");

	/* Set segment ID */
	packet_set_segment_id(MY_SEGMENT_ID);

	/* Phase 2: Initialize networking */
	ret = network_init();
	if (ret < 0) {
		printk("ERROR: Network initialization failed: %d\n", ret);
		printk("Phase 2 FAILED - stopping here\n");
		return ret;
	}

	printk("[Phase 2] Network initialization started\n");
	printk("Waiting for DHCP to assign IP address...\n\n");

	/* Main loop */
	while (1) {
		k_sleep(K_SECONDS(1));

		if (network_is_ready()) {
			/* Start servers once after DHCP success */
			if (!servers_started) {
				ret = network_start_servers();
				if (ret < 0) {
					printk("ERROR: Failed to start servers: %d\n", ret);
					return ret;
				}
				servers_started = true;

				printk("[Phase 3] Packet Protocol - READY\n");
				printk("Listening for commands on:\n");
				printk("  - TCP port %d (trajectory, config)\n", TCP_LISTEN_PORT);
				printk("  - UDP port %d (emergency stop)\n\n", UDP_LISTEN_PORT);

				if (network_get_ip_address(ip_addr, sizeof(ip_addr)) == 0) {
					printk("Ready to receive packets at: %s\n\n", ip_addr);
				}
			}

			/* Send diagnostics packet periodically (1 Hz) */
			uint32_t now = k_uptime_get_32();
			if (now - last_diag_time >= DIAGNOSTICS_INTERVAL_MS) {
				diagnostics_packet_t diag_pkt;
				packet_build_diagnostics(&diag_pkt, MY_SEGMENT_ID);

				/* Try to send via TCP (will fail if no client connected) */
				ret = network_send_tcp((uint8_t *)&diag_pkt, sizeof(diag_pkt));
				if (ret > 0) {
					printk("[Feedback] Sent DIAGNOSTICS packet (%d bytes)\n", ret);
				}

				last_diag_time = now;
			}

		} else {
			/* Still waiting for network */
			printk("Heartbeat: Waiting for network...\n");
		}
	}

	return 0;
}
