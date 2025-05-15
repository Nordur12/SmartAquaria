#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <FirebaseESP32.h>
#include <Preferences.h>
#include <WebServer.h>

// Preferences
Preferences preferences;

// Pin definitions (from your original code)
#define TURBIDITY_PIN 34
#define PH_SENSOR_PIN 35
#define ONE_WIRE_BUS 21
#define SERVO_PIN_1 18
#define SERVO_PIN_2 19

// AP mode credentials
const char* ap_ssid = "SmartAquaria_SetupDev1";
const char* ap_password = "setup12345";

// Variables to store STA credentials
char sta_ssid[32] = "";
char sta_password[64] = "";
bool wifi_configured = false;

// Firebase project credentials (from your original code)
#define FIREBASE_HOST "https://smartaquaria-9ad3b-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "xrNhRX3uhGpjCULm27wKmID0LLpulTuM5PZSjVWS"

// Firebase objects
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData firebaseData;

// Device ID
String deviceID;

// Create WebServer on port 80
WebServer server(80);

// Temperature sensor setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Turbidity sensor setup
const int numReadings = 20;
int readings[numReadings];
int readIndex = 0;
int total = 0;

// pH sensor calibration values
float calibration_value = 25.81;
float slope = -5.70;
float lastPHLevel = 0.0;
float lastTurbidity = 0.0;
float lastTemp = 0.0;

// Servo setup
Servo servo1;
Servo servo2;

// Function declarations
void setupAP();
void handleRoot();
void handleSave();
void handleScan();
void connectToWiFi();
void initFirebase();
void readTemperature();
void readTurbidity();
void readPH();
void checkServo();
void uploadStatus();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nStarting Smart Aquaria System");
  
  // Initialize Preferences
  preferences.begin("wifi-config", false);
  
  // Check if WiFi is already configured
  if (preferences.getBool("configured", false)) {
    // Read configuration from Preferences
    String saved_ssid = preferences.getString("ssid", "");
    String saved_password = preferences.getString("password", "");
    
    if (saved_ssid.length() > 0) {
      strncpy(sta_ssid, saved_ssid.c_str(), sizeof(sta_ssid) - 1);
      strncpy(sta_password, saved_password.c_str(), sizeof(sta_password) - 1);
      sta_ssid[sizeof(sta_ssid) - 1] = '\0';
      sta_password[sizeof(sta_password) - 1] = '\0';
      
      Serial.println("Found saved WiFi configuration");
      Serial.print("SSID: ");
      Serial.println(sta_ssid);
      wifi_configured = true;
    }
  }
  
  // Initialize sensors and servos
  sensors.begin();
  servo1.attach(SERVO_PIN_1);
  servo2.attach(SERVO_PIN_2);
  servo1.write(0);
  servo2.write(0);
  
  // If WiFi is configured, try to connect in STA mode
  if (wifi_configured) {
    Serial.println("Attempting to connect to WiFi network");
    connectToWiFi();
  } 
  
  // If connection failed or WiFi is not configured, start AP mode
  if (WiFi.status() != WL_CONNECTED) {
    setupAP();
  } else {
    // Format MAC address like Device 1
    deviceID = "DEVICE_" + WiFi.macAddress();
    deviceID.replace(":", "");
    
    // Initialize Firebase connection
    initFirebase();
  }
}

void loop() {
  // Handle web server requests if in AP mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    server.handleClient();
  } 
  // If in STA mode and connected
  else if (WiFi.status() == WL_CONNECTED) {
    readTemperature();
    readTurbidity();
    readPH();
    checkServo();
    uploadStatus();
  } 
  // If connection lost, try to reconnect
  else {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    connectToWiFi();
    
    // If reconnection fails, go back to AP mode
    if (WiFi.status() != WL_CONNECTED) {
      setupAP();
    }
  }
  
  delay(1000);
}

// Set up Access Point and web server
void setupAP() {
  Serial.println("Starting Access Point mode");
  
  // Stop any previous WiFi activities
  WiFi.disconnect(true);
  delay(1000);
  
  // Configure as AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Set up web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", HTTP_GET, handleScan);
  server.begin();
  
  Serial.println("HTTP server started");
  Serial.println("Please connect to the AP and visit http://192.168.4.1 to configure WiFi");
}

