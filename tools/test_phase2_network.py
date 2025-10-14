#!/usr/bin/env python3
"""
Phase 2 Network Testing Tool

Tests Ethernet/DHCP functionality of segment controller firmware.

Usage:
    python3 test_phase2_network.py --board-ip 192.168.1.100
    python3 test_phase2_network.py --scan  # Auto-detect board

Requirements:
    - Board connected via Ethernet
    - Board running Phase 2 firmware
    - DHCP server running (or static IP configured)
"""

import socket
import argparse
import sys
import subprocess
import time

TCP_PORT = 5000
UDP_PORT = 6000
EXPECTED_IP_RANGE = range(100, 108)  # 192.168.1.100-107


def ping_board(ip_address, count=3):
    """Ping the board to check basic connectivity"""
    print(f"\n[Test 1/4] Pinging board at {ip_address}...")
    try:
        result = subprocess.run(
            ['ping', '-c', str(count), '-W', '2', ip_address],
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            print(f"✓ Ping successful! Board is reachable.")
            return True
        else:
            print(f"✗ Ping failed. Board not responding.")
            return False
    except FileNotFoundError:
        print("✗ 'ping' command not found. Skipping ping test.")
        return None


def test_tcp_connection(ip_address):
    """Test TCP connection on port 5000"""
    print(f"\n[Test 2/4] Testing TCP connection to {ip_address}:{TCP_PORT}...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        result = sock.connect_ex((ip_address, TCP_PORT))
        sock.close()

        if result == 0:
            print(f"✓ TCP port {TCP_PORT} is open and accepting connections!")
            return True
        else:
            print(f"✗ TCP port {TCP_PORT} is not accessible (connection refused)")
            print(f"  This is EXPECTED for Phase 2 - TCP socket not yet implemented")
            print(f"  TCP functionality will be added in Phase 3")
            return None  # Not a failure for Phase 2
    except socket.timeout:
        print(f"✗ TCP connection timed out")
        return False
    except Exception as e:
        print(f"✗ TCP test error: {e}")
        return False


def test_udp_socket(ip_address):
    """Test UDP socket on port 6000"""
    print(f"\n[Test 3/4] Testing UDP socket to {ip_address}:{UDP_PORT}...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(2)

        # Send a test packet
        test_message = b"PHASE2_TEST"
        sock.sendto(test_message, (ip_address, UDP_PORT))
        print(f"✓ UDP test packet sent to {ip_address}:{UDP_PORT}")
        print(f"  This is EXPECTED for Phase 2 - UDP socket not yet implemented")
        print(f"  UDP functionality will be added in Phase 3")

        sock.close()
        return None  # Not a failure for Phase 2
    except Exception as e:
        print(f"✗ UDP test error: {e}")
        return False


def check_ip_range(ip_address):
    """Verify IP is in expected DHCP range"""
    print(f"\n[Test 4/4] Checking IP address range...")
    try:
        last_octet = int(ip_address.split('.')[-1])
        if last_octet in EXPECTED_IP_RANGE:
            print(f"✓ IP {ip_address} is in expected DHCP range (192.168.1.100-107)")
            return True
        else:
            print(f"⚠ IP {ip_address} is outside expected range")
            print(f"  Expected: 192.168.1.100-107")
            print(f"  This might be OK if you have a different DHCP configuration")
            return None
    except:
        print(f"✗ Could not parse IP address")
        return False


def scan_for_board():
    """Scan for board in expected IP range"""
    print("\nScanning for board in range 192.168.1.100-107...")
    print("This may take up to 30 seconds...")

    for i in EXPECTED_IP_RANGE:
        ip = f"192.168.1.{i}"
        try:
            result = subprocess.run(
                ['ping', '-c', '1', '-W', '1', ip],
                capture_output=True,
                timeout=2
            )
            if result.returncode == 0:
                print(f"\n✓ Found responsive device at {ip}")
                return ip
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        sys.stdout.write('.')
        sys.stdout.flush()

    print("\n✗ No board found in expected IP range")
    return None


def main():
    parser = argparse.ArgumentParser(description='Test Phase 2 Network Functionality')
    parser.add_argument('--board-ip', type=str, help='IP address of the board')
    parser.add_argument('--scan', action='store_true', help='Auto-scan for board')
    args = parser.parse_args()

    print("=" * 60)
    print("  Phase 2 Network Testing Tool")
    print("=" * 60)

    # Determine board IP
    board_ip = None
    if args.scan:
        board_ip = scan_for_board()
        if not board_ip:
            print("\nTroubleshooting:")
            print("1. Is the Ethernet cable connected?")
            print("2. Is the board powered on?")
            print("3. Check serial console for DHCP success message")
            print("4. Try specifying IP manually: --board-ip 192.168.1.X")
            sys.exit(1)
    elif args.board_ip:
        board_ip = args.board_ip
    else:
        print("Error: Must specify --board-ip or --scan")
        parser.print_help()
        sys.exit(1)

    print(f"\nTesting board at: {board_ip}")
    print("=" * 60)

    # Run tests
    results = []
    results.append(("Ping Test", ping_board(board_ip)))
    results.append(("TCP Connection", test_tcp_connection(board_ip)))
    results.append(("UDP Socket", test_udp_socket(board_ip)))
    results.append(("IP Range Check", check_ip_range(board_ip)))

    # Summary
    print("\n" + "=" * 60)
    print("  Test Summary")
    print("=" * 60)

    passed = sum(1 for _, result in results if result is True)
    failed = sum(1 for _, result in results if result is False)
    skipped = sum(1 for _, result in results if result is None)

    for test_name, result in results:
        status = "✓ PASS" if result is True else "✗ FAIL" if result is False else "⊘ N/A"
        print(f"{test_name:20s} {status}")

    print("=" * 60)
    print(f"Passed: {passed}  |  Failed: {failed}  |  N/A: {skipped}")

    if failed == 0 and passed >= 1:
        print("\n✓ Phase 2 Basic Network Testing: SUCCESS")
        print("  The board has network connectivity via Ethernet!")
        print("  Next step: Flash Phase 3 firmware (Packet Protocol)")
        sys.exit(0)
    else:
        print("\n✗ Phase 2 Testing: INCOMPLETE")
        print("  Check serial console output for errors")
        sys.exit(1)


if __name__ == "__main__":
    main()
