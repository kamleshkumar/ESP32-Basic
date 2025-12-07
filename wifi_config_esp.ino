// ===========================================================================
// 🎮 ESP32 REMOTE CONTROLLER - SIMPLIFIED VERSION
// ===========================================================================
// Lightweight firmware for controlling ESP32 devices via WebSocket
// Features:
// 1. WiFi Configuration via AP Mode
// 2. WebSocket Client (port 81)
// 3. Simple Device Control (mDNS name or IP → GPIO Pin)
// 4. GPIO Control (DIRECT or TOUCH)
// ===========================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>

// ====== AP Settings ======
const char* apSSID = "ESP32_Remote_Config";
const char* apPassword = "12345678";

// ====== Global Objects ======
WebServer server(80);
Preferences prefs;
WebSocketsClient webSocket;

String targetSSID = "";
String targetPass = "";

// ====== Device Config ======
struct DeviceConfig {
  String deviceName;      // e.g., "LivingRoom_Light"
  String deviceAddress;   // mDNS name (esp32-04cfe8.local) or IP
  int targetGPIO;         // GPIO pin on target device
  int localGPIO;          // GPIO pin on remote (button/touch)
  int controlType;        // 0=DIRECT, 1=TOUCH
  bool currentState;
  bool lastState;
  bool lastTouchState;    // Track previous touch state to detect edge
  unsigned long lastTouchTime;
  unsigned long lastCommandTime; // Prevent rapid commands
};

#define MAX_DEVICES 5
DeviceConfig devices[MAX_DEVICES];
int numDevices = 0;

// ====== System State ======
bool wifiConnected = false;
bool wsConnected = false;
String currentDeviceIP = "";
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;
const unsigned long TOUCH_DEBOUNCE = 500; // Increased to 500ms
const unsigned long COMMAND_COOLDOWN = 1000; // 1 second between commands
const int TOUCH_THRESHOLD = 40;

// ====== Device Discovery ======
struct DiscoveredDevice {
  String ip;
  String name;
  String mdns;
  String mac;
};
#define MAX_DISCOVERED 20
DiscoveredDevice discoveredDevices[MAX_DISCOVERED];
int numDiscovered = 0;
unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 30000; // Scan every 30 seconds

