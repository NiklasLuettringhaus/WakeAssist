# WakeAssist - Remote-Triggered Wake-Up Device
A WiFi-connected alarm unit designed to remotely wake a heavy sleeper through escalating buzzer alarms. Triggered via Telegram bot for maximum simplicity and reliability.

---

## 1. Core Concept
A bedroom device that can be triggered remotely via Telegram messaging app.

**MVP Approach: Buzzers Only**
- **Small buzzer:** Moderate alarm (~80 dB)
- **Large buzzer:** Loud alarm (~95+ dB)
- No speaker/audio system in MVP (future enhancement)
- Simple, reliable, fewer components to fail

**User Experience:**
1. User sends `/wake` command to their Telegram bot
2. Device receives command (within 5 seconds)
3. Alarm activates with escalating intensity:
   - **WARNING:** Small buzzer, pulsing pattern (30s)
   - **ALERT:** Small buzzer, continuous + LED flashing (30s)
   - **EMERGENCY:** Large buzzer, continuous (until stopped, max 5 min)
4. User can stop alarm via `/stop` command or physical button

**Simple Setup for Non-Technical Users:**
1. Create Telegram bot via @BotFather (5 minutes)
2. Plug in device → connects to temporary WiFi
3. Enter home WiFi credentials + bot token via web form
4. Device reboots and connects to home WiFi
5. Send `/start` to bot → ready to use

Uses WiFi for communication and stable 12V power backbone for electronics.

---

## 2. System Architecture

### 2.1 Power Architecture (MVP)
- **Main supply:** 12 V DC (≥2 A).
- **Buck converter:** 12 V → 5 V (≥1–2 A recommended).
- **5 V rail powers:** ESP32 only.
- **12 V rail powers:** Small buzzer, large buzzer, LEDs.

Why?
12 V is stable and efficient for buzzers; 5 V rail for ESP32 logic.

**Power consumption estimate:**
- ESP32: ~150-250 mA (WiFi active)
- Small buzzer: ~10-30 mA
- Large buzzer: ~30-100 mA
- LEDs: ~20-60 mA (3 LEDs)
- Total: <500 mA typical, 1A peak (safe with 2A supply)

---

## 2.2 Electronics Architecture (MVP)

### Main Controller
- **ESP32** (WiFi, Telegram bot client).
- Receives secure trigger commands via Telegram API.
- Drives 2× MOSFETs for buzzers.
- PWM capable (optional tone/volume control).
- Controls 3× status LEDs.
- Monitors 3× buttons (test/silence/reset).

### Buzzer System (Two-Stage Alarm)

**Small Buzzer (WARNING/ALERT stages):**
- **12V active buzzer** (e.g., TMB12A05, ~80 dB)
- Current draw: ~10-30 mA
- Switched via logic-level MOSFET (AO3400 or IRLZ44N)
- Gate: 100 kΩ pulldown + 100 Ω gate resistor
- PWM capable (GPIO pin with PWM support)

**Large Buzzer (EMERGENCY stage):**
- **12V active buzzer** (e.g., SFM-27, ~95+ dB)
- Current draw: ~30-100 mA
- Switched via logic-level MOSFET (AO3400 or IRLZ44N)
- Gate: 100 kΩ pulldown + 100 Ω gate resistor
- PWM capable (GPIO pin with PWM support)

**PWM Control Notes:**
- **Active buzzers:** Have built-in oscillator. PWM can only control on/off or slight volume reduction (not tone). Limited effectiveness.
- **Passive buzzers:** Require PWM signal to generate tone. Full control over frequency (tone) and duty cycle (volume).
- **Recommendation:** Use active buzzers for MVP (simpler, more reliable). PWM can create pulsing patterns (on/off rhythms) even if not controlling tone.

