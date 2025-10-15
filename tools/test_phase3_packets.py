#!/usr/bin/env python3
"""
Phase 3: Packet Protocol Testing Tool
Tests binary packet communication with STM32 Nucleo-H753ZI firmware

Supports:
- Sending command packets (TRAJECTORY, EMERGENCY_STOP, SET_MODE, etc.)
- Receiving feedback packets (MOTOR_STATE, DIAGNOSTICS)
- CRC16-CCITT validation
- Interactive testing mode
"""

import socket
import struct
import time
import sys
from typing import Optional, Tuple

# ==================================================
# PACKET PROTOCOL CONSTANTS
# ==================================================

# Magic headers
MAGIC_MASTER_TO_STM32 = 0xAA55
MAGIC_STM32_TO_MASTER = 0xBB55

# Command packet types (Master → STM32)
CMD_TRAJECTORY = 0x01
CMD_EMERGENCY_STOP = 0x02
CMD_START_HOMING = 0x03
CMD_JOG_MOTOR = 0x07
CMD_SET_MODE = 0x08
CMD_SET_ZERO_OFFSET = 0x09

# Feedback packet types (STM32 → Master)
FEEDBACK_MOTOR_STATE = 0x01
FEEDBACK_CAPACITIVE_GRID = 0x02
FEEDBACK_DIAGNOSTICS = 0x03

# Operating modes
MODE_IDLE = 0x01
MODE_HOMING = 0x02
MODE_OPERATION = 0x03

# Network configuration
TCP_PORT = 5000
UDP_PORT = 6000

# ==================================================
# CRC16-CCITT IMPLEMENTATION
# ==================================================

def crc16_ccitt(data: bytes) -> int:
    """
    Calculate CRC16-CCITT checksum
    Polynomial: 0x1021, Initial: 0xFFFF
    """
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc

def verify_crc(packet: bytes) -> bool:
    """Verify CRC of received packet"""
    if len(packet) < 2:
        return False
    data = packet[:-2]
    expected_crc = struct.unpack('<H', packet[-2:])[0]
    calculated_crc = crc16_ccitt(data)
    return calculated_crc == expected_crc

# ==================================================
# PACKET BUILDERS
# ==================================================

def build_emergency_stop(segment_id: int = 0xFF, stop_reason: int = 0x01) -> bytes:
    """
    Build EMERGENCY_STOP packet (7 bytes)
    segment_id: 0xFF = broadcast to all segments
    stop_reason: reason code for stopping
    """
    packet = struct.pack('<HBBBxx',
        MAGIC_MASTER_TO_STM32,  # magic_header
        CMD_EMERGENCY_STOP,     # packet_type
        segment_id,             # segment_id
        stop_reason             # stop_reason
    )
    # Calculate CRC on everything except the CRC field itself
    crc = crc16_ccitt(packet[:-2])
    packet = packet[:-2] + struct.pack('<H', crc)
    return packet

def build_set_mode(segment_id: int, mode: int) -> bytes:
    """
    Build SET_MODE packet (7 bytes)
    mode: MODE_IDLE, MODE_HOMING, or MODE_OPERATION
    """
    packet = struct.pack('<HBBBxx',
        MAGIC_MASTER_TO_STM32,  # magic_header
        CMD_SET_MODE,           # packet_type
        segment_id,             # segment_id
        mode                    # mode
    )
    crc = crc16_ccitt(packet[:-2])
    packet = packet[:-2] + struct.pack('<H', crc)
    return packet

def build_start_homing(segment_id: int, homing_mode: int = 0x01) -> bytes:
    """
    Build START_HOMING packet (7 bytes)
    homing_mode: 0x01=full, 0x02=quick verify
    """
    packet = struct.pack('<HBBBxx',
        MAGIC_MASTER_TO_STM32,  # magic_header
        CMD_START_HOMING,       # packet_type
        segment_id,             # segment_id
        homing_mode             # homing_mode
    )
    crc = crc16_ccitt(packet[:-2])
    packet = packet[:-2] + struct.pack('<H', crc)
    return packet

def build_jog_motor(segment_id: int, motor_id: int, mode: int, value: float, speed_percent: int) -> bytes:
    """
    Build JOG_MOTOR packet (13 bytes)
    motor_id: 1, 2, or 3
    mode: 0x01=mm, 0x02=encoder ticks
    value: distance to move
    speed_percent: 0-100%
    """
    packet = struct.pack('<HBBBBfBxx',
        MAGIC_MASTER_TO_STM32,  # magic_header
        CMD_JOG_MOTOR,          # packet_type
        segment_id,             # segment_id
        motor_id,               # motor_id
        mode,                   # mode
        value,                  # value
        speed_percent           # speed_percent
    )
    crc = crc16_ccitt(packet[:-2])
    packet = packet[:-2] + struct.pack('<H', crc)
    return packet

