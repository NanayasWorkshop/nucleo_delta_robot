# Quick Demo - Testing the 3D IMU Visualizer

## Try it NOW (no hardware needed!)

### Step 1: Install dependencies
```bash
cd /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware/tools
pip3 install -r requirements.txt
```

### Step 2: Open two terminals

**Terminal 1 - Mock Server:**
```bash
cd /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware/tools
python3 mock_imu_server.py rotate
```

You should see:
```
=== Mock IMU Server ===
Animation mode: rotate
Listening on 0.0.0.0:5000

Waiting for client connection...
Run the visualizer with: python3 imu_visualizer_3d.py 127.0.0.1

Press Ctrl+C to stop
```

**Terminal 2 - 3D Visualizer:**
```bash
cd /home/yuuki/claude-projects/delta_robot/workspace/dev-boards/nucleo-firmware/tools
python3 imu_visualizer_3d.py 127.0.0.1
```

You should see:
```
Connecting to 127.0.0.1:5000...
Connected! Waiting for MOTOR_STATE packets...

=== 3D IMU Visualizer ===
Controls:
  - Mouse: Rotate view
  - ESC or Q: Quit
  - R: Reset view

Waiting for IMU data...

Roll: -12.34°  Pitch: 5.67°  Yaw: 123.45°
```

And a **3D OpenGL window** showing a rotating board!

---

## What You'll See

A 3D rectangular box (representing the Nucleo board) that:
- **Rotates smoothly** in real-time
- Shows **colored faces** (blue=top, green=bottom)
- Displays **RGB coordinate axes** (X=red, Y=green, Z=blue)
- Updates at **60 FPS** (smooth animation)

---

## Try Different Animations

Stop both terminals (Ctrl+C) and try:

```bash
# Smooth rotation around all axes
python3 mock_imu_server.py rotate

# Tilt back and forth (like rocking)
python3 mock_imu_server.py tilt

# Random wobbling motion
python3 mock_imu_server.py wobble

# Static orientation (no movement)
python3 mock_imu_server.py still
```

Then restart the visualizer in the other terminal.

---

## Interact with the View

While the visualizer is running:
- **Click and drag mouse** → Rotate camera around the board
- **Press R** → Reset camera to default view
- **Press ESC or Q** → Quit

---

## When Your Hardware Arrives

Replace `127.0.0.1` with your actual Nucleo IP:

```bash
python3 imu_visualizer_3d.py 192.168.1.100
```

Now it will show the **real orientation** of your board as you physically move it!

---

## Troubleshooting

**"ModuleNotFoundError: No module named 'OpenGL'"**
→ Run: `pip3 install -r requirements.txt`

**"Connection refused"**
→ Make sure mock server is running in Terminal 1 first

**Black window / no graphics**
→ Check if you have OpenGL drivers installed
→ Try: `glxinfo | grep OpenGL` (should show your GPU info)

**Low FPS / laggy**
→ This uses GPU acceleration, should be 60 FPS even on modest hardware
→ Check if other GPU-intensive programs are running