// ====== Simple HTML Pages ======
String htmlFormPage() {
  String page = "<!DOCTYPE html><html><head>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
                "<title>ESP32 Remote</title>"
                "<style>body{font-family:Arial;max-width:400px;margin:40px auto;padding:20px;}"
                "input,select{width:100%;padding:8px;margin:5px 0;box-sizing:border-box;}"
                "button{width:100%;padding:10px;background:#4CAF50;color:white;border:none;border-radius:4px;cursor:pointer;}"
                ".btn-on{background:#4CAF50;color:white;padding:6px 12px;border:none;border-radius:4px;cursor:pointer;margin:2px;}"
                ".btn-off{background:#f44336;color:white;padding:6px 12px;border:none;border-radius:4px;cursor:pointer;margin:2px;}"
                ".btn-on:hover{background:#45a049;}"
                ".btn-off:hover{background:#da190b;}"
                ".device-buttons{display:flex;gap:5px;margin-top:8px;}"
                ".status{padding:10px;margin:10px 0;border-radius:4px;}"
                ".success{background:#e8f5e9;}"
                ".error{background:#ffebee;}"
                "</style></head><body>";
  
  page += "<h2>🎮 ESP32 Remote Controller</h2>";
  
  if (wifiConnected) {
    page += "<div class='status success'>✅ WiFi: " + WiFi.localIP().toString() + "</div>";
    page += "<div class='status " + String(wsConnected ? "success" : "error") + "'>";
    page += wsConnected ? "✅ WebSocket Connected" : "❌ WebSocket Disconnected";
    page += "</div><hr>";
    
    // Check if editing
    String editIdx = "";
    if (server.hasArg("edit")) {
      editIdx = server.arg("edit");
    }
    
    String formTitle = editIdx.length() > 0 ? "Edit Device" : "Add Device";
    String submitText = editIdx.length() > 0 ? "Update Device" : "Add Device";
    
    page += "<h3>" + formTitle + "</h3>";
    page += "<form action='/add' method='POST' id='deviceForm'>";
    page += "<input type='hidden' name='edit_idx' id='edit_idx' value='" + editIdx + "'>";
    
    // Get device values if editing
    String devName = "";
    String devAddr = "";
    String devGpio = "";
    String devLocal = "";
    String devType = "0";
    if (editIdx.length() > 0) {
      int idx = editIdx.toInt();
      if (idx >= 0 && idx < numDevices) {
        devName = devices[idx].deviceName;
        devAddr = devices[idx].deviceAddress;
        devGpio = String(devices[idx].targetGPIO);
        devLocal = String(devices[idx].localGPIO);
        devType = String(devices[idx].controlType);
      }
    }
    
    page += "Device Name:<br><input type='text' name='name' id='dev_name' value='" + devName + "' placeholder='Kitchen Light' required><br>";
    
    // Device Discovery Section
    page += "<div style='background:#e3f2fd;padding:10px;border-radius:4px;margin:10px 0;'>";
    page += "<strong>🔍 Discover Devices:</strong> ";
    page += "<button type='button' onclick='scanDevices()' style='padding:6px 12px;background:#2196F3;color:white;border:none;border-radius:4px;cursor:pointer;'>Scan Network</button>";
    page += "<div id='scanStatus' style='margin-top:5px;font-size:12px;'></div>";
    page += "<select id='discoveredDevices' onchange='selectDevice()' style='width:100%;padding:8px;margin-top:8px;display:none;'>";
    page += "<option value=''>-- Select Discovered Device --</option>";
    page += "</select>";
    page += "</div>";
    
    page += "Device Address:<br><input type='text' name='addr' id='dev_addr' value='" + devAddr + "' placeholder='esp32-04cfe8.local or 192.168.1.100' required><br>";
    page += "Target GPIO:<br><input type='number' name='gpio' id='dev_gpio' value='" + devGpio + "' placeholder='13' min='0' max='39' required><br>";
    page += "Local GPIO (button/touch):<br><input type='number' name='local' id='dev_local' value='" + devLocal + "' placeholder='15' min='0' max='39' required><br>";
    page += "Type:<br><select name='type' id='dev_type'>";
    String selected0 = (devType == "0") ? " selected" : "";
    String selected1 = (devType == "1") ? " selected" : "";
    page += "<option value='0'" + selected0 + ">DIRECT</option>";
    page += "<option value='1'" + selected1 + ">TOUCH</option>";
    page += "</select><br>";
    page += "<button type='submit'>" + submitText + "</button>";
    if (editIdx.length() > 0) {
      page += " <a href='/' style='padding:8px 15px;background:#999;color:white;text-decoration:none;border-radius:4px;display:inline-block;'>Cancel</a>";
    }
    page += "</form>";
    
    // JavaScript for device discovery
    page += "<script>";
    page += "function scanDevices() {";
    page += "  document.getElementById('scanStatus').textContent = 'Scanning...';";
    page += "  fetch('/api/discover')";
    page += "    .then(r => r.json())";
    page += "    .then(data => {";
    page += "      const select = document.getElementById('discoveredDevices');";
    page += "      select.innerHTML = '<option value=\"\">-- Select Discovered Device --</option>';";
    page += "      if (data.devices && data.devices.length > 0) {";
    page += "        data.devices.forEach(dev => {";
    page += "          const opt = document.createElement('option');";
    page += "          opt.value = dev.addr;";
    page += "          opt.textContent = dev.name + ' (' + dev.addr + ')';";
    page += "          opt.setAttribute('data-name', dev.name);";
    page += "          select.appendChild(opt);";
    page += "        });";
    page += "        select.style.display = 'block';";
    page += "        document.getElementById('scanStatus').textContent = 'Found ' + data.devices.length + ' device(s)';";
    page += "      } else {";
    page += "        document.getElementById('scanStatus').textContent = 'No devices found';";
    page += "      }";
    page += "    })";
    page += "    .catch(e => {";
    page += "      document.getElementById('scanStatus').textContent = 'Scan failed: ' + e;";
    page += "    });";
    page += "}";
    page += "function selectDevice() {";
    page += "  const select = document.getElementById('discoveredDevices');";
    page += "  const selected = select.options[select.selectedIndex];";
    page += "  if (selected.value) {";
    page += "    document.getElementById('dev_addr').value = selected.value;";
    page += "    const name = selected.getAttribute('data-name');";
    page += "    if (name && !document.getElementById('dev_name').value) {";
    page += "      document.getElementById('dev_name').value = name;";
    page += "    }";
    page += "  }";
    page += "}";
    page += "</script>";
    
    // Scroll to form when editing
    if (editIdx.length() > 0) {
      page += "<script>window.onload = function() { document.getElementById('deviceForm').scrollIntoView({behavior: 'smooth', block: 'start'}); };</script>";
    }
    
    page += "<hr>";
    
    page += "<h3>Devices (" + String(numDevices) + ")</h3>";
    for (int i = 0; i < numDevices; i++) {
      String stateColor = devices[i].currentState ? "#4CAF50" : "#999";
      String stateText = devices[i].currentState ? "ON" : "OFF";
      
      page += "<div style='background:#f5f5f5;padding:10px;margin:5px 0;border-radius:4px;'>";
      page += "<strong>" + devices[i].deviceName + "</strong> ";
      page += "<span style='background:" + stateColor + ";color:white;padding:2px 8px;border-radius:3px;font-size:12px;'>" + stateText + "</span><br>";
      page += "<small>Address: " + devices[i].deviceAddress + "</small><br>";
      page += "<div style='margin:5px 0;'>";
      page += "<span style='background:#2196F3;color:white;padding:4px 10px;border-radius:4px;font-weight:bold;font-size:13px;'>📌 Physical Pin: GPIO " + String(devices[i].localGPIO) + "</span> ";
      page += "<span style='background:#FF9800;color:white;padding:4px 10px;border-radius:4px;font-size:12px;'>" + String(devices[i].controlType == 0 ? "DIRECT" : "TOUCH") + "</span>";
      page += "</div>";
      page += "<small><strong>Target Device GPIO:</strong> " + String(devices[i].targetGPIO) + "</small>";
      page += "<div class='device-buttons'>";
      page += "<button class='btn-on' onclick=\"fetch('/control?i=" + String(i) + "&a=on').then(()=>location.reload())\">ON</button>";
      page += "<button class='btn-off' onclick=\"fetch('/control?i=" + String(i) + "&a=off').then(()=>location.reload())\">OFF</button>";
      page += "<button class='btn-on' onclick=\"fetch('/control?i=" + String(i) + "&a=toggle').then(()=>location.reload())\" style='background:#2196F3;'>TOGGLE</button>";
      page += "<a href='/?edit=" + String(i) + "' style='padding:6px 12px;background:#9C27B0;color:white;text-decoration:none;border-radius:4px;margin:2px;display:inline-block;font-size:12px;'>Edit</a>";
      page += "<a href='/del?i=" + String(i) + "' style='padding:6px 12px;background:#666;color:white;text-decoration:none;border-radius:4px;margin:2px;display:inline-block;font-size:12px;'>Delete</a>";
      page += "</div>";
      page += "</div>";
    }
  } else {
    page += "<div class='status error'>⚠️ WiFi Not Connected</div>";
    page += "<h3>WiFi Setup</h3>";
    page += "<form action='/save' method='POST'>";
    page += "SSID:<br><input type='text' name='ssid' required><br>";
    page += "Password:<br><input type='password' name='pass'><br>";
    page += "<button type='submit'>Connect</button></form>";
  }
  
  page += "<hr><p><a href='/'>Refresh</a> | <a href='/test'>Test Command</a></p>";
  page += "</body></html>";
  return page;
}

