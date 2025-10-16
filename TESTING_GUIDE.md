# Segment Controller Firmware Testing Guide

This guide explains how to test each phase of firmware development when your Nucleo board arrives.

---

## Prerequisites

### Hardware:
- STM32 Nucleo-H753ZI board
- USB cable (for serial console + power)
- Ethernet cable
- Network with DHCP server OR direct connection to Master PC

### Software:
- Serial terminal (minicom, screen, or PuTTY)
- Python 3.x
- ping command
- (Optional) Wireshark for packet analysis

---

## Phase 1: Basic Bringup

### What Phase 1 Does:
- Initializes Zephyr RTOS
- Prints startup banner over UART
- Sends heartbeat message every 5 seconds

### Testing Steps:

1. **Flash the firmware:**
   ```bash
   cd ~/zephyrproject
   west flash
   ```

2. **Open serial console:**
   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

   Alternative with screen:
   ```bash
   screen /dev/ttyACM0 115200
   ```

3. **Expected output:**
   ```
   ========================================
     Segment Controller Firmware
   ========================================
   Board: nucleo_h753zi
   Zephyr Version: 4.2.99
   ========================================

   [Phase 1] Basic Bringup - SUCCESS
   Heartbeat: System running
   Heartbeat: System running
   ...
   ```

4. **Success criteria:**
   - ✓ Startup banner appears
   - ✓ Board name and version printed
   - ✓ Heartbeat messages every 5 seconds
   - ✓ No error messages

5. **If it fails:**
   - Check USB cable connection
   - Try different serial ports: `ls /dev/ttyACM*` or `ls /dev/ttyUSB*`
   - Verify baud rate is 115200
   - Try reflashing: `west flash --recover`

### Rollback if needed:
```bash
cd /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware
git checkout phase1-ready
cd ~/zephyrproject && west build -b nucleo_h753zi /path/to/nucleo-firmware && west flash
```

---

## Phase 2: Ethernet Networking

### What Phase 2 Adds:
- Ethernet interface initialization
- DHCP client to get IP address
- Network stack (TCP/UDP support)
- Reports assigned IP via serial console

### Network Setup Options:

#### Option A: Using a Router/Switch (Easiest)
```
Master PC ──┐
            ├──► Router (DHCP enabled)
Nucleo ─────┘
```
- Both devices get IPs from router
- No configuration needed

#### Option B: Direct Connection (Need DHCP server on PC)
```
Master PC ◄──Ethernet Cable──► Nucleo
```

Set up DHCP server on your Linux PC:
```bash
# Install dnsmasq
sudo apt install dnsmasq

# Configure dnsmasq
sudo nano /etc/dnsmasq.conf
# Add these lines:
interface=eth0  # or your Ethernet interface
dhcp-range=192.168.1.100,192.168.1.107,12h
dhcp-option=3,192.168.1.1  # Gateway

# Restart dnsmasq
sudo systemctl restart dnsmasq

# Set your PC's static IP
sudo ip addr add 192.168.1.1/24 dev eth0
```

### Testing Steps:

1. **Flash Phase 2 firmware:**
   ```bash
   cd ~/zephyrproject
   west build -b nucleo_h753zi /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware
   west flash
   ```

2. **Connect Ethernet cable** to Nucleo board

3. **Monitor serial console:**
   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

4. **Expected output:**
   ```
   ========================================
     Segment Controller Firmware
   ========================================
   Board: nucleo_h753zi
   Zephyr Version: 4.2.99
   ========================================

   [Phase 1] Basic Bringup - SUCCESS

   [Phase 2] Initializing Network...
   Network interface found: eth0
   DHCP event handler registered
   Starting DHCP client...
   Waiting for IP address (this may take 10-30 seconds)...

   === DHCP Success ===
   IP Address assigned: 192.168.1.100
   Network is ready!
   ====================

   Heartbeat: System running | IP: 192.168.1.100
   ```

5. **Run automated tests:**
   ```bash
   cd /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware

   # Auto-scan for board
   python3 tools/test_phase2_network.py --scan

   # Or specify IP if you know it
   python3 tools/test_phase2_network.py --board-ip 192.168.1.100
   ```

6. **Manual tests:**
   ```bash
   # Ping the board
   ping 192.168.1.100

   # Should respond with:
   # 64 bytes from 192.168.1.100: icmp_seq=1 ttl=64 time=1.23 ms
   ```

