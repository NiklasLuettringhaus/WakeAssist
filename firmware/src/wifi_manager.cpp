/*
 * ===============================================================
 * WakeAssist - WiFi Management Module (Implementation)
 * ===============================================================
 *
 * This file implements the WiFi management functionality
 * declared in wifi_manager.h
 *
 * KEY CONCEPTS:
 * - Captive Portal: Automatic popup when user connects to device
 * - Preferences: ESP32's built-in flash storage for credentials
 * - WiFiManager: Third-party library that handles portal UI
 *
 * ===============================================================
 */

#include "wifi_manager.h"

// ===============================================================
// GLOBAL INSTANCE
// ===============================================================
// Create the global WiFi manager object
// This can be accessed from anywhere using: extern WiFiMgr wifiMgr;

WiFiMgr wifiMgr;

// ===============================================================
// CONSTRUCTOR
// ===============================================================
// Initialize member variables to safe defaults

WiFiMgr::WiFiMgr() {
    status = WIFI_NOT_INITIALIZED;
    reconnectAttempts = 0;
    lastConnectionCheck = 0;
    lastReconnectAttempt = 0;
    storedSSID = "";
    storedPassword = "";

    // Callbacks are null by default (no functions assigned)
    callbackConfigPortalStart = nullptr;
    callbackConnect = nullptr;
    callbackDisconnect = nullptr;
}

// ===============================================================
// INITIALIZATION
// ===============================================================
// Set up WiFi system and load saved credentials from flash

bool WiFiMgr::begin() {
    DEBUG_PRINTLN("[WiFi] Initializing WiFi Manager...");

    // Initialize ESP32's Preferences library for flash storage
    // "wakeassist" is our namespace (like a folder name)
    // 'false' means read-write mode (not read-only)
    if (!preferences.begin(STORAGE_NAMESPACE, false)) {
        DEBUG_PRINTLN("[WiFi] ERROR: Failed to initialize Preferences!");
        return false;
    }

    // Try to load saved WiFi credentials
    // These were saved during previous config portal session
    loadCredentials(storedSSID, storedPassword);

    if (storedSSID.length() > 0) {
        DEBUG_PRINTF("[WiFi] Found stored credentials for: %s\n", storedSSID.c_str());
    } else {
        DEBUG_PRINTLN("[WiFi] No stored credentials found - will need setup");
    }

    // Configure WiFi mode to Station (client) + AP (access point)
    // This allows seamless switching between modes
    WiFi.mode(WIFI_AP_STA);

    updateStatus(WIFI_NOT_INITIALIZED);

    DEBUG_PRINTLN("[WiFi] Initialization complete");
    return true;
}

// ===============================================================
// CONNECTION MANAGEMENT
// ===============================================================

// Connect to WiFi network (auto-detect if setup needed)

bool WiFiMgr::connect(bool autoConnect) {
    DEBUG_PRINTLN("[WiFi] Starting connection process...");

    updateStatus(WIFI_CONNECTING);

    // If autoConnect is true AND we have stored credentials, try them
    if (autoConnect && hasStoredCredentials()) {
        DEBUG_PRINTLN("[WiFi] Attempting connection with stored credentials...");

        if (connectToStoredNetwork()) {
            // Success! We're connected
            updateStatus(WIFI_CONNECTED);

            DEBUG_PRINT("[WiFi] Connected! IP Address: ");
            DEBUG_PRINTLN(WiFi.localIP());

            return true;
        } else {
            DEBUG_PRINTLN("[WiFi] Stored credentials didn't work");
        }
    }

    // If we reach here, either:
    // 1. No stored credentials exist, OR
    // 2. Stored credentials failed to connect
    //
    // Solution: Start config portal for user setup

    DEBUG_PRINTLN("[WiFi] Starting configuration portal...");
    return startConfigPortal();
}

// Check connection and reconnect if needed (call in loop())