def build_trajectory(segment_id: int, trajectory_id: int, start_timestamp: int,
                     duration_ms: int, motor_1_coeffs: list, motor_2_coeffs: list,
                     motor_3_coeffs: list) -> bytes:
    """
    Build TRAJECTORY packet (112 bytes)
    coeffs: list of 8 floats (septic polynomial a0-a7)
    """
    if len(motor_1_coeffs) != 8 or len(motor_2_coeffs) != 8 or len(motor_3_coeffs) != 8:
        raise ValueError("Each motor must have exactly 8 coefficients")

    packet = struct.pack('<HBBIIHxx',
        MAGIC_MASTER_TO_STM32,  # magic_header
        CMD_TRAJECTORY,         # packet_type
        segment_id,             # segment_id
        trajectory_id,          # trajectory_id
        start_timestamp,        # start_timestamp
        duration_ms             # duration_ms
    )

    # Add motor coefficients (8 floats per motor)
    for coeff in motor_1_coeffs:
        packet += struct.pack('<f', coeff)
    for coeff in motor_2_coeffs:
        packet += struct.pack('<f', coeff)
    for coeff in motor_3_coeffs:
        packet += struct.pack('<f', coeff)

    # Add CRC
    crc = crc16_ccitt(packet)
    packet += struct.pack('<H', crc)

    return packet

# ==================================================
# PACKET PARSERS
# ==================================================

def parse_motor_state(packet: bytes) -> Optional[dict]:
    """Parse MOTOR_STATE feedback packet (83 bytes)"""
    if len(packet) != 83:
        print(f"Error: Expected 83 bytes for MOTOR_STATE, got {len(packet)}")
        return None

    if not verify_crc(packet):
        print("Error: CRC check failed for MOTOR_STATE packet")
        return None

    # Unpack the packet
    data = struct.unpack('<HBBIffffffffffffffffffffffffBxx', packet[:-2])

    return {
        'magic_header': data[0],
        'packet_type': data[1],
        'segment_id': data[2],
        'timestamp': data[3],
        'motor_1': {
            'position': data[4],
            'velocity': data[5],
            'acceleration': data[6],
            'jerk': data[7],
            'current': data[8]
        },
        'motor_2': {
            'position': data[9],
            'velocity': data[10],
            'acceleration': data[11],
            'jerk': data[12],
            'current': data[13]
        },
        'motor_3': {
            'position': data[14],
            'velocity': data[15],
            'acceleration': data[16],
            'jerk': data[17],
            'current': data[18]
        },
        'imu': {
            'roll': data[19],
            'pitch': data[20],
            'yaw': data[21]
        },
        'status_flags': data[22]
    }

def parse_diagnostics(packet: bytes) -> Optional[dict]:
    """Parse DIAGNOSTICS feedback packet (26 bytes)"""
    if len(packet) != 26:
        print(f"Error: Expected 26 bytes for DIAGNOSTICS, got {len(packet)}")
        return None

    if not verify_crc(packet):
        print("Error: CRC check failed for DIAGNOSTICS packet")
        return None

    # Unpack the packet
    data = struct.unpack('<HBBIffHBBxx', packet[:-2])

    return {
        'magic_header': data[0],
        'packet_type': data[1],
        'segment_id': data[2],
        'timestamp': data[3],
        'tmc9660_temp_avg': data[4],
        'stm32_temp': data[5],
        'error_count': data[6],
        'last_error_code': data[7],
        'cpu_usage': data[8]
    }

# ==================================================
# TEST FUNCTIONS
# ==================================================

def test_emergency_stop(ip_address: str):
    """Test EMERGENCY_STOP packet via UDP"""
    print("\n=== Testing EMERGENCY_STOP (UDP) ===")

    packet = build_emergency_stop(segment_id=0xFF, stop_reason=0x01)
    print(f"Built EMERGENCY_STOP packet: {len(packet)} bytes")
    print(f"Hex: {packet.hex()}")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(2.0)
        sock.sendto(packet, (ip_address, UDP_PORT))
        print(f"Sent EMERGENCY_STOP to {ip_address}:{UDP_PORT}")

        # Try to receive response (may not get one for E-stop)
        try:
            data, addr = sock.recvfrom(1024)
            print(f"Received response: {len(data)} bytes from {addr}")
        except socket.timeout:
            print("No response (expected for emergency stop)")

        sock.close()
        return True
    except Exception as e:
        print(f"Error: {e}")
        return False

def test_set_mode(ip_address: str, mode: int = MODE_IDLE):
    """Test SET_MODE packet via TCP"""
    print(f"\n=== Testing SET_MODE (TCP) - Mode {mode} ===")

    packet = build_set_mode(segment_id=0, mode=mode)
    print(f"Built SET_MODE packet: {len(packet)} bytes")
    print(f"Hex: {packet.hex()}")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((ip_address, TCP_PORT))
        print(f"Connected to {ip_address}:{TCP_PORT}")

        sock.send(packet)
        print("Sent SET_MODE packet")

        # Try to receive feedback
        time.sleep(0.5)
        try:
            data = sock.recv(1024)
            if len(data) > 0:
                print(f"Received response: {len(data)} bytes")
                # Try to parse as diagnostics
                if len(data) == 26:
                    diag = parse_diagnostics(data)
                    if diag:
                        print(f"DIAGNOSTICS: Segment {diag['segment_id']}, Temp: {diag['stm32_temp']:.1f}°C, CPU: {diag['cpu_usage']}%")
        except socket.timeout:
            print("No immediate response")

        sock.close()
        return True
    except Exception as e:
        print(f"Error: {e}")
        return False