7. **Success criteria:**
   - ✓ DHCP assigns IP address (192.168.1.100-107)
   - ✓ Board responds to ping
   - ✓ Heartbeat shows IP address
   - ✓ Python test script shows "PASS" for ping test

8. **If it fails:**

   **No IP assigned (DHCP timeout):**
   - Check Ethernet cable is connected
   - Verify DHCP server is running
   - Check network LED on Nucleo board (should blink)
   - Try different Ethernet cable

   **IP assigned but ping fails:**
   - Check firewall on your PC
   - Verify IP is on same subnet (192.168.1.x)
   - Try: `sudo arp -a` to see if board is in ARP table

   **Phase 1 still works?**
   - Yes → Debug Phase 2 network code
   - No → Rollback to Phase 1

### Rollback to Phase 1:
```bash
git checkout phase1-ready
cd ~/zephyrproject && west build -b nucleo_h753zi /path/to/nucleo-firmware && west flash
```

---

## Phase 3: Packet Protocol

### What Phase 3 Adds:
- Binary packet protocol implementation
- CRC16-CCITT checksum validation
- TCP server for command packets (port 5000)
- UDP server for emergency stop (port 6000)
- Packet parsing and handling:
  - EMERGENCY_STOP (UDP)
  - SET_MODE (TCP)
  - START_HOMING (TCP)
  - JOG_MOTOR (TCP)
  - SET_ZERO_OFFSET (TCP)
  - TRAJECTORY (TCP)
- Feedback packet generation:
  - DIAGNOSTICS (sent at 1 Hz via TCP)
  - MOTOR_STATE (not yet implemented)

### Testing Steps:

1. **Flash Phase 3 firmware:**
   ```bash
   cd ~/zephyrproject
   west build -b nucleo_h753zi /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware --pristine
   west flash
   ```

2. **Monitor serial console:**
   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

3. **Expected output:**
   ```
   ========================================
     Segment Controller Firmware
   ========================================
   Board: nucleo_h753zi
   Zephyr Version: 4.2.99
   ========================================

   [Phase 1] Basic Bringup - SUCCESS

   [Phase 2] Initializing Network...
   Network interface found: eth0
   DHCP event handler registered
   Starting DHCP client...
   Waiting for IP address (this may take 10-30 seconds)...

   === DHCP Success ===
   IP Address assigned: 192.168.1.100
   Network is ready!
   ====================

   [Phase 3] Packet Protocol - READY
   Listening for commands on:
     - TCP port 5000 (trajectory, config)
     - UDP port 6000 (emergency stop)

   Ready to receive packets at: 192.168.1.100

   [TCP] Waiting for client connection on port 5000...
   [UDP] Server thread started on port 6000
   ```

4. **Run interactive tests:**
   ```bash
   cd /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware

   # Interactive mode (recommended for first test)
   python3 tools/test_phase3_packets.py 192.168.1.100 interactive
   ```

   Interactive menu:
   ```
   Available tests:
     1. Send EMERGENCY_STOP (UDP)
     2. Send SET_MODE IDLE (TCP)
     3. Send SET_MODE HOMING (TCP)
     4. Send SET_MODE OPERATION (TCP)
     5. Listen for DIAGNOSTICS packets (5s)
     6. Send simple test trajectory
     q. Quit
   ```

5. **Test sequence:**

   **Test 1: Connect and receive DIAGNOSTICS**
   - In interactive menu, select option 5
   - Should receive DIAGNOSTICS packets every 1 second
   - Expected output:
     ```
     Received packet #1: 26 bytes
       Segment ID: 0
       Timestamp: 12345 ms
       TMC9660 Temp: 0.0°C
       STM32 Temp: 25.0°C
       Error Count: 0
       Last Error: 0x00
       CPU Usage: 0%
     ```

   **Test 2: Send SET_MODE**
   - Select option 2 (SET_MODE IDLE)
   - Serial console should show:
     ```
     [TCP] Client connected from 192.168.1.X:XXXXX
     [TCP] Received 7 bytes
     [Command] Received SET_MODE packet
     ```

   **Test 3: Send EMERGENCY_STOP**
   - Select option 1 (EMERGENCY_STOP)
   - Serial console should show:
     ```
     [UDP] Received 7 bytes
     [Command] EMERGENCY_STOP received!
     ```

   **Test 4: Send TRAJECTORY**
   - Select option 6 (test trajectory)
   - Serial console should show:
     ```
     [TCP] Received 112 bytes
     [Command] Received TRAJECTORY packet
     ```

