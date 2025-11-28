/*
 * ===============================================================
 * WakeAssist - Telegram Bot Module (Header File)
 * ===============================================================
 *
 * This module handles all Telegram Bot API interactions:
 * - Polling for new messages from user
 * - Parsing commands (/wake, /test, /status, etc.)
 * - Sending responses and notifications
 * - Rate limiting to prevent spam
 *
 * WHY TELEGRAM?
 * - Works anywhere with internet (no VPN needed)
 * - Secure (encrypted messaging)
 * - Free API with no server costs
 * - Easy for non-technical users
 *
 * ===============================================================
 */

#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>  // For HTTPS connections
#include <ArduinoJson.h>        // For parsing Telegram JSON responses
#include <Preferences.h>        // For storing bot token
#include "config.h"             // Configuration constants

// ===============================================================
// TELEGRAM MESSAGE STRUCTURE
// ===============================================================
// Stores information about a received Telegram message

struct TelegramMessage {
    int64_t chatId;          // Unique chat ID (user identifier)
    int32_t messageId;       // Message ID
    String text;             // Message text content
    String username;         // Sender's username
    unsigned long timestamp; // When message was received (Unix timestamp)
};

// ===============================================================
// TELEGRAM BOT STATUS ENUMERATION
// ===============================================================
// Represents the current state of the Telegram bot

enum TelegramBotStatus {
    BOT_NOT_INITIALIZED,     // Bot not yet set up
    BOT_NO_TOKEN,            // No bot token configured
    BOT_CONNECTING,          // Attempting to connect to Telegram API
    BOT_ONLINE,              // Successfully connected and polling
    BOT_OFFLINE,             // Connection lost or failed
    BOT_RATE_LIMITED         // Temporary rate limit from Telegram API
};

// ===============================================================
// TELEGRAM BOT CLASS
// ===============================================================
// Handles all communication with Telegram Bot API

class TelegramBot {
public:
    // ---------------------------------------------------------------
    // CONSTRUCTOR
    // ---------------------------------------------------------------
    TelegramBot();

    // ---------------------------------------------------------------
    // INITIALIZATION
    // ---------------------------------------------------------------

    // Initialize Telegram bot with token and authorized user ID
    //
    // botToken: Bot token from @BotFather (e.g., "123456:ABC-DEF...")
    // userId: Telegram user ID who can control device (for security)
    //
    // WHY USER ID?
    // Without it, ANYONE could send /wake to your device!
    // User ID ensures only the authorized person can trigger alarms.
    //
    // RETURNS: true if initialization successful
    bool begin(const String& botToken, int64_t userId);

    // Initialize using stored credentials from flash
    // Useful for auto-start after reboot
    //
    // RETURNS: true if credentials found and loaded
    bool beginFromStorage();

    // ---------------------------------------------------------------
    // BOT CONFIGURATION
    // ---------------------------------------------------------------

    // Set bot token (from @BotFather)
    // RETURNS: true if token format is valid
    bool setBotToken(const String& token);

    // Set authorized user ID (from Telegram)
    // Only this user can send commands to the device
    void setAuthorizedUserId(int64_t userId);

    // Save bot configuration to flash memory
    // RETURNS: true if saved successfully
    bool saveConfiguration();

    // Load bot configuration from flash memory
    // RETURNS: true if configuration found
    bool loadConfiguration();

    // Check if bot is configured with token and user ID
    // RETURNS: true if ready to use
    bool isConfigured() const;

    // ---------------------------------------------------------------
    // MESSAGE POLLING
    // ---------------------------------------------------------------

    // Check for new messages from Telegram
    // Call this periodically in loop() (every 5 seconds recommended)
    //
    // IMPORTANT: This is "polling" mode (we ask for messages)
    // NOT "webhook" mode (Telegram pushes messages to us)
    // Polling is simpler and doesn't require public IP/domain
    //
    // RETURNS: true if new messages were processed
    bool poll();

    // Get next unprocessed message (if available)
    // Use this in a loop after poll() returns true
    //
    // message: Reference to store the message
    // RETURNS: true if message available, false if no more messages
    bool getNextMessage(TelegramMessage& message);

    // Mark all current messages as read
    // Used to ignore old messages after startup
    void markAllRead();

    // ---------------------------------------------------------------
    // SENDING MESSAGES
    // ---------------------------------------------------------------

    // Send text message to authorized user
    // text: Message content (supports Telegram markdown formatting)
    // RETURNS: true if sent successfully
    bool sendMessage(const String& text);

