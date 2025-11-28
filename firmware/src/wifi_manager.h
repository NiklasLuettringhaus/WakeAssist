/*
 * ===============================================================
 * WakeAssist - WiFi Management Module (Header File)
 * ===============================================================
 *
 * This module handles all WiFi-related functionality:
 * - Captive portal setup for non-technical users
 * - WiFi connection and reconnection
 * - Credential storage in flash memory
 * - Connection status monitoring
 *
 * WHY SEPARATE FILE?
 * WiFi management is complex and keeping it separate makes
 * the code easier to maintain and test independently.
 *
 * ===============================================================
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>              // ESP32 WiFi library
#include <WiFiManager.h>       // Captive portal library
#include <Preferences.h>       // ESP32 flash storage
#include "config.h"            // Our configuration constants

// ===============================================================
// WIFI CONNECTION STATUS ENUMERATION
// ===============================================================
// Represents the current WiFi connection state
//
// USAGE: if (wifiManager.getStatus() == WIFI_CONNECTED) { ... }

enum WiFiConnectionStatus {
    WIFI_NOT_INITIALIZED,     // WiFi not yet started
    WIFI_CONNECTING,          // Attempting to connect
    WIFI_CONNECTED,           // Successfully connected to WiFi
    WIFI_DISCONNECTED,        // Lost connection
    WIFI_AP_MODE,             // Running as Access Point (setup mode)
    WIFI_FAILED               // Connection failed after retries
};

// ===============================================================
// WIFI MANAGER CLASS
// ===============================================================
// Encapsulates all WiFi management functions
//
// DESIGN PATTERN:
// This class uses the WiFiManager library to create a captive
// portal that automatically pops up when a user connects to
// the device's temporary WiFi network.

class WiFiMgr {
public:
    // ---------------------------------------------------------------
    // CONSTRUCTOR
    // ---------------------------------------------------------------
    WiFiMgr();

    // ---------------------------------------------------------------
    // INITIALIZATION
    // ---------------------------------------------------------------
    // Initializes WiFi system and loads stored credentials
    // MUST be called once in setup() before using WiFi
    //
    // RETURNS: true if initialization successful
    bool begin();

    // ---------------------------------------------------------------
    // CONNECTION MANAGEMENT
    // ---------------------------------------------------------------

    // Start WiFi connection process
    // This will either:
    // 1. Connect to stored WiFi credentials (if available)
    // 2. Start captive portal for first-time setup
    //
    // autoConnect: If true, automatically tries stored credentials
    //              If false, always starts captive portal
    //
    // RETURNS: true if connected successfully
    bool connect(bool autoConnect = true);

    // Check WiFi connection and attempt reconnection if needed
    // Call this periodically in loop() to maintain connection
    //
    // RETURNS: true if connected, false if disconnected
    bool maintainConnection();

    // Disconnect from WiFi and stop all networking
    // Used for testing or when entering low-power mode
    void disconnect();

    // Start captive portal for WiFi setup
    // Creates temporary WiFi network "WakeAssist-XXXX"
    // User can connect and configure WiFi settings
    //
    // timeout: How long to wait for user (milliseconds)
    //          0 = wait forever
    //
    // RETURNS: true if configuration received and connected
    bool startConfigPortal(unsigned long timeout = WIFI_AP_TIMEOUT_MS);

    // ---------------------------------------------------------------
    // CREDENTIAL MANAGEMENT
    // ---------------------------------------------------------------

    // Save WiFi credentials to flash memory
    // These will persist across reboots
    //
    // ssid: WiFi network name
    // password: WiFi password
    //
    // RETURNS: true if saved successfully
    bool saveCredentials(const String& ssid, const String& password);

    // Load WiFi credentials from flash memory
    // Called automatically by begin()
    //
    // ssid: Reference to store loaded SSID
    // password: Reference to store loaded password
    //
    // RETURNS: true if credentials exist in flash
    bool loadCredentials(String& ssid, String& password);

    // Check if WiFi credentials are stored
    // Useful to determine if this is first-time setup
    //
    // RETURNS: true if credentials exist
    bool hasStoredCredentials();

    // Clear stored WiFi credentials from flash
    // Used for factory reset or troubleshooting
    //
    // RETURNS: true if cleared successfully
    bool clearCredentials();

    // ---------------------------------------------------------------
    // STATUS & INFORMATION
    // ---------------------------------------------------------------

    // Get current WiFi connection status
    // RETURNS: WiFiConnectionStatus enum value
    WiFiConnectionStatus getStatus() const;

    // Check if currently connected to WiFi
    // Convenience function for: getStatus() == WIFI_CONNECTED
    //
    // RETURNS: true if connected
    bool isConnected() const;

    // Get current IP address (if connected)
    // RETURNS: IP address as String (e.g., "192.168.1.100")
    //          Returns empty string if not connected
    String getIPAddress() const;

    // Get connected WiFi network name
    // RETURNS: SSID as String, empty if not connected
    String getSSID() const;

    // Get WiFi signal strength (RSSI)
    // RETURNS: Signal strength in dBm (-30 = excellent, -90 = poor)
    //          Returns 0 if not connected
    int getRSSI() const;

    // Get human-readable status string for debugging
    // RETURNS: String like "Connected to 'MyWiFi' (192.168.1.100)"
    String getStatusString() const;

    // ---------------------------------------------------------------
    // CONFIGURATION PORTAL CALLBACKS
    // ---------------------------------------------------------------
    // These allow other parts of the code to know when setup happens

    // Set callback function to run when config portal starts
    // Useful for LED indicators or status updates
    //
    // USAGE:
    //   wifiMgr.onConfigPortalStart([]() {
    //       Serial.println("Config portal started!");
    //   });
    void onConfigPortalStart(std::function<void()> callback);

    // Set callback function to run when WiFi connects
    // Useful for success notifications
    void onConnect(std::function<void()> callback);

    // Set callback function to run when WiFi disconnects
    // Useful for error handling
    void onDisconnect(std::function<void()> callback);

private:
    // ---------------------------------------------------------------
    // PRIVATE MEMBER VARIABLES
    // ---------------------------------------------------------------

    WiFiManager wifiManager;          // WiFiManager library instance
    Preferences preferences;          // ESP32 flash storage

    WiFiConnectionStatus status;      // Current connection status

    unsigned int reconnectAttempts;   // How many reconnection attempts made
    unsigned long lastConnectionCheck;// Last time we checked connection
    unsigned long lastReconnectAttempt;// Last time we tried to reconnect

    String storedSSID;                // Cached WiFi network name
    String storedPassword;            // Cached WiFi password

    // Callback functions
    std::function<void()> callbackConfigPortalStart;
    std::function<void()> callbackConnect;
    std::function<void()> callbackDisconnect;

    // ---------------------------------------------------------------
    // PRIVATE HELPER FUNCTIONS
    // ---------------------------------------------------------------

    // Attempt to connect using stored credentials
    // RETURNS: true if connection successful
    bool connectToStoredNetwork();

    // Generate unique Access Point name based on ESP32 chip ID
    // RETURNS: String like "WakeAssist-A3B5"
    String generateAPName();

    // Configure WiFiManager portal parameters
    // Sets up custom fields for Telegram bot token
    void configurePortalParameters();

    // Update connection status and trigger callbacks if changed
    void updateStatus(WiFiConnectionStatus newStatus);
};

// ===============================================================
// GLOBAL WIFI MANAGER INSTANCE
// ===============================================================
// Create a single WiFi manager instance that can be used
// throughout the program
//
// USAGE IN OTHER FILES:
//   extern WiFiMgr wifiMgr;
//   if (wifiMgr.isConnected()) { ... }

extern WiFiMgr wifiMgr;

#endif // WIFI_MANAGER_H

/*
 * ===============================================================
 * USAGE EXAMPLE:
 * ===============================================================
 *
 * // In main.cpp:
 * #include "wifi_manager.h"
 *
 * void setup() {
 *     Serial.begin(115200);
 *
 *     // Initialize WiFi manager
 *     if (!wifiMgr.begin()) {
 *         Serial.println("WiFi init failed!");
 *     }
 *
 *     // Set up callbacks for status updates
 *     wifiMgr.onConnect([]() {
 *         Serial.println("Connected to WiFi!");
 *         hardware.setWiFiLED(true);  // Turn on WiFi LED
 *     });
 *
 *     wifiMgr.onDisconnect([]() {
 *         Serial.println("WiFi lost!");
 *         hardware.blinkWiFiLED(LED_BLINK_FAST);  // Blink LED fast
 *     });
 *
 *     // Connect to WiFi (auto-start config portal if needed)
 *     if (!wifiMgr.connect()) {
 *         Serial.println("Failed to connect!");
 *     }
 *
 *     Serial.print("IP Address: ");
 *     Serial.println(wifiMgr.getIPAddress());
 * }
 *
 * void loop() {
 *     // Check WiFi connection status periodically
 *     wifiMgr.maintainConnection();
 *
 *     if (wifiMgr.isConnected()) {
 *         // Do network operations...
 *     }
 * }
 *
 * ===============================================================
 * CAPTIVE PORTAL FLOW FOR NON-TECHNICAL USERS:
 * ===============================================================
 *
 * 1. Device boots up for first time (no stored WiFi credentials)
 * 2. Device creates WiFi network: "WakeAssist-A3B5" (no password)
 * 3. User connects phone to "WakeAssist-A3B5"
 * 4. Phone automatically shows setup page (captive portal)
 *    - If not automatic, navigate to: http://192.168.4.1
 * 5. User sees simple form asking for:
 *    - Their home WiFi network name
 *    - Their home WiFi password
 * 6. User clicks "Save"
 * 7. Device saves credentials and reboots
 * 8. Device connects to user's home WiFi automatically
 * 9. Device is now accessible remotely via internet
 *
 * TROUBLESHOOTING FOR USERS:
 * - If setup page doesn't appear: Open browser, go to 192.168.4.1
 * - If connection fails: Hold RESET button 10s to factory reset
 * - If still problems: Check WiFi password is correct
 *
 * ===============================================================
 */
