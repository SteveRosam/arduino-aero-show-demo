#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_INA260.h>
#include <vector>
#include <WiFiManager.h>
#define XSTR(x) #x
#define STR(x) XSTR(x)
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>

// Pin definitions
#define ESC_PIN 18          // PWM pin for ESC control
const int HX711_DT_PIN = 4;   // Data pin
const int HX711_SCK_PIN = 5;  // Clock pin

// ESC configuration
#define ESC_MIN_PULSE 1000  // Minimum pulse width in microseconds
#define ESC_MAX_PULSE 2000  // Maximum pulse width in microseconds

// Global objects
Servo esc;  // Create servo object to control the ESC
Adafruit_INA260 ina260 = Adafruit_INA260();
WiFiManager wifiManager;

// Load cell state
float lastValidLoadCellValue = 0.0f;
unsigned long lastLoadCellUpdate = 0;
const unsigned long LOAD_CELL_TIMEOUT = 10; // Consider value stale after 1 second
float loadCellTareValue = 0.0f;

// Test control variables
bool testRunning = false;
String currentTestId = "";
unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 10; // Send data every 100ms
bool b_loadCellReady = false;
float currentSpeed = 0.0;  // Track current motor speed (0.0 to 1.0)

// Non-blocking test state
struct TestState {
  std::vector<float> speeds;
  int currentSpeedIndex = 0;
  unsigned long speedStartTime = 0;
  int rampDelay = 0;
  
  void setSpeeds(JsonArray jsonSpeeds) {
    speeds.clear();
    for (JsonVariant speed : jsonSpeeds) {
      speeds.push_back(speed.as<float>());
    }
  }
} testState;

// Serial data storage
const int SERIAL_BUFFER_SIZE = 100;  // Store last 100 lines
String serialBuffer[SERIAL_BUFFER_SIZE];
int bufferIndex = 0;


// Buffer structures and timing variables for batch processing
const size_t MAX_SAMPLES = 200;  // Adjust based on available RAM
const unsigned long SEND_INTERVAL_MS = 2000;  // Send every 2 seconds

struct SensorData {
  unsigned long timestamp = 0;
  float load_cell = 0.0f;
  float voltage = 0.0f;
  float current = 0.0f;
  float speed = 0.0f;
  
  SensorData() : timestamp(millis()), load_cell(0), voltage(0), current(0), speed(0) {}
};

// Try both common resolutions for 0.91" displays
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32  // Try changing this to 64 if 32 doesn't work

#define OLED_RESET -1

// Try both common I2C addresses
#define SCREEN_ADDRESS_1 0x3C
#define SCREEN_ADDRESS_2 0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


std::vector<SensorData> sensorBuffer;
unsigned long lastSendTime = 0;

// Function prototypes
void setupWebServer();
void setupSensors();
void setupESC();
void handleMotorControl();
void handleRoot();
void startMotorTest(JsonDocument& config);
void updateMotorTest();
void runMotorTest(JsonDocument& config);
float readLoadCell();
bool isLoadCellReady();
float readINA260Voltage();
float readINA260Current();
void sendBufferedData();
void resetHX711();
long readHX711();
void addToSerialBuffer(String line);
String getSerialOutput();
void log(String message);
void showText(String text, int line);
void configureOTA();

// void scanI2C() {
//   byte error, address;
//   int devices = 0;

//   for(address = 1; address < 127; address++) {
//     Wire.beginTransmission(address);
//     error = Wire.endTransmission();

//     if (error == 0) {
//       log("I2C device found at address 0x");
//       if (address < 16) log("0");
//       log(String(address));
//       log(" !");
//       devices++;
//     }
//   }
  
//   if (devices == 0) {
//     log("No I2C devices found - check wiring!");
//   } else {
//     log("Found ");
//     log(String(devices));
//     log(" device(s)");
//   }
//   log("\n");
// }