    // Send message to specific chat ID
    // chatId: Recipient's chat ID
    // text: Message content
    // RETURNS: true if sent successfully
    bool sendMessage(int64_t chatId, const String& text);

    // Send message with inline keyboard buttons
    // Useful for yes/no confirmations
    //
    // text: Message content
    // buttons: Array of button labels
    // buttonCount: Number of buttons
    // RETURNS: true if sent successfully
    bool sendMessageWithButtons(const String& text,
                               const String buttons[],
                               int buttonCount);

    // ---------------------------------------------------------------
    // COMMAND HANDLING
    // ---------------------------------------------------------------

    // Register callback for specific command
    // Command callbacks are triggered when user sends matching command
    //
    // USAGE:
    //   telegramBot.onCommand("/wake", [](TelegramMessage msg) {
    //       Serial.println("Wake command received!");
    //   });
    //
    // command: Command string (e.g., "/wake", "/test")
    // callback: Function to call when command is received
    void onCommand(const String& command,
                  std::function<void(TelegramMessage)> callback);

    // Process a message and trigger appropriate command callback
    // Called automatically by poll()
    void processMessage(const TelegramMessage& message);

    // ---------------------------------------------------------------
    // RATE LIMITING
    // ---------------------------------------------------------------

    // Check if /wake command is currently rate-limited
    // Prevents spam (max 1 /wake per 5 minutes)
    //
    // RETURNS: true if rate limit active, false if allowed
    bool isWakeRateLimited() const;

    // Get remaining cooldown time for /wake command
    // RETURNS: Seconds remaining until next /wake allowed
    unsigned long getWakeCooldownRemaining() const;

    // Reset rate limit (for testing or manual override)
    void resetWakeRateLimit();

    // ---------------------------------------------------------------
    // STATUS & INFORMATION
    // ---------------------------------------------------------------

    // Get current bot status
    // RETURNS: TelegramBotStatus enum value
    TelegramBotStatus getStatus() const;

    // Check if bot is currently online and polling
    // RETURNS: true if online
    bool isOnline() const;

    // Get bot username (from Telegram)
    // RETURNS: Bot username (e.g., "@WakeAssistBot")
    String getBotUsername() const;

    // Get authorized user ID
    // RETURNS: User ID, or 0 if not set
    int64_t getAuthorizedUserId() const;

    // Get human-readable status string
    // RETURNS: String like "Online - Polling every 5s"
    String getStatusString() const;

    // ---------------------------------------------------------------
    // CALLBACK REGISTRATION
    // ---------------------------------------------------------------

    // Set callback for when bot goes online
    void onOnline(std::function<void()> callback);

    // Set callback for when bot goes offline
    void onOffline(std::function<void()> callback);

    // Set callback for unauthorized access attempts
    // Triggered when wrong user tries to send commands
    void onUnauthorizedAccess(std::function<void(int64_t, String)> callback);

private:
    // ---------------------------------------------------------------
    // PRIVATE MEMBER VARIABLES
    // ---------------------------------------------------------------

    WiFiClientSecure client;      // HTTPS client for Telegram API
    Preferences preferences;      // Flash storage for config

    TelegramBotStatus status;     // Current bot status
    String botToken;              // Bot token from @BotFather
    int64_t authorizedUserId;     // User ID who can control device
    String botUsername;           // Bot's username (cached from API)

    int32_t lastUpdateId;         // Last processed message ID
    unsigned long lastPollTime;   // Last time we polled for messages

    // Rate limiting
    unsigned long lastWakeTime;   // Last time /wake was sent

    // Message queue (simple FIFO buffer)
    static const int MESSAGE_QUEUE_SIZE = 10;
    TelegramMessage messageQueue[MESSAGE_QUEUE_SIZE];
    int messageQueueHead;         // Next position to write
    int messageQueueTail;         // Next position to read
    int messageQueueCount;        // Number of messages in queue

    // Command callbacks (map of command -> function)
    struct CommandCallback {
        String command;
        std::function<void(TelegramMessage)> callback;
    };
    static const int MAX_COMMANDS = 10;
    CommandCallback commandCallbacks[MAX_COMMANDS];
    int commandCallbackCount;

    // Status callbacks
    std::function<void()> callbackOnline;
    std::function<void()> callbackOffline;
    std::function<void(int64_t, String)> callbackUnauthorizedAccess;

    // ---------------------------------------------------------------
    // PRIVATE HELPER FUNCTIONS
    // ---------------------------------------------------------------

    // Make HTTPS GET request to Telegram API
    // endpoint: API endpoint (e.g., "getUpdates")
    // params: URL parameters (e.g., "offset=123&limit=100")
    // RETURNS: JSON response as String, empty if failed
    String makeRequest(const String& endpoint, const String& params = "");

