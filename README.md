# 🚀 ESP8266 Wake-on-LAN Controller

**A complete DIY Wake-on-LAN solution with a beautiful web interface for remote PC control.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP8266](https://img.shields.io/badge/ESP8266-Compatible-green.svg)](https://www.espressif.com/en/products/socs/esp8266)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)]()

## ✨ Features

- 🔌 **Wake your PC** from shutdown (S5) or sleep (S3) via Wi-Fi
- 🎮 **Remote Control** - Sleep, shutdown, and restart your PC from anywhere
- 🌐 **Modern Web UI** - Beautiful, responsive interface with animations
- 🔒 **Secure** - Token-based authentication with customizable security
- 📱 **Mobile-Friendly** - Works perfectly on phones and tablets
- 🔧 **Cross-Platform** - Host agent works on Windows, Linux, and macOS
- ⚡ **Easy Setup** - Single configuration file, no hardcoded values
- 🚫 **No Subscription** - Completely free, no third-party dependencies
- 📊 **Real-time Status** - Live system monitoring and logging
- 🌍 **Remote Access** - Control from anywhere with VPN support

## 📁 Project Structure

```
wake-on-lan-controller/
├── 📁 config/
│   ├── device.example.yaml         # Configuration template
│   └── device.yaml                 # Your actual config (create this)
├── 📁 esp8266/
│   └── firmware_wol_controller.ino # ESP8266 firmware with web UI
├── 📁 host-agent/
│   ├── listener.py                 # Cross-platform PC control service
│   ├── requirements.txt            # Python dependencies
│   ├── 📁 windows/                 # Windows service installer
│   ├── 📁 linux/                   # Linux systemd service
│   └── 📁 macos/                   # macOS LaunchAgent
├── 📄 README.md                    # This file
├── 📄 LICENSE                      # MIT License
└── 📄 .gitignore                   # Git ignore rules
```

## 🎯 Quick Start

**5 minutes to get started:**

1. **Clone** this repository
2. **Configure** your settings in `config/device.yaml`
3. **Flash** the ESP8266 firmware
4. **Install** the host agent on your PC
5. **Enjoy** remote PC control! 🎉

## ⚙️ Configuration

### Step 1: Create Configuration File

```bash
# Copy the template
cp config/device.example.yaml config/device.yaml
```

### Step 2: Fill in Your Settings

Edit `config/device.yaml` with your actual values:

| Setting | Description | How to Find |
|---------|-------------|-------------|
| **wifi.ssid** | Your 2.4GHz Wi-Fi name | Windows taskbar → Wi-Fi icon → "Connected" network |
| **wifi.password** | Your Wi-Fi password | Router label or admin panel |
| **esp.token** | Security token (20-40 chars) | Generate: `openssl rand -base64 32` |
| **esp.target_mac** | PC Ethernet MAC (AA:BB:CC:DD:EE:FF) | Windows: `ipconfig /all` → Physical Address → replace `-` with `:` |
| **esp.broadcast_ip** | Network broadcast IP | If IP is 192.168.X.Y with mask 255.255.255.0 → use 192.168.X.255 |
| **esp.wol_port** | Wake-on-LAN port | Usually 9 or 7 |
| **host.ip** | Your PC's IPv4 address | Windows: `ipconfig` → IPv4 Address |
| **host.port** | Host agent port | Default: 8888 |
| **host.allowed_ips** | IP whitelist | Leave `[]` for LAN access |

## 🔧 ESP8266 Setup

### Step 1: Install Arduino IDE

1. **Download**: [Arduino IDE](https://www.arduino.cc/en/software)
2. **Install ESP8266 boards**:
   - File → Preferences → Additional Boards Manager URLs:
   - Add: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Tools → Board → Boards Manager → Search "ESP8266" → Install

### Step 2: Configure Firmware

1. **Open** `esp8266/firmware_wol_controller.ino` in Arduino IDE
2. **Update** the configuration block with your values from `config/device.yaml`:

```cpp
// Wi-Fi credentials
const char* WIFI_SSID = "Your_2G_WiFi_Name";
const char* WIFI_PASSWORD = "Your_WiFi_Password";

// Target PC configuration  
const char* TARGET_MAC = "AA:BB:CC:DD:EE:FF";
const char* BROADCAST_IP = "192.168.x.255";
const char* SECRET_TOKEN = "change_this_to_a_long_random";
const char* PC_IP_ADDRESS = "192.168.x.y";
```

### Step 3: Flash the ESP8266

1. **Board**: Tools → Board → ESP8266 → NodeMCU 1.0 (ESP-12E)
2. **Port**: Tools → Port → Select your ESP8266 port (COMx on Windows)
3. **Upload**: Click the upload button (→)
4. **Monitor**: Open Serial Monitor @115200 baud

### Step 4: Verify Connection

**Expected output:**
```
[Wi-Fi] Connected successfully!
[Wi-Fi] IP Address: 192.168.x.y
[Server] Access via: http://wol.local/
[Server] System ready!
```

**Test the web interface:**
- 🌐 Open: `http://wol.local/` or `http://<ESP_IP>/`
- ✅ Should show beautiful web interface with control buttons

## 💻 Host Agent Installation

The host agent runs on your PC to handle sleep, shutdown, and restart commands.

### 🪟 Windows Installation

**Run as Administrator:**

```powershell
cd host-agent\windows
.\install_service_windows.bat
```

**What it does:**
- ✅ Creates Python virtual environment (`.venv`)
- ✅ Installs required dependencies
- ✅ Registers Windows service `WolShutdownSleep`
- ✅ Configures auto-start before login
- ✅ Opens firewall rule for port 8888

**Verify installation:**
```cmd
sc query WolShutdownSleep
```

**Uninstall:**
```powershell
.\uninstall_service_windows.bat
```

### 🐧 Linux Installation

```bash
cd host-agent/linux
chmod +x *.sh
./install_service_linux.sh
```

**What it does:**
- ✅ Creates Python virtual environment
- ✅ Installs dependencies
- ✅ Creates systemd user service
- ✅ Enables and starts the service

**Verify installation:**
```bash
systemctl --user status wol-host-agent.service
```

**Run without login:**
```bash
loginctl enable-linger $(whoami)
```

### 🍎 macOS Installation

```bash
cd host-agent/macos
chmod +x *.sh
./install_service_macos.sh
```

**What it does:**
- ✅ Creates Python virtual environment
- ✅ Installs dependencies
- ✅ Creates LaunchAgent
- ✅ Loads and starts the service

**Verify installation:**
```bash
launchctl list | grep com.wol.host.agent
```

## 🧪 Testing & Verification

### Step 1: Test Host Agent

```bash
# Check if host agent is running
curl http://localhost:8888/status
```

**Expected response:**
```json
{"status": "online", "uptime": "2h 15m", "service": "WolHostAgent"}
```

### Step 2: Test ESP8266 Web Interface

1. **Open your browser** and go to: `http://wol.local/`
2. **Verify** you see the beautiful web interface with buttons
3. **Test each button**:
   - 🔌 **Wake PC** - Should wake your computer from sleep/shutdown
   - 😴 **Sleep PC** - Should put your computer to sleep
   - 🚫 **Shutdown PC** - Should shut down your computer
   - 🔃 **Restart PC** - Should restart your computer
   - 📊 **Status** - Should show system information
   - 📋 **Logs** - Should show recent activity

### Step 3: Test API Endpoints

```bash
# Test wake command
curl "http://wol.local/wake?token=YOUR_TOKEN"

# Test status
curl "http://wol.local/status"
```

## 🎉 You're Done!

Your Wake-on-LAN system is now ready! You can:
- 🌐 **Control your PC** from the web interface
- 📱 **Use your phone** to wake/shutdown your PC
- 🌍 **Access remotely** using Tailscale or port forwarding
- 🔒 **Keep it secure** with your custom token

## 🌍 Remote Access (Optional)

### Option 1: Tailscale VPN (Recommended)

**Free and secure remote access:**

1. **Install Tailscale** on your PC and mobile device
2. **Sign up** for a free Tailscale account
3. **Connect** both devices to the same Tailscale network
4. **Access** your ESP8266 using the Tailscale IP address
5. **Control** your PC from anywhere in the world!

### Option 2: Port Forwarding (Less Secure)

1. **Configure** your router to forward port 80 to your ESP8266
2. **Use** your public IP address to access the system
3. **Consider** using DuckDNS for dynamic DNS

### Option 3: Mobile Hotspot

- **Create** a mobile hotspot from your phone
- **Connect** ESP8266 to the hotspot
- **Control** your PC from your phone's network

## 🔧 Troubleshooting

### Common Issues

| Problem | Solution |
|---------|----------|
| **PC won't wake** | Check BIOS ErP setting is **disabled** |
| **ESP won't connect** | Ensure 2.4GHz Wi-Fi and correct password |
| **Wrong broadcast IP** | If IP is 192.168.x.y with mask 255.255.255.0 → use 192.168.x.255 |
| **MAC format error** | Use colons: `AA:BB:CC:DD:EE:FF` (Windows shows dashes) |
| **Token issues** | URL-encode special characters or use alphanumeric only |
| **Host agent not working** | Check service status: `sc query WolShutdownSleep` |

## 🛡️ PC Configuration for Wake-on-LAN

### BIOS Settings (Critical!)

**Access BIOS** (usually DEL, F2, or F12 during boot):

| Setting | Value | Notes |
|---------|-------|-------|
| **Wake on LAN** | ✅ Enabled | Enable network wake |
| **ErP** | ❌ **Disabled** | Most common issue! |
| **PME Event Wake Up** | ✅ Enabled | Power management events |
| **PCIe Power On** | ✅ Enabled | PCIe device wake |

**For MSI B550M PRO-VDH WIFI**:
- Settings → Advanced → Wake Up Event Setup
- Resume By PCI-E Device: **Enabled**
- Wake Up by LAN: **Enabled**
- ErP Ready: **Disabled**

### Windows 10/11 Settings

1. **Device Manager** → Network Adapters → Your Ethernet Adapter → Properties
   - **Power Management** tab:
     - ✅ Allow this device to wake the computer
     - ✅ Only allow a magic packet to wake the computer
   - **Advanced** tab:
     - Wake on Magic Packet: **Enabled**

2. **Power Options** → Choose what the power button does
   - Change settings that are currently unavailable
   - **Uncheck** "Turn on fast startup"

3. **Connect via Ethernet** (not Wi-Fi)

### Linux Settings

```bash
# Check current WoL setting
sudo ethtool eth0 | grep Wake-on

# Enable WoL
sudo ethtool -s eth0 wol g

# Make persistent (Ubuntu/Debian)
sudo nano /etc/network/interfaces
# Add: post-up ethtool -s eth0 wol g
```

## 🤝 Contributing

**Found a bug?** Open an issue!  
**Have a feature suggestion?** We'd love to hear it!  
**Want to improve the project?** Pull requests welcome!

## 📄 License

This project is licensed under the **MIT License**.

## 🙏 Acknowledgments

**Inspiration**:
- [Simple Wake on LAN by herzhenr](https://github.com/herzhenr/simple-wake-on-lan) - Flutter mobile app
- [WakeOnLAN by basildane](https://github.com/basildane/WakeOnLAN) - Windows desktop application

**Libraries**:
- ESP8266WiFi - Wi-Fi connectivity
- ESP8266WebServer - HTTP server
- WiFiUdp - UDP packet transmission

---

**Made with ❤️ for the DIY community**