void configureOTA() {
  // Configure OTA
  ArduinoOTA.setHostname("ESP32-OTA");
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    log("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    log("\nOTA Update Complete!");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    log(String("Progress: %u%%\r", (progress / (total / 100))));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    log(String("Error[%u]: ", error));
    if (error == OTA_AUTH_ERROR) {
      log("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      log("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      log("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      log("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      log("End Failed");
    }
  });

  ArduinoOTA.begin();
  log("OTA Ready"); 
}

const int TEXT_HEIGHT = 8;
void showText(String text = "", int line = 0) {
  
  // Clear just this line by drawing a black rectangle over it
  display.fillRect(0, line * TEXT_HEIGHT, SCREEN_WIDTH, TEXT_HEIGHT, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, line * TEXT_HEIGHT);
  display.println(text);
  display.display();
}

void setupDisplay(){
  log("Trying address 0x3C...");
  if(display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_1)) {
    log("Display found at 0x3C!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Display Ready!");
    display.display();
  } else {
    log("Error setting up display. No display at 0x3C");
  }
}

const unsigned char logo_bits [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x3f, 0x00, 0x01, 0xfe, 0x78, 0xf0, 0x1f, 0x3e, 0x3c, 0x07, 0x80, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x3f, 0x00, 0x07, 0xff, 0x78, 0xf0, 0x1f, 0x3e, 0x3e, 0x0f, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x3f, 0x00, 0x07, 0xff, 0x78, 0xf0, 0x1f, 0x3e, 0x1e, 0x1f, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x3f, 0x00, 0x0f, 0x81, 0xf8, 0xf0, 0x1f, 0x3e, 0x0f, 0x1e, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x3f, 0x00, 0x0f, 0x00, 0xf8, 0xf0, 0x1f, 0x3e, 0x07, 0xbc, 0x00, 0x00, 0x00, 
0xfc, 0x07, 0xe0, 0x3f, 0x00, 0x1f, 0x00, 0xf8, 0xf0, 0x1f, 0x3e, 0x07, 0xf8, 0x00, 0x00, 0x00, 
0xfc, 0x07, 0xe0, 0x3f, 0x00, 0x1e, 0x00, 0xf8, 0xf0, 0x1f, 0x3e, 0x03, 0xf8, 0x00, 0x00, 0x00, 
0xfc, 0x07, 0xe0, 0x3f, 0x00, 0x1e, 0x00, 0xf8, 0xf0, 0x1f, 0x3e, 0x01, 0xf0, 0x00, 0x00, 0x00, 
0xfc, 0x07, 0xe0, 0x3f, 0x00, 0x1e, 0x00, 0xf8, 0xf0, 0x1f, 0x3e, 0x01, 0xf0, 0x00, 0x00, 0x00, 
0xfc, 0x07, 0xe0, 0x3f, 0x00, 0x1e, 0x00, 0xf8, 0xf0, 0x1f, 0x3e, 0x03, 0xf0, 0x00, 0x00, 0x00, 
0xfc, 0x07, 0xe0, 0x3f, 0x00, 0x1e, 0x00, 0xf8, 0xf0, 0x1f, 0x3e, 0x07, 0xf8, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0xf8, 0xf0, 0x1f, 0x3e, 0x0f, 0xbc, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x0f, 0x80, 0x00, 0x0f, 0x01, 0xf8, 0xf0, 0x3f, 0x3e, 0x0f, 0x3e, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x0f, 0x80, 0x00, 0x0f, 0xc3, 0xf8, 0xf8, 0x7f, 0x3e, 0x1e, 0x1e, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x0f, 0x80, 0x00, 0x07, 0xff, 0xf8, 0xff, 0xef, 0x3e, 0x3c, 0x0f, 0x00, 0x00, 0x00, 
0xfc, 0x00, 0x0f, 0x80, 0x00, 0x03, 0xfe, 0xf8, 0x7f, 0xcf, 0x3e, 0x7c, 0x0f, 0x80, 0x00, 0x00, 
0xfc, 0x00, 0x0f, 0x80, 0x00, 0x01, 0xfc, 0xf8, 0x3f, 0x0f, 0x3e, 0x78, 0x07, 0xc0, 0x00, 0x00, 
0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void drawLogo() {
  display.clearDisplay();
  display.drawBitmap(2, 0, logo_bits, 128, 32, SSD1306_WHITE);
  display.display();
  delay(5000);
  display.clearDisplay();
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give time for serial to initialize

  log("OLED Troubleshooting Started");
  
  // Initialize I2C with specific pins for ESP32
  Wire.begin(21, 22); // SDA=21, SCL=22 for ESP32
  setupDisplay();
  drawLogo();
  // display.drawBitmap(0, 0, 128, 32, logo_bits, SSD1306_WHITE);
  // delay(5000);
  // display.clearDisplay();
  
  // Scan for I2C devices
  // log("Scanning for I2C devices...");
  // scanI2C();

  // Try first address
  // log("Trying address 0x3C...");
  // if(display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_1)) {
  //   log("Display found at 0x3C!");
  // } else {
  //   log("No display at 0x3C, trying 0x3D...");
    
  //   // Try second address
  //   if(display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_2)) {
  //     log("Display found at 0x3D!");
  //   } else {
  //     log("No display found at either address!");
  //     log("Check wiring:");
  //     log("SDA -> GPIO21");
  //     log("SCL -> GPIO22");
  //     log("VCC -> 3.3V");
  //     log("GND -> GND");
  //   }
  // }


  // Initial debug output
  log("\n=== AeroShow ESP32 Starting ===");
  log("Debug output initialized");
  log("Free heap: ");
  log(String(ESP.getFreeHeap()));
  log(" bytes");
  log("ESP32 Motor Control & Sensor System Starting...");

  wifiManager.setReportURL("https://gateway-3-demo-aerospacetestingdevelopmentshow-prod.demo.quix.io/data");

  showText("Connecting to WiFi");

  if (!wifiManager.begin()) {
    log("Failed to connect to WiFi");
    showText("WiFi Connect Failed");
    // Handle WiFi connection failure (e.g., blink LED)
    while (1) {
      digitalWrite(2, HIGH);
      delay(100);
      digitalWrite(2, LOW);
      delay(100);
    }
  }
  
  setupSensors();
  setupESC();
  setupWebServer();
  
  loadCellTareValue = readLoadCell();
  log("Load cell tare value: ");
  log(String(loadCellTareValue));

  log("System ready!");
  log("IP Address: ");
  log(String(wifiManager.getIPAddress()));

  showText("IP:" + String(wifiManager.getIPAddress()));

  configureOTA();
}

bool led_state = true;
long led_millis = millis();
long OK_BLINK_RATE = 1000;

void loop() {

  ArduinoOTA.handle();  // CRITICAL: This must run frequently!

  if (millis() - OK_BLINK_RATE > 1000) { 
    digitalWrite(2, led_state ? HIGH : LOW);
  }  

  static unsigned long lastLoopDebug = 0;
  if (millis() - lastLoopDebug > 5000) {  // Every 5 seconds
    lastLoopDebug = millis();
  }
  
  WebServer& server = wifiManager.getServer();
  server.handleClient();
  
  // Update motor test state if running
  if (testRunning) {
    updateMotorTest();
  }
  
  static unsigned long lastDebugOutput = 0;
  if (testRunning) {
    // Collect sensor data as fast as possible
    SensorData reading;
    reading.timestamp = millis();
    
    // Read INA260
    reading.voltage = ina260.readBusVoltage();
    reading.current = ina260.readCurrent();
    
    // Always try to read the load cell, the function will handle retries and timeouts
    reading.load_cell = readLoadCell() - loadCellTareValue;
    
    // Add current speed (0.0-1.0) to the reading
    reading.speed = currentSpeed;
    
    // Debug: Print buffer status
    static unsigned long lastBufferDebug = 0;
    if (millis() - lastBufferDebug > 1000) {
      lastBufferDebug = millis();
    }
    
    // Add to buffer if we haven't reached max samples
    if (sensorBuffer.size() < MAX_SAMPLES) {
      sensorBuffer.push_back(reading);
      
      // Debug output every second
      if (millis() - lastDebugOutput > 1000) {
        lastDebugOutput = millis();
      }
    } else {
      // Buffer full, waiting to send
      if (millis() - lastDebugOutput > 1000) {
        lastDebugOutput = millis();
        log("Buffer full, waiting to send...");
      }
    }
    
    // Check if it's time to send data
    if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
      if (!sensorBuffer.empty()) {
        sendBufferedData();
        sensorBuffer.clear();
      }
      
      lastSendTime = millis();
    }
  }
  else{
    float voltage = readINA260Voltage();
    showText("Voltage: " + String(voltage, 2), 3);
  }
  
  delay(1);  // Small delay to prevent watchdog issues
}

