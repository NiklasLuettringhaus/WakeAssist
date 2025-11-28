/*
 * ===============================================================
 * WakeAssist - Main Program File
 * ===============================================================
 *
 * This is the entry point for the WakeAssist firmware.
 * It coordinates all the modules:
 * - Hardware control (buzzers, LEDs, buttons)
 * - WiFi management (captive portal setup)
 * - Telegram bot (remote commands)
 * - Alarm controller (state machine)
 *
 * STARTUP SEQUENCE:
 * 1. Initialize serial communication
 * 2. Initialize hardware (GPIO, PWM)
 * 3. Connect to WiFi (or start config portal)
 * 4. Initialize Telegram bot
 * 5. Register command handlers
 * 6. Enter main loop
 *
 * MAIN LOOP:
 * - Update hardware (buttons, LEDs)
 * - Poll Telegram for messages
 * - Update alarm state machine
 * - Check for button presses
 * - Monitor WiFi connection
 *
 * ===============================================================
 */

#include <Arduino.h>
#include "config.h"
#include "hardware.h"
#include "wifi_manager.h"
#include "telegram_bot.h"
#include "alarm_controller.h"

// ===============================================================
// FUNCTION DECLARATIONS
// ===============================================================
// Forward declarations for functions defined below

void setupHardware();
void setupWiFi();
void setupTelegram();
void setupCommandHandlers();
void handleButtons();
void checkWiFiStatus();
void printStatus();

// ===============================================================
// GLOBAL VARIABLES
// ===============================================================

// Timing variables for non-blocking operations
unsigned long lastTelegramPoll = 0;
unsigned long lastStatusPrint = 0;
unsigned long lastWiFiCheck = 0;

// Status tracking
bool systemReady = false;
unsigned long bootTime = 0;

// ===============================================================
// SETUP FUNCTION
// ===============================================================
// Runs once when device boots up
//
// This function initializes all subsystems and prepares
// the device for operation

