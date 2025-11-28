/*
 * ===============================================================
 * WakeAssist - Alarm Controller Module (Header File)
 * ===============================================================
 *
 * This module manages the alarm state machine and escalation logic:
 * - Three-stage alarm: WARNING → ALERT → EMERGENCY
 * - Automatic escalation based on time
 * - Safety timeout to prevent running forever
 * - Integration with hardware (buzzers) and Telegram (notifications)
 *
 * STATE MACHINE FLOW:
 * IDLE → TRIGGERED (3s delay) → WARNING (30s, pulsing small buzzer)
 *   → ALERT (30s, continuous small buzzer) → EMERGENCY (large buzzer)
 *
 * WHY THREE STAGES?
 * Gradual escalation gives the user multiple chances to wake up
 * without jumping straight to maximum volume.
 *
 * ===============================================================
 */

#ifndef ALARM_CONTROLLER_H
#define ALARM_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "hardware.h"
#include "telegram_bot.h"

// ===============================================================
// ALARM STATE ENUMERATION
// ===============================================================
// Represents the current alarm state

enum AlarmState {
    ALARM_IDLE,              // No alarm active
    ALARM_TRIGGERED,         // Alarm triggered, waiting 3s before starting
    ALARM_WARNING,           // Stage 1: Small buzzer pulsing (30s)
    ALARM_ALERT,             // Stage 2: Small buzzer continuous (30s)
    ALARM_EMERGENCY,         // Stage 3: Large buzzer (until stopped)
    ALARM_STOPPED_USER,      // Stopped by user (silence button or Telegram)
    ALARM_STOPPED_TIMEOUT,   // Stopped by safety timeout (5 minutes)
    ALARM_STOPPED_ERROR      // Stopped due to hardware error
};

// ===============================================================
// ALARM STOP SOURCE ENUMERATION
// ===============================================================
// Tracks how the alarm was stopped (for logging and notifications)

enum AlarmStopSource {
    STOP_NONE,                    // Not stopped
    STOP_TELEGRAM_COMMAND,        // User sent /stop via Telegram
    STOP_SILENCE_BUTTON,          // Physical silence button pressed
    STOP_SAFETY_TIMEOUT,          // 5-minute safety timeout expired
    STOP_HARDWARE_ERROR,          // Hardware failure detected
    STOP_COMPLETED                // Alarm ran through all stages naturally
};

// ===============================================================
// ALARM STATISTICS STRUCTURE
// ===============================================================
// Stores information about the alarm session for reporting

struct AlarmStatistics {
    unsigned long startTime;         // When alarm was triggered (millis)
    unsigned long stopTime;          // When alarm was stopped (millis)
    unsigned long duration;          // Total duration in seconds
    AlarmStopSource stopSource;      // How it was stopped
    AlarmState maxStageReached;      // Highest escalation stage reached
    bool hardwareIssueDetected;      // Was there a hardware problem?
};

// ===============================================================
// ALARM CONTROLLER CLASS
// ===============================================================
// Manages the alarm state machine and all related logic

class AlarmController {
public:
    // ---------------------------------------------------------------
    // CONSTRUCTOR
    // ---------------------------------------------------------------
    AlarmController();

    // ---------------------------------------------------------------
    // INITIALIZATION
    // ---------------------------------------------------------------
    // Initialize alarm controller
    // MUST be called once in setup() after hardware.begin()
    //
    // RETURNS: true if initialization successful
    bool begin();

    // ---------------------------------------------------------------
    // ALARM CONTROL
    // ---------------------------------------------------------------

    // Start alarm sequence
    // This initiates the state machine: IDLE → TRIGGERED
    // Actual alarm starts after 3-second delay
    //
    // RETURNS: true if started, false if already running
    bool start();

    // Stop alarm immediately
    // source: How the alarm was stopped (for statistics)
    //
    // RETURNS: true if stopped, false if not running
    bool stop(AlarmStopSource source);

    // Check if alarm is currently active (any state except IDLE)
    // RETURNS: true if alarm running
    bool isActive() const;