bool WiFiMgr::maintainConnection() {
    unsigned long currentTime = millis();

    // Only check periodically to avoid excessive CPU usage
    // WIFI_CHECK_INTERVAL_MS is defined in config.h (typically 30 seconds)
    if (currentTime - lastConnectionCheck < WIFI_CHECK_INTERVAL_MS) {
        return isConnected();  // Return cached status
    }

    lastConnectionCheck = currentTime;

    // Check actual WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
        // We're connected!
        if (status != WIFI_CONNECTED) {
            // Status changed from disconnected to connected
            DEBUG_PRINTLN("[WiFi] Connection restored!");
            updateStatus(WIFI_CONNECTED);
        }

        reconnectAttempts = 0;  // Reset retry counter
        return true;

    } else {
        // We're NOT connected
        if (status == WIFI_CONNECTED) {
            // Status changed from connected to disconnected
            DEBUG_PRINTLN("[WiFi] Connection lost!");
            updateStatus(WIFI_DISCONNECTED);
        }

        // Should we attempt reconnection?
        // Don't retry too frequently (wait at least 5 seconds between attempts)
        if (currentTime - lastReconnectAttempt >= 5000) {
            lastReconnectAttempt = currentTime;

            if (reconnectAttempts < WIFI_MAX_RECONNECT_ATTEMPTS) {
                reconnectAttempts++;
                DEBUG_PRINTF("[WiFi] Reconnection attempt %d/%d...\n",
                           reconnectAttempts, WIFI_MAX_RECONNECT_ATTEMPTS);

                // Try to reconnect with stored credentials
                if (connectToStoredNetwork()) {
                    updateStatus(WIFI_CONNECTED);
                    reconnectAttempts = 0;
                    return true;
                }
            } else {
                // Max retries exceeded
                DEBUG_PRINTLN("[WiFi] Max reconnection attempts reached");
                updateStatus(WIFI_FAILED);
            }
        }

        return false;
    }
}

// Disconnect from WiFi

void WiFiMgr::disconnect() {
    DEBUG_PRINTLN("[WiFi] Disconnecting...");
    WiFi.disconnect();
    updateStatus(WIFI_DISCONNECTED);
}

// Start captive portal for WiFi configuration

bool WiFiMgr::startConfigPortal(unsigned long timeout) {
    DEBUG_PRINTLN("[WiFi] Starting captive portal...");

    updateStatus(WIFI_AP_MODE);

    // Trigger callback if set (e.g., to blink LED)
    if (callbackConfigPortalStart != nullptr) {
        callbackConfigPortalStart();
    }

    // Generate unique AP name based on ESP32 chip ID
    // Example: "WakeAssist-A3B5"
    String apName = generateAPName();

    DEBUG_PRINTF("[WiFi] AP Name: %s\n", apName.c_str());
    DEBUG_PRINTLN("[WiFi] AP Password: (none - open network)");
    DEBUG_PRINTLN("[WiFi] Connect to this network and setup page will appear");
    DEBUG_PRINTLN("[WiFi] Or navigate to: http://192.168.4.1");

    // Configure portal settings
    wifiManager.setConfigPortalTimeout(timeout / 1000);  // Convert ms to seconds
    wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000);

    // Customize portal appearance
    wifiManager.setTitle("WakeAssist Setup");
    wifiManager.setDarkMode(false);  // Use light theme for simplicity

    // Start the captive portal
    // This will block until:
    // 1. User submits WiFi credentials and connection succeeds, OR
    // 2. Timeout expires
    //
    // WiFiManager handles all the web server, DNS, and form processing!
    bool result = wifiManager.autoConnect(apName.c_str(), WIFI_AP_PASSWORD);

    if (result) {
        // User successfully configured WiFi and device connected!
        DEBUG_PRINTLN("[WiFi] Configuration successful!");

        // Save the credentials that worked
        String newSSID = WiFi.SSID();
        String newPassword = WiFi.psk();  // Get WiFi password

        saveCredentials(newSSID, newPassword);

        updateStatus(WIFI_CONNECTED);
        return true;

    } else {
        // Config portal timed out or user didn't configure
        DEBUG_PRINTLN("[WiFi] Configuration failed or timed out");
        updateStatus(WIFI_FAILED);
        return false;
    }
}

