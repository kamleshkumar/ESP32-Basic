#include <WiFi.h>
#include <FirebaseClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "Sweet Home";
const char* password = "sweetHome74B";

// Firebase configuration
#define FIREBASE_PROJECT_ID "kkmsgapp"
#define FIREBASE_DATABASE_URL "https://kkmsgapp.firebaseio.com"
#define API_KEY "AIzaSyANzGeVd0d6TWP_Vpti1aLu_PTh_xRLyp0"
#define USER_EMAIL "esp32@kkmsgapp.com"
#define USER_PASSWORD "esp32password123"

// Firebase objects - CORRECT SYNTAX (NO getNetwork!)
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
FirebaseApp app;
RealtimeDatabase db;
WiFiClientSecure ssl_client;
AsyncClientClass async_client(ssl_client);  // ✅ CORRECT!
AsyncResult dbResult;
bool firebaseInitialized = false;

// Device info
const char* deviceId = "ESP32_FIREBASE_TEST";
const char* deviceName = "Firebase Test Device";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("🔥 ESP32 Firebase Test Firmware Starting...");
  Serial.println("==========================================");
  
  // Connect to WiFi
  connectToWiFi();
  
  // Initialize Firebase
  initializeFirebase();
  
  Serial.println("✅ Setup complete - Listening for Firebase commands...");
  Serial.println("📡 Device ID: " + String(deviceId));
  Serial.println("📡 Device Name: " + String(deviceName));
  Serial.println("==========================================");
}

void loop() {
  // Check Firebase for commands every 2 seconds
  listenForFirebaseCommands();
  delay(2000);
}

void connectToWiFi() {
  Serial.println("📶 Connecting to WiFi: " + String(ssid));
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("✅ WiFi connected!");
    Serial.println("📡 IP address: " + WiFi.localIP().toString());
  } else {
    Serial.println();
    Serial.println("❌ WiFi connection failed!");
    Serial.println("🔄 Restarting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }
}

void initializeFirebase() {
  Serial.println("🔥 Initializing Firebase...");
  
  // Initialize Firebase app
  app.begin(API_KEY);
  
  // Authenticate user
  Serial.println("🔐 Authenticating Firebase user...");
  AuthResult authResult = user_auth.signUp();
  
  if (authResult.error.code == 0) {
    Serial.println("✅ Firebase authentication successful!");
  } else {
    Serial.println("❌ Firebase authentication failed: " + String(authResult.error.message));
    Serial.println("🔄 Restarting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }
  
  // Initialize database
  db.begin(&app, FIREBASE_DATABASE_URL);
  firebaseInitialized = true;
  
  Serial.println("✅ Firebase initialized successfully!");
}

void listenForFirebaseCommands() {
  if (!firebaseInitialized) {
    return;
  }
  
  Serial.println("🔥 Checking Firebase for commands...");
  
  // Get commands from Firebase
  String path = "/commands/" + String(deviceId);
  dbResult = db.get(&async_client, path);
  
  if (dbResult.isEvent()) {
    String response = dbResult.to<String>();
    Serial.println("📥 Firebase Response: " + response);
    
    if (response != "null" && response.length() > 0) {
      // Parse the response
      parseFirebaseCommand(response);
      
      // Clear the command after processing
      db.setString(&async_client, path, "");
      Serial.println("🧹 Command cleared from Firebase");
    } else {
      Serial.println("📭 No new commands");
    }
  }
}

void parseFirebaseCommand(String jsonResponse) {
  Serial.println("🔍 Parsing Firebase command...");
  
  // Parse JSON response
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonResponse);
  
  if (error) {
    Serial.println("❌ JSON parsing failed: " + String(error.c_str()));
    return;
  }
  
  // Extract command information
  if (doc.containsKey("command")) {
    String command = doc["command"];
    Serial.println("📝 Command: " + command);
    
    // Parse unified command format: group_device_gpio_action
    parseUnifiedCommand(command);
  } else {
    Serial.println("⚠️ No 'command' field found in Firebase response");
  }
}

void parseUnifiedCommand(String command) {
  Serial.println("🔧 Parsing unified command: " + command);
  
  // Split command by underscores
  int firstUnderscore = command.indexOf('_');
  int secondUnderscore = command.indexOf('_', firstUnderscore + 1);
  int thirdUnderscore = command.indexOf('_', secondUnderscore + 1);
  int fourthUnderscore = command.indexOf('_', thirdUnderscore + 1);
  
  if (firstUnderscore == -1 || secondUnderscore == -1 || thirdUnderscore == -1 || fourthUnderscore == -1) {
    Serial.println("❌ Invalid command format. Expected: group_device_gpio_action");
    return;
  }
  
  String group = command.substring(0, firstUnderscore);
  String device = command.substring(firstUnderscore + 1, secondUnderscore);
  String gpioStr = command.substring(secondUnderscore + 1, thirdUnderscore);
  String action = command.substring(fourthUnderscore + 1);
  
  int gpioPin = gpioStr.toInt();
  
  Serial.println("🏠 Group: " + group);
  Serial.println("🔧 Device: " + device);
  Serial.println("📌 GPIO Pin: " + String(gpioPin));
  Serial.println("⚡ Action: " + action);
  
  // Execute the command
  executeDeviceCommand(device, gpioPin, action);
}

void executeDeviceCommand(String deviceName, int gpioPin, String action) {
  Serial.println("🎮 Executing command...");
  Serial.println("   Device: " + deviceName);
  Serial.println("   GPIO: " + String(gpioPin));
  Serial.println("   Action: " + action);
  
  // Initialize GPIO pin
  pinMode(gpioPin, OUTPUT);
  
  // Execute action
  if (action == "on" || action == "high") {
    digitalWrite(gpioPin, HIGH);
    Serial.println("✅ GPIO " + String(gpioPin) + " → ON");
  } else if (action == "off" || action == "low") {
    digitalWrite(gpioPin, LOW);
    Serial.println("✅ GPIO " + String(gpioPin) + " → OFF");
  } else if (action == "toggle") {
    int currentState = digitalRead(gpioPin);
    digitalWrite(gpioPin, !currentState);
    Serial.println("🔄 GPIO " + String(gpioPin) + " → " + (currentState ? "OFF" : "ON"));
  } else {
    Serial.println("❌ Unknown action: " + action);
    return;
  }
  
  // Update Firebase with device status
  updateDeviceStatus(deviceName, gpioPin, action);
}

void updateDeviceStatus(String deviceName, int gpioPin, String action) {
  if (!firebaseInitialized) {
    return;
  }
  
  Serial.println("📤 Updating device status in Firebase...");
  
  // Create status object
  DynamicJsonDocument statusDoc(512);
  statusDoc["device_name"] = deviceName;
  statusDoc["device_type"] = "light";
  statusDoc["pin"] = gpioPin;
  statusDoc["is_on"] = (action == "on" || action == "high");
  statusDoc["last_updated"] = millis();
  
  String statusJson;
  serializeJson(statusDoc, statusJson);
  
  // Update Firebase
  String statusPath = "/devices/" + String(deviceId) + "/" + deviceName;
  db.setString(&async_client, statusPath, statusJson);
  
  Serial.println("✅ Status updated: " + statusJson);
}