void log(String message) {
  Serial.println(message);
  if (message.length() < 200) {
    addToSerialBuffer(message);
  }else{
    addToSerialBuffer(message.substring(0, 200));
  }
}

void addToSerialBuffer(String line) {
  serialBuffer[bufferIndex] = line;
  bufferIndex = (bufferIndex + 1) % SERIAL_BUFFER_SIZE;
}

void setupSensors() {
  log("Initializing sensors...");
  
  // Initialize I2C for INA260
  Wire.begin();
  log(" - I2C initialized");
  
  // Initialize INA260
  log(" - Initializing INA260...");
  if (ina260.begin()) {
    log(" - INA260 initialized successfully");
  } else {
    log("Error: Could not find INA260 chip");
    while (1); // Halt if INA260 not found
  }
  
  // Initialize HX711 pins
  log(" - Initializing HX711...");
  pinMode(HX711_DT_PIN, INPUT);
  pinMode(HX711_SCK_PIN, OUTPUT);
  digitalWrite(HX711_SCK_PIN, LOW);
  
  // Reset HX711
  resetHX711();
  delay(100);
  
  // Test HX711
  if (digitalRead(HX711_DT_PIN) == LOW) {
    log(" - HX711 initialized successfully");
  } else {
    log(" - HX711 not responding");
  }
}