**Circuit per buzzer:**
```
ESP32 GPIO ──→ [100Ω] ──→ MOSFET Gate
                           │
                      [100kΩ] to GND

12V ──→ Buzzer+ ──→ Buzzer- ──→ MOSFET Drain
                                    │
                                  Source
                                    │
                                   GND
```

### Hardware Verification System

**Option 1: Basic (No Extra Hardware)**
- Check GPIO pin state after setting HIGH
- Detects: GPIO failure, severe wiring issues
- Reliability: ~30%
- **Limitation:** Cannot verify buzzer actually makes sound

**Option 2: Current Sensing (Recommended for V2)**
- Add INA219 I²C current sensor per buzzer (~$5 each)
- Measure actual current draw when buzzer activated
- Detects: Disconnected buzzer, dead buzzer, wiring issues
- Reliability: ~90%
- Brief 100ms test pulse to verify

**Option 3: User Confirmation (MVP Approach)**
- `/test` command activates each buzzer briefly
- User confirms via Telegram buttons: "Did you hear it?"
- Reliability: 100% (if user responds honestly)
- Weekly test reminders via bot

**MVP Implementation:**
- Use Option 1 (basic GPIO check) before every alarm
- Use Option 3 (user test) for weekly verification
- Report status: "⚠️ Basic circuits OK - not fully verified. Run /test weekly"

### Future Enhancements (Post-MVP)
- Speaker + I²S amplifier for gentle sounds
- Digital microphone (INMP441) for sound verification
- Current sensors (INA219) for automatic hardware validation
- MicroSD card for custom alarm sounds
- 12V LED strobe for visual alarm

---

## 3. Diodes & Protection

### Use from your kit:
| Purpose | Diode |
|---------|--------|
| 12 V reverse-polarity protection | **1N5822** (3 A, 40 V Schottky) |
| ESP32 / logic protection | **1N5819** or **1N4148** |
| Future inductive loads | **FR107** or **FR207** |

### Additional protection
- **TVS diode**: SMBJ12A on 12 V input.
- **Polyfuses:**  
  - 1–2 A on 5 V  
  - 3–5 A on 12 V  
- Reverse-polarity diode at input: 1N5822.

---

## 4. Capacitors & Power Stability (MVP)

### Ceramics (your 2 pF–100 nF kit)
- 100 nF at ESP32 power pins
- 100 nF at buck converter output
- 1 µF or 10 µF ceramics as needed

### Electrolytics (your 0.1 µF–1000 µF kit)
- 12 V input: 470–1000 µF (smooth input, handle inrush)
- 5 V rail: 220–470 µF (stable ESP32 power during WiFi bursts)
- Near buzzers: 10-47 µF (reduce switching noise)

---

## 5. Networking & Software

### WiFi Setup (WiFiManager)
**First Boot (No Credentials):**
- Device creates temporary WiFi network: `WakeAssist-XXXX`
- Serves captive portal on http://192.168.4.1
- Web form collects:
  - Home WiFi SSID
  - Home WiFi password
  - Telegram bot token
- Saves credentials to flash memory
- Reboots and connects to home WiFi

**Normal Operation:**
- Auto-connect to saved WiFi on boot
- Auto-reconnect if connection drops
- Fall back to AP mode after 3 failed attempts (with red LED indicator)
- Reset button (hold 10s) returns to setup mode

### Communication (Telegram Bot API)
**No backend server required!**

**Device → Telegram:**
- Polls `https://api.telegram.org/bot<TOKEN>/getUpdates` every 5 seconds
- Uses long polling (timeout=5s) to reduce latency
- Secure HTTPS connection (TLS)
- Parses JSON responses with ArduinoJson library

