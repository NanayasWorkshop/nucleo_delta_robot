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

## Phase 3: Packet Protocol (Coming Soon)

Will add:
- Binary packet parsing (EMERGENCY_STOP, TRAJECTORY, etc.)
- CRC16 validation
- UDP/TCP packet handling

Testing will involve sending actual command packets from Master PC.

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
```

### Switch between phases:
```bash
# Go to Phase 1
git checkout phase1-ready

# Go to Phase 2
git checkout phase2-ready

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