6. **Run automated tests:**
   ```bash
   python3 tools/test_phase3_packets.py 192.168.1.100 auto
   ```

7. **Success criteria:**
   - ✓ TCP and UDP servers start successfully
   - ✓ DIAGNOSTICS packets received at 1 Hz
   - ✓ SET_MODE command parsed and logged
   - ✓ EMERGENCY_STOP received via UDP
   - ✓ TRAJECTORY command parsed and logged
   - ✓ All packets have valid CRC checksums
   - ✓ No CRC errors reported

8. **If it fails:**

   **Cannot connect to TCP port:**
   - Check firewall: `sudo ufw status`
   - Verify board IP: Check serial console for "Ready to receive packets at: X.X.X.X"
   - Try: `telnet 192.168.1.100 5000` (should connect)

   **CRC errors:**
   - This indicates Python tool and firmware CRC don't match
   - Check that both use CRC16-CCITT (polynomial 0x1021, init 0xFFFF)

   **No DIAGNOSTICS packets:**
   - DIAGNOSTICS only sent if TCP client is connected
   - Make sure to stay connected (option 5 in interactive mode)

   **Phase 2 still works?**
   - Yes → Debug Phase 3 packet code
   - No → Rollback to Phase 2

### Rollback to Phase 2:
```bash
git checkout phase2-ready
cd ~/zephyrproject && west build -b nucleo_h753zi /path/to/nucleo-firmware --pristine && west flash
```

---

## Phase 4: IMU Integration

### What Phase 4 Adds:
- LSM6DSO 6-axis IMU (accelerometer + gyroscope) integration
- I2C communication on Arduino connector pins D14/D15
- Madgwick AHRS sensor fusion algorithm
- Real-time orientation calculation (roll, pitch, yaw)
- IMU data included in MOTOR_STATE feedback packets
- 10 Hz IMU update rate (will be 100 Hz in Phase 7)

### Hardware Requirements:
- LSM6DSO breakout board
- 4 jumper wires for I2C connection:
  - VCC → 3.3V (pin on Nucleo)
  - GND → GND
  - SDA → D14 (I2C1_SDA)
  - SCL → D15 (I2C1_SCL)

### Testing Steps:

1. **Connect LSM6DSO to Nucleo:**
   ```
   LSM6DSO Breakout    →    Nucleo H753ZI
   ─────────────────────────────────────────
   VCC (3.3V)          →    3.3V
   GND                 →    GND
   SDA                 →    D14 (I2C1_SDA)
   SCL                 →    D15 (I2C1_SCL)
   ```

2. **Flash Phase 4 firmware:**
   ```bash
   cd ~/zephyrproject
   west build -b nucleo_h753zi /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware --pristine
   west flash
   ```

3. **Monitor serial console:**
   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

4. **Expected output:**
   ```
   ========================================
     Segment Controller Firmware
   ========================================
   Board: nucleo_h753zi
   Zephyr Version: 4.2.99
   ========================================

   [Phase 1] Basic Bringup - SUCCESS

   [Phase 2] Initializing Network...
   Network interface found: eth0
   DHCP event handler registered
   Starting DHCP client...
   Waiting for IP address (this may take 10-30 seconds)...

   [Phase 4] Initializing IMU (LSM6DSO)...
   LSM6DSO device found: lsm6dso@6a
   Madgwick filter initialized (beta=0.10, freq=100 Hz)
   [Phase 4] IMU initialized successfully

   === DHCP Success ===
   IP Address assigned: 192.168.1.100
   Network is ready!
   ====================

   [Phase 3] Packet Protocol - READY
   Listening for commands on:
     - TCP port 5000 (trajectory, config)
     - UDP port 6000 (emergency stop)

   Ready to receive packets at: 192.168.1.100
   ```

5. **Test IMU readings:**

   **Connect to receive MOTOR_STATE packets:**
   ```bash
   python3 tools/test_phase3_packets.py 192.168.1.100 interactive
   # Select option 5 to listen for packets
   ```

   **You should see orientation data updating:**
   ```
   Received MOTOR_STATE packet: 83 bytes
     IMU Roll:  0.123 rad (7.0°)
     IMU Pitch: -0.052 rad (-3.0°)
     IMU Yaw:   0.000 rad (0.0°)
   ```

   **Physical test:**
   - Tilt the Nucleo board left/right → Roll changes
   - Tilt forward/backward → Pitch changes
   - Rotate around vertical axis → Yaw changes

