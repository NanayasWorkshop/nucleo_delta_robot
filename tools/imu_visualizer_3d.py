#!/usr/bin/env python3
"""
Real-Time 3D IMU Visualization using OpenGL
Connects to Nucleo board and displays orientation in 3D
"""

import socket
import struct
import sys
import math
from OpenGL.GL import *
from OpenGL.GLU import *
import pygame
from pygame.locals import *

# Packet protocol constants
MAGIC_HEADER_STM32_TO_MASTER = 0xBB55
PACKET_TYPE_MOTOR_STATE = 0x01

class IMUVisualizer:
    def __init__(self, board_ip, port=5000):
        self.board_ip = board_ip
        self.port = port
        self.sock = None

        # Current orientation (radians)
        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0

        # PyGame and OpenGL setup
        pygame.init()
        self.display = (800, 600)
        pygame.display.set_mode(self.display, DOUBLEBUF | OPENGL)
        pygame.display.set_caption(f"IMU Orientation - {board_ip}")

        # Setup OpenGL perspective
        glEnable(GL_DEPTH_TEST)
        gluPerspective(45, (self.display[0] / self.display[1]), 0.1, 50.0)
        glTranslatef(0.0, 0.0, -5)

    def connect(self):
        """Connect to the Nucleo board via TCP"""
        print(f"Connecting to {self.board_ip}:{self.port}...")
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(2.0)
        try:
            self.sock.connect((self.board_ip, self.port))
            print("Connected! Waiting for MOTOR_STATE packets...")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    def receive_motor_state(self):
        """Receive and parse MOTOR_STATE packet"""
        try:
            # Read header (2 bytes magic + 1 byte type + 1 byte segment_id + 4 bytes timestamp)
            header = self.sock.recv(8)
            if len(header) < 8:
                return False

            magic, pkt_type, segment_id, timestamp = struct.unpack('<HBBI', header)

            if magic != MAGIC_HEADER_STM32_TO_MASTER or pkt_type != PACKET_TYPE_MOTOR_STATE:
                return False

            # Read motor data (3 motors × 5 floats = 60 bytes)
            motor_data = self.sock.recv(60)
            if len(motor_data) < 60:
                return False

            # Read IMU data (3 floats = 12 bytes)
            imu_data = self.sock.recv(12)
            if len(imu_data) < 12:
                return False

            self.roll, self.pitch, self.yaw = struct.unpack('<fff', imu_data)

            # Read status flags + CRC (1 + 2 = 3 bytes)
            footer = self.sock.recv(3)

            return True

        except socket.timeout:
            return False
        except Exception as e:
            print(f"Error receiving packet: {e}")
            return False

    def draw_cube(self):
        """Draw a 3D box representing the board"""
        # Define vertices for a rectangular box (board-like proportions)
        vertices = [
            # Top face (Z = 0.2)
            [-1, -0.5, 0.2], [1, -0.5, 0.2], [1, 0.5, 0.2], [-1, 0.5, 0.2],
            # Bottom face (Z = -0.2)
            [-1, -0.5, -0.2], [1, -0.5, -0.2], [1, 0.5, -0.2], [-1, 0.5, -0.2],
        ]

        # Define edges
        edges = [
            (0, 1), (1, 2), (2, 3), (3, 0),  # Top face
            (4, 5), (5, 6), (6, 7), (7, 4),  # Bottom face
            (0, 4), (1, 5), (2, 6), (3, 7),  # Vertical edges
        ]

        # Define faces with colors
        faces = [
            (0, 1, 2, 3, (0, 0, 1)),    # Top (blue)
            (4, 5, 6, 7, (0, 1, 0)),    # Bottom (green)
            (0, 1, 5, 4, (1, 0, 0)),    # Front (red)
            (2, 3, 7, 6, (1, 1, 0)),    # Back (yellow)
            (0, 3, 7, 4, (1, 0, 1)),    # Left (magenta)
            (1, 2, 6, 5, (0, 1, 1)),    # Right (cyan)
        ]

        # Draw faces
        glBegin(GL_QUADS)
        for face in faces:
            color = face[4]
            glColor3f(*color)
            for vertex_idx in face[:4]:
                glVertex3fv(vertices[vertex_idx])
        glEnd()

        # Draw edges (black lines)
        glColor3f(0, 0, 0)
        glLineWidth(2)
        glBegin(GL_LINES)
        for edge in edges:
            for vertex_idx in edge:
                glVertex3fv(vertices[vertex_idx])
        glEnd()

        # Draw coordinate axes
        glLineWidth(3)
        glBegin(GL_LINES)
        # X-axis (red)
        glColor3f(1, 0, 0)
        glVertex3f(0, 0, 0)
        glVertex3f(1.5, 0, 0)
        # Y-axis (green)
        glColor3f(0, 1, 0)
        glVertex3f(0, 0, 0)
        glVertex3f(0, 1.5, 0)
        # Z-axis (blue)
        glColor3f(0, 0, 1)
        glVertex3f(0, 0, 0)
        glVertex3f(0, 0, 1.5)
        glEnd()

    def draw_text(self):
        """Draw orientation values as text overlay"""
        # Note: Simple text rendering in PyOpenGL is complex
        # We'll use pygame surface and blit as texture
        pass

    def run(self):
        """Main visualization loop"""
        if not self.connect():
            return

        clock = pygame.time.Clock()
        running = True

        print("\n=== 3D IMU Visualizer ===")
        print("Controls:")
        print("  - Mouse: Rotate view")
        print("  - ESC or Q: Quit")
        print("  - R: Reset view")
        print("\nWaiting for IMU data...\n")

        mouse_down = False
        last_mouse_pos = (0, 0)
        rotation_x = 0
        rotation_y = 0

        while running:
            # Handle events
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE or event.key == pygame.K_q:
                        running = False
                    elif event.key == pygame.K_r:
                        rotation_x = 0
                        rotation_y = 0
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    if event.button == 1:  # Left click
                        mouse_down = True
                        last_mouse_pos = pygame.mouse.get_pos()
                elif event.type == pygame.MOUSEBUTTONUP:
                    if event.button == 1:
                        mouse_down = False
                elif event.type == pygame.MOUSEMOTION:
                    if mouse_down:
                        mouse_pos = pygame.mouse.get_pos()
                        dx = mouse_pos[0] - last_mouse_pos[0]
                        dy = mouse_pos[1] - last_mouse_pos[1]
                        rotation_y += dx * 0.5
                        rotation_x += dy * 0.5
                        last_mouse_pos = mouse_pos

            # Receive new IMU data
            if self.receive_motor_state():
                # Print orientation values
                roll_deg = math.degrees(self.roll)
                pitch_deg = math.degrees(self.pitch)
                yaw_deg = math.degrees(self.yaw)
                print(f"\rRoll: {roll_deg:7.2f}°  Pitch: {pitch_deg:7.2f}°  Yaw: {yaw_deg:7.2f}°", end="")

            # Clear screen
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

            # Apply transformations
            glPushMatrix()

            # Apply user rotation (mouse control)
            glRotatef(rotation_x, 1, 0, 0)
            glRotatef(rotation_y, 0, 1, 0)

            # Apply IMU orientation (convert from aerospace to OpenGL coordinates)
            # Aerospace: Roll (X), Pitch (Y), Yaw (Z)
            # OpenGL: Apply rotations in reverse order
            glRotatef(math.degrees(self.yaw), 0, 0, 1)    # Yaw (Z-axis)
            glRotatef(math.degrees(self.pitch), 1, 0, 0)  # Pitch (X-axis)
            glRotatef(math.degrees(self.roll), 0, 1, 0)   # Roll (Y-axis)

            # Draw the board
            self.draw_cube()

            glPopMatrix()

            # Update display
            pygame.display.flip()
            clock.tick(60)  # 60 FPS

        # Cleanup
        if self.sock:
            self.sock.close()
        pygame.quit()


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 imu_visualizer_3d.py <board_ip>")
        print("Example: python3 imu_visualizer_3d.py 192.168.1.100")
        sys.exit(1)

    board_ip = sys.argv[1]

    visualizer = IMUVisualizer(board_ip)
    visualizer.run()


if __name__ == "__main__":
    main()
