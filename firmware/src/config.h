/*
 * ===============================================================
 * WakeAssist - Configuration File
 * ===============================================================
 *
 * This file contains ALL hardware pin assignments and constants
 * for the WakeAssist project.
 *
 * IMPORTANT: Only modify values in this file, not in other code!
 * This makes it easy to adapt the project to different hardware.
 *
 * ===============================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

// ===============================================================
// HARDWARE VERSION
// ===============================================================
// Change this if you make hardware revisions
#define HARDWARE_VERSION "1.0"
#define SOFTWARE_VERSION "1.0.0"

// ===============================================================
// GPIO PIN ASSIGNMENTS
// ===============================================================
//
// IMPORTANT: These pin numbers refer to GPIO numbers, NOT physical pins!
// See ESP32 pinout diagram: https://lastminuteengineers.com/esp32-pinout-reference/
//
// PINS TO AVOID:
// - GPIO 6-11: Connected to flash chip (will brick ESP32!)
// - GPIO 34-39: Input only (can't be used for outputs)
// - GPIO 0, 2, 15: Strapping pins (can cause boot issues)
//
// ===============================================================

// ---------------------------------------------------------------
// BUZZER CONTROL PINS (connected to MOSFET gates)
// ---------------------------------------------------------------
// These pins control the MOSFETs that switch the 12V buzzers
// They need to be PWM-capable for pulsing patterns

#define PIN_SMALL_BUZZER    25    // Small buzzer (WARNING/ALERT stages)
                                  // GPIO 25: PWM capable, safe for output

#define PIN_LARGE_BUZZER    26    // Large buzzer (EMERGENCY stage)
                                  // GPIO 26: PWM capable, safe for output

// ---------------------------------------------------------------
// LED INDICATOR PINS
// ---------------------------------------------------------------
// Status LEDs to show WiFi state, alarm state, and system health
// Each LED should have a 220Ω resistor in series

#define PIN_LED_WIFI        16    // Blue/Red LED: WiFi status
                                  // Blue solid = connected, Red blink = error

#define PIN_LED_ALARM       17    // Yellow/Orange/Red LED: Alarm state
                                  // Color changes based on alarm stage

#define PIN_LED_STATUS      18    // Green/Red LED: System health
                                  // Green = OK, Red = hardware error

// ---------------------------------------------------------------
// BUTTON INPUT PINS
// ---------------------------------------------------------------
// Push buttons for manual control (with internal pullup resistors)
// Buttons connect between GPIO pin and GND

#define PIN_BUTTON_TEST     21    // TEST button: Try alarm (gentle only)
                                  // GPIO 21: Has internal pullup

#define PIN_BUTTON_SILENCE  22    // SILENCE button: Stop active alarm
                                  // GPIO 22: Has internal pullup

#define PIN_BUTTON_RESET    23    // RESET button: Hold 10s to factory reset
                                  // GPIO 23: Has internal pullup

// ===============================================================
// BUTTON CONFIGURATION
// ===============================================================

// Button debounce time in milliseconds
// Prevents multiple triggers from a single press
#define BUTTON_DEBOUNCE_MS  50

// How long to hold RESET button for factory reset (milliseconds)
#define RESET_HOLD_TIME_MS  10000   // 10 seconds

// Button state (with internal pullup, pressed = LOW)
#define BUTTON_PRESSED      LOW
#define BUTTON_RELEASED     HIGH

// ===============================================================
// LED BLINK PATTERNS
// ===============================================================
// Define blink speeds for different LED states (milliseconds)

#define LED_BLINK_SLOW      1000    // Slow blink: 1 second on/off
#define LED_BLINK_MEDIUM    500     // Medium blink: 0.5 second on/off
#define LED_BLINK_FAST      200     // Fast blink: 0.2 second on/off

// ===============================================================
// ALARM TIMING CONFIGURATION
// ===============================================================
// How long each alarm stage lasts before escalating

#define ALARM_WARNING_DURATION_MS   30000   // WARNING stage: 30 seconds
#define ALARM_ALERT_DURATION_MS     30000   // ALERT stage: 30 seconds
// EMERGENCY stage runs until stopped or timeout

#define ALARM_SAFETY_TIMEOUT_MS     300000  // Safety timeout: 5 minutes (300 seconds)
                                            // Auto-stop after this time to prevent
                                            // running forever if user unresponsive

#define ALARM_TRIGGERED_DELAY_MS    3000    // Delay before alarm starts: 3 seconds
                                            // Gives user time to prepare after /wake

// ===============================================================
// BUZZER PWM CONFIGURATION
// ===============================================================
// PWM settings for buzzer control (pulsing patterns)

#define BUZZER_PWM_CHANNEL_SMALL    0       // PWM channel for small buzzer
#define BUZZER_PWM_CHANNEL_LARGE    1       // PWM channel for large buzzer
#define BUZZER_PWM_FREQUENCY        1000    // PWM frequency: 1kHz (doesn't affect active buzzer tone)
#define BUZZER_PWM_RESOLUTION       8       // 8-bit resolution (0-255 duty cycle)

// PWM duty cycles (0-255, where 255 = 100% on)
#define BUZZER_OFF                  0       // Buzzer completely off
#define BUZZER_ON                   255     // Buzzer fully on

// Pulsing pattern for WARNING stage (milliseconds)
#define BUZZER_PULSE_ON_MS          500     // 0.5 seconds on
#define BUZZER_PULSE_OFF_MS         500     // 0.5 seconds off

// ===============================================================
// WIFI CONFIGURATION
// ===============================================================

// WiFi Manager timeout (milliseconds)
// How long to wait in AP mode before giving up
#define WIFI_AP_TIMEOUT_MS          300000  // 5 minutes

// WiFi connection timeout (milliseconds)
// How long to wait for connection before considering it failed
#define WIFI_CONNECT_TIMEOUT_MS     10000   // 10 seconds

// WiFi reconnection attempts before falling back to AP mode
#define WIFI_MAX_RECONNECT_ATTEMPTS 3

// WiFi check interval (milliseconds)
// How often to check if WiFi is still connected
#define WIFI_CHECK_INTERVAL_MS      30000   // 30 seconds

// Access Point (AP) mode configuration
// This creates the "WakeAssist-XXXX" WiFi network for setup
#define WIFI_AP_SSID_PREFIX         "WakeAssist"  // Will be: WakeAssist-1234
#define WIFI_AP_PASSWORD            ""            // Empty = no password (easier for setup)
#define WIFI_AP_CHANNEL             1             // WiFi channel (1-13)

// ===============================================================
// TELEGRAM BOT CONFIGURATION
// ===============================================================

// How often to poll Telegram API for new messages (milliseconds)
#define TELEGRAM_POLL_INTERVAL_MS   5000    // 5 seconds (don't make this too fast!)

// Telegram API timeout (milliseconds)
// How long to wait for Telegram API to respond
#define TELEGRAM_API_TIMEOUT_MS     10000   // 10 seconds

// HTTP request timeout for Telegram
#define TELEGRAM_HTTP_TIMEOUT_MS    10000   // 10 seconds

// Rate limiting: minimum time between /wake commands (milliseconds)
#define TELEGRAM_WAKE_COOLDOWN_MS   300000  // 5 minutes (prevents spam)

// ===============================================================
// HARDWARE VERIFICATION
// ===============================================================

// How often to send test reminders (milliseconds)
#define TEST_REMINDER_INTERVAL_MS   604800000   // 7 days (1 week)

// GPIO check delay (microseconds)
// Brief delay when checking if GPIO pin is HIGH after setting it
#define GPIO_CHECK_DELAY_US         10

// ===============================================================
// PERSISTENT STORAGE KEYS
// ===============================================================
// Keys used to store data in ESP32 flash memory (Preferences library)
// These must be unique strings

#define STORAGE_NAMESPACE           "wakeassist"    // Namespace for all stored data

// Individual storage keys (max 15 characters each)
#define KEY_WIFI_SSID              "wifi_ssid"
#define KEY_WIFI_PASSWORD          "wifi_pass"
#define KEY_TELEGRAM_TOKEN         "tg_token"
#define KEY_TELEGRAM_USER_ID       "tg_user_id"
#define KEY_LAST_TEST_TIME         "last_test"
#define KEY_SETUP_COMPLETE         "setup_done"

// ===============================================================
// SERIAL DEBUG CONFIGURATION
// ===============================================================

// Serial baud rate (must match platformio.ini)
#define SERIAL_BAUD_RATE            115200

// Enable/disable debug messages
// Set to 1 to see detailed debug output, 0 to disable
#define DEBUG_ENABLED               1

// Debug macro - only prints if DEBUG_ENABLED is 1
#if DEBUG_ENABLED
  #define DEBUG_PRINT(x)        Serial.print(x)
  #define DEBUG_PRINTLN(x)      Serial.println(x)
  #define DEBUG_PRINTF(...)     Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// ===============================================================
// HARDWARE CONSTANTS
// ===============================================================

// Power supply voltages (for validation/monitoring)
#define VOLTAGE_12V                 12.0    // Expected 12V rail voltage
#define VOLTAGE_5V                  5.0     // Expected 5V rail voltage

// Buzzer current limits (mA) - for future current sensing
#define BUZZER_SMALL_CURRENT_MIN    5       // Minimum expected current
#define BUZZER_SMALL_CURRENT_MAX    40      // Maximum expected current
#define BUZZER_LARGE_CURRENT_MIN    20
#define BUZZER_LARGE_CURRENT_MAX    120

// ===============================================================
// SYSTEM BEHAVIOR
// ===============================================================

// Watchdog timer timeout (milliseconds)
// Automatically restart ESP32 if it freezes for this long
#define WATCHDOG_TIMEOUT_MS         60000   // 60 seconds

// Status report interval (milliseconds)
// How often to print system status to serial monitor
#define STATUS_REPORT_INTERVAL_MS   60000   // 1 minute

// ===============================================================
// TELEGRAM MESSAGES (Templates)
// ===============================================================
// Pre-defined messages sent to user via Telegram

// Success messages
#define MSG_WAKE_RECEIVED          "✅ Command received. Starting alarm in 3s..."
#define MSG_WARNING_STARTED        "⏰ WARNING stage started - small buzzer pulsing"
#define MSG_ALERT_STARTED          "🔔 ALERT stage - small buzzer continuous"
#define MSG_EMERGENCY_STARTED      "🚨 EMERGENCY - LARGE BUZZER ACTIVATED!"
#define MSG_ALARM_STOPPED          "✅ Alarm stopped. Duration: %lus. Source: %s"
#define MSG_ALARM_TIMEOUT          "⏰ Alarm auto-stopped after 5 minutes (safety)"

// Error messages
#define MSG_ERROR_BUZZER_SMALL     "⚠️ Small buzzer circuit issue - using large buzzer only"
#define MSG_ERROR_BUZZER_LARGE     "❌ CRITICAL: Large buzzer not responding! Check device"
#define MSG_ERROR_BOTH_BUZZERS     "❌ CRITICAL: No buzzers responding! Device may not work!"
#define MSG_ERROR_WIFI_LOST        "⚠️ WiFi lost - alarm continuing offline"
#define MSG_ERROR_WIFI_RESTORED    "✅ WiFi reconnected - alarm at %s stage"

// Test messages
#define MSG_TEST_START             "🧪 Testing buzzers..."
#define MSG_TEST_SMALL             "Small buzzer test in 3... 2... 1..."
#define MSG_TEST_LARGE             "Large buzzer test (LOUD!) in 3... 2... 1..."
#define MSG_TEST_COMPLETE          "✅ Test complete! Both buzzers working."

// Status messages
#define MSG_DEVICE_ONLINE          "🟢 WakeAssist connected! Send /wake to test."
#define MSG_RATE_LIMITED           "⏰ Please wait %d more seconds before next /wake"
#define MSG_TEST_REMINDER          "⏰ Weekly reminder: Run /test to verify your device works. Last test: %d days ago"

// ===============================================================
// VALIDATION MACROS
// ===============================================================
// These check if configuration values are valid at compile time

// Check if pin numbers are valid (compile-time check)
#if PIN_SMALL_BUZZER >= 40 || PIN_SMALL_BUZZER < 0
  #error "PIN_SMALL_BUZZER must be between 0 and 39"
#endif

#if PIN_LARGE_BUZZER >= 40 || PIN_LARGE_BUZZER < 0
  #error "PIN_LARGE_BUZZER must be between 0 and 39"
#endif

// Check if dangerous pins are being used
#if defined(PIN_SMALL_BUZZER) && (PIN_SMALL_BUZZER >= 6 && PIN_SMALL_BUZZER <= 11)
  #error "PIN_SMALL_BUZZER cannot be GPIO 6-11 (flash pins - will brick ESP32!)"
#endif

#if defined(PIN_LARGE_BUZZER) && (PIN_LARGE_BUZZER >= 6 && PIN_LARGE_BUZZER <= 11)
  #error "PIN_LARGE_BUZZER cannot be GPIO 6-11 (flash pins - will brick ESP32!)"
#endif

// ===============================================================
// END OF CONFIGURATION
// ===============================================================

#endif // CONFIG_H

/*
 * ===============================================================
 * NOTES FOR BEGINNERS:
 * ===============================================================
 *
 * 1. PIN NUMBERS:
 *    - These are GPIO numbers, NOT physical pin numbers
 *    - Always refer to ESP32 pinout diagram when wiring
 *
 * 2. MODIFYING PINS:
 *    - If you need to change pin assignments, ONLY edit this file
 *    - Never use GPIO 6-11 (will permanently damage ESP32!)
 *    - Never use GPIO 34-39 for outputs (they're input-only)
 *
 * 3. TIMING VALUES:
 *    - All timing constants end in _MS (milliseconds) or _US (microseconds)
 *    - 1000 milliseconds = 1 second
 *    - Adjust durations to suit your needs
 *
 * 4. DEBUG OUTPUT:
 *    - Set DEBUG_ENABLED to 1 to see detailed messages
 *    - Set to 0 before final deployment to save memory
 *    - View debug output in Serial Monitor at 115200 baud
 *
 * 5. COMPILATION ERRORS:
 *    - If you see "#error" messages during compilation,
 *      it means your pin configuration is invalid
 *    - Read the error message and fix the pin numbers
 *
 * ===============================================================
 */
