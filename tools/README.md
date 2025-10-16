# Testing Tools for Nucleo Firmware

## Installation

Install Python dependencies:

```bash
pip3 install -r requirements.txt
```

---

## Phase 2: Network Testing

Test basic network connectivity (DHCP and ping):

```bash
# Auto-scan for board
python3 test_phase2_network.py --scan

# Or specify IP if known
python3 test_phase2_network.py --board-ip 192.168.1.100
```

---

## Phase 3: Packet Protocol Testing

Send commands and receive feedback packets:

```bash
# Interactive mode (recommended)
python3 test_phase3_packets.py 192.168.1.100 interactive

# Automated tests
python3 test_phase3_packets.py 192.168.1.100 auto
```

**Interactive menu:**
1. Send EMERGENCY_STOP (UDP)
2. Send SET_MODE IDLE (TCP)
3. Send SET_MODE HOMING (TCP)
4. Send SET_MODE OPERATION (TCP)
5. Listen for DIAGNOSTICS packets (5s)
6. Send simple test trajectory
q. Quit

---

## Phase 4: Real-Time 3D IMU Visualization

Visualize board orientation in real-time 3D:

```bash
python3 imu_visualizer_3d.py 192.168.1.100
```

### Testing Without Hardware (Mock Server)

Test the visualizer before your board arrives:

**Terminal 1 - Start mock server:**
```bash
# Default animation (smooth rotation)
python3 mock_imu_server.py

# Or choose animation mode:
python3 mock_imu_server.py rotate   # Rotate around all axes (default)
python3 mock_imu_server.py tilt     # Tilt back and forth
python3 mock_imu_server.py wobble   # Random wobbling
python3 mock_imu_server.py still    # Static orientation
```

**Terminal 2 - Start visualizer:**
```bash
python3 imu_visualizer_3d.py 127.0.0.1
```

The mock server simulates the Nucleo board sending MOTOR_STATE packets with animated IMU data!

**Controls:**
- **Mouse drag**: Rotate view around board
- **R**: Reset view to default
- **ESC or Q**: Quit

**What you'll see:**
- 3D rectangular box representing the Nucleo board
- **Blue face**: Top of board
- **Green face**: Bottom of board
- **Red/Yellow/Magenta/Cyan**: Side faces
- **RGB axes**: X (red), Y (green), Z (blue)

**Real-time values printed:**
```
Roll: -12.34°  Pitch: 5.67°  Yaw: 0.00°
```

**Physical test:**
- Tilt board left/right → Roll changes
- Tilt forward/backward → Pitch changes
- Rotate around vertical → Yaw changes

The 3D visualization updates in real-time as you move the board!

---

## Troubleshooting

### Connection refused
- Check board IP with `ping 192.168.1.100`
- Verify Phase 3 firmware is running (check serial console)
- Check firewall: `sudo ufw status`

### No IMU data (all zeros)
- Check IMU wiring (D14=SDA, D15=SCL)
- Verify 3.3V power (NOT 5V!)
- Check serial console for IMU initialization errors

### OpenGL errors
- Make sure you have graphics drivers installed
- On headless systems, you'll need X11 forwarding or virtual display

---

## Tips

- Keep **serial console** (`minicom`) open in one terminal to see board logs
- Run **test tools** in a separate terminal
- IMU visualizer runs at **60 FPS** for smooth animation
- Packet reception is asynchronous, so 3D view updates as data arrives