6. **Success criteria:**
   - ✓ IMU initialization message appears
   - ✓ LSM6DSO device found at I2C address 0x6a
   - ✓ Madgwick filter initialized
   - ✓ No I2C communication errors
   - ✓ MOTOR_STATE packets contain non-zero orientation values
   - ✓ Orientation changes when board is tilted/rotated
   - ✓ Values are reasonable (±π radians, ±180°)

7. **If it fails:**

   **IMU initialization failed:**
   - Check I2C wiring (SDA/SCL not swapped)
   - Verify 3.3V power to LSM6DSO (not 5V!)
   - Check I2C address: LSM6DSO uses 0x6A (SDO/SA0 pin low) or 0x6B (high)
   - Try: `i2cdetect -y 1` on another system to verify sensor works

   **IMU initialized but orientation is all zeros:**
   - Check that imu_update() is being called (should see in logs)
   - Sensor might be in sleep mode - check ODR configuration
   - Verify accelerometer/gyro data is non-zero

   **Orientation values drift/unstable:**
   - Normal for Madgwick without magnetometer
   - Yaw will drift over time (no absolute reference)
   - Roll/pitch should be stable when stationary

   **Phase 3 still works?:**
   - Yes → Debug Phase 4 IMU code
   - No → Rollback to Phase 3

### Rollback to Phase 3:
```bash
git checkout phase3-ready
cd ~/zephyrproject && west build -b nucleo_h753zi /path/to/nucleo-firmware --pristine && west flash
```

---

## Phase 5: TMC9660 Motor Driver (UART)

### What Phase 5 Adds:
- TMC9660 smart gate driver with FOC controller
- UART bootloader communication protocol
- 8-byte command/reply message structure with CRC8 checksum
- Register read/write capability
- Configuration memory access
- Chip identification and version reading
- 115200 baud UART with autobaud support

### Hardware Requirements:
- TMC9660 evaluation board or custom board
- 3 jumper wires for UART connection:
  - GND → GND
  - TMC_TX (GPIO6/Pin 62) → Nucleo RX (D1/PD6)
  - TMC_RX (GPIO7/Pin 63) → Nucleo TX (D0/PD5)

### TMC9660 Pin Connections:
```
TMC9660 Pins         →    Nucleo H753ZI (Arduino)
──────────────────────────────────────────────────
GND                  →    GND
GPIO6 (UART_TX)      →    D1 (USART2_RX/PD6)
GPIO7 (UART_RX)      →    D0 (USART2_TX/PD5)
VDD (optional)       →    3.3V or 5V (depending on TMC9660 board)
```

**Note:** TMC9660 supports autobaud detection, so no crystal is required for basic UART communication.

### Testing Steps:

1. **Connect TMC9660 to Nucleo:**
   ```
   TMC9660          →    Nucleo H753ZI
   ────────────────────────────────────
   GND              →    GND
   GPIO6 (TX)       →    D1 (RX)
   GPIO7 (RX)       →    D0 (TX)
   ```

2. **Flash Phase 5 firmware:**
   ```bash
   cd ~/zephyrproject
   west build -b nucleo_h753zi /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware --pristine
   west flash
   ```

3. **Monitor serial console:**
   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

4. **Expected output (TMC9660 connected):**
   ```
   ========================================
     Segment Controller Firmware
   ========================================
   Board: nucleo_h753zi
   Zephyr Version: 4.2.99
   ========================================

   [Phase 1] Basic Bringup - SUCCESS

   [Phase 2] Initializing Network...
   Network interface found: eth0
   DHCP event handler registered
   Starting DHCP client...

   [Phase 4] Initializing IMU (LSM6DSO)...
   LSM6DSO device found: lsm6dso@6a
   Madgwick filter initialized (beta=0.10, freq=100 Hz)
   [Phase 4] IMU initialized successfully

   [Phase 5] Initializing TMC9660 motor driver...
   TMC9660 UART initialized
   Chip type verified: 0x544D0001
   Chip version: 1
   Bootloader version: 1.0

   [Phase 5] TMC9660 Motor Driver - INITIALIZED
     Chip Type: 0x544D0001
     Chip Version: 1
     Bootloader: 1.0

   === DHCP Success ===
   IP Address assigned: 192.168.1.100
   Network is ready!
   ====================

   [Phase 3] Packet Protocol - READY
   Listening for commands on:
     - TCP port 5000 (trajectory, config)
     - UDP port 6000 (emergency stop)

   Ready to receive packets at: 192.168.1.100
   ```

