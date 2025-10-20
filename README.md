# Wake-on-LAN Controller (ESP8266 + Cross-Platform Host Agent)

Clone → edit one config → flash ESP → install host agent → done.

- Wake your PC from shutdown (S5) or sleep (S3)
- Sleep, shutdown, restart the PC via HTTP endpoints
- Works on Windows, Linux, and macOS (host agent runs before sign‑in)
- One configuration file; no mock values or hardcoded paths

## Repository layout
```
config/
  device.example.yaml         # copy to device.yaml and fill actual values
esp8266/
  firmware_wol_controller.ino # ESP8266 firmware (wake, sleep, shutdown)
host-agent/
  listener.py                 # status/sleep/shutdown/restart, reads config
  requirements.txt
  windows/
    install_service_windows.bat
    uninstall_service_windows.bat
  linux/
    install_service_linux.sh
    uninstall_service_linux.sh
  macos/
    install_service_macos.sh
    uninstall_service_macos.sh
.venv/                        # auto-created by installers (kept out of git)
```

## 1) Configure (only place to edit)
Copy the template and fill YOUR actual values:
```
cp config/device.example.yaml config/device.yaml
```
Edit `config/device.yaml`:
- wifi.ssid: Your 2.4 GHz Wi‑Fi name
  - Windows taskbar → Wi‑Fi icon → the “Connected” network name
- wifi.password: Your Wi‑Fi password
  - Router label or router admin UI
- esp.token: Long random (20–40 chars). Generate with `openssl rand -base64 32`
- esp.target_mac: Your PC Ethernet MAC, colon format AA:BB:CC:DD:EE:FF
  - Windows: cmd → `ipconfig /all` → Physical Address → replace `-` with `:`
- esp.broadcast_ip: For IPv4 192.168.X.Y with mask 255.255.255.0 → use 192.168.X.255
  - Windows: cmd → `ipconfig` → note IPv4 and Subnet Mask
- esp.wol_port: 9 (or 7)
- host.ip: Your PC’s IPv4 address
  - Windows: cmd → `ipconfig` → IPv4 Address
- host.port: 8888 (default)
- host.allowed_ips: Optional IP whitelist (e.g., `["<ESP_IP>"]`). Leave `[]` for LAN.

## 2) Flash the ESP8266
1. Open `esp8266/firmware_wol_controller.ino` in Arduino IDE
2. Paste the values from `config/device.yaml` into the top config block:
   - WIFI_SSID, WIFI_PASSWORD, SECRET_TOKEN, TARGET_MAC, BROADCAST_IP, WOL_PORT, PC_IP_ADDRESS (host.ip), PC_SHUTDOWN_PORT (host.port)
3. Tools → Board → ESP8266 → NodeMCU 1.0 (ESP‑12E)
4. Tools → Port → select the COMx for your board
5. Upload (→). Open Serial Monitor @115200, press reset and note the ESP IP and endpoints

First‑time Arduino IDE setup (only once):
- Install Arduino IDE: https://www.arduino.cc/en/software
- Add ESP8266 boards: File → Preferences → Additional Boards Manager URLs:
  `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
  Then Tools → Board → Boards Manager → search “ESP8266 by ESP8266 Community” → Install.
- Drivers: If Windows can’t see the port, install CH340 or CP210x driver based on your board.

Expected Serial Monitor output after a successful flash/reset:
```
[Wi‑Fi] Connecting to: <Your SSID>
..........
[Wi‑Fi] Connected successfully!
[Wi‑Fi] IP Address: 192.168.X.Y
[Server] Endpoints:
  - http://192.168.X.Y/
  - http://192.168.X.Y/wake?token=<TOKEN>
  - http://192.168.X.Y/sleep?token=<TOKEN>
  - http://192.168.X.Y/shutdown?token=<TOKEN>
  - http://192.168.X.Y/status
```

Verify on same Wi‑Fi:
- ESP status: `http://<ESP_IP>/status`
- Wake: `http://<ESP_IP>/wake?token=<TOKEN>`
- Sleep: `http://<ESP_IP>/sleep?token=<TOKEN>`
- Shutdown: `http://<ESP_IP>/shutdown?token=<TOKEN>`

## 3) Install the Host Agent (service; starts before sign‑in)
The host agent runs on the PC you want to control (the machine with the Ethernet NIC).