// Handle root web page
void handleRoot() {
  String html = "<html><head><title>Smart Aquaria WiFi Setup</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += "h1 { color: #0066cc; }";
  html += ".container { max-width: 500px; margin: 0 auto; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 8px 0; box-sizing: border-box; }";
  html += "button { background-color: #0066cc; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; }";
  html += "button:hover { background-color: #0052a3; }";
  html += ".networks { margin-top: 20px; }";
  html += ".network { padding: 8px; border-bottom: 1px solid #ddd; cursor: pointer; }";
  html += ".network:hover { background-color: #f0f0f0; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Smart Aquaria WiFi Setup</h1>";
  html += "<form action='/save' method='post'>";
  html += "SSID:<br><input type='text' name='ssid' id='ssid'><br>";
  html += "Password:<br><input type='password' name='password'><br><br>";
  html += "<button type='submit'>Save and Connect</button>";
  html += "</form>";
  html += "<button onclick='scanNetworks()' style='margin-top: 10px;'>Scan Networks</button>";
  html += "<div id='networks' class='networks'></div>";
  html += "</div>";
  html += "<script>";
  html += "function scanNetworks() {";
  html += "  document.getElementById('networks').innerHTML = 'Scanning...';";
  html += "  fetch('/scan')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      let html = '';";
  html += "      for(let i = 0; i < data.length; i++) {";
  html += "        html += `<div class='network' onclick='selectNetwork(\"${data[i]}\")'>${data[i]}</div>`;";
  html += "      }";
  html += "      document.getElementById('networks').innerHTML = html;";
  html += "    });";
  html += "}";
  html += "function selectNetwork(ssid) {";
  html += "  document.getElementById('ssid').value = ssid;";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Handle WiFi scan request
void handleScan() {
  Serial.println("Scanning for networks...");
  
  int n = WiFi.scanNetworks();
  String networks[n];
  
  for (int i = 0; i < n; ++i) {
    networks[i] = WiFi.SSID(i);
  }
  
  String json = "[";
  for (int i = 0; i < n; ++i) {
    json += "\"" + networks[i] + "\"";
    if (i < n - 1) {
      json += ",";
    }
  }
  json += "]";
  
  server.send(200, "application/json", json);
}

// Handle saving WiFi credentials
void handleSave() {
  String new_ssid = server.arg("ssid");
  String new_password = server.arg("password");
  
  Serial.print("Received new WiFi credentials - SSID: ");
  Serial.println(new_ssid);
  
  // Store in variables
  strncpy(sta_ssid, new_ssid.c_str(), sizeof(sta_ssid) - 1);
  strncpy(sta_password, new_password.c_str(), sizeof(sta_password) - 1);
  sta_ssid[sizeof(sta_ssid) - 1] = '\0';
  sta_password[sizeof(sta_password) - 1] = '\0';
  
  // Save to Preferences
  preferences.putString("ssid", String(sta_ssid));
  preferences.putString("password", String(sta_password));
  preferences.putBool("configured", true);
  preferences.end();
  preferences.begin("wifi-config", false);
  
  String html = "<html><head><title>WiFi Configuration Saved</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += "h1 { color: #0066cc; }";
  html += ".container { max-width: 500px; margin: 0 auto; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }";
  html += "</style>";
  html += "<meta http-equiv='refresh' content='10;url=/' />";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>WiFi Configuration Saved</h1>";
  html += "<p>Attempting to connect to the WiFi network...</p>";
  html += "<p>The device will automatically switch to station mode if connection is successful.</p>";
  html += "<p>If connection fails, the device will return to AP mode and you can try again.</p>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
  
  // Try to connect to WiFi
  wifi_configured = true;
  delay(1000);
  connectToWiFi();
}

// Connect to WiFi using stored credentials
void connectToWiFi() {
  Serial.println("Attempting to connect to WiFi in station mode");
  
  // Set WiFi mode to STA
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Connect using stored credentials
  WiFi.begin(sta_ssid, sta_password);
  
  // Wait for connection with timeout
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Format MAC address and set device ID
    deviceID = "DEVICE_" + WiFi.macAddress();
    deviceID.replace(":", "");
    
    // Initialize Firebase only if not already initialized
    initFirebase();
    
    // Stop the web server if it was running
    server.stop();
  } else {
    Serial.println("\nFailed to connect to WiFi!");
  }
}

// Initialize Firebase connection
void initFirebase() {
  // Firebase configuration
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("Connected to Firebase!");
}

// The following functions are from your original code
void readTemperature() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature: ");
    Serial.println(tempC);

    if (abs(tempC - lastTemp) > 0.1) {
      if (Firebase.ready()) {
        FirebaseJson json;
        json.set("temperature", tempC);

        if (Firebase.updateNode(firebaseData, "/devices/" + deviceID + "/temperature", json)) {
          Serial.println("Temperature sent to Firebase");
          lastTemp = tempC;
        } else {
          Serial.print("Failed to send temperature: ");
          Serial.println(firebaseData.errorReason());
        }
      }
    }
  }
}