**Supported Commands:**
| Command | Immediate Response | After Action | Purpose |
|---------|-------------------|--------------|---------|
| `/start` | "✅ Registering device..." | "✅ WakeAssist connected! Send /wake to test." | Register user (first becomes owner) |
| `/wake` | "✅ Command received. Starting in 3s..." | "⏰ WARNING stage started" | Trigger alarm sequence |
| `/stop` | "🔕 Stopping alarm..." | "✅ Stopped. Duration: Xs" | Silence active alarm |
| `/status` | [Full status report with hardware checks] | N/A | Check device health & state |
| `/test` | "🧪 Starting test..." | "✅ Test complete - hardware OK" | Test speaker (gentle sound) |
| `/volume <1-10>` | "🔊 Setting volume..." | "✅ Volume set to X/10" | Adjust alarm volume |

**Security:**
- Only authorized user ID can send commands
- First person to send `/start` becomes owner
- Bot token stored securely in flash
- Rate limiting: max 1 `/wake` per 5 minutes

### Alarm State Machine with Status Reporting (MVP - Buzzers Only)
```
IDLE (waiting for command)
  ↓ /wake received
  → Telegram: "✅ Command received. Starting in 3s..."

TRIGGERED (hardware check, 3s delay)
  → Check buzzers: GPIO pin state after setting HIGH
  → If hardware issue: Send warning message
  ↓ After 3 seconds
  → Telegram: "⏰ WARNING stage started - small buzzer pulsing"

WARNING (small buzzer pulsing, 30s)
  → Small buzzer: PWM pattern (0.5s ON, 0.5s OFF)
  → Yellow LED on
  → If /stop or button: → IDLE (send confirmation)
  ↓ After 30 seconds (no stop)
  → Telegram: "🔔 ALERT stage - small buzzer continuous"

ALERT (small buzzer continuous, 30s)
  → Small buzzer: Continuous ON
  → Orange LED flashing (0.5s period)
  → If /stop or button: → IDLE (send confirmation)
  ↓ After 30 seconds (no stop)
  → Telegram: "🚨 EMERGENCY - LARGE BUZZER ACTIVATED!"

EMERGENCY (large buzzer continuous, until stopped)
  → Small buzzer: OFF
  → Large buzzer: Continuous ON (~95 dB)
  → Red LED flashing rapidly (0.2s period)
  → If /stop or button: → IDLE (send confirmation)
  ↓ After 5 minutes (safety timeout)
  → Telegram: "⏰ Alarm auto-stopped after 5 minutes (safety)"

IDLE (alarm stopped)
  → Telegram: "✅ Alarm stopped. Duration: Xm Ys. Source: [button/telegram]"
  → All buzzers off
  → All alarm LEDs off
  → Green status LED solid
```

**PWM Patterns for Active Buzzers:**
While active buzzers have built-in oscillators and won't change tone/volume significantly with PWM, we can still create useful patterns:
- **Pulsing:** Alternating on/off (WARNING stage)
- **Beeping:** Short beeps with pauses
- **Continuous:** 100% duty cycle (ALERT/EMERGENCY)
- **Rapid pulsing:** Fast on/off for urgency

**Example PWM code:**
```cpp
// WARNING stage: Pulsing pattern
void warningPattern() {
    analogWrite(SMALL_BUZZER_PIN, 255);  // Full ON
    delay(500);
    analogWrite(SMALL_BUZZER_PIN, 0);    // OFF
    delay(500);
    // Repeat for 30 seconds
}

// ALERT stage: Continuous
void alertPattern() {
    analogWrite(SMALL_BUZZER_PIN, 255);  // Continuous ON
}

// EMERGENCY stage: Large buzzer
void emergencyPattern() {
    analogWrite(SMALL_BUZZER_PIN, 0);    // Turn off small
    analogWrite(LARGE_BUZZER_PIN, 255);  // Turn on large
}
```

**Status Updates During Alarm:**
- Immediate acknowledgment when command received
- Confirmation when each stage starts
- Warning messages if hardware issues detected
- Stop confirmation with duration and stop source
- Error notifications if WiFi drops during alarm