// ===============================================================
// CREDENTIAL MANAGEMENT
// ===============================================================

// Save WiFi credentials to flash memory

bool WiFiMgr::saveCredentials(const String& ssid, const String& password) {
    DEBUG_PRINTLN("[WiFi] Saving credentials to flash...");

    // Store SSID
    if (!preferences.putString(KEY_WIFI_SSID, ssid)) {
        DEBUG_PRINTLN("[WiFi] ERROR: Failed to save SSID");
        return false;
    }

    // Store password
    if (!preferences.putString(KEY_WIFI_PASSWORD, password)) {
        DEBUG_PRINTLN("[WiFi] ERROR: Failed to save password");
        return false;
    }

    // Update cached values
    storedSSID = ssid;
    storedPassword = password;

    DEBUG_PRINTLN("[WiFi] Credentials saved successfully");
    return true;
}

// Load WiFi credentials from flash memory

bool WiFiMgr::loadCredentials(String& ssid, String& password) {
    DEBUG_PRINTLN("[WiFi] Loading credentials from flash...");

    // Read SSID
    ssid = preferences.getString(KEY_WIFI_SSID, "");

    // Read password
    password = preferences.getString(KEY_WIFI_PASSWORD, "");

    if (ssid.length() > 0) {
        DEBUG_PRINTF("[WiFi] Loaded credentials for: %s\n", ssid.c_str());

        // Update cached values
        storedSSID = ssid;
        storedPassword = password;

        return true;
    } else {
        DEBUG_PRINTLN("[WiFi] No credentials found in flash");
        return false;
    }
}

// Check if credentials are stored

bool WiFiMgr::hasStoredCredentials() {
    // Check cached value (faster than flash read)
    if (storedSSID.length() > 0) {
        return true;
    }

    // If cache empty, check flash storage
    String ssid = preferences.getString(KEY_WIFI_SSID, "");
    return (ssid.length() > 0);
}

// Clear stored credentials (factory reset)

bool WiFiMgr::clearCredentials() {
    DEBUG_PRINTLN("[WiFi] Clearing stored credentials...");

    // Remove SSID
    preferences.remove(KEY_WIFI_SSID);

    // Remove password
    preferences.remove(KEY_WIFI_PASSWORD);

    // Clear cached values
    storedSSID = "";
    storedPassword = "";

    DEBUG_PRINTLN("[WiFi] Credentials cleared");
    return true;
}

// ===============================================================
// STATUS & INFORMATION
// ===============================================================

WiFiConnectionStatus WiFiMgr::getStatus() const {
    return status;
}

bool WiFiMgr::isConnected() const {
    return (status == WIFI_CONNECTED && WiFi.status() == WL_CONNECTED);
}

String WiFiMgr::getIPAddress() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "";
}

String WiFiMgr::getSSID() const {
    if (isConnected()) {
        return WiFi.SSID();
    }
    return storedSSID;  // Return stored SSID even if not connected
}

int WiFiMgr::getRSSI() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

String WiFiMgr::getStatusString() const {
    String result = "[WiFi] Status: ";

    switch (status) {
        case WIFI_NOT_INITIALIZED:
            result += "Not Initialized";
            break;
        case WIFI_CONNECTING:
            result += "Connecting...";
            break;
        case WIFI_CONNECTED:
            result += "Connected to '" + getSSID() + "' (" + getIPAddress() + ")";
            result += " RSSI: " + String(getRSSI()) + " dBm";
            break;
        case WIFI_DISCONNECTED:
            result += "Disconnected";
            break;
        case WIFI_AP_MODE:
            result += "Access Point Mode (Setup)";
            break;
        case WIFI_FAILED:
            result += "Failed";
            break;
        default:
            result += "Unknown";
    }

    return result;
}

// ===============================================================
// CALLBACK FUNCTIONS
// ===============================================================

void WiFiMgr::onConfigPortalStart(std::function<void()> callback) {
    callbackConfigPortalStart = callback;
}

