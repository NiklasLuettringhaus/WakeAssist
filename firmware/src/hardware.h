/*
 * ===============================================================
 * WakeAssist - Hardware Control Module (Header File)
 * ===============================================================
 *
 * This module handles all physical hardware interactions:
 * - Buzzer control (via MOSFETs and PWM)
 * - LED indicators (status, alarm, WiFi)
 * - Button input (test, silence, reset)
 * - Hardware health checks (GPIO validation)
 *
 * WHY SEPARATE FILE?
 * Keeping hardware control separate from business logic makes
 * the code easier to understand, test, and modify.
 *
 * ===============================================================
 */

#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>      // Arduino core functions (digitalWrite, pinMode, etc.)
#include "config.h"       // Our pin definitions and constants

// ===============================================================
// HARDWARE STATUS ENUMERATION
// ===============================================================
// Represents the health status of hardware components
//
// USAGE: if (hardwareStatus.smallBuzzer == HW_STATUS_FAILED) { ... }

enum HardwareStatus {
    HW_STATUS_UNKNOWN,        // Not yet tested
    HW_STATUS_OK,             // Working correctly
    HW_STATUS_FAILED,         // Hardware check failed
    HW_STATUS_DISABLED        // Intentionally disabled (for testing)
};

// ===============================================================
// HARDWARE STATE STRUCTURE
// ===============================================================
// Stores the current status of all hardware components
//
// This makes it easy to check hardware health from anywhere in code

struct HardwareState {
    HardwareStatus smallBuzzer;     // Status of small buzzer circuit
    HardwareStatus largeBuzzer;     // Status of large buzzer circuit

    bool buttonTest;                // Current state of TEST button (true = pressed)
    bool buttonSilence;             // Current state of SILENCE button
    bool buttonReset;               // Current state of RESET button

    unsigned long resetButtonPressTime;  // When RESET button was first pressed
                                         // (Used to detect long hold for factory reset)

    bool ledsEnabled;               // Master LED enable (can disable all for testing)
};

// ===============================================================
// HARDWARE CLASS
// ===============================================================
// Encapsulates all hardware control functions
//
// WHY A CLASS?
// - Keeps related functions together
// - Prevents global variable pollution
// - Makes testing easier
// - Clear interface for other modules

class Hardware {
public:
    // ---------------------------------------------------------------
    // CONSTRUCTOR
    // ---------------------------------------------------------------
    // Called when Hardware object is created
    // Initializes internal state
    Hardware();

    // ---------------------------------------------------------------
    // INITIALIZATION
    // ---------------------------------------------------------------
    // Sets up all GPIO pins, PWM channels, and initial states
    // MUST be called once in setup() before using hardware
    //
    // RETURNS: true if initialization successful, false if error
    bool begin();

    // ---------------------------------------------------------------
    // BUZZER CONTROL
    // ---------------------------------------------------------------

    // Turn small buzzer on at specified duty cycle
    // dutyCycle: 0-255 (0 = off, 255 = full power)
    void setSmallBuzzer(uint8_t dutyCycle);

    // Turn large buzzer on at specified duty cycle
    // dutyCycle: 0-255 (0 = off, 255 = full power)
    void setLargeBuzzer(uint8_t dutyCycle);

    // Turn both buzzers off immediately
    // Used for emergency stop or alarm completion
    void stopAllBuzzers();

    // Create pulsing pattern for WARNING stage
    // This function handles timing internally
    // RETURNS: true if pattern is complete (30 seconds elapsed)
    bool pulseSmallBuzzer();

    // ---------------------------------------------------------------
    // LED CONTROL
    // ---------------------------------------------------------------

    // WiFi status LED control
    void setWiFiLED(bool state);              // Simple on/off
    void blinkWiFiLED(uint16_t interval);     // Blink at specified interval (ms)

    // Alarm status LED control
    void setAlarmLED(bool state);
    void blinkAlarmLED(uint16_t interval);

    // System status LED control
    void setStatusLED(bool state);
    void blinkStatusLED(uint16_t interval);

    // Update all blinking LEDs (call this in loop())
    // This handles the timing for LED blink patterns
    void updateLEDs();

    // Turn off all LEDs (for power saving or testing)
    void turnOffAllLEDs();

    // ---------------------------------------------------------------
    // BUTTON INPUT
    // ---------------------------------------------------------------

    // Read button states with debouncing
    // Debouncing prevents false triggers from electrical noise
    // Call these functions to check if buttons are pressed
    bool isTestButtonPressed();
    bool isSilenceButtonPressed();
    bool isResetButtonPressed();

