/*
 * ===============================================================
 * WakeAssist - Alarm Controller Module (Implementation)
 * ===============================================================
 *
 * This file implements the alarm state machine and logic
 * declared in alarm_controller.h
 *
 * STATE MACHINE IMPLEMENTATION:
 * Each state has an update function that:
 * 1. Checks if duration exceeded
 * 2. Updates hardware (buzzers)
 * 3. Transitions to next state when ready
 *
 * ===============================================================
 */

#include "alarm_controller.h"

// ===============================================================
// GLOBAL INSTANCE
// ===============================================================

AlarmController alarmController;

// ===============================================================
// CONSTRUCTOR
// ===============================================================

AlarmController::AlarmController() {
    currentState = ALARM_IDLE;
    previousState = ALARM_IDLE;
    stageStartTime = 0;
    alarmStartTime = 0;
    telegramNotificationsEnabled = true;
    hardwareChecksEnabled = true;
    testMode = false;
    lastHardwareError = "";

    // Initialize statistics
    lastStatistics.startTime = 0;
    lastStatistics.stopTime = 0;
    lastStatistics.duration = 0;
    lastStatistics.stopSource = STOP_NONE;
    lastStatistics.maxStageReached = ALARM_IDLE;
    lastStatistics.hardwareIssueDetected = false;
}

// ===============================================================
// INITIALIZATION
// ===============================================================

bool AlarmController::begin() {
    DEBUG_PRINTLN("[Alarm] Initializing alarm controller...");

    // Verify hardware is initialized
    if (hardware.getState().smallBuzzer == HW_STATUS_UNKNOWN) {
        DEBUG_PRINTLN("[Alarm] WARNING: Hardware not initialized!");
        return false;
    }

    // Ensure all buzzers are off
    hardware.stopAllBuzzers();

    currentState = ALARM_IDLE;
    DEBUG_PRINTLN("[Alarm] Initialization complete");

    return true;
}

// ===============================================================
// ALARM CONTROL
// ===============================================================

bool AlarmController::start() {
    if (isActive()) {
        DEBUG_PRINTLN("[Alarm] Cannot start - alarm already active");
        return false;
    }

    DEBUG_PRINTLN("[Alarm] Starting alarm sequence...");

    // Reset state
    alarmStartTime = millis();
    testMode = false;

    // Transition to TRIGGERED state (3-second delay before WARNING)
    transitionToState(ALARM_TRIGGERED);

    // Send initial notification
    sendTelegramNotification(MSG_WAKE_RECEIVED);

    return true;
}

bool AlarmController::stop(AlarmStopSource source) {
    if (!isActive()) {
        DEBUG_PRINTLN("[Alarm] Cannot stop - alarm not active");
        return false;
    }

    DEBUG_PRINTF("[Alarm] Stopping alarm (source: %d)...\n", source);

    // Turn off all buzzers immediately
    hardware.stopAllBuzzers();

    // Calculate statistics
    calculateStatistics(source);

    // Determine which stopped state to enter
    AlarmState stopState;
    switch (source) {
        case STOP_SAFETY_TIMEOUT:
            stopState = ALARM_STOPPED_TIMEOUT;
            sendTelegramNotification(MSG_ALARM_TIMEOUT);
            break;

        case STOP_HARDWARE_ERROR:
            stopState = ALARM_STOPPED_ERROR;
            break;

        default:
            stopState = ALARM_STOPPED_USER;
            // Format stop message with duration and source
            char msg[128];
            const char* sourceStr = (source == STOP_TELEGRAM_COMMAND) ? "Telegram" :
                                   (source == STOP_SILENCE_BUTTON) ? "Button" : "Unknown";
            snprintf(msg, sizeof(msg), MSG_ALARM_STOPPED,
                    lastStatistics.duration, sourceStr);
            sendTelegramNotification(msg);
            break;
    }

    // Transition to stopped state
    transitionToState(stopState);

    // Return to idle
    transitionToState(ALARM_IDLE);

    return true;
}

bool AlarmController::isActive() const {
    return (currentState != ALARM_IDLE &&
            currentState != ALARM_STOPPED_USER &&
            currentState != ALARM_STOPPED_TIMEOUT &&
            currentState != ALARM_STOPPED_ERROR);
}

