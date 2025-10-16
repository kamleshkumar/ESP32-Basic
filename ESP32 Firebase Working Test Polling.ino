/*
 * ESP32 Firebase Working Test
 * Based on your existing working Firebase code
 */

// Firebase Client Library defines - MUST be before includes!
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

// WiFi Settings
const char* ssid = "Sweet1212";
const char* password = "home2121";

// Firebase configuration
#define FIREBASE_PROJECT_ID "kkmsgapp"
#define FIREBASE_DATABASE_URL "https://kkmsgapp.firebaseio.com"
#define API_KEY "AIzaSyANzGExtGeVd0d6TWP_Vpti1aLu_PTh_xRLyp0"
#define USER_EMAIL "esp43434Brt@kkmsgapp.com"
#define USER_PASSWORD "esp32232fdd123"

// Firebase objects - CORRECT SYNTAX (from your working code)
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
FirebaseApp app;
RealtimeDatabase db;
WiFiClientSecure ssl_client;
AsyncClientClass async_client(ssl_client);  // ✅ CORRECT!
AsyncResult dbResult;
bool firebaseInitialized = false;

// Check interval
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 5000; // Check every 5 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("🔥 ESP32 Firebase Working Test");
  Serial.println("📱 Based on your existing working code");
  
  // Connect to WiFi
  Serial.print("📶 Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("✅ WiFi Connected: " + WiFi.localIP().toString());
  
  // Initialize Firebase (same as your working code)
  Serial.println("🔥 Initializing Firebase...");
  ssl_client.setInsecure();
  ssl_client.setHandshakeTimeout(5);
  initializeApp(async_client, app, getAuth(user_auth), auth_debug_print, "authTask");
  app.getApp<RealtimeDatabase>(db);
  db.url(FIREBASE_DATABASE_URL);
  Serial.println("✅ Firebase init started");
  
  Serial.println("🎯 Starting Firebase monitoring...");
  Serial.println("📡 Will check for data changes every 5 seconds");
  Serial.println("----------------------------");
}

void loop() {
  // Process Firebase operations (same as your working code)
  app.loop();
  
  if (app.ready()) {
    if (!firebaseInitialized) {
      firebaseInitialized = true;
      Serial.println("✅ Firebase authenticated!");
    }
    
    // Check for Firebase data every 5 seconds
    if (millis() - lastCheck > CHECK_INTERVAL) {
      lastCheck = millis();
      checkFirebaseData();
    }
  }
  
  // Process Firebase results
  if (dbResult.available()) {
    Firebase.printf("Firebase Result: %s\n", dbResult.c_str());
    String data = dbResult.c_str();
    if (data.length() > 0 && data != "null") {
      Serial.println("🎉 FIREBASE DATA RECEIVED!");
      Serial.println("📊 Raw Data: " + data);
      Serial.println("⏰ Time: " + String(millis()));
      Serial.println("----------------------------");
    }
  }
}

void checkFirebaseData() {
  if (!app.ready()) {
    Serial.println("❌ Firebase not ready");
    return;
  }
  
  Serial.println("🔍 Checking Firebase root path...");
  db.get(async_client, "/", dbResult);
}

void auth_debug_print(AsyncResult &aResult) {
  if (aResult.isEvent()) {
    Firebase.printf("Event: %s, code: %d\n", aResult.appEvent().message().c_str(), aResult.appEvent().code());
  }
  if (aResult.isError()) {
    Firebase.printf("Error: %s, code: %d\n", aResult.error().message().c_str(), aResult.error().code());
  }
}