    // Check if RESET button has been held for factory reset time (10 seconds)
    // RETURNS: true if held long enough, false otherwise
    bool isFactoryResetRequested();

    // Update button state tracking (call this in loop())
    // Handles debouncing and long-press detection
    void updateButtons();

    // ---------------------------------------------------------------
    // HARDWARE HEALTH CHECKS
    // ---------------------------------------------------------------

    // Perform basic GPIO checks on buzzer circuits
    // Tests if GPIO pins can be set HIGH and read back
    //
    // LIMITATIONS: This does NOT verify that buzzers make sound!
    // Only detects severe electrical problems (disconnected, short circuit)
    //
    // RETURNS: true if tests pass, false if hardware issue detected
    bool checkBuzzerCircuits();

    // Test a specific buzzer for brief period (100ms)
    // Makes actual sound to verify buzzer works
    //
    // WARNING: This will make noise! Only use during /test command
    //
    // buzzerPin: PIN_SMALL_BUZZER or PIN_LARGE_BUZZER
    // durationMs: How long to activate buzzer (default 100ms)
    void testBuzzer(uint8_t buzzerPin, uint16_t durationMs = 100);

    // ---------------------------------------------------------------
    // STATE ACCESS
    // ---------------------------------------------------------------

    // Get current hardware state structure
    // Allows other modules to check hardware status
    //
    // USAGE: if (hardware.getState().smallBuzzer == HW_STATUS_FAILED) { ... }
    HardwareState getState() const;

    // Get human-readable status string for debugging
    // RETURNS: String like "Small Buzzer: OK, Large Buzzer: OK"
    String getStatusString() const;

private:
    // ---------------------------------------------------------------
    // PRIVATE MEMBER VARIABLES
    // ---------------------------------------------------------------
    // Only accessible within this class (encapsulation)

    HardwareState state;          // Current hardware state

    // LED blinking state tracking
    struct LEDState {
        bool enabled;             // Is this LED currently blinking?
        bool currentState;        // Current on/off state
        uint16_t interval;        // Blink interval in milliseconds
        unsigned long lastToggle; // Last time LED state changed (millis())
    };

    LEDState wifiLED;
    LEDState alarmLED;
    LEDState statusLED;

    // Button debouncing state
    struct ButtonState {
        bool lastReading;         // Last raw button reading
        bool stableState;         // Debounced button state
        unsigned long lastChange; // Last time button changed state
    };

    ButtonState testButton;
    ButtonState silenceButton;
    ButtonState resetButton;

    // Buzzer pulsing state (for WARNING stage)
    unsigned long lastPulseToggle;  // Last time pulse pattern changed
    bool pulseState;                // Current pulse on/off state
    unsigned long pulseStartTime;   // When pulsing started

    // ---------------------------------------------------------------
    // PRIVATE HELPER FUNCTIONS
    // ---------------------------------------------------------------
    // Internal functions used by public methods

    // Initialize PWM channels for buzzer control
    void setupPWM();

    // Read button with debouncing logic
    // rawState: Current raw reading from digitalRead()
    // buttonState: Reference to button state structure to update
    // RETURNS: Debounced button state (true = pressed)
    bool debounceButton(bool rawState, ButtonState& buttonState);

    // Update a single LED's blink state
    void updateSingleLED(LEDState& led, uint8_t pin);
};

// ===============================================================
// GLOBAL HARDWARE INSTANCE
// ===============================================================
// Create a single hardware controller instance that can be used
// throughout the program
//
// USAGE IN OTHER FILES:
//   extern Hardware hardware;  // Declare external reference
//   hardware.setSmallBuzzer(255);  // Use it

extern Hardware hardware;

#endif // HARDWARE_H

/*
 * ===============================================================
 * USAGE EXAMPLE:
 * ===============================================================
 *
 * // In main.cpp:
 * #include "hardware.h"
 *
 * void setup() {
 *     // Initialize hardware
 *     if (!hardware.begin()) {
 *         Serial.println("Hardware init failed!");
 *     }
 *
 *     // Check hardware health
 *     if (!hardware.checkBuzzerCircuits()) {
 *         Serial.println("Buzzer circuit issue detected");
 *     }
 * }
 *
 * void loop() {
 *     // Update button and LED states
 *     hardware.updateButtons();
 *     hardware.updateLEDs();
 *
 *     // Check for button press
 *     if (hardware.isSilenceButtonPressed()) {
 *         hardware.stopAllBuzzers();
 *     }
 *
 *     // Control buzzers
 *     hardware.setSmallBuzzer(255);  // Turn on full power
 *     delay(1000);
 *     hardware.setSmallBuzzer(0);    // Turn off
 * }
 *
 * ===============================================================
 */
