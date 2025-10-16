#!/usr/bin/env python3
"""
Mock IMU Server - Simulates Nucleo board sending MOTOR_STATE packets
Useful for testing the 3D visualizer without actual hardware
"""

import socket
import struct
import time
import math
import sys

# Packet protocol constants
MAGIC_HEADER_STM32_TO_MASTER = 0xBB55
PACKET_TYPE_MOTOR_STATE = 0x01

def calculate_crc16_ccitt(data):
    """Calculate CRC16-CCITT checksum (polynomial 0x1021, init 0xFFFF)"""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc

def build_motor_state_packet(timestamp, roll, pitch, yaw):
    """Build a MOTOR_STATE packet with simulated data"""

    # Header
    magic = MAGIC_HEADER_STM32_TO_MASTER
    packet_type = PACKET_TYPE_MOTOR_STATE
    segment_id = 0

    # Motor data (3 motors × 5 floats: position, velocity, acceleration, jerk, current)
    # All zeros for mock (only IMU matters for visualization)
    motor_data = [0.0] * 15  # 3 motors × 5 values

    # IMU data
    imu_roll = roll
    imu_pitch = pitch
    imu_yaw = yaw

    # Status flags
    status_flags = 0x20  # TRAJECTORY_EXECUTING bit set

    # Build packet without CRC
    packet = struct.pack('<HBBI', magic, packet_type, segment_id, timestamp)
    packet += struct.pack('<15f', *motor_data)  # Motor data
    packet += struct.pack('<3f', imu_roll, imu_pitch, imu_yaw)  # IMU data
    packet += struct.pack('<B', status_flags)

    # Calculate CRC
    crc = calculate_crc16_ccitt(packet)
    packet += struct.pack('<H', crc)

    return packet

def run_server(host='0.0.0.0', port=5000, animation_mode='rotate'):
    """
    Run mock IMU server

    Animation modes:
    - 'rotate': Slowly rotate around all axes
    - 'tilt': Tilt back and forth (pitch)
    - 'wobble': Random wobbling motion
    - 'still': Static orientation
    """

    print("=== Mock IMU Server ===")
    print(f"Animation mode: {animation_mode}")
    print(f"Listening on {host}:{port}")
    print("\nWaiting for client connection...")
    print("Run the visualizer with: python3 imu_visualizer_3d.py 127.0.0.1")
    print("\nPress Ctrl+C to stop\n")

    # Create TCP socket
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind((host, port))
    server_sock.listen(1)

    try:
        while True:
            # Wait for client connection
            client_sock, addr = server_sock.accept()
            print(f"✓ Client connected from {addr[0]}:{addr[1]}")

            # Send packets at ~100 Hz
            timestamp = 0
            start_time = time.time()

            try:
                while True:
                    elapsed = time.time() - start_time
                    timestamp = int(elapsed * 1000)  # ms since start

                    # Generate orientation based on animation mode
                    if animation_mode == 'rotate':
                        # Slow rotation around all axes
                        roll = math.sin(elapsed * 0.5) * 0.5  # ±0.5 rad (±28°)
                        pitch = math.sin(elapsed * 0.3) * 0.3  # ±0.3 rad (±17°)
                        yaw = elapsed * 0.2  # Continuous rotation

                    elif animation_mode == 'tilt':
                        # Tilt back and forth (pitch only)
                        roll = 0.0
                        pitch = math.sin(elapsed * 1.0) * 0.8  # ±0.8 rad (±46°)
                        yaw = 0.0

                    elif animation_mode == 'wobble':
                        # Random wobbling
                        roll = math.sin(elapsed * 2.1) * 0.4 + math.cos(elapsed * 1.3) * 0.2
                        pitch = math.sin(elapsed * 1.7) * 0.3 + math.cos(elapsed * 2.5) * 0.3
                        yaw = math.sin(elapsed * 0.9) * 0.5

                    elif animation_mode == 'still':
                        # Static orientation (slightly tilted)
                        roll = 0.1
                        pitch = -0.2
                        yaw = 0.0

                    else:
                        roll = pitch = yaw = 0.0

                    # Build and send packet
                    packet = build_motor_state_packet(timestamp, roll, pitch, yaw)
                    client_sock.sendall(packet)

                    # Print status
                    roll_deg = math.degrees(roll)
                    pitch_deg = math.degrees(pitch)
                    yaw_deg = math.degrees(yaw)
                    print(f"\r[{timestamp:6d}ms] Roll: {roll_deg:7.2f}°  Pitch: {pitch_deg:7.2f}°  Yaw: {yaw_deg:7.2f}°", end='')

                    # Sleep to maintain ~100 Hz
                    time.sleep(0.01)

            except (BrokenPipeError, ConnectionResetError):
                print("\n\n✗ Client disconnected")
                client_sock.close()
                print("Waiting for new connection...\n")

    except KeyboardInterrupt:
        print("\n\n✓ Server stopped")

    finally:
        server_sock.close()

def main():
    if len(sys.argv) > 1:
        animation_mode = sys.argv[1]
    else:
        animation_mode = 'rotate'

    valid_modes = ['rotate', 'tilt', 'wobble', 'still']
    if animation_mode not in valid_modes:
        print(f"Invalid animation mode: {animation_mode}")
        print(f"Valid modes: {', '.join(valid_modes)}")
        print("\nUsage: python3 mock_imu_server.py [mode]")
        print("  rotate  - Slowly rotate around all axes (default)")
        print("  tilt    - Tilt back and forth")
        print("  wobble  - Random wobbling motion")
        print("  still   - Static orientation")
        sys.exit(1)

    run_server(animation_mode=animation_mode)

if __name__ == "__main__":
    main()