// ====== WiFi Functions ======
bool tryConnectToWiFi(const char* ssid, const char* password) {
  if (!ssid || strlen(ssid) == 0) return false;
  
  Serial.println("📶 Connecting to: " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  for (int i = 0; i < 30; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ WiFi Connected: " + WiFi.localIP().toString());
      wifiConnected = true;
      if (!MDNS.begin("esp32-remote")) {
        Serial.println("⚠️ mDNS failed");
      } else {
        MDNS.addService("http", "tcp", 80);
        Serial.println("✅ mDNS started: esp32-remote.local");
      }
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n❌ WiFi Failed");
  wifiConnected = false;
  return false;
}

void startConfigAP() {
  Serial.println("📡 Starting AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  Serial.println("✅ AP: " + String(apSSID) + " | IP: " + WiFi.softAPIP().toString());
}

// ====== mDNS Resolution ======
String resolveMDNS(String hostname) {
  Serial.println("🔍 Resolving: " + hostname);
  IPAddress ip = MDNS.queryHost(hostname);
  if (ip.toString() != "0.0.0.0") {
    Serial.println("✅ Resolved: " + hostname + " -> " + ip.toString());
    return ip.toString();
  }
  Serial.println("❌ mDNS resolution failed, using as IP");
  return hostname; // Return as-is, might be IP already
}

// ====== WebSocket Functions ======
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("🔌 WebSocket Disconnected");
      wsConnected = false;
      break;
      
    case WStype_CONNECTED:
      Serial.printf("✅ WebSocket Connected: %s\n", payload);
      wsConnected = true;
      break;
      
    case WStype_TEXT:
      Serial.printf("📨 Received: %s\n", payload);
      break;
      
    default:
      break;
  }
}

void connectWebSocket(String address) {
  if (address.length() == 0) return;
  
  // Resolve mDNS if needed
  String ip = address;
  if (address.indexOf(".local") > 0) {
    ip = resolveMDNS(address);
  }
  
  Serial.println("🔌 Connecting to: " + ip + ":81");
  webSocket.begin(ip.c_str(), 81, "/ws");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  currentDeviceIP = ip;
}

// ====== Command Sending ======
String formatDeviceName(String name) {
  // Replace spaces with underscores and convert to lowercase (matching app format)
  String formatted = "";
  for (int i = 0; i < name.length(); i++) {
    char c = name.charAt(i);
    if (c == ' ') {
      formatted += '_';
    } else if (c >= 'A' && c <= 'Z') {
      formatted += (char)(c + 32); // Convert to lowercase
    } else {
      formatted += c;
    }
  }
  return formatted;
}

void sendDeviceCommand(String deviceName, int gpioPin, String action) {
  if (!wsConnected) {
    Serial.println("⚠️ WebSocket not connected");
    return;
  }
  
  // Format matching app: groupName_deviceName_gpioPin_action
  // Example: "nogroup_kitchen_light_13_on"
  String groupName = "nogroup"; // Default group
  String formattedDeviceName = formatDeviceName(deviceName);
  
  // Build command: nogroup_device_name_gpio_action
  String command = groupName + "_" + formattedDeviceName + "_" + String(gpioPin) + "_" + action;
  
  // Create JSON command payload (matching app format)
  StaticJsonDocument<512> doc;
  doc["type"] = "device_command";
  doc["command"] = command;
  
  JsonObject deviceInfo = doc.createNestedObject("device_info");
  deviceInfo["name"] = deviceName;
  deviceInfo["gpioPin"] = gpioPin;
  deviceInfo["groupName"] = groupName;
  
  doc["timestamp"] = millis();
  doc["source"] = "esp32_remote";
  
  String msg;
  serializeJson(doc, msg);
  
  webSocket.sendTXT(msg);
  Serial.println("📤 Command: " + command);
}

void sendDeviceCommandByIndex(int idx, String action) {
  if (idx < 0 || idx >= numDevices) return;
  
  if (action == "toggle") {
    devices[idx].currentState = !devices[idx].currentState;
    action = devices[idx].currentState ? "on" : "off";
  } else {
    devices[idx].currentState = (action == "on");
  }
  
  sendDeviceCommand(devices[idx].deviceName, devices[idx].targetGPIO, action);
  saveDevices();
}

// ====== Device Management ======
void loadDevices() {
  prefs.begin("devices", true);
  numDevices = prefs.getInt("count", 0);
  if (numDevices > MAX_DEVICES) numDevices = MAX_DEVICES;
  
  for (int i = 0; i < numDevices; i++) {
    String p = "d" + String(i) + "_";
    devices[i].deviceName = prefs.getString((p + "name").c_str(), "");
    devices[i].deviceAddress = prefs.getString((p + "addr").c_str(), "");
    devices[i].targetGPIO = prefs.getInt((p + "gpio").c_str(), 0);
    devices[i].localGPIO = prefs.getInt((p + "local").c_str(), 0);
    devices[i].controlType = prefs.getInt((p + "type").c_str(), 0);
    devices[i].currentState = prefs.getBool((p + "state").c_str(), false);
    devices[i].lastState = false;
    devices[i].lastTouchState = false;
    devices[i].lastTouchTime = 0;
    devices[i].lastCommandTime = 0;
  }
  prefs.end();
  
  Serial.println("📋 Loaded " + String(numDevices) + " device(s)");
  
  if (numDevices > 0 && wifiConnected) {
    connectWebSocket(devices[0].deviceAddress);
  }
}

void saveDevices() {
  prefs.begin("devices", false);
  prefs.putInt("count", numDevices);
  
  for (int i = 0; i < numDevices; i++) {
    String p = "d" + String(i) + "_";
    prefs.putString((p + "name").c_str(), devices[i].deviceName);
    prefs.putString((p + "addr").c_str(), devices[i].deviceAddress);
    prefs.putInt((p + "gpio").c_str(), devices[i].targetGPIO);
    prefs.putInt((p + "local").c_str(), devices[i].localGPIO);
    prefs.putInt((p + "type").c_str(), devices[i].controlType);
    prefs.putBool((p + "state").c_str(), devices[i].currentState);
  }
  prefs.end();
}

void addDevice(String name, String addr, int gpio, int local, int type) {
  if (numDevices >= MAX_DEVICES) {
    Serial.println("❌ Max devices reached");
    return;
  }
  
  devices[numDevices].deviceName = name;
  devices[numDevices].deviceAddress = addr;
  devices[numDevices].targetGPIO = gpio;
  devices[numDevices].localGPIO = local;
  devices[numDevices].controlType = type;
    devices[numDevices].currentState = false;
    devices[numDevices].lastState = false;
    devices[numDevices].lastTouchState = false;
    devices[numDevices].lastTouchTime = 0;
    devices[numDevices].lastCommandTime = 0;
  
  numDevices++;
  saveDevices();
  
  if (numDevices == 1 && wifiConnected) {
    connectWebSocket(addr);
  }
  
  Serial.println("✅ Added: " + name);
}

void updateDevice(int idx, String name, String addr, int gpio, int local, int type) {
  if (idx < 0 || idx >= numDevices) {
    Serial.println("❌ Invalid device index");
    return;
  }
  
  devices[idx].deviceName = name;
  devices[idx].deviceAddress = addr;
  devices[idx].targetGPIO = gpio;
  devices[idx].localGPIO = local;
  devices[idx].controlType = type;
  // Keep current state, don't reset it
  
  saveDevices();
  
  // Reconnect WebSocket if this was the first device
  if (idx == 0 && wifiConnected) {
    connectWebSocket(addr);
  }
  
  Serial.println("✅ Updated: " + name);
}

void deleteDevice(int idx) {
  if (idx < 0 || idx >= numDevices) return;
  for (int i = idx; i < numDevices - 1; i++) {
    devices[i] = devices[i + 1];
  }
  numDevices--;
  saveDevices();
  Serial.println("🗑️ Deleted device " + String(idx));
}

// ====== Device Discovery ======
void scanForDevices() {
  if (!wifiConnected) return;
  
  Serial.println("🔍 Scanning for ESP32 devices...");
  numDiscovered = 0;
  
  IPAddress localIP = WiFi.localIP();
  String subnet = String(localIP[0]) + "." + String(localIP[1]) + "." + String(localIP[2]) + ".";
  
  HTTPClient http;
  http.setTimeout(500);
  
  for (int i = 1; i < 255 && numDiscovered < MAX_DISCOVERED; i++) {
    String testIP = subnet + String(i);
    
    // Skip our own IP
    if (testIP == WiFi.localIP().toString()) continue;
    
    // Try to get device status
    http.begin("http://" + testIP + "/api/device/status");
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      if (payload.indexOf("ESP32") >= 0 || payload.indexOf("device_id") >= 0 || payload.indexOf("device_name") >= 0) {
        // Parse JSON
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        
        String deviceName = "ESP32_Device_" + String(i);
        String deviceMac = "";
        String deviceMDNS = "";
        
        if (!error) {
          if (doc.containsKey("custom_name") && doc["custom_name"].as<String>().length() > 0) {
            deviceName = doc["custom_name"].as<String>();
          } else if (doc.containsKey("device_name") && doc["device_name"].as<String>().length() > 0) {
            deviceName = doc["device_name"].as<String>();
          }
          
          if (doc.containsKey("device_id")) {
            deviceMac = doc["device_id"].as<String>();
          }
          
          if (doc.containsKey("mdns_name")) {
            deviceMDNS = doc["mdns_name"].as<String>();
          }
        }
        
        // Check if already in list
        bool exists = false;
        for (int j = 0; j < numDiscovered; j++) {
          if (discoveredDevices[j].ip == testIP) {
            exists = true;
            break;
          }
        }
        
        if (!exists) {
          discoveredDevices[numDiscovered].ip = testIP;
          discoveredDevices[numDiscovered].name = deviceName;
          discoveredDevices[numDiscovered].mdns = deviceMDNS;
          discoveredDevices[numDiscovered].mac = deviceMac;
          numDiscovered++;
          Serial.println("  ✅ Found: " + deviceName + " at " + testIP);
        }
      }
    }
    http.end();
  }
  
  Serial.println("✅ Scan complete. Found " + String(numDiscovered) + " device(s)");
}

