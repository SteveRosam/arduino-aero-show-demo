#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>

class WiFiManager {
public:
    // Constructor
    WiFiManager();
    
    // Initialize the WiFi manager
    bool begin();
    
    // Handle client connections
    void handleClient();
    
    // Check if connected to WiFi
    bool isConnected();
    
    // Get current IP address
    String getIPAddress();
    
    // Get MAC address
    String getMacAddress();
    
    // Set the report URL
    void setReportURL(const String& url);
    
    // Get the WebServer instance
    WebServer& getServer() { return server; }

private:
    // Web server instance
    WebServer server;
    
    // Preferences for persistent storage
    Preferences preferences;
    
    // Report URL for sending device info
    String reportURL;
    
    // Connection handling
    bool connectToWiFi(const String& ssid, const String& password, int maxRetries = 5);
    void reportDeviceInfo();
    
    // Web server handlers
    void handleRoot();
    void handleSetWiFi();
    void handleNotFound();
};

#endif // WIFI_MANAGER_H
