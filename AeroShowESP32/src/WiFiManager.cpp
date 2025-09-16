#include "WiFiManager.h"
#include <HTTPClient.h>

WiFiManager::WiFiManager() : server(80) {
    // Constructor implementation
}

bool WiFiManager::begin() {

    // Initialize LED pin
    pinMode(2, OUTPUT);
    
    // Try to connect to WiFi
    if (connectToWiFi(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("Connected to saved WiFi network");
        reportDeviceInfo();
        return true;
    }
        
    // If both connection attempts failed
    Serial.println("Failed to connect to WiFi");
    return false;
}

void WiFiManager::handleClient() {
    server.handleClient();
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIPAddress() {
    return WiFi.localIP().toString();
}

void WiFiManager::setReportURL(const String& url) {
    reportURL = url;
}

bool WiFiManager::connectToWiFi(const String& ssid, const String& password, int maxRetries) {
    int retryCount = 0;
    
    while (retryCount < maxRetries) {
        Serial.printf("Connecting to %s...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());

        int attempts = 0;
        while (attempts < 20) { // 10 seconds timeout (20 * 500ms)
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("WiFi connected");
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
                return true;
            }
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        Serial.println("\nConnection attempt failed");
        retryCount++;
    }
    
    return false;
}

void WiFiManager::reportDeviceInfo() {
    if (reportURL.length() == 0) return;
    
    HTTPClient http;
    http.begin(reportURL);
    http.addHeader("Content-Type", "application/json");
    
    String json = "{\"device_id\":\"" + String(ESP.getEfuseMac(), HEX) + 
                  "\",\"ip\":\"" + WiFi.localIP().toString() + 
                  "\",\"ssid\":\"" + WiFi.SSID() + "\"}";
    
    int httpResponseCode = http.POST(json);
    
    if (httpResponseCode > 0) {
        Serial.printf("Device info sent. Response code: %d\n", httpResponseCode);
    } else {
        Serial.printf("Error sending device info: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    
    http.end();
}

void WiFiManager::handleRoot() {
    String html = "<html><body>";
    html += "<h1>WiFi Configuration</h1>";
    html += "<form method='post' action='/setwifi'>";
    html += "SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='password'><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form></body></html>";
    
    server.send(200, "text/html", html);
}

void WiFiManager::handleNotFound() {
    server.send(404, "text/plain", "Not found");
}