void readTurbidity() {
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(TURBIDITY_PIN);
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;

  int average = total / numReadings;
  int turbidity = max((long)map(average, 2000, 3000, 100, 0), (long)0);

  String condition = (turbidity > 40) ? "DIRTY" : (turbidity > 10) ? "CLOUDY" : "CLEAR";

  Serial.print("Turbidity: ");
  Serial.println(condition);

  if (abs(turbidity - lastTurbidity) > 1.0) {
    if (Firebase.ready()) {
      FirebaseJson json;
      json.set("sensorValue", average);
      json.set("condition", condition);

      if (Firebase.updateNode(firebaseData, "/devices/" + deviceID + "/turbidity", json)) {
        Serial.println("Turbidity data sent to Firebase");
        lastTurbidity = turbidity;
      } else {
        Serial.print("Failed to send turbidity: ");
        Serial.println(firebaseData.errorReason());
      }
    }
  }
}

void readPH() {
  int buffer_arr[10];
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(PH_SENSOR_PIN);
    delay(30);
  }

  int avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }

  float voltage = (float)avgval * 3.3 / 4095.0 / 6;
  float ph_act = slope * voltage + calibration_value;

  Serial.print("pH Value: ");
  Serial.println(ph_act);

  if (abs(ph_act - lastPHLevel) > 0.1) {
    if (Firebase.ready()) {
      FirebaseJson json;
      json.set("voltage", voltage);
      json.set("pHLevel", ph_act);

      if (Firebase.updateNode(firebaseData, "/devices/" + deviceID + "/ph", json)) {
        Serial.println("pH data sent to Firebase");
        lastPHLevel = ph_act;
      } else {
        Serial.print("Failed to send pH: ");
        Serial.println(firebaseData.errorReason());
      }
    }
  }
}

void checkServo() {
  if (Firebase.ready()) {
    // Control Servo 1
    if (Firebase.getBool(firebaseData, "/devices/" + deviceID + "/feeder/servo1")) {
      if (firebaseData.boolData()) {
        servo1.write(90);
        delay(1000);
        servo1.write(0);
        Firebase.setBool(firebaseData, "/devices/" + deviceID + "/feeder/servo1", false);
        Serial.println("Servo 1 activated and reset.");
      }
    }

    // Control Servo 2
    if (Firebase.getBool(firebaseData, "/devices/" + deviceID + "/feeder/servo2")) {
      if (firebaseData.boolData()) {
        servo2.write(90);
        delay(1000);
        servo2.write(0);
        Firebase.setBool(firebaseData, "/devices/" + deviceID + "/feeder/servo2", false);
        Serial.println("Servo 2 activated and reset.");
      }
    }
  }
}

void uploadStatus() {
  if (Firebase.ready()) {
    FirebaseJson json;
    json.set("status", "Online");

    if (Firebase.updateNode(firebaseData, "/devices/" + deviceID + "/status", json)) {
      Serial.println("Status sent to Firebase");
    } else {
      Serial.print("Failed to send status: ");
      Serial.println(firebaseData.errorReason());
    }
  }
}