void AlarmController::update() {
    // Call state-specific update function
    switch (currentState) {
        case ALARM_IDLE:
            updateIdleState();
            break;

        case ALARM_TRIGGERED:
            updateTriggeredState();
            break;

        case ALARM_WARNING:
            updateWarningState();
            break;

        case ALARM_ALERT:
            updateAlertState();
            break;

        case ALARM_EMERGENCY:
            updateEmergencyState();
            break;

        default:
            // Stopped states - do nothing
            break;
    }

    // Check safety timeout (applies to all active states)
    if (isActive() && isSafetyTimeoutReached()) {
        DEBUG_PRINTLN("[Alarm] Safety timeout reached!");
        stop(STOP_SAFETY_TIMEOUT);
    }

    // Perform periodic hardware checks (if enabled)
    if (isActive() && hardwareChecksEnabled) {
        static unsigned long lastCheck = 0;
        if (millis() - lastCheck >= 10000) {  // Check every 10 seconds
            lastCheck = millis();
            if (!checkHardwareHealth()) {
                DEBUG_PRINTLN("[Alarm] Hardware check failed!");
                stop(STOP_HARDWARE_ERROR);
            }
        }
    }
}

// ===============================================================
// STATE INFORMATION
// ===============================================================

AlarmState AlarmController::getState() const {
    return currentState;
}

String AlarmController::getStateString() const {
    String result;

    switch (currentState) {
        case ALARM_IDLE:
            result = "Idle";
            break;

        case ALARM_TRIGGERED:
            result = "Triggered (starting in " +
                    String(getTimeRemainingInStage()) + "s)";
            break;

        case ALARM_WARNING:
            result = "WARNING (" +
                    String(getTimeRemainingInStage()) + "s remaining)";
            break;

        case ALARM_ALERT:
            result = "ALERT (" +
                    String(getTimeRemainingInStage()) + "s remaining)";
            break;

        case ALARM_EMERGENCY:
            result = "EMERGENCY (stop to silence)";
            break;

        case ALARM_STOPPED_USER:
            result = "Stopped by user";
            break;

        case ALARM_STOPPED_TIMEOUT:
            result = "Stopped by timeout";
            break;

        case ALARM_STOPPED_ERROR:
            result = "Stopped due to error";
            break;

        default:
            result = "Unknown";
    }

    return result;
}

unsigned long AlarmController::getTimeRemainingInStage() const {
    if (!isActive()) {
        return 0;
    }

    unsigned long elapsed = (millis() - stageStartTime) / 1000;  // Convert to seconds
    unsigned long duration = getStateDuration(currentState) / 1000;

    if (elapsed >= duration) {
        return 0;
    }

    return duration - elapsed;
}

unsigned long AlarmController::getDuration() const {
    if (alarmStartTime == 0) {
        return 0;
    }

    return (millis() - alarmStartTime) / 1000;  // Convert to seconds
}

AlarmStatistics AlarmController::getLastStatistics() const {
    return lastStatistics;
}

// ===============================================================
// CONFIGURATION
// ===============================================================

void AlarmController::setTelegramNotificationsEnabled(bool enabled) {
    telegramNotificationsEnabled = enabled;
    DEBUG_PRINTF("[Alarm] Telegram notifications: %s\n",
                enabled ? "enabled" : "disabled");
}

void AlarmController::setHardwareChecksEnabled(bool enabled) {
    hardwareChecksEnabled = enabled;
    DEBUG_PRINTF("[Alarm] Hardware checks: %s\n",
                enabled ? "enabled" : "disabled");
}

// ===============================================================
// TESTING & DIAGNOSTICS
// ===============================================================

bool AlarmController::testAlarm() {
    if (isActive()) {
        DEBUG_PRINTLN("[Alarm] Cannot test - alarm already active");
        return false;
    }

    DEBUG_PRINTLN("[Alarm] Starting test mode...");

    testMode = true;

    // Send notification
    sendTelegramNotification(MSG_TEST_START);
    delay(1000);

    // Test small buzzer
    sendTelegramNotification(MSG_TEST_SMALL);
    delay(3000);

    hardware.testBuzzer(PIN_SMALL_BUZZER, 1000);  // 1 second
    delay(2000);

    // Test large buzzer (warn user it's loud!)
    sendTelegramNotification(MSG_TEST_LARGE);
    delay(3000);

    hardware.testBuzzer(PIN_LARGE_BUZZER, 500);  // 0.5 second (shorter for loud buzzer)
    delay(1000);

    // Test complete
    sendTelegramNotification(MSG_TEST_COMPLETE);

    testMode = false;

    DEBUG_PRINTLN("[Alarm] Test complete");
    return true;
}

String AlarmController::getLastHardwareError() const {
    return lastHardwareError;
}