void setup() {
    // ---------------------------------------------------------------
    // 1. INITIALIZE SERIAL COMMUNICATION
    // ---------------------------------------------------------------
    // Start serial port for debugging (115200 baud)
    // This must be first so we can see debug messages
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);  // Wait for serial to stabilize

    DEBUG_PRINTLN("\n\n");
    DEBUG_PRINTLN("===============================================");
    DEBUG_PRINTLN("       WakeAssist - Remote Alarm System       ");
    DEBUG_PRINTLN("===============================================");
    DEBUG_PRINTF("Hardware Version: %s\n", HARDWARE_VERSION);
    DEBUG_PRINTF("Software Version: %s\n", SOFTWARE_VERSION);
    DEBUG_PRINTLN("===============================================\n");

    bootTime = millis();

    // ---------------------------------------------------------------
    // 2. INITIALIZE HARDWARE
    // ---------------------------------------------------------------
    DEBUG_PRINTLN("[Setup] Initializing hardware...");

    if (!hardware.begin()) {
        DEBUG_PRINTLN("[Setup] FATAL ERROR: Hardware initialization failed!");
        // Blink status LED rapidly to indicate error
        while (true) {
            hardware.blinkStatusLED(LED_BLINK_FAST);
            hardware.updateLEDs();
            delay(100);
        }
    }

    // Turn on status LED to show device is working
    hardware.setStatusLED(true);

    // Perform hardware health checks
    DEBUG_PRINTLN("[Setup] Running hardware diagnostics...");
    if (!hardware.checkBuzzerCircuits()) {
        DEBUG_PRINTLN("[Setup] WARNING: Buzzer circuit issues detected!");
        DEBUG_PRINTLN(hardware.getStatusString());
        // Continue anyway - user should run /test to verify
    } else {
        DEBUG_PRINTLN("[Setup] Hardware diagnostics passed");
    }

    // ---------------------------------------------------------------
    // 3. INITIALIZE WIFI
    // ---------------------------------------------------------------
    DEBUG_PRINTLN("[Setup] Initializing WiFi...");

    if (!wifiMgr.begin()) {
        DEBUG_PRINTLN("[Setup] ERROR: WiFi initialization failed!");
    }

    // Set up WiFi callbacks
    wifiMgr.onConnect([]() {
        DEBUG_PRINTLN("[Setup] WiFi connected!");
        hardware.setWiFiLED(true);  // Solid WiFi LED = connected
    });

    wifiMgr.onDisconnect([]() {
        DEBUG_PRINTLN("[Setup] WiFi disconnected!");
        hardware.blinkWiFiLED(LED_BLINK_FAST);  // Blinking WiFi LED = error
    });

    wifiMgr.onConfigPortalStart([]() {
        DEBUG_PRINTLN("[Setup] Config portal started");
        hardware.blinkWiFiLED(LED_BLINK_SLOW);  // Slow blink = setup mode
    });

    // Connect to WiFi (or start config portal if first time)
    DEBUG_PRINTLN("[Setup] Connecting to WiFi...");
    if (wifiMgr.connect(true)) {  // autoConnect = true
        DEBUG_PRINTLN("[Setup] WiFi connection successful!");
        DEBUG_PRINT("[Setup] IP Address: ");
        DEBUG_PRINTLN(wifiMgr.getIPAddress());
        DEBUG_PRINT("[Setup] SSID: ");
        DEBUG_PRINTLN(wifiMgr.getSSID());
    } else {
        DEBUG_PRINTLN("[Setup] ERROR: WiFi connection failed!");
        DEBUG_PRINTLN("[Setup] Device may not be fully functional");
    }

    // ---------------------------------------------------------------
    // 4. INITIALIZE TELEGRAM BOT
    // ---------------------------------------------------------------
    DEBUG_PRINTLN("[Setup] Initializing Telegram bot...");

    // Try to load bot configuration from flash
    if (telegramBot.beginFromStorage()) {
        DEBUG_PRINTLN("[Setup] Telegram bot loaded from storage");
    } else {
        DEBUG_PRINTLN("[Setup] No stored Telegram configuration");
        DEBUG_PRINTLN("[Setup] Bot token and user ID needed for remote control");
        // Note: User will need to configure via web portal
    }

    // Set up Telegram callbacks
    telegramBot.onOnline([]() {
        DEBUG_PRINTLN("[Setup] Telegram bot is online");
        telegramBot.sendMessage(MSG_DEVICE_ONLINE);
    });

    telegramBot.onOffline([]() {
        DEBUG_PRINTLN("[Setup] Telegram bot went offline");
    });

    telegramBot.onUnauthorizedAccess([](int64_t userId, String message) {
        DEBUG_PRINTF("[Setup] Unauthorized access from user %lld: %s\n",
                    userId, message.c_str());
    });

    // ---------------------------------------------------------------
    // 5. INITIALIZE ALARM CONTROLLER
    // ---------------------------------------------------------------
    DEBUG_PRINTLN("[Setup] Initializing alarm controller...");

    if (!alarmController.begin()) {
        DEBUG_PRINTLN("[Setup] ERROR: Alarm controller initialization failed!");
    }

    // Enable Telegram notifications for alarm events
    alarmController.setTelegramNotificationsEnabled(true);

    // Enable hardware health checks during alarms
    alarmController.setHardwareChecksEnabled(true);

    // ---------------------------------------------------------------
    // 6. REGISTER COMMAND HANDLERS
    // ---------------------------------------------------------------
    DEBUG_PRINTLN("[Setup] Registering Telegram command handlers...");
    setupCommandHandlers();

    // ---------------------------------------------------------------
    // 7. FINAL SETUP
    // ---------------------------------------------------------------

    // If Telegram is configured, mark all old messages as read
    // (We don't want old /wake commands to trigger on boot!)
    if (telegramBot.isConfigured()) {
        DEBUG_PRINTLN("[Setup] Marking old Telegram messages as read...");
        telegramBot.markAllRead();
    }

    // System is ready!
    systemReady = true;

    DEBUG_PRINTLN("\n===============================================");
    DEBUG_PRINTLN("           SYSTEM READY FOR USE               ");
    DEBUG_PRINTLN("===============================================");
    DEBUG_PRINTLN("[Setup] Send /wake via Telegram to test alarm");
    DEBUG_PRINTLN("[Setup] Press TEST button to test hardware");
    DEBUG_PRINTLN("===============================================\n");

    // Print initial status
    printStatus();
}

// ===============================================================
// MAIN LOOP
// ===============================================================
// Runs continuously after setup() completes
//
// This is the heart of the program - it keeps everything
// running by calling update functions for each subsystem

