# paso-chan
CEN3907C Senior Design Project: Paso-Chan
Hardware selection document (WIP): https://docs.google.com/document/d/1BDaP8xXUTaKsWSGs0UN_m29KRFAre18QROHcax4RM5g/edit?usp=sharing

# Completed Work
1) Networking proof-of-concept
2) Consistent data storage model
3) Frontend UI for Paso-Chan desktop app

# Project Architecture
On one end of the data transmission, we have User 1's Paso-Chan. This Paso-Chan communicates data about its state via the Paso-Chan desktop app, which User 1 will have installed. The desktop app allows data to be transmitted to a relay server, which is responsible for syncing Paso-Chan's state data across both users' Paso-Chans and desktop apps. This data communication goes between User 1 and User 2's Paso-Chans and respective apps.

# Quickstart: Running network_test.c on a ESP32 device

## Prerequisites

### 1. Install ESP-IDF
You'll need ESP-IDF v5.0 or later:

```bash
# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git

# Install ESP-IDF
cd ~/esp/esp-idf
./install.sh esp32

# Set up environment
echo '. ~/esp/esp-idf/export.sh' >> ~/.bashrc && source ~/.bashrc


```

### 2. Hardware Requirements
- **ESP32 development board** (any variant with ESP32)
- ** OPTIONAL SSD1306 OLED Display** (128x64, I2C interface)
- **Wiring**:
  - Display SDA → GPIO 21 (default)
  - Display SCL → GPIO 22 (default)
  - Display VCC → 3.3V
  - Display GND → GND

### 3. Server Setup
The network_test.c application connects to a relay server. There is a basic one at networking/relay_server.py:
- Server IP address
- Server port (default: 8888)

## Configuration Steps

### 1. Clone the Repository
```bash
git clone https://github.com/bsibb22/paso-chan.git
cd paso-chan
```

### 2. Configure Network Settings
Edit `src/network_test.c` and update lines 26-30 with your network credentials:

```c
#define WIFI_SSID           "YourWiFiSSID"        // Your WiFi network name
#define WIFI_PASSWORD       "YourWiFiPassword"   // Your WiFi password
#define SERVER_IP           "192.168.1.XXX"      // Your server's IP address
#define SERVER_PORT         8888                  // Server port
#define DEVICE_NAME         "Device1"             // Unique device identifier
```

## Build and Flash

### 1. Set Up ESP-IDF Environment
```bash
# Run this in every new terminal session
. ~/esp/esp-idf/export.sh
```

### 2. Navigate to Project Directory
```bash
cd /path/to/paso-chan
```

### 3. Set Target Device
```bash
idf.py set-target esp32
```

### 4. Build the Project
```bash
idf.py build
```

### 5. Connect ESP32 and Flash
```bash
# Find your device port (usually /dev/ttyUSB0 or /dev/ttyACM0)
ls /dev/tty*

# Flash and open serial monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your actual port.

## Usage

Once flashed and running, the application will:

1. **Display startup screen** on the OLED
2. **Connect to WiFi** - Status shown on display
3. **Connect to server** - Connection status updates on display
4. **Display shows**:
   - Device name
   - Connection status
   - WiFi signal strength (RSSI)
   - TX/RX message counts
   - Error count (if any)

### Interacting with the Device

- **BOOT Button**: Press the BOOT button (GPIO 0) to send a test message to the server
- **Automatic Heartbeats**: Device sends heartbeat messages every 15 seconds
- **Incoming Messages**: Received messages appear on the display for 3 seconds

## Troubleshooting

### ESP32 Not Found
```bash
# Check USB permissions
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```

### Build Errors
```bash
# Clean and rebuild
idf.py fullclean
idf.py build
```

### Display Not Working
- Verify I2C wiring (SDA → GPIO 21, SCL → GPIO 22)
- Check I2C address (default: 0x3C)
- Ensure proper power supply to display

### WiFi Connection Issues
- Double-check SSID and password in `src/network_test.c:26-27`
- Ensure WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)

### Server Connection Issues
- Verify server is running and accessible
- Check server IP and port in `src/network_test.c:28-29`
- Ensure ESP32 and server are on the same network (see Known Bugs below)

## Quick Commands Reference

```bash
# Set up environment
. ~/esp/esp-idf/export.sh

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Monitor only (after flashing)
idf.py -p /dev/ttyUSB0 monitor

# Exit monitor: Ctrl+]
```
## How network.c All Works Together

### **Startup Sequence:**

1. **Hardware Init** → Configure GPIO, I2C
2. **Display Init** → Initialize OLED screen
3. **Network Init** → Prepare networking subsystem
4. **WiFi Connect** → Join WiFi network (gets IP address)
5. **Server Connect** → Establish TCP connection to server
6. **Task Creation** → Start background tasks (button, heartbeat, status)

### **Runtime Operation of network_test.c:**
```
┌─────────────┐
│ Button Task │ ──(press)──> network_send_message() ──> [TX Queue]
└─────────────┘                                              │
                                                             ↓
┌──────────────┐                                      ┌──────────────┐
│ Network Task │ <──(recv)── TCP Socket <── Server    │ Network Task │
│              │                                      │              │
│  Receives    │                                      │  Sends from  │
│  messages    │                                      │  TX Queue    │
│      ↓       │                                      └──────────────┘
│  Calls       │
│  callback    │
└──────────────┘
       ↓
┌──────────────┐
│   Display    │ <── Show received message
│   on OLED    │
└──────────────┘
```
# Known Bugs
> Relay server is not properly port-forwarded, meaning both users must be connected to the same Wi-Fi network