void AlarmController::reset() {
    DEBUG_PRINTLN("[Alarm] Resetting controller...");

    hardware.stopAllBuzzers();
    currentState = ALARM_IDLE;
    previousState = ALARM_IDLE;
    stageStartTime = 0;
    alarmStartTime = 0;
    lastHardwareError = "";

    // Clear statistics
    lastStatistics.startTime = 0;
    lastStatistics.stopTime = 0;
    lastStatistics.duration = 0;
    lastStatistics.stopSource = STOP_NONE;
    lastStatistics.maxStageReached = ALARM_IDLE;
    lastStatistics.hardwareIssueDetected = false;
}

// ===============================================================
// PRIVATE HELPER FUNCTIONS
// ===============================================================

void AlarmController::transitionToState(AlarmState newState) {
    if (newState == currentState) {
        return;  // Already in this state
    }

    DEBUG_PRINTF("[Alarm] State transition: %d -> %d\n", currentState, newState);

    previousState = currentState;
    currentState = newState;
    stageStartTime = millis();

    // Perform state entry actions
    switch (newState) {
        case ALARM_IDLE:
            hardware.stopAllBuzzers();
            hardware.setAlarmLED(false);
            break;

        case ALARM_TRIGGERED:
            hardware.setAlarmLED(true);
            break;

        case ALARM_WARNING:
            sendTelegramNotification(MSG_WARNING_STARTED);
            hardware.blinkAlarmLED(LED_BLINK_SLOW);
            break;

        case ALARM_ALERT:
            sendTelegramNotification(MSG_ALERT_STARTED);
            hardware.blinkAlarmLED(LED_BLINK_MEDIUM);
            break;

        case ALARM_EMERGENCY:
            sendTelegramNotification(MSG_EMERGENCY_STARTED);
            hardware.blinkAlarmLED(LED_BLINK_FAST);
            break;

        default:
            break;
    }
}

bool AlarmController::isStageDurationExceeded() const {
    unsigned long elapsed = millis() - stageStartTime;
    unsigned long duration = getStateDuration(currentState);

    return (elapsed >= duration);
}

bool AlarmController::isSafetyTimeoutReached() const {
    if (alarmStartTime == 0) {
        return false;
    }

    unsigned long elapsed = millis() - alarmStartTime;
    return (elapsed >= ALARM_SAFETY_TIMEOUT_MS);
}

void AlarmController::updateBuzzerOutput() {
    switch (currentState) {
        case ALARM_WARNING:
            // Pulsing pattern (handled by hardware module)
            hardware.pulseSmallBuzzer();
            break;

        case ALARM_ALERT:
            // Continuous small buzzer
            hardware.setSmallBuzzer(BUZZER_ON);
            break;

        case ALARM_EMERGENCY:
            // Large buzzer at full power
            hardware.setLargeBuzzer(BUZZER_ON);
            break;

        default:
            // No buzzer output
            break;
    }
}

bool AlarmController::checkHardwareHealth() {
    HardwareState hwState = hardware.getState();

    // Check if small buzzer failed
    if (currentState == ALARM_WARNING || currentState == ALARM_ALERT) {
        if (hwState.smallBuzzer == HW_STATUS_FAILED) {
            lastHardwareError = "Small buzzer circuit failure";
            sendTelegramNotification(MSG_ERROR_BUZZER_SMALL);
            return false;
        }
    }

    // Check if large buzzer failed
    if (currentState == ALARM_EMERGENCY) {
        if (hwState.largeBuzzer == HW_STATUS_FAILED) {
            lastHardwareError = "Large buzzer circuit failure";
            sendTelegramNotification(MSG_ERROR_BUZZER_LARGE);
            return false;
        }
    }

    // Check if both buzzers failed
    if (hwState.smallBuzzer == HW_STATUS_FAILED &&
        hwState.largeBuzzer == HW_STATUS_FAILED) {
        lastHardwareError = "Both buzzer circuits failed";
        sendTelegramNotification(MSG_ERROR_BOTH_BUZZERS);
        return false;
    }

    return true;
}

void AlarmController::sendTelegramNotification(const String& message) {
    if (!telegramNotificationsEnabled) {
        return;
    }

    if (!telegramBot.isOnline()) {
        DEBUG_PRINTLN("[Alarm] Cannot send notification - Telegram bot offline");
        return;
    }

    telegramBot.sendMessage(message);
}

// ===============================================================
// STATE UPDATE FUNCTIONS
// ===============================================================

void AlarmController::updateIdleState() {
    // Nothing to do in idle state
    // Waiting for start() to be called
}