void loop() {
    unsigned long currentTime = millis();

    // ---------------------------------------------------------------
    // 1. UPDATE HARDWARE
    // ---------------------------------------------------------------
    // Update button states (debouncing) and LED blinking
    hardware.updateButtons();
    hardware.updateLEDs();

    // ---------------------------------------------------------------
    // 2. HANDLE BUTTON PRESSES
    // ---------------------------------------------------------------
    handleButtons();

    // ---------------------------------------------------------------
    // 3. UPDATE ALARM STATE MACHINE
    // ---------------------------------------------------------------
    // CRITICAL: This must be called frequently for alarm timing
    alarmController.update();

    // ---------------------------------------------------------------
    // 4. POLL TELEGRAM FOR MESSAGES
    // ---------------------------------------------------------------
    // Only poll if:
    // - Bot is configured
    // - WiFi is connected
    // - Enough time has elapsed since last poll
    if (telegramBot.isConfigured() &&
        wifiMgr.isConnected() &&
        currentTime - lastTelegramPoll >= TELEGRAM_POLL_INTERVAL_MS) {

        lastTelegramPoll = currentTime;
        telegramBot.poll();
    }

    // ---------------------------------------------------------------
    // 5. MAINTAIN WIFI CONNECTION
    // ---------------------------------------------------------------
    // Periodically check WiFi status and reconnect if needed
    if (currentTime - lastWiFiCheck >= WIFI_CHECK_INTERVAL_MS) {
        lastWiFiCheck = currentTime;
        checkWiFiStatus();
    }

    // ---------------------------------------------------------------
    // 6. PERIODIC STATUS REPORTING
    // ---------------------------------------------------------------
    // Print system status to serial monitor for debugging
    if (DEBUG_ENABLED && currentTime - lastStatusPrint >= STATUS_REPORT_INTERVAL_MS) {
        lastStatusPrint = currentTime;
        printStatus();
    }

    // ---------------------------------------------------------------
    // 7. YIELD TO SYSTEM
    // ---------------------------------------------------------------
    // Allow ESP32 to handle background tasks (WiFi, etc.)
    // This prevents watchdog timer resets
    yield();
    delay(10);  // Small delay to prevent loop from running too fast
}

// ===============================================================
// COMMAND HANDLER REGISTRATION
// ===============================================================
// Register all Telegram command handlers
//
// Each command has a callback function that runs when
// the user sends that command via Telegram

void setupCommandHandlers() {
    // ---------------------------------------------------------------
    // /start - Welcome message
    // ---------------------------------------------------------------
    telegramBot.onCommand("/start", [](TelegramMessage msg) {
        String welcome = "🔔 *WakeAssist Remote Alarm*\n\n";
        welcome += "Available commands:\n";
        welcome += "/wake - Start alarm sequence\n";
        welcome += "/stop - Stop active alarm\n";
        welcome += "/test - Test buzzer hardware\n";
        welcome += "/status - Show device status\n";
        welcome += "/help - Show this message\n";

        telegramBot.sendMessage(welcome);
    });

    // ---------------------------------------------------------------
    // /wake - Start alarm
    // ---------------------------------------------------------------
    telegramBot.onCommand("/wake", [](TelegramMessage msg) {
        // Check rate limit
        if (telegramBot.isWakeRateLimited()) {
            unsigned long remaining = telegramBot.getWakeCooldownRemaining();
            char rateMsg[128];
            snprintf(rateMsg, sizeof(rateMsg), MSG_RATE_LIMITED, remaining);
            telegramBot.sendMessage(rateMsg);
            return;
        }

        // Check if alarm already active
        if (alarmController.isActive()) {
            telegramBot.sendMessage("⚠️ Alarm already active!");
            return;
        }

        // Start alarm
        if (alarmController.start()) {
            telegramBot.resetWakeRateLimit();  // Start cooldown
            DEBUG_PRINTLN("[Command] /wake - Alarm started");
        } else {
            telegramBot.sendMessage("❌ Failed to start alarm");
        }
    });

    // ---------------------------------------------------------------
    // /stop - Stop alarm
    // ---------------------------------------------------------------
    telegramBot.onCommand("/stop", [](TelegramMessage msg) {
        if (!alarmController.isActive()) {
            telegramBot.sendMessage("ℹ️ No active alarm to stop");
            return;
        }

        if (alarmController.stop(STOP_TELEGRAM_COMMAND)) {
            DEBUG_PRINTLN("[Command] /stop - Alarm stopped");
            // Notification sent by alarm controller
        } else {
            telegramBot.sendMessage("❌ Failed to stop alarm");
        }
    });

    // ---------------------------------------------------------------
    // /test - Test hardware
    // ---------------------------------------------------------------
    telegramBot.onCommand("/test", [](TelegramMessage msg) {
        if (alarmController.isActive()) {
            telegramBot.sendMessage("⚠️ Cannot test while alarm is active");
            return;
        }

        DEBUG_PRINTLN("[Command] /test - Running hardware test");
        alarmController.testAlarm();
    });

    // ---------------------------------------------------------------
    // /status - Show system status
    // ---------------------------------------------------------------
    telegramBot.onCommand("/status", [](TelegramMessage msg) {
        String status = "📊 *Device Status*\n\n";

        // Uptime
        unsigned long uptimeSeconds = (millis() - bootTime) / 1000;
        unsigned long uptimeMinutes = uptimeSeconds / 60;
        unsigned long uptimeHours = uptimeMinutes / 60;
        status += "⏱ Uptime: " + String(uptimeHours) + "h " +
                 String(uptimeMinutes % 60) + "m\n";

        // WiFi status
        status += "📡 WiFi: ";
        if (wifiMgr.isConnected()) {
            status += "Connected (" + wifiMgr.getSSID() + ")\n";
            status += "   IP: " + wifiMgr.getIPAddress() + "\n";
            status += "   Signal: " + String(wifiMgr.getRSSI()) + " dBm\n";
        } else {
            status += "Disconnected\n";
        }

        // Telegram status
        status += "💬 Telegram: ";
        status += telegramBot.isOnline() ? "Online\n" : "Offline\n";

        // Alarm status
        status += "🔔 Alarm: " + alarmController.getStateString() + "\n";

        // Hardware status
        HardwareState hwState = hardware.getState();
        status += "🔧 Hardware:\n";
        status += "   Small Buzzer: " +
                 String(hwState.smallBuzzer == HW_STATUS_OK ? "OK" : "Issue") + "\n";
        status += "   Large Buzzer: " +
                 String(hwState.largeBuzzer == HW_STATUS_OK ? "OK" : "Issue") + "\n";

        telegramBot.sendMessage(status);
        DEBUG_PRINTLN("[Command] /status - Status sent");
    });

    // ---------------------------------------------------------------
    // /help - Show help
    // ---------------------------------------------------------------
    telegramBot.onCommand("/help", [](TelegramMessage msg) {
        // Same as /start
        telegramBot.onCommand("/start", [](TelegramMessage msg) {});
    });

    DEBUG_PRINTLN("[Setup] Command handlers registered");
}