void WiFiMgr::onConnect(std::function<void()> callback) {
    callbackConnect = callback;
}

void WiFiMgr::onDisconnect(std::function<void()> callback) {
    callbackDisconnect = callback;
}

// ===============================================================
// PRIVATE HELPER FUNCTIONS
// ===============================================================

// Connect using stored credentials

bool WiFiMgr::connectToStoredNetwork() {
    if (!hasStoredCredentials()) {
        DEBUG_PRINTLN("[WiFi] No stored credentials to connect with");
        return false;
    }

    DEBUG_PRINTF("[WiFi] Connecting to: %s\n", storedSSID.c_str());

    // Start WiFi connection
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());

    // Wait for connection (with timeout)
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DEBUG_PRINT(".");

        // Check timeout
        if (millis() - startTime > WIFI_CONNECT_TIMEOUT_MS) {
            DEBUG_PRINTLN("\n[WiFi] Connection timeout!");
            return false;
        }
    }

    DEBUG_PRINTLN("\n[WiFi] Connected successfully!");
    return true;
}

// Generate unique AP name from chip ID

String WiFiMgr::generateAPName() {
    // Get ESP32's unique chip ID (last 4 hex digits)
    // Example: If chip ID is 0x123456A3B5, we get "A3B5"
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i = i + 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }

    // Format: "WakeAssist-XXXX" where XXXX is last 4 hex digits
    char apName[32];
    snprintf(apName, sizeof(apName), "%s-%04X",
             WIFI_AP_SSID_PREFIX, (uint16_t)(chipId & 0xFFFF));

    return String(apName);
}

// Update status and trigger callbacks

void WiFiMgr::updateStatus(WiFiConnectionStatus newStatus) {
    // Only trigger callbacks if status actually changed
    if (newStatus == status) {
        return;
    }

    WiFiConnectionStatus oldStatus = status;
    status = newStatus;

    DEBUG_PRINTF("[WiFi] Status changed: %d -> %d\n", oldStatus, newStatus);

    // Trigger appropriate callback
    if (newStatus == WIFI_CONNECTED && callbackConnect != nullptr) {
        callbackConnect();
    } else if (oldStatus == WIFI_CONNECTED && newStatus != WIFI_CONNECTED) {
        // We lost connection
        if (callbackDisconnect != nullptr) {
            callbackDisconnect();
        }
    }
}

/*
 * ===============================================================
 * IMPLEMENTATION NOTES:
 * ===============================================================
 *
 * CAPTIVE PORTAL MECHANICS:
 * When a user connects to the "WakeAssist-XXXX" WiFi network,
 * their phone/computer sends DNS queries for various domains
 * (like apple.com, google.com, etc.) to check internet connectivity.
 *
 * WiFiManager responds to ALL DNS queries with the ESP32's IP (192.168.4.1),
 * which tricks the device into thinking there's a "captive portal"
 * (like hotel WiFi login pages). The OS automatically opens a browser
 * showing our configuration page.
 *
 * This is why non-technical users can set up the device without
 * typing any IP addresses - it just "pops up"!
 *
 * ===============================================================
 *
 * FLASH STORAGE DETAILS:
 * The Preferences library stores data in ESP32's flash memory,
 * which persists across reboots and power loss. Each key-value
 * pair is stored under a "namespace" (like a folder).
 *
 * Our namespace: "wakeassist"
 * Our keys:      "wifi_ssid", "wifi_pass"
 *
 * IMPORTANT: Flash memory has limited write cycles (~100,000).
 * Don't write to flash in a loop! Only write when values change.
 *
 * ===============================================================
 *
 * ERROR HANDLING STRATEGY:
 * This implementation uses a "graceful degradation" approach:
 *
 * 1. Try stored credentials first
 * 2. If that fails, start config portal
 * 3. If portal times out, keep retrying stored credentials
 * 4. User can always factory reset to start fresh
 *
 * The device never becomes "bricked" - there's always a way to
 * reconfigure it (hold RESET button 10 seconds).
 *
 * ===============================================================
 */
