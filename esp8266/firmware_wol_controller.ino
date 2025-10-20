/*
 * ESP8266 Wake-on-LAN System
 * 
 * This sketch turns your ESP8266 into a secure Wake-on-LAN trigger
 * with a modern web interface for remote control.
 * 
 * Features:
 * - Wi-Fi connectivity with auto-reconnect
 * - Secure HTTP endpoint with token authentication
 * - Wake-on-LAN magic packet sender
 * - Sleep, shutdown, restart commands
 * - Built-in LED feedback
 * - Responsive web UI with validation
 * - Comprehensive logging
 * 
 * Hardware:
 * - ESP8266 (NodeMCU, Wemos D1 Mini, or ESP8266MOD)
 * - Built-in LED for status feedback
 * 
 * Author: DIY Wake-on-LAN Project
 * License: MIT
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>
#include <ESP8266mDNS.h>

// ============================================
// CONFIGURATION - Fill with config/device.yaml values
// ============================================

// Wi-Fi credentials (wifi.ssid, wifi.password)
const char* WIFI_SSID = "Your_2G_WiFi_Name";
const char* WIFI_PASSWORD = "Your_WiFi_Password";

// Target PC configuration (esp.target_mac with colons; esp.broadcast_ip; esp.wol_port)
const char* TARGET_MAC = "AA:BB:CC:DD:EE:FF";
const char* BROADCAST_IP = "192.168.x.255";
const int WOL_PORT = 9;

// Security token (esp.token)
const char* SECRET_TOKEN = "change_this_to_a_long_random";

// PC IP address and port (host.ip, host.port)
const char* PC_IP_ADDRESS = "192.168.x.y";
const int PC_SHUTDOWN_PORT = 8888;

// Using built-in LED (GPIO2, active LOW)
const int LED_PIN = LED_BUILTIN;

const int HTTP_PORT = 80;

// ============================================
// END CONFIGURATION
// ============================================

ESP8266WebServer server(HTTP_PORT);
WiFiUDP udp;

// Global variables
unsigned long lastWiFiCheck = 0;
unsigned long lastHeartbeat = 0;
int connectionAttempts = 0;
bool systemReady = false;
unsigned long bootTime = 0;

// Log storage system
const int MAX_LOG_ENTRIES = 50;
const int MAX_LOG_LENGTH = 100;
struct LogEntry {
  String timestamp;
  String level;
  String message;
  bool isEmpty;
};
LogEntry logEntries[MAX_LOG_ENTRIES];
int currentLogIndex = 0;
int totalLogEntries = 0;

// Function prototypes
void setupWiFi();
void setupServer();
void handleRoot();
void handleWake();
void handleShutdown();
void handleSleep();
void handleRestart();
void handleStatus();
void handleNotFound();
bool sendWakeOnLAN(const char* macAddress);
bool sendShutdownCommand();
bool sendSleepCommand();
bool sendRestartCommand();
void blinkLED(int times);
byte* parseMACAddress(const char* macStr);
void logMessage(String level, String message);
void checkWiFiConnection();
void handleAPI();
String getUptime();
void handleLogs();
void storeLogEntry(String level, String message);
String getRecentLogsJSON(int count = 10);
String escapeJSONString(String input);

void setup() {
  Serial.begin(115200);
  delay(100);
  
  bootTime = millis(); // Record boot time
  
  logMessage("INFO", "ESP8266 Wake-on-LAN System Starting...");
  logMessage("INFO", "Boot time recorded: " + String(bootTime) + "ms");
  
  // Initialize SPIFFS file system
  if (SPIFFS.begin()) {
    logMessage("SUCCESS", "SPIFFS file system initialized");
  } else {
    logMessage("ERROR", "SPIFFS file system initialization failed");
  }
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  
  setupWiFi();
  setupServer();
  
  // Initialize mDNS
  if (MDNS.begin("wol")) {
    logMessage("SUCCESS", "mDNS responder started: wol.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    logMessage("WARNING", "mDNS responder failed to start");
  }
  
  systemReady = true;
  blinkLED(3);
  
  logMessage("SUCCESS", "System ready! IP: " + WiFi.localIP().toString());
  logMessage("INFO", "Current uptime: " + getUptime());
}

void loop() { 
  server.handleClient();
  MDNS.update();
  checkWiFiConnection();
  
  // Heartbeat every 5 minutes
  if (millis() - lastHeartbeat > 300000) {
    lastHeartbeat = millis();
    logMessage("INFO", "System heartbeat - Uptime: " + getUptime());
  }
}

void setupWiFi() {
  logMessage("INFO", "Connecting to Wi-Fi: " + String(WIFI_SSID));
  
  WiFi.mode(WIFI_STA);
  WiFi.hostname("wol");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { 
    delay(500); 
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
    attempts++; 
  }
  
  digitalWrite(LED_BUILTIN, HIGH);
  
  if (WiFi.status() == WL_CONNECTED) {
    logMessage("SUCCESS", "Wi-Fi connected! IP: " + WiFi.localIP().toString());
    logMessage("INFO", "Signal strength: " + String(WiFi.RSSI()) + " dBm");
    connectionAttempts = 0;
  } else { 
    logMessage("ERROR", "Wi-Fi connection failed! Restarting...");
    delay(2000); 
    ESP.restart(); 
  }
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/wake", HTTP_GET, handleWake);
  server.on("/shutdown", HTTP_GET, handleShutdown);
  server.on("/sleep", HTTP_GET, handleSleep);
  server.on("/restart", HTTP_GET, handleRestart);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/api", HTTP_GET, handleAPI);
  server.on("/logs", HTTP_GET, handleLogs);
  server.onNotFound(handleNotFound);
  
  server.begin();
  
  logMessage("SUCCESS", "HTTP server started on port " + String(HTTP_PORT));
  logMessage("INFO", "Available endpoints:");
  logMessage("INFO", "  - http://wol.local/ (Friendly URL)");
  logMessage("INFO", "  - http://" + WiFi.localIP().toString() + "/ (IP Address)");
  logMessage("INFO", "  - http://" + WiFi.localIP().toString() + "/wake?token=YOUR_TOKEN");
  logMessage("INFO", "  - http://" + WiFi.localIP().toString() + "/sleep?token=YOUR_TOKEN");
  logMessage("INFO", "  - http://" + WiFi.localIP().toString() + "/shutdown?token=YOUR_TOKEN");
  logMessage("INFO", "  - http://" + WiFi.localIP().toString() + "/restart?token=YOUR_TOKEN");
  logMessage("INFO", "  - http://" + WiFi.localIP().toString() + "/status");
  logMessage("INFO", "  - http://" + WiFi.localIP().toString() + "/logs");
}

void handleRoot() {
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP8266 Wake-on-LAN</title>";
  html += "<style>";
  html += "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
  html += ".container { max-width: 800px; margin: 0 auto; }";
  html += ".card { background: white; border-radius: 12px; padding: 24px; margin: 16px 0; box-shadow: 0 4px 20px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; margin: 0 0 8px 0; font-size: 28px; }";
  html += ".subtitle { color: #666; margin: 0 0 24px 0; font-size: 16px; }";
  html += ".status { display: inline-block; padding: 4px 12px; border-radius: 20px; font-size: 14px; font-weight: 500; }";
  html += ".status.online { background: #d4edda; color: #155724; }";
  html += ".status.offline { background: #f8d7da; color: #721c24; }";
  html += ".info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin: 20px 0; }";
  html += ".info-item { background: #f8f9fa; padding: 16px; border-radius: 8px; }";
  html += ".info-label { font-weight: 600; color: #495057; font-size: 14px; margin-bottom: 4px; }";
  html += ".info-value { color: #212529; font-size: 16px; font-family: 'Courier New', monospace; }";
  html += ".actions-container { display: flex; flex-direction: column; gap: 20px; margin: 24px 0; }";
  html += ".actions-row { display: flex; justify-content: center; gap: 15px; flex-wrap: wrap; }";
  html += ".actions-row.two-buttons { justify-content: center; flex-wrap: nowrap; }";
  html += ".btn { padding: 12px 20px; border: none; border-radius: 12px; font-size: 14px; font-weight: 600; cursor: pointer; text-decoration: none; text-align: center; display: inline-block; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); box-shadow: 0 4px 12px rgba(0,0,0,0.15); position: relative; overflow: hidden; min-width: 120px; max-width: 150px; flex: 1; }";
  html += ".btn::before { content: ''; position: absolute; top: 0; left: -100%; width: 100%; height: 100%; background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent); transition: left 0.5s; }";
  html += ".btn:hover::before { left: 100%; }";
  html += ".btn:active { transform: translateY(2px) scale(0.98); box-shadow: 0 2px 8px rgba(0,0,0,0.2); }";
  html += ".btn-primary { background: linear-gradient(135deg, #007bff, #0056b3); color: white; box-shadow: 0 4px 12px rgba(0,123,255,0.3); }";
  html += ".btn-primary:hover { background: linear-gradient(135deg, #0056b3, #004085); transform: translateY(-3px) scale(1.05); box-shadow: 0 8px 20px rgba(0,123,255,0.4); }";
  html += ".btn-warning { background: linear-gradient(135deg, #ffc107, #e0a800); color: #212529; box-shadow: 0 4px 12px rgba(255,193,7,0.3); }";
  html += ".btn-warning:hover { background: linear-gradient(135deg, #e0a800, #d39e00); transform: translateY(-3px) scale(1.05); box-shadow: 0 8px 20px rgba(255,193,7,0.4); }";
  html += ".btn-danger { background: linear-gradient(135deg, #dc3545, #c82333); color: white; box-shadow: 0 4px 12px rgba(220,53,69,0.3); }";
  html += ".btn-danger:hover { background: linear-gradient(135deg, #c82333, #bd2130); transform: translateY(-3px) scale(1.05); box-shadow: 0 8px 20px rgba(220,53,69,0.4); }";
  html += ".btn-secondary { background: linear-gradient(135deg, #6c757d, #545b62); color: white; box-shadow: 0 4px 12px rgba(108,117,125,0.3); }";
  html += ".btn-secondary:hover { background: linear-gradient(135deg, #545b62, #495057); transform: translateY(-3px) scale(1.05); box-shadow: 0 8px 20px rgba(108,117,125,0.4); }";
  html += ".btn-orange { background: linear-gradient(135deg, #fd7e14, #e55a00); color: white; box-shadow: 0 4px 12px rgba(253,126,20,0.3); }";
  html += ".btn-orange:hover { background: linear-gradient(135deg, #e55a00, #cc5500); transform: translateY(-3px) scale(1.05); box-shadow: 0 8px 20px rgba(253,126,20,0.4); }";
  html += ".toast { position: fixed; top: 20px; right: 20px; padding: 16px 24px; border-radius: 8px; color: white; font-weight: 500; z-index: 1000; transform: translateX(400px); transition: transform 0.3s ease; }";
  html += ".toast.show { transform: translateX(0); }";
  html += ".toast.success { background: #28a745; }";
  html += ".toast.error { background: #dc3545; }";
  html += ".toast.info { background: #17a2b8; }";
  html += ".warning { background: #fff3cd; border: 1px solid #ffeaa7; border-radius: 8px; padding: 16px; margin: 16px 0; }";
  html += ".warning-title { font-weight: 600; color: #856404; margin-bottom: 8px; }";
  html += ".warning-text { color: #856404; font-size: 14px; }";
  html += ".footer { text-align: center; margin-top: 32px; color: #6c757d; font-size: 14px; }";
  html += ".notification-container { position: fixed; top: 20px; right: 20px; z-index: 1000; }";
  html += ".notification-bell { position: relative; background: #007bff; color: white; width: 50px; height: 50px; border-radius: 50%; display: flex; align-items: center; justify-content: center; cursor: pointer; box-shadow: 0 4px 12px rgba(0,123,255,0.3); transition: all 0.2s; }";
  html += ".notification-bell:hover { background: #0056b3; transform: scale(1.1); }";
  html += ".bell-icon { font-size: 20px; }";
  html += ".notification-count { position: absolute; top: -5px; right: -5px; background: #dc3545; color: white; border-radius: 50%; width: 20px; height: 20px; font-size: 12px; display: flex; align-items: center; justify-content: center; font-weight: bold; }";
  html += ".notification-dropdown { position: absolute; top: 60px; right: 0; background: white; border-radius: 8px; box-shadow: 0 8px 25px rgba(0,0,0,0.15); width: 350px; max-height: 400px; overflow-y: auto; display: none; }";
  html += ".notification-dropdown.show { display: block; }";
  html += ".notification-header { padding: 16px; border-bottom: 1px solid #e9ecef; font-weight: 600; color: #333; }";
  html += ".notification-content { max-height: 250px; overflow-y: auto; }";
  html += ".notification-item { padding: 12px 16px; border-bottom: 1px solid #f8f9fa; font-size: 14px; }";
  html += ".notification-item:last-child { border-bottom: none; }";
  html += ".notification-footer { padding: 12px 16px; border-top: 1px solid #e9ecef; text-align: center; }";
  html += ".notification-link { color: #007bff; text-decoration: none; font-size: 14px; font-weight: 500; }";
  html += ".notification-link:hover { text-decoration: underline; }";
  html += "@media (max-width: 768px) { .container { padding: 0 10px; } .card { padding: 16px; } .actions-row { gap: 10px; } .actions-row.two-buttons { gap: 10px; } .notification-dropdown { width: 280px; } .btn { min-width: 100px; max-width: 130px; font-size: 13px; padding: 10px 16px; } }";
  html += "@media (max-width: 480px) { .actions-row { gap: 8px; } .actions-row.two-buttons { gap: 8px; } .btn { min-width: 110px; max-width: 140px; font-size: 12px; padding: 8px 14px; } }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  
  // Notification dropdown
  html += "<div class='notification-container'>";
  html += "<div class='notification-bell' onclick='toggleNotifications()'>";
  html += "<span class='bell-icon'>&#128276;</span>";
  html += "<span class='notification-count' id='notification-count'>0</span>";
  html += "</div>";
  html += "<div class='notification-dropdown' id='notification-dropdown'>";
  html += "<div class='notification-header'>Recent Logs</div>";
  html += "<div class='notification-content' id='notification-content'>";
  html += "<div class='notification-item'>Loading logs...</div>";
  html += "</div>";
  html += "<div class='notification-footer'>";
  html += "<a href='/logs' class='notification-link'>View All Logs</a>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h1>&#128640; ESP8266 Wake-on-LAN</h1>";
  html += "<p class='subtitle'>Remote PC Control System</p>";
  html += "<span class='status online'>&#9679; Online</span>";
  html += "<p style='font-size: 14px; color: #666; margin-top: 8px;'>Access via: <strong>http://wol.local/</strong></p>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>System Status</h2>";
  html += "<div class='info-grid'>";
  html += "<div class='info-item'><div class='info-label'>IP Address</div><div class='info-value'>" + WiFi.localIP().toString() + "</div></div>";
  html += "<div class='info-item'><div class='info-label'>MAC Address</div><div class='info-value'>" + WiFi.macAddress() + "</div></div>";
  html += "<div class='info-item'><div class='info-label'>Signal Strength</div><div class='info-value'>" + String(WiFi.RSSI()) + " dBm</div></div>";
  html += "<div class='info-item'><div class='info-label'>Uptime</div><div class='info-value'>" + getUptime() + "</div></div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>Target PC</h2>";
  html += "<div class='info-grid'>";
  html += "<div class='info-item'><div class='info-label'>MAC Address</div><div class='info-value'>" + String(TARGET_MAC) + "</div></div>";
  html += "<div class='info-item'><div class='info-label'>Broadcast IP</div><div class='info-value'>" + String(BROADCAST_IP) + "</div></div>";
  html += "<div class='info-item'><div class='info-label'>WoL Port</div><div class='info-value'>" + String(WOL_PORT) + "</div></div>";
  html += "<div class='info-item'><div class='info-label'>PC IP</div><div class='info-value'>" + String(PC_IP_ADDRESS) + "</div></div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>Quick Actions</h2>";
  html += "<div class='actions-container'>";
  html += "<div class='actions-row'>";
  html += "<a href='/wake?token=" + String(SECRET_TOKEN) + "' class='btn btn-primary' onclick='showToast(\"Alerting PC...\", \"info\")'>&#128268; Wake PC</a>";
  html += "<a href='/sleep?token=" + String(SECRET_TOKEN) + "' class='btn btn-warning' onclick='showToast(\"Sending sleep command...\", \"info\")'>&#128564; Sleep PC</a>";
  html += "<a href='/shutdown?token=" + String(SECRET_TOKEN) + "' class='btn btn-danger' onclick='showToast(\"Sending shutdown command...\", \"info\")'>&#128683; Shutdown PC</a>";
  html += "<a href='/restart?token=" + String(SECRET_TOKEN) + "' class='btn btn-orange' onclick='showToast(\"Sending restart command...\", \"info\")'>&#128259; Restart PC</a>";
  html += "</div>";
  html += "<div class='actions-row two-buttons'>";
  html += "<a href='/status' class='btn btn-secondary' onclick='showToast(\"Checking status...\", \"info\")'>&#128202; Status</a>";
  html += "<a href='/logs' class='btn btn-secondary' onclick='showToast(\"Opening logs...\", \"info\")'>&#128203; Logs</a>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='warning'>";
  html += "<div class='warning-title'>&#9888;&#65039; Security Notice</div>";
  html += "<div class='warning-text'>This system uses token-based authentication. All commands require a valid security token. Keep your token secure and never share it publicly.</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>API Endpoints</h2>";
  html += "<p>Use these endpoints with your security token:</p>";
  html += "<div style='background: #f8f9fa; padding: 16px; border-radius: 8px; font-family: monospace; font-size: 14px;'>";
  html += "<div>GET /wake?token=YOUR_TOKEN</div>";
  html += "<div>GET /sleep?token=YOUR_TOKEN</div>";
  html += "<div>GET /shutdown?token=YOUR_TOKEN</div>";
  html += "<div>GET /restart?token=YOUR_TOKEN</div>";
  html += "<div>GET /status</div>";
  html += "<div>GET /logs (System Logs)</div>";
  html += "</div>";
  html += "<div style='margin-top: 10px; padding: 10px; background: #e7f3ff; border-radius: 5px; font-size: 12px;'>";
  html += "<strong>URL:</strong> http://wol.local/";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='footer'>";
  html += "<p>ESP8266 Wake-on-LAN System v1.0</p>";
  html += "<p>Built with &#10084;&#65039; for DIY enthusiasts</p>";
  html += "</div>";
  
  html += "</div>";
  
  // Toast notification system
  html += "<div id='toast' class='toast'></div>";
  
  html += "<script>";
  html += "function showToast(message, type) {";
  html += "  const toast = document.getElementById('toast');";
  html += "  toast.textContent = message;";
  html += "  toast.className = 'toast ' + type + ' show';";
  html += "  setTimeout(() => {";
  html += "    toast.className = 'toast ' + type;";
  html += "  }, 3000);";
  html += "}";
  html += "function toggleNotifications() {";
  html += "  const dropdown = document.getElementById('notification-dropdown');";
  html += "  dropdown.classList.toggle('show');";
  html += "  if (dropdown.classList.contains('show')) {";
  html += "    loadRecentLogs();";
  html += "    startNotificationRefresh();";
  html += "  } else {";
  html += "    stopNotificationRefresh();";
  html += "  }";
  html += "}";
  html += "let lastLogCount = 0;";
  html += "function loadRecentLogs() {";
  html += "  fetch('/logs?json=true&count=5')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      const content = document.getElementById('notification-content');";
  html += "      const count = document.getElementById('notification-count');";
  html += "      if (data.logs && data.logs.length > 0) {";
  html += "        content.innerHTML = '';";
  html += "        data.logs.forEach(log => {";
  html += "          const item = document.createElement('div');";
  html += "          item.className = 'notification-item';";
  html += "          item.innerHTML = '<strong>' + log.level + '</strong> ' + log.message;";
  html += "          content.appendChild(item);";
  html += "        });";
  html += "        count.textContent = data.logs.length;";
  html += "        lastLogCount = data.logs.length;";
  html += "      } else {";
  html += "        content.innerHTML = '<div class=\"notification-item\">No recent logs</div>';";
  html += "        count.textContent = '0';";
  html += "        lastLogCount = 0;";
  html += "      }";
  html += "    })";
  html += "    .catch(error => {";
  html += "      const content = document.getElementById('notification-content');";
  html += "      content.innerHTML = '<div class=\"notification-item\">Error loading logs</div>';";
  html += "    });";
  html += "}";
  html += "document.addEventListener('click', function(e) {";
  html += "  const dropdown = document.getElementById('notification-dropdown');";
  html += "  const bell = document.querySelector('.notification-bell');";
  html += "  if (!bell.contains(e.target) && dropdown.classList.contains('show')) {";
  html += "    dropdown.classList.remove('show');";
  html += "  }";
  html += "});";
  html += "let refreshInterval;";
  html += "function startNotificationRefresh() {";
  html += "  refreshInterval = setInterval(() => {";
  html += "    const dropdown = document.getElementById('notification-dropdown');";
  html += "    if (dropdown.classList.contains('show')) {";
  html += "      loadRecentLogs();";
  html += "    }";
  html += "  }, 30000);";
  html += "}";
  html += "function stopNotificationRefresh() {";
  html += "  if (refreshInterval) {";
  html += "    clearInterval(refreshInterval);";
  html += "  }";
  html += "}";
  html += "window.addEventListener('beforeunload', stopNotificationRefresh);";
  html += "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleWake() {
  
  if (!server.hasArg("token")) {
    logMessage("ERROR", "Wake request rejected: Missing token");
    server.send(401, "application/json", "{\"error\":\"Missing token\",\"code\":401}");
    blinkLED(3);
    return;
  }
  
  String providedToken = server.arg("token");
  if (providedToken != SECRET_TOKEN) {
    logMessage("ERROR", "Wake request rejected: Invalid token");
    server.send(401, "application/json", "{\"error\":\"Invalid token\",\"code\":401}");
    blinkLED(3);
    return;
  }
  
  logMessage("SUCCESS", "Wake request authorized");
  bool success = sendWakeOnLAN(TARGET_MAC);
  
  if (success) {
    logMessage("SUCCESS", "Wake-on-LAN packet sent successfully");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Wake command sent\",\"target\":\"" + String(TARGET_MAC) + "\"}");
    blinkLED(2);
  } else {
    logMessage("ERROR", "Failed to send Wake-on-LAN packet");
    server.send(500, "application/json", "{\"error\":\"Failed to send wake packet\",\"code\":500}");
    blinkLED(5);
  }
}

void handleShutdown() {
  
  if (!server.hasArg("token")) {
    logMessage("ERROR", "Shutdown request rejected: Missing token");
    server.send(401, "application/json", "{\"error\":\"Missing token\",\"code\":401}");
    blinkLED(3);
    return;
  }
  
  String providedToken = server.arg("token");
  if (providedToken != SECRET_TOKEN) {
    logMessage("ERROR", "Shutdown request rejected: Invalid token");
    server.send(401, "application/json", "{\"error\":\"Invalid token\",\"code\":401}");
    blinkLED(3);
    return;
  }
  
  logMessage("SUCCESS", "Shutdown request authorized");
  bool success = sendShutdownCommand();
  
  if (success) {
    logMessage("SUCCESS", "Shutdown command sent successfully");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Shutdown command sent\",\"target\":\"" + String(PC_IP_ADDRESS) + "\"}");
    blinkLED(3);
  } else {
    logMessage("ERROR", "Failed to send shutdown command");
    server.send(500, "application/json", "{\"error\":\"Failed to send shutdown command\",\"code\":500}");
    blinkLED(5);
  }
}

void handleSleep() {
  
  if (!server.hasArg("token")) {
    logMessage("ERROR", "Sleep request rejected: Missing token");
    server.send(401, "application/json", "{\"error\":\"Missing token\",\"code\":401}");
    blinkLED(3);
    return;
  }
  
  String providedToken = server.arg("token");
  if (providedToken != SECRET_TOKEN) {
    logMessage("ERROR", "Sleep request rejected: Invalid token");
    server.send(401, "application/json", "{\"error\":\"Invalid token\",\"code\":401}");
    blinkLED(3);
    return;
  }
  
  logMessage("SUCCESS", "Sleep request authorized");
  bool success = sendSleepCommand();
  
  if (success) {
    logMessage("SUCCESS", "Sleep command sent successfully");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Sleep command sent\",\"target\":\"" + String(PC_IP_ADDRESS) + "\"}");
    blinkLED(2);
  } else {
    logMessage("ERROR", "Failed to send sleep command");
    server.send(500, "application/json", "{\"error\":\"Failed to send sleep command\",\"code\":500}");
    blinkLED(5);
  }
}

void handleRestart() {
  
  if (!server.hasArg("token")) {
    logMessage("ERROR", "Restart request rejected: Missing token");
    server.send(401, "application/json", "{\"error\":\"Missing token\",\"code\":401}");
    blinkLED(3);
    return;
  }
  
  String providedToken = server.arg("token");
  if (providedToken != SECRET_TOKEN) {
    logMessage("ERROR", "Restart request rejected: Invalid token");
    server.send(401, "application/json", "{\"error\":\"Invalid token\",\"code\":401}");
    blinkLED(3);
    return;
  }
  
  logMessage("SUCCESS", "Restart request authorized");
  bool success = sendRestartCommand();
  
  if (success) {
    logMessage("SUCCESS", "Restart command sent successfully");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Restart command sent\",\"target\":\"" + String(PC_IP_ADDRESS) + "\"}");
    blinkLED(4);
  } else {
    logMessage("ERROR", "Failed to send restart command");
    server.send(500, "application/json", "{\"error\":\"Failed to send restart command\",\"code\":500}");
    blinkLED(5);
  }
}

void handleStatus() {
  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
  json += "\"target_mac\":\"" + String(TARGET_MAC) + "\",";
  json += "\"broadcast_ip\":\"" + String(BROADCAST_IP) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() { server.send(404, "text/plain", "Not found"); }

bool sendWakeOnLAN(const char* macAddress) {
  byte* mac = parseMACAddress(macAddress);
  if (mac == NULL) return false;
  byte magicPacket[102];
  for (int i = 0; i < 6; i++) magicPacket[i] = 0xFF;
  for (int i = 0; i < 16; i++) for (int j = 0; j < 6; j++) magicPacket[6 + i * 6 + j] = mac[j];
  IPAddress broadcastIP; if (!broadcastIP.fromString(BROADCAST_IP)) { delete[] mac; return false; }
  udp.begin(WOL_PORT); udp.beginPacket(broadcastIP, WOL_PORT); udp.write(magicPacket, sizeof(magicPacket)); bool ok = udp.endPacket(); udp.stop(); delete[] mac; return ok;
}

bool sendShutdownCommand() {
  WiFiClient client; HTTPClient http;
  String url = "http://" + String(PC_IP_ADDRESS) + ":" + String(PC_SHUTDOWN_PORT) + "/shutdown?token=" + String(SECRET_TOKEN);
  http.begin(client, url); http.setTimeout(5000); int code = http.GET(); http.end(); return code == 200;
}

bool sendSleepCommand() {
  WiFiClient client; HTTPClient http;
  String url = "http://" + String(PC_IP_ADDRESS) + ":" + String(PC_SHUTDOWN_PORT) + "/sleep?token=" + String(SECRET_TOKEN);
  http.begin(client, url); http.setTimeout(5000); int code = http.GET(); http.end(); return code == 200;
}

bool sendRestartCommand() {
  WiFiClient client; HTTPClient http;
  String url = "http://" + String(PC_IP_ADDRESS) + ":" + String(PC_SHUTDOWN_PORT) + "/restart?token=" + String(SECRET_TOKEN);
  http.begin(client, url); http.setTimeout(5000); int code = http.GET(); http.end(); return code == 200;
}

byte* parseMACAddress(const char* macStr) {
  byte* mac = new byte[6]; if (strlen(macStr) != 17) { delete[] mac; return NULL; }
  for (int i = 0; i < 6; i++) { char byteStr[3] = {macStr[i*3], macStr[i*3+1], '\0'}; mac[i] = (byte)strtol(byteStr, NULL, 16); }
  return mac;
}

void blinkLED(int times) { 
  for (int i=0;i<times;i++){ 
    digitalWrite(LED_BUILTIN, LOW); 
    delay(100); 
    digitalWrite(LED_BUILTIN, HIGH); 
    delay(100);
  } 
}

// ============================================
// Utility Functions
// ============================================

void logMessage(String level, String message) {
  String timestamp = String(millis() / 1000);
  Serial.println("[" + timestamp + "s] [" + level + "] " + message);
  
  // Store in circular buffer
  storeLogEntry(level, message);
}


void checkWiFiConnection() {
  if (millis() - lastWiFiCheck > 10000) { // Check every 10 seconds
    lastWiFiCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      connectionAttempts++;
      logMessage("WARNING", "Wi-Fi disconnected! Attempting reconnection... (" + String(connectionAttempts) + ")");
      
      if (connectionAttempts > 5) {
        logMessage("ERROR", "Too many connection failures. Restarting...");
        ESP.restart();
      }
      
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    } else {
      if (connectionAttempts > 0) {
        logMessage("SUCCESS", "Wi-Fi reconnected!");
        connectionAttempts = 0;
      }
    }
  }
}

void handleAPI() {
  
  String json = "{";
  json += "\"system\":{";
  json += "\"status\":\"online\",";
  json += "\"uptime\":\"" + getUptime() + "\",";
  json += "\"version\":\"1.0.0\"";
  json += "},";
  json += "\"wifi\":{";
  json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI());
  json += "},";
  json += "\"target\":{";
  json += "\"mac\":\"" + String(TARGET_MAC) + "\",";
  json += "\"broadcast\":\"" + String(BROADCAST_IP) + "\",";
  json += "\"port\":" + String(WOL_PORT) + ",";
  json += "\"pc_ip\":\"" + String(PC_IP_ADDRESS) + "\"";
  json += "},";
  json += "\"endpoints\":[";
  json += "\"/wake\",\"/sleep\",\"/shutdown\",\"/restart\",\"/status\"";
  json += "]";
  json += "}";
  
  server.send(200, "application/json", json);
}

String getUptime() {
  unsigned long currentTime = millis();
  unsigned long uptimeMs;
  
  // Use bootTime for actual system uptime
  if (currentTime >= bootTime) {
    uptimeMs = currentTime - bootTime;
  } else {
    // Handle millis() overflow (happens every ~49 days)
    uptimeMs = (ULONG_MAX - bootTime) + currentTime + 1;
  }
  
  unsigned long seconds = uptimeMs / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  // Format as days, hours, minutes, seconds
  String result = "";
  if (days > 0) {
    result += String(days) + "d ";
  }
  if (hours % 24 > 0) {
    result += String(hours % 24) + "h ";
  }
  if (minutes % 60 > 0) {
    result += String(minutes % 60) + "m ";
  }
  result += String(seconds % 60) + "s";
  
  
  return result;
}


void handleLogs() {
  String clientIP = server.client().remoteIP().toString();
  
  // Check if JSON format requested
  if (server.hasArg("json") && server.arg("json") == "true") {
    int count = 10;
    if (server.hasArg("count")) {
      count = server.arg("count").toInt();
      if (count > MAX_LOG_ENTRIES) count = MAX_LOG_ENTRIES;
      if (count < 1) count = 1;
    }
    
    String json = getRecentLogsJSON(count);
    if (json.length() > 0) {
      server.send(200, "application/json", json);
    } else {
      server.send(500, "application/json", "{\"error\":\"Failed to generate logs JSON\"}");
    }
    return;
  }
  
  // HTML logs page
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>System Logs</title>";
  html += "<style>";
  html += "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
  html += ".container { max-width: 1000px; margin: 0 auto; }";
  html += ".card { background: white; border-radius: 12px; padding: 24px; margin: 16px 0; box-shadow: 0 4px 20px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; margin: 0 0 8px 0; font-size: 28px; }";
  html += ".subtitle { color: #666; margin: 0 0 24px 0; font-size: 16px; }";
  html += ".btn { padding: 12px 24px; border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; text-decoration: none; text-align: center; display: inline-block; transition: all 0.2s; background: #007bff; color: white; }";
  html += ".btn:hover { background: #0056b3; transform: translateY(-1px); }";
  html += ".log-container { background: #f8f9fa; border-radius: 8px; padding: 20px; max-height: 600px; overflow-y: auto; font-family: 'Courier New', monospace; font-size: 14px; }";
  html += ".log-entry { margin-bottom: 8px; padding: 8px; border-radius: 4px; }";
  html += ".log-entry.INFO { background: #d1ecf1; color: #0c5460; }";
  html += ".log-entry.SUCCESS { background: #d4edda; color: #155724; }";
  html += ".log-entry.WARNING { background: #fff3cd; color: #856404; }";
  html += ".log-entry.ERROR { background: #f8d7da; color: #721c24; }";
  html += ".log-entry.DEBUG { background: #e2e3e5; color: #383d41; }";
  html += ".refresh-btn { background: #28a745; margin-left: 10px; }";
  html += ".refresh-btn:hover { background: #1e7e34; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='card'>";
  html += "<h1>&#128203; System Logs</h1>";
  html += "<p class='subtitle'>Recent system activity and debug information</p>";
  html += "<div style='margin-bottom: 20px;'>";
  html += "<a href='/' class='btn'>&#8592; Back to Main</a>";
  html += "<button onclick='refreshLogs()' class='btn refresh-btn'>&#8635; Refresh</button>";
  html += "</div>";
  html += "<div class='log-container' id='log-container'>";
  html += "Loading logs...";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<script>";
  html += "function refreshLogs() {";
  html += "  const container = document.getElementById('log-container');";
  html += "  container.innerHTML = 'Loading logs...';";
  html += "  fetch('/logs?json=true&count=50')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      if (data.logs && data.logs.length > 0) {";
  html += "        let html = '';";
  html += "        data.logs.forEach(log => {";
  html += "          html += '<div class=\"log-entry ' + log.level + '\">';";
  html += "          html += '<strong>[' + log.timestamp + 's]</strong> [' + log.level + '] ' + log.message;";
  html += "          html += '</div>';";
  html += "        });";
  html += "        container.innerHTML = html;";
  html += "        container.scrollTop = container.scrollHeight;";
  html += "      } else {";
  html += "        container.innerHTML = '<div class=\"log-entry INFO\">No logs available</div>';";
  html += "      }";
  html += "    })";
  html += "    .catch(error => {";
  html += "      container.innerHTML = '<div class=\"log-entry ERROR\">Error loading logs: ' + error + '</div>';";
  html += "    });";
  html += "}";
  html += "document.addEventListener('DOMContentLoaded', refreshLogs);";
  html += "setInterval(refreshLogs, 10000);";
  html += "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void storeLogEntry(String level, String message) {
  // Store in circular buffer
  logEntries[currentLogIndex].timestamp = String(millis() / 1000);
  logEntries[currentLogIndex].level = level;
  logEntries[currentLogIndex].message = message;
  logEntries[currentLogIndex].isEmpty = false;
  
  currentLogIndex = (currentLogIndex + 1) % MAX_LOG_ENTRIES;
  if (totalLogEntries < MAX_LOG_ENTRIES) {
    totalLogEntries++;
  }
}

String getRecentLogsJSON(int count) {
  String json = "{";
  json += "\"total_entries\":" + String(totalLogEntries) + ",";
  json += "\"requested_count\":" + String(count) + ",";
  json += "\"logs\":[";
  
  int entriesToShow = (count > totalLogEntries) ? totalLogEntries : count;
  bool firstEntry = true;
  int entriesAdded = 0;
  
  for (int i = 0; i < entriesToShow && entriesAdded < count; i++) {
    int index = (currentLogIndex - 1 - i + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
    if (!logEntries[index].isEmpty) {
      if (!firstEntry) json += ",";
      json += "{";
      json += "\"timestamp\":\"" + escapeJSONString(logEntries[index].timestamp) + "\",";
      json += "\"level\":\"" + escapeJSONString(logEntries[index].level) + "\",";
      json += "\"message\":\"" + escapeJSONString(logEntries[index].message) + "\"";
      json += "}";
      firstEntry = false;
      entriesAdded++;
    }
  }
  
  json += "]";
  json += "}";
  
  return json;
}

String escapeJSONString(String input) {
  String escaped = "";
  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    switch (c) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default: escaped += c; break;
    }
  }
  return escaped;
}