void handleDiscover() {
  scanForDevices();
  
  StaticJsonDocument<2048> doc;
  JsonArray devicesArray = doc.createNestedArray("devices");
  
  for (int i = 0; i < numDiscovered; i++) {
    JsonObject device = devicesArray.createNestedObject();
    device["name"] = discoveredDevices[i].name;
    device["addr"] = discoveredDevices[i].mdns.length() > 0 ? discoveredDevices[i].mdns : discoveredDevices[i].ip;
    device["ip"] = discoveredDevices[i].ip;
    device["mac"] = discoveredDevices[i].mac;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ====== GPIO Monitoring ======
void monitorGPIOs() {
  if (!wifiConnected || !wsConnected) return;
  
  unsigned long now = millis();
  
  for (int i = 0; i < numDevices; i++) {
    // Prevent rapid commands - cooldown period
    if (now - devices[i].lastCommandTime < COMMAND_COOLDOWN) {
      continue;
    }
    
    int pin = devices[i].localGPIO;
    
    // ===== PHYSICAL BUTTON MODE (DIRECT) =====
    if (devices[i].controlType == 0) {
      // Button wiring: GPIO ----[BUTTON]---- GND
      // Use internal pull-up so idle = HIGH, pressed = LOW
      pinMode(pin, INPUT_PULLUP);
      bool pressed = (digitalRead(pin) == LOW);
      
      // Trigger only on PRESS edge (not on release)
      if (pressed && !devices[i].lastState && (now - devices[i].lastTouchTime > TOUCH_DEBOUNCE)) {
        devices[i].lastTouchTime = now;
        devices[i].lastCommandTime = now;
        devices[i].lastState = true;   // remember it's currently pressed
        
        // One press = toggle target device
        sendDeviceCommandByIndex(i, "toggle");
        Serial.println("🖲 Button press on GPIO " + String(pin));
      }
      
      // When button is released, just update lastState (no command)
      if (!pressed && devices[i].lastState) {
        devices[i].lastState = false;
      }
    }
    
    // ===== TOUCH MODE =====
    else {
      int touchVal = touchRead(pin);
      bool touched = (touchVal < TOUCH_THRESHOLD);
      
      // Only trigger on transition from NOT touched to TOUCHED (edge detection)
      if (touched && !devices[i].lastTouchState) {
        // Touch just started - check debounce
        if (now - devices[i].lastTouchTime > TOUCH_DEBOUNCE) {
          devices[i].lastTouchTime = now;
          devices[i].lastCommandTime = now;
          sendDeviceCommandByIndex(i, "toggle");
          Serial.println("👆 Touch detected: GPIO " + String(pin) + " (val: " + String(touchVal) + ")");
        }
      }
      
      // Update touch state
      devices[i].lastTouchState = touched;
    }
  }
}

// ====== HTTP Handlers ======
void handleRoot() {
  server.send(200, "text/html", htmlFormPage());
}

void handleSave() {
  if (server.hasArg("ssid")) {
    targetSSID = server.arg("ssid");
    targetPass = server.arg("pass");
    
    prefs.begin("wifi", false);
    prefs.putString("ssid", targetSSID);
    prefs.putString("pass", targetPass);
    prefs.end();
    
    server.send(200, "text/html", "<html><body><h2>Connecting...</h2><script>setTimeout(()=>location.href='/',2000)</script></body></html>");
    
    WiFi.softAPdisconnect(true);
    delay(1000);
    if (tryConnectToWiFi(targetSSID.c_str(), targetPass.c_str())) {
      loadDevices();
    } else {
      startConfigAP();
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleAdd() {
  if (server.hasArg("name") && server.hasArg("addr") && server.hasArg("gpio") && server.hasArg("local")) {
    String name = server.arg("name");
    String addr = server.arg("addr");
    int gpio = server.arg("gpio").toInt();
    int local = server.arg("local").toInt();
    int type = server.arg("type").toInt();
    
    // Check if editing
    if (server.hasArg("edit_idx") && server.arg("edit_idx").length() > 0) {
      int editIdx = server.arg("edit_idx").toInt();
      if (editIdx >= 0 && editIdx < numDevices) {
        updateDevice(editIdx, name, addr, gpio, local, type);
        server.send(200, "text/html", "<html><body><h2>Device Updated!</h2><script>setTimeout(()=>location.href='/',1000)</script></body></html>");
      } else {
        server.send(400, "text/plain", "Invalid device index");
      }
    } else {
      // Adding new device
      addDevice(name, addr, gpio, local, type);
      server.send(200, "text/html", "<html><body><h2>Device Added!</h2><script>setTimeout(()=>location.href='/',1000)</script></body></html>");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleDelete() {
  if (server.hasArg("i")) {
    int idx = server.arg("i").toInt();
    deleteDevice(idx);
    server.send(200, "text/html", "<html><body><h2>Deleted!</h2><script>setTimeout(()=>location.href='/',1000)</script></body></html>");
  }
}

void handleTest() {
  if (numDevices > 0) {
    sendDeviceCommandByIndex(0, "toggle");
    server.send(200, "text/plain", "Test command sent to first device");
  } else {
    server.send(200, "text/plain", "No devices configured");
  }
}

void handleControl() {
  if (server.hasArg("i") && server.hasArg("a")) {
    int idx = server.arg("i").toInt();
    String action = server.arg("a");
    
    if (idx >= 0 && idx < numDevices && (action == "on" || action == "off" || action == "toggle")) {
      sendDeviceCommandByIndex(idx, action);
      server.send(200, "application/json", "{\"success\":true,\"action\":\"" + action + "\",\"device\":\"" + devices[idx].deviceName + "\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid parameters\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Missing parameters\"}");
  }
}

// ====== Setup & Loop ======
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n🎮 ESP32 Remote Controller - Simplified");
  
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  
  if (ssid.length() > 0) {
    if (tryConnectToWiFi(ssid.c_str(), pass.c_str())) {
      loadDevices();
    } else {
      startConfigAP();
    }
  } else {
    startConfigAP();
  }
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/add", HTTP_POST, handleAdd);
  server.on("/del", HTTP_GET, handleDelete);
  server.on("/test", HTTP_GET, handleTest);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/api/discover", HTTP_GET, handleDiscover);
  server.begin();
  Serial.println("✅ Web server started");
}

void loop() {
  server.handleClient();
  webSocket.loop();
  
  if (wifiConnected && !wsConnected && numDevices > 0) {
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      connectWebSocket(devices[0].deviceAddress);
    }
  }
  
  if (wifiConnected && wsConnected) {
    monitorGPIOs();
  }
  
  delay(10);
}