void setupESC() {
  esc.attach(ESC_PIN, ESC_MIN_PULSE, ESC_MAX_PULSE);
  
  // ESC initialization sequence
  log("Initializing ESC...");
  esc.writeMicroseconds(ESC_MIN_PULSE); // Send minimum throttle
  delay(2000); // Wait for ESC to recognize the signal
  
  log("ESC initialized");
}

void setupWebServer() {
  log("Setting up web server...");
  
  // Get the server instance from WiFiManager
  WebServer& server = wifiManager.getServer();
  
  // Set up root handler
  // server.on("/", HTTP_GET, [&server]() {
  //   server.send(200, "text/plain", "ESP32 Web Server is running!");
  // });

  server.on("/", HTTP_GET, handleRoot);

  log(" - Root handler registered");
  
  // Set up motor control endpoint
  server.on("/motor/control", HTTP_POST, handleMotorControl);
  log(" - Motor control endpoint registered");
  
  // Start the server on all interfaces
  server.begin(80);
  log("Web server started on http://");
  log(wifiManager.getIPAddress());
  log(":80");
}

String getSerialOutput() {
  String output = "";
  
  // Get lines in chronological order
  for (int i = 0; i < SERIAL_BUFFER_SIZE; i++) {
    int idx = (bufferIndex + i) % SERIAL_BUFFER_SIZE;
    if (serialBuffer[idx].length() > 0) {
      output += serialBuffer[idx] + "<br>";
    }
  }
  
  if (output.length() == 0) {
    output = "No serial data captured yet...";
  }
  
  return output;
}

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESP32 Motor Control System</h1>";
  html += "<p>System Status: " + String(testRunning ? "Test Running" : "Ready") + "</p>";
  html += "<p>Current Test ID: " + currentTestId + "</p>";
  html += "<h2>Sensor Readings:</h2>";
  html += "<p>Load Cell: " + String(readLoadCell()) + "</p>";
  html += "<p>INA260 Voltage: " + String(readINA260Voltage()) + "V</p>";
  html += "<p>INA260 Current: " + String(readINA260Current()) + "mA</p>";
  html += "<p>Serial: " + getSerialOutput() + "</p>";
  html += "</body></html>";
  
  WebServer& server = wifiManager.getServer();
  server.send(200, "text/html", html);
}