**Safety Features:**
- Auto-timeout after 5 minutes in EMERGENCY state
- Physical buttons override all network commands (always work)
- Watchdog timer restarts device if frozen
- Offline mode: buttons still work without WiFi
- Hardware health checks before each stage
- Redundant stop mechanisms (button, command, timeout)

### Firmware Features (MVP)
- **WiFiManager:** Easy setup for non-technical users
- **Telegram Bot:** Remote control from anywhere
- **Status Reporting:** Multi-level confirmation system
- **Hardware Monitoring:** Buzzer GPIO checks, WiFi health checks
- **Multi-Stage Alarms:** WARNING (pulsing) → ALERT (continuous small) → EMERGENCY (large buzzer)
- **Persistent Storage:** Settings survive power loss (WiFi, bot token)
- **Button Controls:** Test/Silence/Reset (hardware fallback, always works)
- **LED Indicators:** WiFi status, alarm state, system health
- **Error Handling:** WiFi reconnection, API retry logic, hardware failures
- **OTA Updates:** (Future) Remote firmware updates

### Status Reporting & Error Handling (MVP)

**Hardware Health Monitoring:**
```cpp
// Basic checks before alarm starts (no extra hardware required)
bool checkHardware() {
    bool allOK = true;

    // Small buzzer GPIO check
    digitalWrite(SMALL_BUZZER_PIN, HIGH);
    delayMicroseconds(10);
    if (digitalRead(SMALL_BUZZER_PIN) != HIGH) {
        allOK = false;
        logError("Small buzzer GPIO failed");
    }
    digitalWrite(SMALL_BUZZER_PIN, LOW);

    // Large buzzer GPIO check
    digitalWrite(LARGE_BUZZER_PIN, HIGH);
    delayMicroseconds(10);
    if (digitalRead(LARGE_BUZZER_PIN) != HIGH) {
        allOK = false;
        logError("Large buzzer GPIO failed");
    }
    digitalWrite(LARGE_BUZZER_PIN, LOW);

    // WiFi check
    if (WiFi.status() != WL_CONNECTED) {
        allOK = false;
        logError("WiFi disconnected");
    }

    return allOK;
}
```

**Limitations of Basic Checking:**
- ⚠️ Can only detect severe GPIO/wiring failures
- ⚠️ Cannot verify buzzers actually make sound
- ⚠️ Cannot detect disconnected/dead buzzers
- ✅ Solution: User must run `/test` command weekly to verify

**Error Messages Sent to Telegram:**
| Problem | Message | Action Taken |
|---------|---------|--------------|
| Small buzzer GPIO fail | "⚠️ Small buzzer circuit issue - using large buzzer only" | Skip to EMERGENCY |
| Large buzzer GPIO fail | "❌ CRITICAL: Large buzzer not responding! Check device" | Use small buzzer only |
| Both buzzers fail | "❌ CRITICAL: No buzzers responding! Device may not work!" | Try anyway, LED indicators |
| WiFi lost during alarm | "⚠️ WiFi lost - alarm continuing offline" | Alarm continues, red WiFi LED |
| WiFi reconnected | "✅ WiFi reconnected - alarm at [STAGE]" | Resume status updates |
| User hasn't tested in 7 days | "⏰ Reminder: Run /test to verify hardware works" | Weekly reminder |

**Status Command Response Example (MVP):**
```
📊 WakeAssist Status Report

🟢 Device: Online
📶 WiFi: MyNetwork (RSSI: -52 dBm - Excellent)
⏱️ Uptime: 3 days, 4 hours, 23 minutes
🔋 Last restart: Normal boot

⏰ Alarm State: IDLE (ready)
📝 Last Activity:
  • 13:45 - /wake (completed normally)
  • 13:46 - Stopped via button (1m 23s duration)

🔧 Hardware Status (Basic GPIO checks):
  ✅ Small Buzzer: GPIO responding
  ✅ Large Buzzer: GPIO responding
  ⚠️ Note: Run /test to verify sound output
  ✅ Buttons: All responsive
  ✅ LEDs: All functional

🧪 Test History:
  📅 Last test: 2 days ago
  ✅ Test results: Both buzzers confirmed working
  ⏰ Next reminder: in 5 days

⚙️ Settings:
  👤 Owner: @YourUsername (ID: 12345678)
  🤖 Bot: Responding (0.3s latency)
  🔐 Token: Stored securely
```

