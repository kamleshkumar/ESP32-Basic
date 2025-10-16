/*
 * ESP32 Firebase Streaming - WORKING VERSION
 * Based on your working authentication, converted to real-time streaming
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
const char* WIFI_SSID = "Dhruvm05";
const char* WIFI_PASSWORD = "ispMob#29";

// Firebase Settings - Same as your working code
#define FIREBASE_PROJECT_ID "kkmsgapp"
#define FIREBASE_DATABASE_URL "https://kkmsgapp.firebaseio.com"
#define API_KEY "AIzaSyANzGeVd0d6TWP_Vpti1aLu_PTh_xRLyp0"
#define USER_EMAIL "esp32@kkmsgapp.com"
#define USER_PASSWORD "esp32password123"

// Firebase objects - Same as your working code
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
FirebaseApp app;
RealtimeDatabase db;
WiFiClientSecure ssl_client;
AsyncClientClass async_client(ssl_client);  // ✅ Same as working code
AsyncResult dbResult;
bool firebaseInitialized = false;

// Streaming variables
WiFiClientSecure stream_ssl_client;
AsyncClientClass stream_client(stream_ssl_client);
AsyncResult streamResult;
bool streamStarted = false;

// Function declarations
void auth_debug_print(AsyncResult &aResult);
void startStreaming();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("🔥 ESP32 Firebase Streaming - WORKING VERSION");
  Serial.println("📱 Real-time streaming with working authentication");
  
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
  
  // Initialize streaming client
  stream_ssl_client.setInsecure();
  stream_ssl_client.setHandshakeTimeout(5);
  
  Serial.println("🎯 Starting Firebase streaming...");
  Serial.println("📡 Will listen for real-time changes");
  Serial.println("----------------------------");
}

void loop() {
  // Process Firebase operations (same as your working code)
  app.loop();
  
  if (app.ready()) {
    if (!firebaseInitialized) {
      firebaseInitialized = true;
      Serial.println("✅ Firebase authenticated!");
      startStreaming();
    }
    
    // Process streaming results
    if (streamResult.available()) {
      Firebase.printf("Stream Result: %s\n", streamResult.c_str());
      String data = streamResult.c_str();
      if (data.length() > 0 && data != "null") {
        Serial.println("🎉 REAL-TIME STREAM DATA RECEIVED!");
        Serial.println("📊 Raw Data: " + data);
        Serial.println("⏰ Time: " + String(millis()));
        Serial.println("----------------------------");
      }
    }
  }
}

void startStreaming() {
  if (streamStarted) return;
  
  Serial.println("🌊 Starting real-time stream...");
  
  // Set SSE filters for streaming
  stream_client.setSSEFilters("get,put,patch,keep-alive");
  
  // Start streaming on root path (correct API usage)
  db.get(stream_client, "/", streamResult, true);
  
  streamStarted = true;
  Serial.println("✅ Real-time stream started!");
  Serial.println("🎯 Listening for ANY data changes...");
}

void auth_debug_print(AsyncResult &aResult) {
  if (aResult.isEvent()) {
    Firebase.printf("Event: %s, code: %d\n", aResult.appEvent().message().c_str(), aResult.appEvent().code());
  }
  if (aResult.isError()) {
    Firebase.printf("Error: %s, code: %d\n", aResult.error().message().c_str(), aResult.error().code());
  }
}