def test_receive_diagnostics(ip_address: str, duration: int = 5):
    """Connect via TCP and listen for DIAGNOSTICS packets"""
    print(f"\n=== Listening for DIAGNOSTICS packets for {duration}s ===")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(duration + 1)
        sock.connect((ip_address, TCP_PORT))
        print(f"Connected to {ip_address}:{TCP_PORT}")

        start_time = time.time()
        packet_count = 0

        while time.time() - start_time < duration:
            try:
                data = sock.recv(1024)
                if len(data) == 0:
                    print("Connection closed")
                    break

                packet_count += 1
                print(f"\nReceived packet #{packet_count}: {len(data)} bytes")

                # Try to parse
                if len(data) == 26:
                    diag = parse_diagnostics(data)
                    if diag:
                        print(f"  Segment ID: {diag['segment_id']}")
                        print(f"  Timestamp: {diag['timestamp']} ms")
                        print(f"  TMC9660 Temp: {diag['tmc9660_temp_avg']:.1f}°C")
                        print(f"  STM32 Temp: {diag['stm32_temp']:.1f}°C")
                        print(f"  Error Count: {diag['error_count']}")
                        print(f"  Last Error: 0x{diag['last_error_code']:02X}")
                        print(f"  CPU Usage: {diag['cpu_usage']}%")
                elif len(data) == 83:
                    motor_state = parse_motor_state(data)
                    if motor_state:
                        print(f"  MOTOR_STATE packet (not implemented yet)")
                else:
                    print(f"  Unknown packet type, length: {len(data)}")
                    print(f"  Hex: {data.hex()}")

            except socket.timeout:
                continue

        sock.close()
        print(f"\nReceived {packet_count} packets in {duration}s")
        return True

    except Exception as e:
        print(f"Error: {e}")
        return False

def interactive_mode(ip_address: str):
    """Interactive testing mode"""
    print("\n" + "="*50)
    print("Phase 3 Packet Protocol - Interactive Mode")
    print("="*50)

    while True:
        print("\nAvailable tests:")
        print("  1. Send EMERGENCY_STOP (UDP)")
        print("  2. Send SET_MODE IDLE (TCP)")
        print("  3. Send SET_MODE HOMING (TCP)")
        print("  4. Send SET_MODE OPERATION (TCP)")
        print("  5. Listen for DIAGNOSTICS packets (5s)")
        print("  6. Send simple test trajectory")
        print("  q. Quit")

        choice = input("\nEnter choice: ").strip().lower()

        if choice == 'q':
            break
        elif choice == '1':
            test_emergency_stop(ip_address)
        elif choice == '2':
            test_set_mode(ip_address, MODE_IDLE)
        elif choice == '3':
            test_set_mode(ip_address, MODE_HOMING)
        elif choice == '4':
            test_set_mode(ip_address, MODE_OPERATION)
        elif choice == '5':
            test_receive_diagnostics(ip_address, duration=5)
        elif choice == '6':
            # Simple trajectory: constant position (a0 = 100mm, all other coeffs = 0)
            coeffs = [100.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
            packet = build_trajectory(segment_id=0, trajectory_id=1,
                                     start_timestamp=0, duration_ms=1000,
                                     motor_1_coeffs=coeffs,
                                     motor_2_coeffs=coeffs,
                                     motor_3_coeffs=coeffs)
            print(f"\nBuilt TRAJECTORY packet: {len(packet)} bytes")
            print("Sending via TCP...")
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5.0)
                sock.connect((ip_address, TCP_PORT))
                sock.send(packet)
                print("Sent TRAJECTORY packet")
                sock.close()
            except Exception as e:
                print(f"Error: {e}")
        else:
            print("Invalid choice")

# ==================================================
# MAIN
# ==================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: ./test_phase3_packets.py <board_ip_address> [mode]")
        print("  mode: 'interactive' (default) or 'auto'")
        sys.exit(1)

    ip_address = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else 'interactive'

    print("="*50)
    print("Phase 3: Packet Protocol Test Tool")
    print("="*50)
    print(f"Target IP: {ip_address}")
    print(f"TCP Port: {TCP_PORT}")
    print(f"UDP Port: {UDP_PORT}")

    if mode == 'interactive':
        interactive_mode(ip_address)
    else:
        # Auto mode - run all tests
        print("\n=== Running automatic tests ===")
        test_set_mode(ip_address, MODE_IDLE)
        time.sleep(1)
        test_receive_diagnostics(ip_address, duration=3)
        time.sleep(1)
        test_emergency_stop(ip_address)
        print("\n=== Tests complete ===")

if __name__ == '__main__':
    main()