**LED Status Indicators (Physical Feedback):**
| LED | Color | Pattern | Meaning |
|-----|-------|---------|---------|
| WiFi | Blue | Solid | Connected & working |
| WiFi | Blue | Slow blink (1s) | Connecting to WiFi |
| WiFi | Red | Fast blink (0.2s) | WiFi connection failed |
| Alarm | Off | - | No alarm active |
| Alarm | Yellow | Solid | WARNING stage active |
| Alarm | Orange | Medium blink (0.5s) | ALERT stage active |
| Alarm | Red | Fast blink (0.2s) | EMERGENCY stage active |
| Status | Green | Solid | All systems operational |
| Status | Red | Solid | Hardware error detected |
| Status | Red/Green | Alternating | Telegram API error |

**WiFi Connection Monitoring:**
- Check connection status every 30 seconds
- If WiFi drops: Retry 3 times with exponential backoff (5s, 10s, 20s)
- If still failed: Continue offline mode + red LED indicator
- If alarm is active when WiFi drops: Keep alarm running, queue status message
- When WiFi reconnects: Send queued messages + current status

**Telegram API Error Handling:**
- HTTP timeout (10 seconds per request)
- Retry failed requests up to 3 times
- Exponential backoff: 2s, 4s, 8s
- If API consistently fails: Continue checking WiFi, retry every 60s
- Log errors to serial for debugging

---

## 6. User Interface & Enclosure

### Buttons
- **TEST**  
- **SILENCE**  
- **RESET/SETUP**

### LED Indicators (3 LEDs minimum)
**WiFi Status LED (Blue/Red):**
- Blue solid: Connected and working
- Blue blinking: Connecting
- Red blinking: Connection failed

**Alarm Status LED (Yellow/Orange/Red):**
- Off: No alarm active
- Yellow: WARNING stage
- Orange blinking: ALERT stage
- Red fast blink: EMERGENCY stage

**System Status LED (Green/Red):**
- Green: All systems OK
- Red: Hardware error detected  