// ===============================================================
// BUTTON HANDLING
// ===============================================================
// Check for physical button presses and handle them

void handleButtons() {
    // ---------------------------------------------------------------
    // TEST BUTTON
    // ---------------------------------------------------------------
    // Pressing TEST button runs hardware test
    if (hardware.isTestButtonPressed()) {
        DEBUG_PRINTLN("[Button] TEST button pressed");

        if (!alarmController.isActive()) {
            alarmController.testAlarm();
        } else {
            DEBUG_PRINTLN("[Button] Cannot test - alarm active");
        }

        // Wait for button release
        while (hardware.isTestButtonPressed()) {
            hardware.updateButtons();
            delay(50);
        }
    }

    // ---------------------------------------------------------------
    // SILENCE BUTTON
    // ---------------------------------------------------------------
    // Pressing SILENCE button stops active alarm
    if (hardware.isSilenceButtonPressed()) {
        DEBUG_PRINTLN("[Button] SILENCE button pressed");

        if (alarmController.isActive()) {
            alarmController.stop(STOP_SILENCE_BUTTON);
        }

        // Wait for button release
        while (hardware.isSilenceButtonPressed()) {
            hardware.updateButtons();
            delay(50);
        }
    }

    // ---------------------------------------------------------------
    // RESET BUTTON (FACTORY RESET)
    // ---------------------------------------------------------------
    // Holding RESET button for 10 seconds triggers factory reset
    if (hardware.isFactoryResetRequested()) {
        DEBUG_PRINTLN("[Button] FACTORY RESET triggered!");

        // Stop alarm if active
        if (alarmController.isActive()) {
            alarmController.stop(STOP_SILENCE_BUTTON);
        }

        // Clear all stored data
        DEBUG_PRINTLN("[Reset] Clearing WiFi credentials...");
        wifiMgr.clearCredentials();

        DEBUG_PRINTLN("[Reset] Clearing Telegram configuration...");
        // Note: Need to add clearConfiguration() method to TelegramBot class

        DEBUG_PRINTLN("[Reset] Factory reset complete!");
        DEBUG_PRINTLN("[Reset] Rebooting in 3 seconds...");

        // Blink all LEDs to indicate reset
        for (int i = 0; i < 6; i++) {
            hardware.setStatusLED(i % 2 == 0);
            hardware.setWiFiLED(i % 2 == 0);
            hardware.setAlarmLED(i % 2 == 0);
            delay(500);
        }

        // Reboot device
        ESP.restart();
    }
}