void AlarmController::updateTriggeredState() {
    // Wait for trigger delay to expire
    if (isStageDurationExceeded()) {
        // Transition to WARNING stage
        transitionToState(ALARM_WARNING);
    }
}

void AlarmController::updateWarningState() {
    // Update buzzer (pulsing pattern)
    updateBuzzerOutput();

    // Check if WARNING stage duration exceeded
    if (isStageDurationExceeded()) {
        // Escalate to ALERT stage
        transitionToState(ALARM_ALERT);
    }
}

void AlarmController::updateAlertState() {
    // Update buzzer (continuous)
    updateBuzzerOutput();

    // Check if ALERT stage duration exceeded
    if (isStageDurationExceeded()) {
        // Escalate to EMERGENCY stage
        transitionToState(ALARM_EMERGENCY);
    }
}

void AlarmController::updateEmergencyState() {
    // Update buzzer (large buzzer)
    updateBuzzerOutput();

    // EMERGENCY stage runs until:
    // 1. User stops it manually, OR
    // 2. Safety timeout is reached (checked in update())
    //
    // No automatic transition from EMERGENCY
}

unsigned long AlarmController::getStateDuration(AlarmState state) const {
    switch (state) {
        case ALARM_TRIGGERED:
            return ALARM_TRIGGERED_DELAY_MS;

        case ALARM_WARNING:
            return ALARM_WARNING_DURATION_MS;

        case ALARM_ALERT:
            return ALARM_ALERT_DURATION_MS;

        case ALARM_EMERGENCY:
            // No fixed duration - runs until stopped
            return UINT32_MAX;

        default:
            return 0;
    }
}

void AlarmController::calculateStatistics(AlarmStopSource source) {
    lastStatistics.startTime = alarmStartTime;
    lastStatistics.stopTime = millis();
    lastStatistics.duration = (lastStatistics.stopTime - lastStatistics.startTime) / 1000;
    lastStatistics.stopSource = source;
    lastStatistics.maxStageReached = currentState;
    lastStatistics.hardwareIssueDetected = (source == STOP_HARDWARE_ERROR);

    DEBUG_PRINTLN("[Alarm] === Alarm Statistics ===");
    DEBUG_PRINTF("[Alarm] Duration: %lu seconds\n", lastStatistics.duration);
    DEBUG_PRINTF("[Alarm] Max stage: %d\n", lastStatistics.maxStageReached);
    DEBUG_PRINTF("[Alarm] Stop source: %d\n", lastStatistics.stopSource);
    DEBUG_PRINTLN("[Alarm] =======================");
}

/*
 * ===============================================================
 * IMPLEMENTATION NOTES:
 * ===============================================================
 *
 * STATE MACHINE DESIGN:
 * This implementation uses a simple switch-based state machine.
 * Each state has:
 * - An update function (called every loop iteration)
 * - Entry actions (performed in transitionToState())
 * - Duration check (isStageDurationExceeded())
 *
 * Alternative approaches considered:
 * - Function pointers: More complex, harder to debug
 * - Hierarchical state machine: Overkill for 5 states
 * - Event-driven: Requires message queue, more memory
 *
 * ===============================================================
 *
 * TIMING CONSIDERATIONS:
 * All timing uses millis() which:
 * - Overflows after ~49 days (acceptable for alarm device)
 * - Has 1ms resolution (sufficient for this application)
 * - Is non-blocking (unlike delay())
 *
 * IMPORTANT: If device runs for >49 days, millis() overflow
 * could cause timing issues. Solution: Reboot monthly.
 *
 * ===============================================================
 *
 * HARDWARE FAILURE HANDLING:
 * The controller checks hardware health periodically:
 * - WARNING/ALERT: Check small buzzer every 10s
 * - EMERGENCY: Check large buzzer every 10s
 *
 * If hardware fails:
 * 1. Send Telegram notification
 * 2. Stop alarm (can't wake user without buzzer!)
 * 3. Log error for debugging
 *
 * User should run /test weekly to catch hardware issues early.
 *
 * ===============================================================
 *
 * ESCALATION PHILOSOPHY:
 * The three-stage approach balances effectiveness with comfort:
 *
 * WARNING (30s): Gentle pulsing gives light sleepers a chance
 * ALERT (30s): Continuous sound for medium sleepers
 * EMERGENCY: Maximum volume for deep sleepers
 *
 * Total ramp-up time: 63 seconds
 * This is intentionally gradual - jarring awakenings are
 * unpleasant and can cause sleep inertia.
 *
 * User feedback can adjust timings in config.h
 *
 * ===============================================================
 */