### Windows (PowerShell as Administrator)
```
cd host-agent\windows
./install_service_windows.bat
```
What it does:
- Creates a virtualenv at repo root: `.venv` (once)
- Installs `host-agent/requirements.txt`
- Registers a Windows service `WolShutdownSleep` (auto‑start)
- Opens firewall rule for TCP 8888

Check service: `sc query WolShutdownSleep`
Uninstall (keeps files/venv):
```
./uninstall_service_windows.bat
```

### Linux (systemd user service)
```
cd host-agent/linux
chmod +x *.sh
./install_service_linux.sh
```
What it does:
- Creates `.venv` at repo root (once)
- Installs requirements
- Writes `~/.config/systemd/user/wol-host-agent.service`
- Enables & starts the service

Check: `systemctl --user status wol-host-agent.service`
Uninstall (keeps files/venv):
```
./uninstall_service_linux.sh
```
If you want it to run without login: `loginctl enable-linger $(whoami)`

### macOS (LaunchAgent)
```
cd host-agent/macos
chmod +x *.sh
./install_service_macos.sh
```
What it does:
- Creates `.venv` at repo root (once)
- Installs requirements
- Writes `~/Library/LaunchAgents/com.wol.host.agent.plist`
- Loads the agent

Check: `launchctl list | grep com.wol.host.agent`
Uninstall (keeps files/venv):
```
./uninstall_service_macos.sh
```

## 4) Test
- Host agent status: `http://<PC_IP>:8888/status`
- ESP status: `http://<ESP_IP>/status`
- Actions:
  - Wake: `http://<ESP_IP>/wake?token=<TOKEN>`
  - Sleep: `http://<ESP_IP>/sleep?token=<TOKEN>`
  - Shutdown: `http://<ESP_IP>/shutdown?token=<TOKEN>`
  - Restart (host only): `http://<PC_IP>:8888/restart?token=<TOKEN>`

## 5) Remote Access (optional)
Access your ESP8266 from anywhere using a free VPN like Tailscale:

1. Install Tailscale on your PC and mobile device
2. Connect both devices to the same Tailscale network
3. Access your ESP8266 using the Tailscale IP address
4. Use the web interface at `http://<ESP_IP>/` for full control

Alternative: Use your router's port forwarding (less secure) or DuckDNS for dynamic DNS.

## 🛡️ PC Configuration for Wake-on-LAN

### BIOS Settings (Critical!)

Access BIOS (usually DEL, F2, or F12 during boot):

1. **Wake on LAN**: Enabled
2. **ErP**: **Disabled** (this is the most common issue!)
3. **PME Event Wake Up**: Enabled
4. **PCIe Power On**: Enabled

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

3. Ensure PC is connected via **Ethernet** (not Wi‑Fi)

### Linux Settings

```
# Check current WoL setting
sudo ethtool eth0 | grep Wake-on

# Enable WoL
sudo ethtool -s eth0 wol g

# Make persistent (Ubuntu/Debian)
sudo sh -c 'echo "post-up /sbin/ethtool -s eth0 wol g" >> /etc/network/interfaces'
```

## Troubleshooting
- PC won’t wake from shutdown: BIOS **ErP** must be disabled; NIC LEDs should remain on when off
- ESP can’t connect Wi‑Fi: ensure 2.4 GHz SSID and correct password; avoid WPA3‑only
- Wrong broadcast IP: if ESP IP is 192.168.x.y and mask is 255.255.255.0, use 192.168.x.255
- MAC format: use colons, e.g., `AA:BB:CC:DD:EE:FF` (Windows shows dashes; replace with colons)
- Token issues: if using symbols (#, ?, &), URL‑encode them when using in URLs
- Host agent not reachable: confirm service status and firewall; test `http://localhost:8888/status`

## 🤝 Contributing

Found a bug or have a feature suggestion? Open an issue!

Want to improve the project? Pull requests welcome!

## 📄 License

This project is licensed under the MIT License.

```
MIT License

Copyright (c) 2025 DIY Wake-on-LAN Project

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 🙏 Acknowledgments

**Inspiration**:
- [Simple Wake on LAN by herzhenr](https://github.com/herzhenr/simple-wake-on-lan)
- [WakeOnLAN by basildane](https://github.com/basildane/WakeOnLAN)

**Libraries**:
- ESP8266WiFi, ESP8266WebServer, WiFiUdp