5. **Expected output (TMC9660 not connected):**
   ```
   [Phase 5] Initializing TMC9660 motor driver...
   TMC9660 UART initialized
   Failed to read chip type: -116
   Warning: TMC9660 initialization failed: -116
   Continuing without motor driver (motor control disabled)
   ```
   This is normal if TMC9660 is not connected yet - firmware continues without motor control.

6. **Test TMC9660 communication:**

   If you have the TMC9660 connected, you can verify communication by checking:
   - Chip type should be `0x544D0001`
   - Chip version should be `1` or `2`
   - Bootloader version should be displayed (e.g., `1.0`)

   The firmware automatically queries the TMC9660 on startup and prints these values.

7. **Success criteria:**
   - ✓ UART2 initialized successfully
   - ✓ TMC9660 chip detected (if connected)
   - ✓ Chip type matches expected value (0x544D0001)
   - ✓ Bootloader version read successfully
   - ✓ No UART communication errors
   - ✓ CRC8 checksums verify correctly
   - ✓ Phase 4 IMU still works
   - ✓ Phase 3 networking still works

8. **If it fails:**

   **UART device not ready:**
   - Check device tree overlay (usart2 enabled?)
   - Verify UART pins configured correctly in overlay
   - Try reflashing firmware

   **Chip type mismatch or read fails:**
   - Check UART wiring (TX/RX not swapped)
   - Verify GND connection between boards
   - Check TMC9660 power supply (must be powered on)
   - Verify TMC9660 is in bootloader mode (default on power-up)
   - Try lower baud rate if autobaud fails

   **CRC8 errors:**
   - UART noise or timing issues
   - Try shorter wires or twisted pair
   - Check ground connection quality
   - Reduce baud rate from 115200 to 57600

   **TMC9660 powers up but no response:**
   - Check GPIO6/GPIO7 are configured as UART (default)
   - Verify device address is 0x01 (default)
   - Try sending NO_OP command manually
   - Check if TMC9660 entered motor control mode (should stay in bootloader)

   **Phase 4 still works?**
   - Yes → Debug Phase 5 TMC9660 code
   - No → Rollback to Phase 4

### Testing without motor:

Even without a motor connected, you can test the TMC9660 driver:
- ✓ Read chip identification
- ✓ Read/write configuration registers
- ✓ Test UART communication
- ✓ Verify CRC8 checksums

You **cannot** test (motor required):
- ✗ Motor current measurement
- ✗ FOC control loop
- ✗ Position/velocity feedback
- ✗ Torque control

### Rollback to Phase 4:
```bash
git checkout phase4-ready
cd ~/zephyrproject && west build -b nucleo_h753zi /path/to/nucleo-firmware --pristine && west flash
```

---

## Troubleshooting

### Serial Console Issues:
```bash
# Check if device exists
ls /dev/ttyACM*

# Check permissions
sudo usermod -a -G dialout $USER
# Log out and back in

# If minicom shows garbage:
# Press Ctrl+A, then Z, then Q to quit
# Try different baud rate or check settings
```

### Network Issues:
```bash
# Check Ethernet interface on PC
ip addr show

# Check if DHCP server is running
sudo systemctl status dnsmasq

# Capture packets to see DHCP requests
sudo tcpdump -i eth0 port 67 or port 68
```

### Build Issues:
```bash
# Clean build
cd ~/zephyrproject
west build -t clean
west build -b nucleo_h753zi /path/to/nucleo-firmware

# Completely pristine build
rm -rf build
west build -b nucleo_h753zi /path/to/nucleo-firmware --pristine
```

---

## Git Workflow

### View available phases:
```bash
cd /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware
git tag
```

Output:
```
phase1-ready
phase2-ready
phase3-ready
phase4-ready
phase5-ready
```

### Switch between phases:
```bash
# Go to Phase 1
git checkout phase1-ready

# Go to Phase 2
git checkout phase2-ready

# Go to Phase 3
git checkout phase3-ready

# Go to Phase 4
git checkout phase4-ready

# Go to Phase 5
git checkout phase5-ready

# Go back to latest
git checkout master
```

### After switching, rebuild and flash:
```bash
cd ~/zephyrproject
west build -b nucleo_h753zi /path/to/nucleo-firmware --pristine
west flash
```

---

## Next Steps

Once Phase 2 passes all tests:
1. Keep Phase 2 running
2. I'll implement Phase 3 (Packet Protocol)
3. You flash Phase 3 and test packet sending/receiving
4. Continue through Phases 4-9

Each phase builds on the previous one, so we can always rollback if something breaks!