    // Make HTTPS POST request to Telegram API
    // endpoint: API endpoint (e.g., "sendMessage")
    // jsonBody: JSON request body
    // RETURNS: JSON response as String, empty if failed
    String makePostRequest(const String& endpoint, const String& jsonBody);

    // Get bot info from Telegram (username, etc.)
    // Called once during initialization
    // RETURNS: true if successful
    bool getBotInfo();

    // Parse JSON response from Telegram API
    // Handles error checking and result extraction
    //
    // response: JSON string from Telegram
    // doc: JsonDocument to store parsed data
    // RETURNS: true if parsing successful and result is OK
    bool parseResponse(const String& response, JsonDocument& doc);

    // Add message to processing queue
    void queueMessage(const TelegramMessage& message);

    // Check if message is from authorized user
    bool isAuthorized(int64_t userId) const;

    // Update bot status and trigger callbacks
    void updateStatus(TelegramBotStatus newStatus);

    // Validate bot token format
    // Bot tokens have format: "123456789:ABCdefGHIjklMNOpqrsTUVwxyz"
    // RETURNS: true if format looks valid
    bool validateTokenFormat(const String& token) const;
};

// ===============================================================
// GLOBAL TELEGRAM BOT INSTANCE
// ===============================================================
// Create a single bot instance that can be used throughout the program
//
// USAGE IN OTHER FILES:
//   extern TelegramBot telegramBot;
//   telegramBot.sendMessage("Hello!");

extern TelegramBot telegramBot;

#endif // TELEGRAM_BOT_H

/*
 * ===============================================================
 * USAGE EXAMPLE:
 * ===============================================================
 *
 * // In main.cpp:
 * #include "telegram_bot.h"
 *
 * void setup() {
 *     // Initialize with bot token and user ID
 *     // Get these from @BotFather and @userinfobot on Telegram
 *     telegramBot.begin("123456789:ABCdef...", 987654321);
 *
 *     // Register command handlers
 *     telegramBot.onCommand("/wake", [](TelegramMessage msg) {
 *         Serial.println("Wake command received!");
 *         alarmController.start();  // Start alarm
 *         telegramBot.sendMessage("Alarm starting in 3 seconds...");
 *     });
 *
 *     telegramBot.onCommand("/status", [](TelegramMessage msg) {
 *         String status = "Device: Online\n";
 *         status += "WiFi: " + wifiMgr.getSSID() + "\n";
 *         status += "Alarm: " + alarmController.getStateString();
 *         telegramBot.sendMessage(status);
 *     });
 *
 *     // Set up status callbacks
 *     telegramBot.onOnline([]() {
 *         Serial.println("Bot is online!");
 *     });
 * }
 *
 * void loop() {
 *     // Poll for new messages every 5 seconds
 *     static unsigned long lastPoll = 0;
 *     if (millis() - lastPoll >= TELEGRAM_POLL_INTERVAL_MS) {
 *         lastPoll = millis();
 *         telegramBot.poll();
 *     }
 * }
 *
 * ===============================================================
 * SECURITY NOTES:
 * ===============================================================
 *
 * 1. USER ID AUTHORIZATION:
 *    Only the user with matching ID can send commands.
 *    Get your user ID from @userinfobot on Telegram.
 *
 * 2. BOT TOKEN SECURITY:
 *    - Never commit bot token to git!
 *    - Store in flash memory, not in code
 *    - Regenerate token if compromised (@BotFather)
 *
 * 3. RATE LIMITING:
 *    Prevents abuse if token is leaked.
 *    /wake limited to once per 5 minutes.
 *
 * 4. HTTPS ONLY:
 *    All communication uses TLS/SSL encryption.
 *
 * ===============================================================
 * TELEGRAM BOT SETUP INSTRUCTIONS:
 * ===============================================================
 *
 * 1. Create Bot:
 *    - Open Telegram, search for "@BotFather"
 *    - Send: /newbot
 *    - Choose name: "My WakeAssist"
 *    - Choose username: "MyWakeAssistBot" (must end in 'bot')
 *    - Save the token: "123456789:ABCdef..."
 *
 * 2. Get Your User ID:
 *    - Search for "@userinfobot" on Telegram
 *    - Send any message
 *    - Bot replies with your user ID (number like 987654321)
 *
 * 3. Configure Device:
 *    - Enter token and user ID during WiFi setup portal
 *    - Or use /config command after first boot
 *
 * ===============================================================
 */