void handleMotorControl() {
  WebServer& server = wifiManager.getServer();
  if (server.hasArg("plain") == false) {
    log("Error: No data received in motor control request");
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  String requestBody = server.arg("plain");
  log("Received motor control request:");
  log(requestBody);
  
  String body = server.arg("plain");
  JsonDocument doc;
  
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Check if required fields exist using the recommended is<T>() method
  if (!doc["test_id"].is<String>() || !doc["speeds"].is<JsonArray>() || !doc["ramp_delay"].is<int>()) {
    server.send(400, "application/json", "{\"error\":\"Missing or invalid required fields\"}");
    return;
  }
  
  if (testRunning) {
    server.send(409, "application/json", "{\"error\":\"Test already running\"}");
    return;
  }
  
  server.send(200, "application/json", "{\"status\":\"Test started\"}");
  
  // Start the motor test (non-blocking)
  log("\n=== Starting test from handleMotorControl ===");
  startMotorTest(doc);
}

void startMotorTest(JsonDocument& config) {
  log("\n=== Starting Motor Test ===");

  testRunning = true;
  currentTestId = config["test_id"].as<String>();
  testState.setSpeeds(config["speeds"]);
  testState.rampDelay = config["ramp_delay"];
  testState.currentSpeedIndex = 0;
  testState.speedStartTime = millis();
  
  // Clear any old data
  sensorBuffer.clear();
  
  // Initialize ESC
  esc.writeMicroseconds(ESC_MIN_PULSE);
  
  // Start data collection
  lastSendTime = millis();
  
  log("Test ID: ");
  log(currentTestId);
  log("Number of speed steps: ");
  log(testState.speeds.size());
  log("Ramp delay (ms): ");
  log(testState.rampDelay);

  showText("Starting Test with " + String(testState.rampDelay / 100, 2) + "s per step", 1);
  delay(1000);
}

// void updateMotorTest() {
//   if (!testRunning || testState.currentSpeedIndex >= testState.speeds.size()) {
//     return;
//   }
  
//   // Check if it's time to move to the next speed
//   if (millis() - testState.speedStartTime >= testState.rampDelay) {
//     testState.currentSpeedIndex++;
//     testState.speedStartTime = millis();
    
//     if (testState.currentSpeedIndex < testState.speeds.size()) {
//       // Set next speed
//       float speedValue = testState.speeds[testState.currentSpeedIndex];
//       int pwmValue = ESC_MIN_PULSE + (speedValue * (ESC_MAX_PULSE - ESC_MIN_PULSE));
      
//       showText("Speed: " + String(speedValue, 2), 2);
      
//       esc.writeMicroseconds(pwmValue);
//       currentSpeed = speedValue;
//     } else {
//       // Test complete
//       log("Test completed: " + currentTestId);
//       showText("Test Complete", 2);
//       esc.writeMicroseconds(ESC_MIN_PULSE);
//       testRunning = false;
      
//       // Send any remaining data
//       if (!sensorBuffer.empty()) {
//         sendBufferedData();
//       }
      
//       currentTestId = "";
//     }
//   }
// }

void updateMotorTest() {
  if (!testRunning || testState.currentSpeedIndex >= testState.speeds.size()) {
    return;
  }
  
  // Calculate time remaining in current stage (in seconds)
  unsigned long elapsedTime = millis() - testState.speedStartTime;
  int millisRemaining = max(0, (int)((testState.rampDelay - elapsedTime)));
  
  // Update display more frequently for smooth countdown
  static unsigned long lastDisplayUpdate = 0;
  static int lastSpeedIndex = -1;
  
  // Update every 50ms or when speed changes
  if (millis() - lastDisplayUpdate >= 10 || testState.currentSpeedIndex != lastSpeedIndex) {
    if (testState.currentSpeedIndex < testState.speeds.size()) {
      float speedValue = testState.speeds[testState.currentSpeedIndex];
      showText("Speed: " + String(speedValue, 2) + " T:" + String(millisRemaining), 2);
    }
    lastDisplayUpdate = millis();
    lastSpeedIndex = testState.currentSpeedIndex;
  }
  
  // Check if it's time to move to the next speed
  if (millis() - testState.speedStartTime >= testState.rampDelay) {
    testState.currentSpeedIndex++;
    testState.speedStartTime = millis();
    
    if (testState.currentSpeedIndex < testState.speeds.size()) {
      // Set next speed
      float speedValue = testState.speeds[testState.currentSpeedIndex];
      int pwmValue = ESC_MIN_PULSE + (speedValue * (ESC_MAX_PULSE - ESC_MIN_PULSE));
      
      // Display will be updated at the top of the next loop iteration
      esc.writeMicroseconds(pwmValue);
      currentSpeed = speedValue;
    } else {
      // Test complete
      log("Test completed: " + currentTestId);
      showText("Test Complete", 1);
      showText(" ", 2);

      esc.writeMicroseconds(ESC_MIN_PULSE);
      testRunning = false;
      
      // Send any remaining data
      if (!sensorBuffer.empty()) {
        sendBufferedData();
      }
      
      currentTestId = "";
    }
  }
}


void runMotorTest(JsonDocument& config) {
  startMotorTest(config);
  
  // This is the old blocking version - shouldn't be used anymore
  while (testRunning) {
    updateMotorTest();
    delay(1);
  }

  showText(" ", 1);
  showText(" ", 2);
  showText(" ", 3);

  float voltage = readINA260Voltage();
  showText("Voltage: " + String(voltage, 2), 3);
}

void sendBufferedData() {
  if (sensorBuffer.empty() || currentTestId.isEmpty()) {
    log("sendBufferedData: Buffer is empty or no test ID");
    return;
  }
  
  log("Sending ");
  showText("Sending buffer", 3);
  log(String(sensorBuffer.size()));
  log(" data points to server...");
  
  HTTPClient http;
  String url = "https://http-api-source-bc198-demo-aerospacetestingdevelopmentshow-prod.demo.quix.io/data/" + currentTestId;
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON document with array of sensor readings
  DynamicJsonDocument doc(16384);  // Allocate 16KB for the document
  JsonArray data = doc.createNestedArray("data");
  
  log("Created JSON document");
  
  // Add all buffered readings to the JSON array
  for (const auto& reading : sensorBuffer) {
    JsonObject point = data.createNestedObject();
    point["timestamp"] = reading.timestamp;
    
    JsonObject ina260 = point.createNestedObject("ina260");
    ina260["voltage_v"] = reading.voltage;
    ina260["current_ma"] = reading.current;
    
    JsonObject loadCell = point.createNestedObject("load_cell");
    loadCell["raw_value"] = reading.load_cell;
    loadCell["is_ready"] = b_loadCellReady;
    
    point["set_speed"] = reading.speed;
  }
  
  // Serialize and send
  String jsonString;
  serializeJson(doc, jsonString);
  
  log("Sending HTTP POST request...");
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    log("Success! Sent " + String(sensorBuffer.size()) + " samples. Response: " + String(httpResponseCode));
    log("Server response: " + String(response));
    showText("Buffer send success", 3);
  } else {
    log("Error sending data: " + String(httpResponseCode));
    log("Error details: " + String(http.errorToString(httpResponseCode).c_str()));
    showText("Buffer send error", 3);
  }
  
  log("Closing HTTP connection");
  http.end();
}

