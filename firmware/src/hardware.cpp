/*
 * ===============================================================
 * WakeAssist - Hardware Control Module (Implementation)
 * ===============================================================
 *
 * This file implements all the hardware control functions declared
 * in hardware.h
 *
 * See hardware.h for function descriptions and usage
 *
 * ===============================================================
 */

#include "hardware.h"

// Create global hardware instance
Hardware hardware;

// ===============================================================
// CONSTRUCTOR
// ===============================================================
// Initializes all member variables to safe default values
//
// This runs automatically when Hardware object is created

Hardware::Hardware() {
    // Initialize hardware status to unknown
    state.smallBuzzer = HW_STATUS_UNKNOWN;
    state.largeBuzzer = HW_STATUS_UNKNOWN;

    // Initialize button states to not pressed
    state.buttonTest = false;
    state.buttonSilence = false;
    state.buttonReset = false;
    state.resetButtonPressTime = 0;

    // Enable LEDs by default
    state.ledsEnabled = true;

    // Initialize LED states (all off, not blinking)
    wifiLED = {false, false, 0, 0};
    alarmLED = {false, false, 0, 0};
    statusLED = {false, false, 0, 0};

    // Initialize button states (not pressed, stable)
    testButton = {false, false, 0};
    silenceButton = {false, false, 0};
    resetButton = {false, false, 0};

    // Initialize pulse state (not pulsing)
    lastPulseToggle = 0;
    pulseState = false;
    pulseStartTime = 0;
}

// ===============================================================
// INITIALIZATION
// ===============================================================
// Sets up all GPIO pins and PWM channels
//
// MUST be called once in setup() before using any hardware functions