    // Update alarm state machine
    // MUST be called frequently in loop()
    //
    // This handles:
    // - Stage transitions (WARNING → ALERT → EMERGENCY)
    // - Buzzer patterns
    // - Safety timeout
    // - Hardware monitoring
    void update();

    // ---------------------------------------------------------------
    // STATE INFORMATION
    // ---------------------------------------------------------------

    // Get current alarm state
    // RETURNS: AlarmState enum value
    AlarmState getState() const;

    // Get human-readable state string
    // RETURNS: String like "WARNING (15s remaining)"
    String getStateString() const;

    // Get time remaining in current stage (seconds)
    // RETURNS: Seconds until next stage, 0 if not applicable
    unsigned long getTimeRemainingInStage() const;

    // Get total alarm duration so far (seconds)
    // RETURNS: Seconds since alarm started, 0 if not active
    unsigned long getDuration() const;

    // Get alarm statistics from last session
    // RETURNS: AlarmStatistics structure with session data
    AlarmStatistics getLastStatistics() const;

    // ---------------------------------------------------------------
    // CONFIGURATION
    // ---------------------------------------------------------------

    // Enable/disable Telegram notifications
    // When enabled, sends updates at each stage transition
    //
    // enabled: true to enable, false to disable
    void setTelegramNotificationsEnabled(bool enabled);

    // Enable/disable hardware health checks during alarm
    // When enabled, checks buzzers periodically during alarm
    //
    // enabled: true to enable, false to disable
    void setHardwareChecksEnabled(bool enabled);

    // ---------------------------------------------------------------
    // TESTING & DIAGNOSTICS
    // ---------------------------------------------------------------

    // Test alarm system (gentle test, small buzzer only)
    // Runs for 3 seconds, doesn't trigger full alarm
    // Used for /test command
    //
    // RETURNS: true if test started
    bool testAlarm();

    // Get last hardware error message (if any)
    // RETURNS: Error description, empty if no error
    String getLastHardwareError() const;

    // Reset alarm controller to initial state
    // Clears all statistics and errors
    void reset();

private:
    // ---------------------------------------------------------------
    // PRIVATE MEMBER VARIABLES
    // ---------------------------------------------------------------

    AlarmState currentState;          // Current state in state machine
    AlarmState previousState;         // Previous state (for transition detection)

    unsigned long stageStartTime;     // When current stage started (millis)
    unsigned long alarmStartTime;     // When alarm was triggered (millis)

    bool telegramNotificationsEnabled; // Send Telegram updates?
    bool hardwareChecksEnabled;        // Check hardware during alarm?

    AlarmStatistics lastStatistics;    // Stats from last alarm session
    String lastHardwareError;          // Last error message

    bool testMode;                     // Is this a test run?

    // ---------------------------------------------------------------
    // PRIVATE HELPER FUNCTIONS
    // ---------------------------------------------------------------

    // State machine transition handler
    // Changes state and performs necessary actions
    void transitionToState(AlarmState newState);

    // Check if current stage has exceeded its duration
    // RETURNS: true if stage time expired
    bool isStageDurationExceeded() const;

    // Check if safety timeout (5 minutes) has been reached
    // RETURNS: true if alarm should be stopped for safety
    bool isSafetyTimeoutReached() const;

    // Update buzzer output based on current state
    void updateBuzzerOutput();

    // Perform hardware health check
    // RETURNS: true if hardware OK, false if issue detected
    bool checkHardwareHealth();

    // Send notification via Telegram (if enabled)
    // message: Text to send
    void sendTelegramNotification(const String& message);

    // Handle state-specific update logic
    void updateIdleState();
    void updateTriggeredState();
    void updateWarningState();
    void updateAlertState();
    void updateEmergencyState();

    // Get duration of current state (for configuration lookup)
    // RETURNS: Duration in milliseconds
    unsigned long getStateDuration(AlarmState state) const;

    // Calculate alarm statistics when stopping
    void calculateStatistics(AlarmStopSource source);
};