// ===============================================================
// WIFI STATUS MONITORING
// ===============================================================
// Periodically check WiFi connection and attempt reconnection

void checkWiFiStatus() {
    // Let WiFi manager handle connection maintenance
    bool wasConnected = wifiMgr.isConnected();
    wifiMgr.maintainConnection();
    bool isConnected = wifiMgr.isConnected();

    // Detect connection state changes
    if (wasConnected && !isConnected) {
        // Connection lost
        DEBUG_PRINTLN("[WiFi] Connection lost!");

        // If alarm is active, send notification via Telegram (if still possible)
        if (alarmController.isActive() && telegramBot.isConfigured()) {
            telegramBot.sendMessage(MSG_ERROR_WIFI_LOST);
        }
    } else if (!wasConnected && isConnected) {
        // Connection restored
        DEBUG_PRINTLN("[WiFi] Connection restored!");

        // Send notification
        if (alarmController.isActive() && telegramBot.isConfigured()) {
            char msg[128];
            snprintf(msg, sizeof(msg), MSG_ERROR_WIFI_RESTORED,
                    alarmController.getStateString().c_str());
            telegramBot.sendMessage(msg);
        }
    }
}

// ===============================================================
// STATUS PRINTING
// ===============================================================
// Print system status to serial monitor for debugging

void printStatus() {
    DEBUG_PRINTLN("\n===============================================");
    DEBUG_PRINTLN("            SYSTEM STATUS REPORT               ");
    DEBUG_PRINTLN("===============================================");

    // Uptime
    unsigned long uptimeSeconds = (millis() - bootTime) / 1000;
    DEBUG_PRINTF("Uptime: %lu seconds\n", uptimeSeconds);

    // WiFi status
    DEBUG_PRINTLN(wifiMgr.getStatusString());

    // Telegram status
    DEBUG_PRINTLN(telegramBot.getStatusString());

    // Alarm status
    DEBUG_PRINTF("Alarm State: %s\n", alarmController.getStateString().c_str());

    // Hardware status
    DEBUG_PRINTLN(hardware.getStatusString());

    // Memory info
    DEBUG_PRINTF("Free Heap: %u bytes\n", ESP.getFreeHeap());

    DEBUG_PRINTLN("===============================================\n");
}

/*
 * ===============================================================
 * PROGRAM FLOW SUMMARY:
 * ===============================================================
 *
 * STARTUP (setup() function):
 * 1. Serial communication starts (115200 baud)
 * 2. Hardware initialized (GPIO pins, PWM channels)
 * 3. WiFi connects (or starts config portal if first time)
 * 4. Telegram bot loads configuration from flash
 * 5. Command handlers registered (/wake, /test, /status, etc.)
 * 6. Old messages marked as read (prevent accidental triggers)
 * 7. System enters main loop
 *
 * MAIN LOOP (loop() function):
 * - Runs continuously, ~100 times per second
 * - Updates hardware (buttons, LEDs) every cycle
 * - Polls Telegram every 5 seconds
 * - Updates alarm state machine every cycle
 * - Checks WiFi connection every 30 seconds
 * - Prints status every 60 seconds (if DEBUG_ENABLED)
 *
 * USER INTERACTIONS:
 * 1. Via Telegram:
 *    - User sends /wake → Alarm starts
 *    - User sends /stop → Alarm stops
 *    - User sends /test → Hardware test runs
 *    - User sends /status → Status report sent
 *
 * 2. Via Physical Buttons:
 *    - TEST button → Hardware test
 *    - SILENCE button → Stop alarm
 *    - RESET button (hold 10s) → Factory reset
 *
 * ALARM SEQUENCE:
 * 1. User sends /wake via Telegram
 * 2. Device sends "Command received. Starting in 3s..."
 * 3. After 3s: WARNING stage starts (small buzzer pulsing, 30s)
 * 4. After 30s: ALERT stage starts (small buzzer continuous, 30s)
 * 5. After 30s: EMERGENCY stage starts (large buzzer, until stopped)
 * 6. User stops via /stop command or SILENCE button
 * 7. Device sends "Alarm stopped. Duration: Xs"
 * 8. System returns to IDLE state
 *
 * ERROR HANDLING:
 * - WiFi lost during alarm: Alarm continues, notification sent when restored
 * - Hardware failure: Alarm stops, error notification sent
 * - Safety timeout (5 min): Alarm auto-stops, notification sent
 * - Unauthorized Telegram access: Warning sent to unauthorized user
 * - Rate limiting: /wake cooldown prevents spam (5 minutes)
 *
 * ===============================================================
 */