void resetHX711() {
  // Send 25+ clock pulses to reset
  for (int i = 0; i < 30; i++) {
    digitalWrite(HX711_SCK_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(HX711_SCK_PIN, LOW);
    delayMicroseconds(10);
  }
}

long readHX711() {
  // Wait for HX711 to be ready (DT goes low)
  unsigned long timeout = millis() + 1000; // 1 second timeout
  while (digitalRead(HX711_DT_PIN) == HIGH && millis() < timeout) {
    delayMicroseconds(10);
  }
  
  if (millis() >= timeout) {
    return 0; // Timeout
  }
  
  // Read 24 bits
  long value = 0;
  for (int i = 0; i < 24; i++) {
    digitalWrite(HX711_SCK_PIN, HIGH);
    delayMicroseconds(1);
    
    value = (value << 1) | digitalRead(HX711_DT_PIN);
    
    digitalWrite(HX711_SCK_PIN, LOW);
    delayMicroseconds(1);
  }
  
  // Send one more pulse to set gain to 128 for next reading
  digitalWrite(HX711_SCK_PIN, HIGH);
  delayMicroseconds(1);
  digitalWrite(HX711_SCK_PIN, LOW);
  delayMicroseconds(1);
  
  // Convert from 24-bit unsigned to signed
  if (value & 0x800000) {
    value |= 0xFF000000; // Sign extend for 32-bit signed
  }
  
  return value;
}

bool isLoadCellReady() {
  bool ready = (digitalRead(HX711_DT_PIN) == LOW);
  static unsigned long lastPrint = 0;
  static int readyCount = 0;
  static int notReadyCount = 0;
  
  // Update counters
  if (ready) {
    readyCount++;
  } else {
    notReadyCount++;
  }
  
  // Print stats every 2 seconds
  unsigned long now = millis();
  if (now - lastPrint > 2000) {
    lastPrint = now;
    float readyPercent = (readyCount * 100.0) / (readyCount + notReadyCount);
    log("HX711 Ready: ");
    log(String(readyPercent));
    log("%, DT pin state: ");
    log(digitalRead(HX711_DT_PIN) ? "HIGH" : "LOW");
    readyCount = 0;
    notReadyCount = 0;
  }
  
  return ready;
}

float readLoadCell() {
  static bool lastReadyState = false;
  static unsigned long lastReadyCheck = 0;
  static unsigned long lastReadAttempt = 0;
  
  // Only try to read every 20ms to avoid overwhelming the HX711
  if (millis() - lastReadAttempt < 20) {
    // If we have a recent valid reading, return it
    if (millis() - lastLoadCellUpdate < LOAD_CELL_TIMEOUT) {
      return lastValidLoadCellValue;
    }
    return 0.0f;
  }
  lastReadAttempt = millis();
  
  // Only check ready state every 100ms to avoid blocking
  if (millis() - lastReadyCheck > 100) {
    lastReadyState = isLoadCellReady();
    lastReadyCheck = millis();
  }
  
  // Try to read the HX711
  long rawValue = readHX711();
  
  // If we got a valid reading (non-zero) or the sensor is ready
  if (rawValue != 0) {
    lastValidLoadCellValue = (float)rawValue;
    lastLoadCellUpdate = millis();
    b_loadCellReady = true;
    
    // Print debug info (throttled)
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
      log("HX711 Raw: ");
      log(String(rawValue));
      log(", Value: ");
      log(String(lastValidLoadCellValue));
      log("\n");
      lastPrint = millis();
    }
    
    return lastValidLoadCellValue;
  }
  
  // If we get here, reading failed
  b_loadCellReady = false;
  
  // If we have a recent valid reading, return it even if the current read failed
  if (millis() - lastLoadCellUpdate < LOAD_CELL_TIMEOUT) {
    return lastValidLoadCellValue;
  }
  
  return 0.0f;
}

float readINA260Voltage() {
  float voltage = ina260.readBusVoltage() / 1000.0; // Convert mV to V
  return voltage;
}

float readINA260Current() {
  return ina260.readCurrent(); // Returns current in mA
}