// ===============================================================
// GLOBAL ALARM CONTROLLER INSTANCE
// ===============================================================
// Create a single alarm controller instance
//
// USAGE IN OTHER FILES:
//   extern AlarmController alarmController;
//   alarmController.start();

extern AlarmController alarmController;

#endif // ALARM_CONTROLLER_H

/*
 * ===============================================================
 * USAGE EXAMPLE:
 * ===============================================================
 *
 * // In main.cpp:
 * #include "alarm_controller.h"
 *
 * void setup() {
 *     // Initialize hardware first
 *     hardware.begin();
 *
 *     // Initialize alarm controller
 *     alarmController.begin();
 *
 *     // Configure notifications
 *     alarmController.setTelegramNotificationsEnabled(true);
 *     alarmController.setHardwareChecksEnabled(true);
 *
 *     // Register Telegram command handlers
 *     telegramBot.onCommand("/wake", [](TelegramMessage msg) {
 *         if (!alarmController.isActive()) {
 *             alarmController.start();
 *         } else {
 *             telegramBot.sendMessage("⚠️ Alarm already running!");
 *         }
 *     });
 *
 *     telegramBot.onCommand("/stop", [](TelegramMessage msg) {
 *         if (alarmController.isActive()) {
 *             alarmController.stop(STOP_TELEGRAM_COMMAND);
 *             telegramBot.sendMessage("✅ Alarm stopped");
 *         }
 *     });
 *
 *     telegramBot.onCommand("/test", [](TelegramMessage msg) {
 *         alarmController.testAlarm();
 *     });
 * }
 *
 * void loop() {
 *     // Update alarm state machine (CRITICAL - must be in loop!)
 *     alarmController.update();
 *
 *     // Check physical silence button
 *     hardware.updateButtons();
 *     if (hardware.isSilenceButtonPressed() && alarmController.isActive()) {
 *         alarmController.stop(STOP_SILENCE_BUTTON);
 *     }
 * }
 *
 * ===============================================================
 * STATE MACHINE DIAGRAM:
 * ===============================================================
 *
 *                    ┌──────────┐
 *                    │   IDLE   │ <──────────────────┐
 *                    └────┬─────┘                    │
 *                         │ start()                  │
 *                         ▼                          │
 *                  ┌──────────────┐                  │
 *                  │  TRIGGERED   │                  │
 *                  │  (3s delay)  │                  │
 *                  └──────┬───────┘                  │
 *                         │ 3s elapsed               │
 *                         ▼                          │
 *                  ┌──────────────┐                  │
 *                  │   WARNING    │                  │
 *                  │ (pulse 30s)  │                  │
 *                  └──────┬───────┘                  │
 *                         │ 30s elapsed              │
 *                         ▼                          │
 *                  ┌──────────────┐                  │
 *                  │    ALERT     │                  │
 *                  │(continuous   │                  │
 *                  │    30s)      │                  │
 *                  └──────┬───────┘                  │
 *                         │ 30s elapsed              │
 *                         ▼                          │
 *                  ┌──────────────┐                  │
 *                  │  EMERGENCY   │                  │
 *                  │ (large buzz) │                  │
 *                  │  (until stop)│                  │
 *                  └──────┬───────┘                  │
 *                         │                          │
 *                         │ stop() OR 5min timeout   │
 *                         └──────────────────────────┘
 *
 * ===============================================================
 * TIMING SUMMARY (from config.h):
 * ===============================================================
 *
 * TRIGGERED:  3 seconds    (ALARM_TRIGGERED_DELAY_MS)
 * WARNING:    30 seconds   (ALARM_WARNING_DURATION_MS)
 * ALERT:      30 seconds   (ALARM_ALERT_DURATION_MS)
 * EMERGENCY:  Until stopped (max 5 minutes - ALARM_SAFETY_TIMEOUT_MS)
 *
 * Total minimum duration: 63 seconds
 * Total maximum duration: 5 minutes (safety timeout)
 *
 * ===============================================================
 */