### Enclosure
- Ventilation for buck converter
- Screw terminals for buzzers
- Wall/bedside mounting options
- Acoustic vents for buzzers (don't muffle sound)

---

## 7. Component List (MVP)

### Power
- **12V DC PSU** (≥2A, wall adapter)
- **Buck converter** 12V → 5V (1-2A, e.g., LM2596 module)
- **TVS diode** SMBJ12A (12V transient protection)
- **Polyfuse** 1-2A on 5V, 2-3A on 12V
- **Schottky diode** 1N5822 (reverse polarity protection)

### Control
- **ESP32 dev board** (WiFi capable, 30+ GPIO)
- **2× MOSFETs** (AO3400 or IRLZ44N, logic-level)
- **2× 100Ω resistors** (gate resistors)
- **2× 100kΩ resistors** (pulldown resistors)

### Alarm (Buzzers Only)
- **Small 12V active buzzer** (~80 dB, e.g., TMB12A05, ~10-30mA)
- **Large 12V active buzzer** (~95+ dB, e.g., SFM-27, ~30-100mA)

**Buzzer Selection Notes:**
- **Active buzzers** recommended (built-in oscillator, just apply 12V)
- **Alternative:** Passive buzzers (require PWM, more control but more complex)
- Look for "continuous tone" or "active" in specifications
- Verify current draw <100mA for safety

### Future Enhancements (Post-MVP)
- **Speaker system:** MAX98357A I²S amp + 8Ω speaker
- **Microphone:** INMP441 digital mic for sound verification
- **Current sensors:** 2× INA219 for automatic hardware validation
- **MicroSD module:** Custom alarm sounds
- **12V LED strobe:** Visual alarm

### Misc
- Ceramic caps 100 nF
- Electrolytics 100–1000 µF
- Ferrite beads
- Buttons
- LEDs + resistors
- Screw terminals
- Heat-shrink, wiring
- ABS enclosure

---

## 8. Software Implementation Plan

### Development Phases

**Phase 1: WiFi Setup & Web Portal (Week 1)**
- Implement WiFiManager captive portal
- Create HTML setup form (WiFi + Bot Token)
- Store credentials in ESP32 flash (Preferences library)
- Test AP mode and reconnection logic
- LED status indicators

**Phase 2: Telegram Bot Integration (Week 1-2)**
- HTTPS client for Telegram API
- Poll getUpdates endpoint (5-second interval)
- JSON parsing (ArduinoJson)
- Implement `/start` command (user registration)
- Implement `/wake` command with immediate acknowledgment
- Send responses via sendMessage API
- **Status Reporting:** Immediate + after-action confirmations

**Phase 3: Buzzer Control & Hardware (Week 2)**
- MOSFET circuit setup for both buzzers
- Basic GPIO control (on/off)
- PWM pattern generation (pulsing, continuous)
- Test both buzzers individually
- LED indicators (WiFi, alarm, system status)
- **LED Patterns:** Implement all status indicator patterns
- Physical button handlers (test/silence/reset)

**Phase 4: Alarm State Machine (Week 2)**
- State machine (IDLE/TRIGGERED/WARNING/ALERT/EMERGENCY)
- Timers for escalation (Ticker or millis())
- Buzzer patterns: pulsing → continuous small → large
- `/stop` command integration with duration reporting
- **Status Updates:** Send Telegram message at each stage transition
- **Duration Tracking:** Record start time, calculate elapsed time
- Safety timeout (5 minutes in EMERGENCY)

**Phase 5: Hardware Verification (Week 2-3)**
- Basic GPIO checks before alarm
- **`/test` Command:** Test both buzzers with user confirmation
- Test history tracking (last test date, results)
- Weekly test reminders
- **Hardware Monitoring:** GPIO health check function
- Error detection and reporting via Telegram

**Phase 6: Error Handling & Polish (Week 3)**
- WiFi reconnection with exponential backoff
- Telegram API error handling with retries
- Watchdog timer implementation
- Reset button (factory reset - hold 10s)
- **`/status` Command:** Full device health report
- **Error Messages:** All failure scenarios covered
- **Message Queue:** Store messages when WiFi down, send when reconnected
- Logging system (serial + stored events)
- Field testing with non-technical user

**Phase 7: Documentation & Deployment (Week 4)**
- User instruction card
- Telegram bot creation guide
- Troubleshooting guide
- Code comments and documentation
- Final assembly and packaging

### Required Libraries
```cpp
#include <WiFi.h>              // ESP32 WiFi
#include <WiFiManager.h>       // Captive portal
#include <WiFiClientSecure.h>  // HTTPS for Telegram
#include <WebServer.h>         // Setup web page
#include <Preferences.h>       // Persistent storage
#include <ArduinoJson.h>       // JSON parsing
#include <driver/i2s.h>        // Audio output
#include <SPIFFS.h>            // File system
#include <Ticker.h>            // Timers
```

### File Structure
```
WakeAssist/
├── src/
│   ├── main.cpp               # Main program & setup
│   ├── wifi_manager.h/cpp     # WiFi setup & management
│   ├── telegram_bot.h/cpp     # Telegram API handler
│   ├── alarm_controller.h/cpp # State machine & logic
│   ├── audio_player.h/cpp     # I²S audio driver
│   ├── hardware.h/cpp         # GPIO, buttons, LEDs, buzzer
│   └── config.h               # Pin definitions & constants
├── data/
│   ├── setup.html             # WiFi setup web page
│   ├── warning.wav            # Gentle alarm sound
│   └── alert.wav              # Loud alarm sound
├── platformio.ini             # PlatformIO configuration
├── projectplan.md             # This document
└── README.md                  # User documentation
```

---

## 9. User Setup Instructions (Non-Technical)

### Quick Start Card (Include in Box)
```
═══════════════════════════════════════════════════
          WAKEASSIST - Quick Start Guide
═══════════════════════════════════════════════════

STEP 1: CREATE YOUR TELEGRAM BOT (5 minutes)

  1. Open Telegram app on your phone
  2. Search for: @BotFather
  3. Send message: /newbot
  4. Follow the prompts:
     - Choose a name (e.g., "My WakeAssist")
     - Choose a username (e.g., "MyWakeBot")
  5. BotFather will send you a TOKEN
     (looks like: 123456789:ABCdefGHIjklMNOpqrsTUVwxyz)
  6. SAVE THIS TOKEN - you'll need it next!

═══════════════════════════════════════════════════

STEP 2: SETUP YOUR DEVICE (2 minutes)

  1. Plug WakeAssist into power outlet
  2. Wait for BLUE light (about 10 seconds)
  3. On your phone, open WiFi settings
  4. Connect to network: "WakeAssist-XXXX"
  5. A setup page will open automatically
     (If not, go to: http://192.168.4.1)
  6. Fill in the form:
     → Your home WiFi name
     → Your home WiFi password
     → Your bot TOKEN (from Step 1)
  7. Press "Connect Device"
  8. Device will restart (takes 20 seconds)
  9. GREEN light means connected!

═══════════════════════════════════════════════════

STEP 3: ACTIVATE (30 seconds)

  1. Open Telegram
  2. Find your bot (search for the name you chose)
  3. Send message: /start
  4. You should see: "✅ WakeAssist connected!"

DONE! Your WakeAssist is ready.

═══════════════════════════════════════════════════

HOW TO USE:

  → Wake someone up:
    Send: /wake
    You'll get: "✅ Command received. Starting in 3s..."
    Then: "⏰ WARNING stage started"
    (More updates as alarm escalates)

  → Stop alarm:
    Send: /stop
    OR press SILENCE button on device
    You'll get: "✅ Alarm stopped. Duration: Xm Ys"

  → Check if everything working:
    Send: /status
    You'll get a full report with WiFi, hardware, etc.

  → Test the alarm:
    Send: /test
    (Plays gentle sound only, confirms speaker works)

═══════════════════════════════════════════════════

BUTTONS ON DEVICE:

  🔵 TEST      - Try alarm (gentle sound only)
  🔴 SILENCE   - Stop any active alarm
  ⚪ RESET     - Hold 10 seconds to factory reset

═══════════════════════════════════════════════════

TROUBLESHOOTING:

  Problem: Red blinking light
  → Device can't connect to WiFi
  → Hold RESET button for 10 seconds
  → Repeat Step 2 (check WiFi password)

  Problem: No response in Telegram
  → Check WiFi connection (green light?)
  → Send /start again
  → Check device has power

  Problem: Alarm won't stop
  → Press SILENCE button on device
  → Unplug power as last resort

═══════════════════════════════════════════════════

TELEGRAM COMMANDS:

  /start   - Connect and register
  /wake    - Trigger alarm
  /stop    - Stop alarm
  /status  - Check device status
  /test    - Play gentle test sound
  /volume 5 - Set volume (1-10)

═══════════════════════════════════════════════════

Need help? Visit: [your support website/email]

═══════════════════════════════════════════════════
```

---

## 10. One-Sentence Summary
A Telegram-controlled WiFi alarm device using an ESP32, digital audio amplifier, and 12V buzzer with multi-stage escalation, providing remote wake-up capability via simple messaging app with zero backend infrastructure required.