bool Hardware::begin() {
    DEBUG_PRINTLN("=== Hardware Initialization ===");

    // ---------------------------------------------------------------
    // Configure Buzzer Pins (Output)
    // ---------------------------------------------------------------
    // These pins control the MOSFET gates
    // Set to OUTPUT mode so we can control them

    pinMode(PIN_SMALL_BUZZER, OUTPUT);
    pinMode(PIN_LARGE_BUZZER, OUTPUT);

    // Start with buzzers off (safe default)
    digitalWrite(PIN_SMALL_BUZZER, LOW);
    digitalWrite(PIN_LARGE_BUZZER, LOW);

    DEBUG_PRINTLN("✓ Buzzer pins configured");

    // ---------------------------------------------------------------
    // Configure LED Pins (Output)
    // ---------------------------------------------------------------
    // Set all LED pins to OUTPUT mode

    pinMode(PIN_LED_WIFI, OUTPUT);
    pinMode(PIN_LED_ALARM, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);

    // Start with all LEDs off
    digitalWrite(PIN_LED_WIFI, LOW);
    digitalWrite(PIN_LED_ALARM, LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    DEBUG_PRINTLN("✓ LED pins configured");

    // ---------------------------------------------------------------
    // Configure Button Pins (Input with Pullup)
    // ---------------------------------------------------------------
    // INPUT_PULLUP enables internal pullup resistor
    // Button connects GPIO to GND, so pressed = LOW

    pinMode(PIN_BUTTON_TEST, INPUT_PULLUP);
    pinMode(PIN_BUTTON_SILENCE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_RESET, INPUT_PULLUP);

    DEBUG_PRINTLN("✓ Button pins configured (pullup enabled)");

    // ---------------------------------------------------------------
    // Setup PWM Channels for Buzzers
    // ---------------------------------------------------------------
    // ESP32 has 16 PWM channels (0-15)
    // We'll use channels 0 and 1 for our two buzzers

    setupPWM();

    DEBUG_PRINTLN("✓ PWM channels configured");

    // ---------------------------------------------------------------
    // Perform Initial Hardware Check
    // ---------------------------------------------------------------
    // Test if buzzer circuits respond to GPIO signals
    // This catches major wiring errors early

    DEBUG_PRINTLN("Performing hardware self-test...");

    if (!checkBuzzerCircuits()) {
        DEBUG_PRINTLN("⚠ Warning: Buzzer circuit check failed");
        // Don't return false - device can still function
        // User will be notified via Telegram
    } else {
        DEBUG_PRINTLN("✓ Hardware self-test passed");
    }

    DEBUG_PRINTLN("=== Hardware initialization complete ===\n");

    return true;  // Initialization successful
}

// ===============================================================
// PWM SETUP (PRIVATE HELPER)
// ===============================================================
// Configures ESP32 hardware PWM for buzzer control
//
// ESP32 PWM explanation:
// - ESP32 has 16 independent PWM channels
// - Each channel can output a PWM signal on any GPIO pin
// - We use PWM to create pulsing patterns (on/off rhythms)

void Hardware::setupPWM() {
    // ---------------------------------------------------------------
    // Configure Small Buzzer PWM Channel
    // ---------------------------------------------------------------
    // Parameters:
    // - Channel number (0-15)
    // - Frequency in Hz (1000 = 1kHz)
    // - Resolution in bits (8-bit = 0-255 duty cycle values)

    ledcSetup(BUZZER_PWM_CHANNEL_SMALL, BUZZER_PWM_FREQUENCY, BUZZER_PWM_RESOLUTION);

    // Attach the PWM channel to physical GPIO pin
    ledcAttachPin(PIN_SMALL_BUZZER, BUZZER_PWM_CHANNEL_SMALL);

    // Start with duty cycle = 0 (off)
    ledcWrite(BUZZER_PWM_CHANNEL_SMALL, 0);

    DEBUG_PRINTF("  Small buzzer PWM: Channel %d, Pin %d\n",
                 BUZZER_PWM_CHANNEL_SMALL, PIN_SMALL_BUZZER);

    // ---------------------------------------------------------------
    // Configure Large Buzzer PWM Channel
    // ---------------------------------------------------------------

    ledcSetup(BUZZER_PWM_CHANNEL_LARGE, BUZZER_PWM_FREQUENCY, BUZZER_PWM_RESOLUTION);
    ledcAttachPin(PIN_LARGE_BUZZER, BUZZER_PWM_CHANNEL_LARGE);
    ledcWrite(BUZZER_PWM_CHANNEL_LARGE, 0);

    DEBUG_PRINTF("  Large buzzer PWM: Channel %d, Pin %d\n",
                 BUZZER_PWM_CHANNEL_LARGE, PIN_LARGE_BUZZER);
}

// ===============================================================
// BUZZER CONTROL FUNCTIONS
// ===============================================================

// ---------------------------------------------------------------
// Set Small Buzzer Power Level
// ---------------------------------------------------------------
// dutyCycle: 0 = off, 255 = full power
//
// NOTE: For active buzzers, intermediate values (1-254) may not
// change volume much, since the buzzer has a built-in oscillator.
// Use 0 (off) or 255 (on) for best results.

void Hardware::setSmallBuzzer(uint8_t dutyCycle) {
    // Write PWM duty cycle to channel
    ledcWrite(BUZZER_PWM_CHANNEL_SMALL, dutyCycle);

    // Debug output (only on state changes to avoid spam)
    static uint8_t lastDutyCycle = 255;  // Track last value
    if (dutyCycle != lastDutyCycle) {
        DEBUG_PRINTF("Small buzzer: %s (duty=%d)\n",
                     dutyCycle > 0 ? "ON" : "OFF", dutyCycle);
        lastDutyCycle = dutyCycle;
    }
}

// ---------------------------------------------------------------
// Set Large Buzzer Power Level
// ---------------------------------------------------------------

void Hardware::setLargeBuzzer(uint8_t dutyCycle) {
    ledcWrite(BUZZER_PWM_CHANNEL_LARGE, dutyCycle);

    static uint8_t lastDutyCycle = 255;
    if (dutyCycle != lastDutyCycle) {
        DEBUG_PRINTF("Large buzzer: %s (duty=%d)\n",
                     dutyCycle > 0 ? "ON" : "OFF", dutyCycle);
        lastDutyCycle = dutyCycle;
    }
}

// ---------------------------------------------------------------
// Stop All Buzzers Immediately
// ---------------------------------------------------------------
// Emergency stop function - turns off all sound

void Hardware::stopAllBuzzers() {
    setSmallBuzzer(0);
    setLargeBuzzer(0);
    DEBUG_PRINTLN("All buzzers stopped");
}

// ---------------------------------------------------------------
// Pulse Small Buzzer (WARNING Stage Pattern)
// ---------------------------------------------------------------
// Creates a pulsing pattern: 0.5s ON, 0.5s OFF
// This function should be called repeatedly in loop()
//
// RETURNS: false while pulsing, true when complete

bool Hardware::pulseSmallBuzzer() {
    unsigned long currentTime = millis();

    // First call - initialize pulsing
    if (pulseStartTime == 0) {
        pulseStartTime = currentTime;
        lastPulseToggle = currentTime;
        pulseState = true;
        setSmallBuzzer(BUZZER_ON);
        DEBUG_PRINTLN("Started pulsing pattern");
    }

    // Check if pulse pattern is complete (30 seconds)
    if (currentTime - pulseStartTime >= ALARM_WARNING_DURATION_MS) {
        stopAllBuzzers();
        pulseStartTime = 0;  // Reset for next time
        DEBUG_PRINTLN("Pulse pattern complete");
        return true;  // Pattern finished
    }

    // Toggle buzzer on/off at correct intervals
    if (pulseState) {
        // Currently ON - check if it's time to turn OFF
        if (currentTime - lastPulseToggle >= BUZZER_PULSE_ON_MS) {
            setSmallBuzzer(BUZZER_OFF);
            pulseState = false;
            lastPulseToggle = currentTime;
        }
    } else {
        // Currently OFF - check if it's time to turn ON
        if (currentTime - lastPulseToggle >= BUZZER_PULSE_OFF_MS) {
            setSmallBuzzer(BUZZER_ON);
            pulseState = true;
            lastPulseToggle = currentTime;
        }
    }

    return false;  // Still pulsing
}

// ===============================================================
// LED CONTROL FUNCTIONS
// ===============================================================

// ---------------------------------------------------------------
// Simple LED On/Off Control
// ---------------------------------------------------------------

void Hardware::setWiFiLED(bool state) {
    if (!this->state.ledsEnabled) return;  // LEDs disabled
    digitalWrite(PIN_LED_WIFI, state ? HIGH : LOW);
    wifiLED.enabled = false;  // Disable blinking if it was on
}

void Hardware::setAlarmLED(bool state) {
    if (!this->state.ledsEnabled) return;
    digitalWrite(PIN_LED_ALARM, state ? HIGH : LOW);
    alarmLED.enabled = false;
}

void Hardware::setStatusLED(bool state) {
    if (!this->state.ledsEnabled) return;
    digitalWrite(PIN_LED_STATUS, state ? HIGH : LOW);
    statusLED.enabled = false;
}

// ---------------------------------------------------------------
// LED Blinking Control
// ---------------------------------------------------------------
// interval: Blink period in milliseconds (e.g., 1000 = 1s on, 1s off)

void Hardware::blinkWiFiLED(uint16_t interval) {
    wifiLED.enabled = true;
    wifiLED.interval = interval;
    wifiLED.lastToggle = millis();
}

void Hardware::blinkAlarmLED(uint16_t interval) {
    alarmLED.enabled = true;
    alarmLED.interval = interval;
    alarmLED.lastToggle = millis();
}

void Hardware::blinkStatusLED(uint16_t interval) {
    statusLED.enabled = true;
    statusLED.interval = interval;
    statusLED.lastToggle = millis();
}

// ---------------------------------------------------------------
// Update LED States (Call in Loop)
// ---------------------------------------------------------------
// Handles timing for all blinking LEDs

void Hardware::updateLEDs() {
    if (!state.ledsEnabled) return;

    updateSingleLED(wifiLED, PIN_LED_WIFI);
    updateSingleLED(alarmLED, PIN_LED_ALARM);
    updateSingleLED(statusLED, PIN_LED_STATUS);
}

// ---------------------------------------------------------------
// Update Single LED State (Private Helper)
// ---------------------------------------------------------------

void Hardware::updateSingleLED(LEDState& led, uint8_t pin) {
    if (!led.enabled) return;  // Not blinking

    unsigned long currentTime = millis();

    // Check if it's time to toggle
    if (currentTime - led.lastToggle >= led.interval) {
        led.currentState = !led.currentState;
        digitalWrite(pin, led.currentState ? HIGH : LOW);
        led.lastToggle = currentTime;
    }
}

// ---------------------------------------------------------------
// Turn Off All LEDs
// ---------------------------------------------------------------

void Hardware::turnOffAllLEDs() {
    wifiLED.enabled = false;
    alarmLED.enabled = false;
    statusLED.enabled = false;

    digitalWrite(PIN_LED_WIFI, LOW);
    digitalWrite(PIN_LED_ALARM, LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    DEBUG_PRINTLN("All LEDs turned off");
}

// ===============================================================
// BUTTON INPUT FUNCTIONS
// ===============================================================

// ---------------------------------------------------------------
// Update Button States (Call in Loop)
// ---------------------------------------------------------------
// Reads buttons and handles debouncing

void Hardware::updateButtons() {
    // Read raw button states (LOW = pressed with pullup)
    bool testRaw = (digitalRead(PIN_BUTTON_TEST) == BUTTON_PRESSED);
    bool silenceRaw = (digitalRead(PIN_BUTTON_SILENCE) == BUTTON_PRESSED);
    bool resetRaw = (digitalRead(PIN_BUTTON_RESET) == BUTTON_PRESSED);

    // Debounce and update states
    state.buttonTest = debounceButton(testRaw, testButton);
    state.buttonSilence = debounceButton(silenceRaw, silenceButton);
    state.buttonReset = debounceButton(resetRaw, resetButton);

    // Track RESET button hold time for factory reset
    if (state.buttonReset) {
        if (state.resetButtonPressTime == 0) {
            // Button just pressed - record time
            state.resetButtonPressTime = millis();
        }
        // Button still held - check for long press in isFactoryResetRequested()
    } else {
        // Button released - reset timer
        state.resetButtonPressTime = 0;
    }
}

// ---------------------------------------------------------------
// Debounce Button (Private Helper)
// ---------------------------------------------------------------
// Prevents false triggers from electrical noise
//
// How debouncing works:
// 1. Button state must be stable for BUTTON_DEBOUNCE_MS (50ms)
// 2. Rapid changes are ignored (noise filtering)
// 3. Only stable state changes are registered

bool Hardware::debounceButton(bool rawState, ButtonState& buttonState) {
    unsigned long currentTime = millis();

    // Check if state has changed
    if (rawState != buttonState.lastReading) {
        // State changed - reset debounce timer
        buttonState.lastChange = currentTime;
        buttonState.lastReading = rawState;
    }

    // Check if state has been stable long enough
    if ((currentTime - buttonState.lastChange) > BUTTON_DEBOUNCE_MS) {
        // State is stable - update if different
        if (rawState != buttonState.stableState) {
            buttonState.stableState = rawState;

            // Log button events for debugging
            if (rawState) {
                DEBUG_PRINTF("Button pressed (pin change detected)\n");
            }
        }
    }

    return buttonState.stableState;
}

// ---------------------------------------------------------------
// Check Button States (Public Interface)
// ---------------------------------------------------------------

bool Hardware::isTestButtonPressed() {
    return state.buttonTest;
}

bool Hardware::isSilenceButtonPressed() {
    return state.buttonSilence;
}

bool Hardware::isResetButtonPressed() {
    return state.buttonReset;
}

// ---------------------------------------------------------------
// Check for Factory Reset Request
// ---------------------------------------------------------------
// RETURNS: true if RESET button held for 10+ seconds

bool Hardware::isFactoryResetRequested() {
    if (state.resetButtonPressTime == 0) {
        return false;  // Not pressed
    }

    unsigned long holdTime = millis() - state.resetButtonPressTime;

    if (holdTime >= RESET_HOLD_TIME_MS) {
        DEBUG_PRINTLN("⚠ Factory reset requested (10s hold)");
        return true;
    }

    return false;
}

// ===============================================================
// HARDWARE HEALTH CHECKS
// ===============================================================

// ---------------------------------------------------------------
// Check Buzzer Circuits
// ---------------------------------------------------------------
// Performs basic GPIO validation
//
// HOW IT WORKS:
// 1. Set GPIO pin HIGH
// 2. Wait briefly (10 microseconds)
// 3. Read pin back
// 4. If reads as HIGH, GPIO is working
//
// LIMITATIONS:
// - Only detects severe problems (disconnected wire, short circuit)
// - Cannot verify buzzer actually makes sound
// - Cannot detect dead buzzer (still draws current but no sound)

bool Hardware::checkBuzzerCircuits() {
    bool allOK = true;

    DEBUG_PRINTLN("Checking buzzer circuits...");

    // ---------------------------------------------------------------
    // Test Small Buzzer Circuit
    // ---------------------------------------------------------------

    digitalWrite(PIN_SMALL_BUZZER, HIGH);  // Set HIGH
    delayMicroseconds(GPIO_CHECK_DELAY_US);  // Wait for pin to stabilize

    if (digitalRead(PIN_SMALL_BUZZER) == HIGH) {
        state.smallBuzzer = HW_STATUS_OK;
        DEBUG_PRINTLN("  ✓ Small buzzer circuit: OK");
    } else {
        state.smallBuzzer = HW_STATUS_FAILED;
        DEBUG_PRINTLN("  ✗ Small buzzer circuit: FAILED");
        allOK = false;
    }

    digitalWrite(PIN_SMALL_BUZZER, LOW);  // Turn back off

    // ---------------------------------------------------------------
    // Test Large Buzzer Circuit
    // ---------------------------------------------------------------

    digitalWrite(PIN_LARGE_BUZZER, HIGH);
    delayMicroseconds(GPIO_CHECK_DELAY_US);

    if (digitalRead(PIN_LARGE_BUZZER) == HIGH) {
        state.largeBuzzer = HW_STATUS_OK;
        DEBUG_PRINTLN("  ✓ Large buzzer circuit: OK");
    } else {
        state.largeBuzzer = HW_STATUS_FAILED;
        DEBUG_PRINTLN("  ✗ Large buzzer circuit: FAILED");
        allOK = false;
    }

    digitalWrite(PIN_LARGE_BUZZER, LOW);

    return allOK;
}

// ---------------------------------------------------------------
// Test Buzzer (Make Actual Sound)
// ---------------------------------------------------------------
// Activates buzzer for brief period to verify it makes sound
//
// WARNING: This is LOUD! Only call during /test command

void Hardware::testBuzzer(uint8_t buzzerPin, uint16_t durationMs) {
    DEBUG_PRINTF("Testing buzzer on pin %d for %dms...\n", buzzerPin, durationMs);

    // Determine which PWM channel to use
    uint8_t channel = (buzzerPin == PIN_SMALL_BUZZER) ?
                      BUZZER_PWM_CHANNEL_SMALL : BUZZER_PWM_CHANNEL_LARGE;

    // Turn on buzzer
    ledcWrite(channel, BUZZER_ON);

    // Wait for specified duration
    delay(durationMs);

    // Turn off buzzer
    ledcWrite(channel, BUZZER_OFF);

    DEBUG_PRINTLN("Test complete");
}

// ===============================================================
// STATE ACCESS FUNCTIONS
// ===============================================================

// ---------------------------------------------------------------
// Get Current Hardware State
// ---------------------------------------------------------------

HardwareState Hardware::getState() const {
    return state;
}

// ---------------------------------------------------------------
// Get Status String (For Debugging)
// ---------------------------------------------------------------

String Hardware::getStatusString() const {
    String status = "Hardware Status:\n";

    // Buzzer status
    status += "  Small Buzzer: ";
    status += (state.smallBuzzer == HW_STATUS_OK) ? "OK" :
              (state.smallBuzzer == HW_STATUS_FAILED) ? "FAILED" : "UNKNOWN";
    status += "\n";

    status += "  Large Buzzer: ";
    status += (state.largeBuzzer == HW_STATUS_OK) ? "OK" :
              (state.largeBuzzer == HW_STATUS_FAILED) ? "FAILED" : "UNKNOWN";
    status += "\n";

    // Button status
    status += "  Buttons: ";
    status += "TEST=";
    status += state.buttonTest ? "PRESSED" : "RELEASED";
    status += ", SILENCE=";
    status += state.buttonSilence ? "PRESSED" : "RELEASED";
    status += ", RESET=";
    status += state.buttonReset ? "PRESSED" : "RELEASED";

    return status;
}

// ===============================================================
// END OF HARDWARE MODULE
// ===============================================================

/*
 * TESTING CHECKLIST:
 *
 * ☐ Test buzzer on/off control
 * ☐ Test pulsing pattern (should be 0.5s on, 0.5s off)
 * ☐ Test LED blinking at different intervals
 * ☐ Test button debouncing (rapid presses should register as one)
 * ☐ Test factory reset (hold RESET for 10s)
 * ☐ Test hardware checks (disconnect buzzer wire, check if detected)
 *
 * DEBUG TIPS:
 * - If buzzer doesn't work: Check MOSFET wiring and resistors
 * - If LEDs don't light: Check resistor values (should be 220Ω)
 * - If buttons don't respond: Check if pullup resistors enabled
 * - If PWM doesn't work: Verify pin supports PWM (check pinout)
 */
