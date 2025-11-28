/*
 * ===============================================================
 * WakeAssist - Telegram Bot Module (Implementation)
 * ===============================================================
 *
 * This file implements the Telegram Bot API functionality
 * declared in telegram_bot.h
 *
 * KEY CONCEPTS:
 * - Long Polling: We ask Telegram "any new messages?" repeatedly
 * - HTTPS: All communication is encrypted (WiFiClientSecure)
 * - JSON: Telegram API uses JSON format for requests/responses
 *
 * ===============================================================
 */

#include "telegram_bot.h"

// Telegram API configuration
#define TELEGRAM_HOST "api.telegram.org"
#define TELEGRAM_PORT 443  // HTTPS port

// ===============================================================
// GLOBAL INSTANCE
// ===============================================================

TelegramBot telegramBot;

// ===============================================================
// CONSTRUCTOR
// ===============================================================

TelegramBot::TelegramBot() {
    status = BOT_NOT_INITIALIZED;
    botToken = "";
    authorizedUserId = 0;
    botUsername = "";
    lastUpdateId = 0;
    lastPollTime = 0;
    lastWakeTime = 0;
    messageQueueHead = 0;
    messageQueueTail = 0;
    messageQueueCount = 0;
    commandCallbackCount = 0;

    // Callbacks are null by default
    callbackOnline = nullptr;
    callbackOffline = nullptr;
    callbackUnauthorizedAccess = nullptr;
}

// ===============================================================
// INITIALIZATION
// ===============================================================

bool TelegramBot::begin(const String& botToken, int64_t userId) {
    DEBUG_PRINTLN("[Telegram] Initializing bot...");

    // Initialize Preferences for flash storage
    if (!preferences.begin(STORAGE_NAMESPACE, false)) {
        DEBUG_PRINTLN("[Telegram] ERROR: Failed to initialize Preferences!");
        return false;
    }

    // Set bot credentials
    if (!setBotToken(botToken)) {
        DEBUG_PRINTLN("[Telegram] ERROR: Invalid bot token format!");
        return false;
    }

    setAuthorizedUserId(userId);

    // Configure HTTPS client
    // IMPORTANT: We're using insecure mode (no certificate validation)
    // This is acceptable for Telegram API as we're not sending sensitive data
    // and it saves flash memory space (no need to store CA certificates)
    client.setInsecure();

    // Get bot information from Telegram
    if (!getBotInfo()) {
        DEBUG_PRINTLN("[Telegram] WARNING: Failed to get bot info");
        // Continue anyway - might be temporary network issue
    }

    // Save configuration to flash
    saveConfiguration();

    updateStatus(BOT_ONLINE);

    DEBUG_PRINTLN("[Telegram] Initialization complete");
    return true;
}

bool TelegramBot::beginFromStorage() {
    DEBUG_PRINTLN("[Telegram] Loading configuration from storage...");

    if (!loadConfiguration()) {
        DEBUG_PRINTLN("[Telegram] No stored configuration found");
        updateStatus(BOT_NO_TOKEN);
        return false;
    }

    // Configure HTTPS client
    client.setInsecure();

    // Get bot information
    if (!getBotInfo()) {
        DEBUG_PRINTLN("[Telegram] WARNING: Failed to get bot info");
    }

    updateStatus(BOT_ONLINE);

    DEBUG_PRINTLN("[Telegram] Loaded from storage successfully");
    return true;
}

// ===============================================================
// BOT CONFIGURATION
// ===============================================================

bool TelegramBot::setBotToken(const String& token) {
    if (!validateTokenFormat(token)) {
        return false;
    }

    botToken = token;
    DEBUG_PRINTLN("[Telegram] Bot token set");
    return true;
}

void TelegramBot::setAuthorizedUserId(int64_t userId) {
    authorizedUserId = userId;
    DEBUG_PRINTF("[Telegram] Authorized user ID: %lld\n", userId);
}

bool TelegramBot::saveConfiguration() {
    DEBUG_PRINTLN("[Telegram] Saving configuration...");

    preferences.putString(KEY_TELEGRAM_TOKEN, botToken);
    preferences.putLong64(KEY_TELEGRAM_USER_ID, authorizedUserId);

    DEBUG_PRINTLN("[Telegram] Configuration saved");
    return true;
}

bool TelegramBot::loadConfiguration() {
    DEBUG_PRINTLN("[Telegram] Loading configuration...");

    botToken = preferences.getString(KEY_TELEGRAM_TOKEN, "");
    authorizedUserId = preferences.getLong64(KEY_TELEGRAM_USER_ID, 0);

    if (botToken.length() == 0 || authorizedUserId == 0) {
        DEBUG_PRINTLN("[Telegram] Incomplete configuration");
        return false;
    }

    DEBUG_PRINTLN("[Telegram] Configuration loaded");
    return true;
}

bool TelegramBot::isConfigured() const {
    return (botToken.length() > 0 && authorizedUserId != 0);
}

// ===============================================================
// MESSAGE POLLING
// ===============================================================

bool TelegramBot::poll() {
    unsigned long currentTime = millis();

    // Don't poll too frequently (respect TELEGRAM_POLL_INTERVAL_MS)
    if (currentTime - lastPollTime < TELEGRAM_POLL_INTERVAL_MS) {
        return false;
    }

    lastPollTime = currentTime;

    if (!isConfigured()) {
        DEBUG_PRINTLN("[Telegram] Cannot poll - bot not configured");
        updateStatus(BOT_NO_TOKEN);
        return false;
    }

    DEBUG_PRINTLN("[Telegram] Polling for new messages...");

    // Build request parameters
    // offset = lastUpdateId + 1 (get only new messages)
    // limit = 10 (max 10 messages per request)
    // timeout = 5 (long polling - wait up to 5 seconds for new messages)
    String params = "offset=" + String(lastUpdateId + 1);
    params += "&limit=10";
    params += "&timeout=5";

    // Make API request
    String response = makeRequest("getUpdates", params);

    if (response.length() == 0) {
        DEBUG_PRINTLN("[Telegram] ERROR: Empty response");
        updateStatus(BOT_OFFLINE);
        return false;
    }

    // Parse JSON response
    JsonDocument doc;
    if (!parseResponse(response, doc)) {
        DEBUG_PRINTLN("[Telegram] ERROR: Failed to parse response");
        return false;
    }

    // Extract messages from response
    JsonArray results = doc["result"].as<JsonArray>();

    if (results.size() == 0) {
        // No new messages
        return false;
    }

    DEBUG_PRINTF("[Telegram] Received %d new message(s)\n", results.size());

    // Process each message
    for (JsonObject result : results) {
        // Update lastUpdateId to mark this message as processed
        lastUpdateId = result["update_id"].as<int32_t>();

        // Extract message data
        if (result.containsKey("message")) {
            JsonObject msg = result["message"];

            TelegramMessage telegramMsg;
            telegramMsg.chatId = msg["chat"]["id"].as<int64_t>();
            telegramMsg.messageId = msg["message_id"].as<int32_t>();
            telegramMsg.text = msg["text"].as<String>();
            telegramMsg.timestamp = msg["date"].as<unsigned long>();

            // Get username if available
            if (msg["from"].containsKey("username")) {
                telegramMsg.username = msg["from"]["username"].as<String>();
            } else {
                telegramMsg.username = "unknown";
            }

            // Check authorization
            if (!isAuthorized(telegramMsg.chatId)) {
                DEBUG_PRINTF("[Telegram] Unauthorized access from: %lld\n", telegramMsg.chatId);

                // Trigger unauthorized callback
                if (callbackUnauthorizedAccess != nullptr) {
                    callbackUnauthorizedAccess(telegramMsg.chatId, telegramMsg.text);
                }

                // Send warning to unauthorized user
                sendMessage(telegramMsg.chatId,
                          "⛔ Unauthorized. This device is registered to another user.");

                continue;  // Skip processing this message
            }

            // Add to message queue
            queueMessage(telegramMsg);

            // Process message immediately
            processMessage(telegramMsg);
        }
    }

    updateStatus(BOT_ONLINE);
    return true;
}

bool TelegramBot::getNextMessage(TelegramMessage& message) {
    if (messageQueueCount == 0) {
        return false;  // No messages in queue
    }

    // Get message from queue tail (FIFO)
    message = messageQueue[messageQueueTail];

    // Update queue pointers
    messageQueueTail = (messageQueueTail + 1) % MESSAGE_QUEUE_SIZE;
    messageQueueCount--;

    return true;
}

void TelegramBot::markAllRead() {
    DEBUG_PRINTLN("[Telegram] Marking all messages as read...");

    // Request with offset=-1 gets the latest update ID
    // Then we can start from there
    String response = makeRequest("getUpdates", "offset=-1&limit=1");

    if (response.length() == 0) {
        return;
    }

    JsonDocument doc;
    if (!parseResponse(response, doc)) {
        return;
    }

    JsonArray results = doc["result"].as<JsonArray>();
    if (results.size() > 0) {
        lastUpdateId = results[0]["update_id"].as<int32_t>();
        DEBUG_PRINTF("[Telegram] Last update ID: %d\n", lastUpdateId);
    }
}

// ===============================================================
// SENDING MESSAGES
// ===============================================================

bool TelegramBot::sendMessage(const String& text) {
    return sendMessage(authorizedUserId, text);
}

bool TelegramBot::sendMessage(int64_t chatId, const String& text) {
    if (!isConfigured()) {
        DEBUG_PRINTLN("[Telegram] Cannot send - bot not configured");
        return false;
    }

    DEBUG_PRINTF("[Telegram] Sending message to %lld: %s\n", chatId, text.c_str());

    // Build JSON request body
    JsonDocument doc;
    doc["chat_id"] = chatId;
    doc["text"] = text;
    doc["parse_mode"] = "Markdown";  // Enable markdown formatting

    String jsonBody;
    serializeJson(doc, jsonBody);

    // Make POST request
    String response = makePostRequest("sendMessage", jsonBody);

    if (response.length() == 0) {
        DEBUG_PRINTLN("[Telegram] ERROR: Failed to send message");
        return false;
    }

    // Check if successful
    JsonDocument responseDoc;
    if (!parseResponse(response, responseDoc)) {
        return false;
    }

    DEBUG_PRINTLN("[Telegram] Message sent successfully");
    return true;
}

bool TelegramBot::sendMessageWithButtons(const String& text,
                                        const String buttons[],
                                        int buttonCount) {
    if (!isConfigured()) {
        return false;
    }

    DEBUG_PRINTLN("[Telegram] Sending message with inline keyboard...");

    // Build JSON with inline keyboard
    JsonDocument doc;
    doc["chat_id"] = authorizedUserId;
    doc["text"] = text;

    JsonArray keyboard = doc["reply_markup"]["inline_keyboard"].to<JsonArray>();

    for (int i = 0; i < buttonCount; i++) {
        JsonArray row = keyboard.add<JsonArray>();
        JsonObject button = row.add<JsonObject>();
        button["text"] = buttons[i];
        button["callback_data"] = buttons[i];
    }

    String jsonBody;
    serializeJson(doc, jsonBody);

    // Make POST request
    String response = makePostRequest("sendMessage", jsonBody);

    return (response.length() > 0);
}

// ===============================================================
// COMMAND HANDLING
// ===============================================================

void TelegramBot::onCommand(const String& command,
                           std::function<void(TelegramMessage)> callback) {
    if (commandCallbackCount >= MAX_COMMANDS) {
        DEBUG_PRINTLN("[Telegram] ERROR: Max commands reached!");
        return;
    }

    // Store command and callback
    commandCallbacks[commandCallbackCount].command = command;
    commandCallbacks[commandCallbackCount].callback = callback;
    commandCallbackCount++;

    DEBUG_PRINTF("[Telegram] Registered command: %s\n", command.c_str());
}

void TelegramBot::processMessage(const TelegramMessage& message) {
    DEBUG_PRINTF("[Telegram] Processing: %s\n", message.text.c_str());

    // Check if message is a command (starts with '/')
    if (message.text.startsWith("/")) {
        // Extract command (everything before first space)
        int spaceIndex = message.text.indexOf(' ');
        String command = (spaceIndex > 0) ?
                        message.text.substring(0, spaceIndex) :
                        message.text;

        // Find matching callback
        for (int i = 0; i < commandCallbackCount; i++) {
            if (commandCallbacks[i].command.equalsIgnoreCase(command)) {
                DEBUG_PRINTF("[Telegram] Triggering callback for: %s\n", command.c_str());
                commandCallbacks[i].callback(message);
                return;
            }
        }

        // No callback found for this command
        DEBUG_PRINTF("[Telegram] Unknown command: %s\n", command.c_str());
        sendMessage("❓ Unknown command. Try:\n/wake - Start alarm\n/status - Device status\n/test - Test buzzers");
    }
}

// ===============================================================
// RATE LIMITING
// ===============================================================

bool TelegramBot::isWakeRateLimited() const {
    if (lastWakeTime == 0) {
        return false;  // Never used /wake before
    }

    unsigned long elapsed = millis() - lastWakeTime;
    return (elapsed < TELEGRAM_WAKE_COOLDOWN_MS);
}

unsigned long TelegramBot::getWakeCooldownRemaining() const {
    if (!isWakeRateLimited()) {
        return 0;
    }

    unsigned long elapsed = millis() - lastWakeTime;
    return (TELEGRAM_WAKE_COOLDOWN_MS - elapsed) / 1000;  // Convert to seconds
}

void TelegramBot::resetWakeRateLimit() {
    lastWakeTime = millis();
}

// ===============================================================
// STATUS & INFORMATION
// ===============================================================

TelegramBotStatus TelegramBot::getStatus() const {
    return status;
}

bool TelegramBot::isOnline() const {
    return (status == BOT_ONLINE);
}

String TelegramBot::getBotUsername() const {
    return botUsername;
}

int64_t TelegramBot::getAuthorizedUserId() const {
    return authorizedUserId;
}

String TelegramBot::getStatusString() const {
    String result = "[Telegram] ";

    switch (status) {
        case BOT_NOT_INITIALIZED:
            result += "Not Initialized";
            break;
        case BOT_NO_TOKEN:
            result += "No Token Configured";
            break;
        case BOT_CONNECTING:
            result += "Connecting...";
            break;
        case BOT_ONLINE:
            result += "Online - Polling every " + String(TELEGRAM_POLL_INTERVAL_MS / 1000) + "s";
            if (botUsername.length() > 0) {
                result += " (@" + botUsername + ")";
            }
            break;
        case BOT_OFFLINE:
            result += "Offline";
            break;
        case BOT_RATE_LIMITED:
            result += "Rate Limited";
            break;
        default:
            result += "Unknown";
    }

    return result;
}

// ===============================================================
// CALLBACK REGISTRATION
// ===============================================================

void TelegramBot::onOnline(std::function<void()> callback) {
    callbackOnline = callback;
}

void TelegramBot::onOffline(std::function<void()> callback) {
    callbackOffline = callback;
}

void TelegramBot::onUnauthorizedAccess(std::function<void(int64_t, String)> callback) {
    callbackUnauthorizedAccess = callback;
}

// ===============================================================
// PRIVATE HELPER FUNCTIONS
// ===============================================================

String TelegramBot::makeRequest(const String& endpoint, const String& params) {
    // Build URL: https://api.telegram.org/bot<TOKEN>/<endpoint>?<params>
    String url = "/bot" + botToken + "/" + endpoint;
    if (params.length() > 0) {
        url += "?" + params;
    }

    DEBUG_PRINTF("[Telegram] GET %s\n", url.c_str());

    // Connect to Telegram API
    if (!client.connect(TELEGRAM_HOST, TELEGRAM_PORT)) {
        DEBUG_PRINTLN("[Telegram] ERROR: Connection failed");
        return "";
    }

    // Send HTTP GET request
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + TELEGRAM_HOST + "\r\n" +
                "User-Agent: ESP32\r\n" +
                "Connection: close\r\n\r\n");

    // Wait for response with timeout
    unsigned long timeout = millis() + TELEGRAM_API_TIMEOUT_MS;
    while (client.available() == 0) {
        if (millis() > timeout) {
            DEBUG_PRINTLN("[Telegram] ERROR: Request timeout");
            client.stop();
            return "";
        }
        delay(10);
    }

    // Read response
    String response = "";
    bool headersEnded = false;

    while (client.available()) {
        String line = client.readStringUntil('\n');

        // Skip HTTP headers (everything before blank line)
        if (!headersEnded) {
            if (line == "\r") {
                headersEnded = true;
            }
            continue;
        }

        // Read response body (JSON)
        response += line;
    }

    client.stop();

    return response;
}

String TelegramBot::makePostRequest(const String& endpoint, const String& jsonBody) {
    // Build URL
    String url = "/bot" + botToken + "/" + endpoint;

    DEBUG_PRINTF("[Telegram] POST %s\n", url.c_str());

    // Connect to Telegram API
    if (!client.connect(TELEGRAM_HOST, TELEGRAM_PORT)) {
        DEBUG_PRINTLN("[Telegram] ERROR: Connection failed");
        return "";
    }

    // Send HTTP POST request
    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                "Host: " + TELEGRAM_HOST + "\r\n" +
                "User-Agent: ESP32\r\n" +
                "Content-Type: application/json\r\n" +
                "Content-Length: " + String(jsonBody.length()) + "\r\n" +
                "Connection: close\r\n\r\n" +
                jsonBody);

    // Wait for response
    unsigned long timeout = millis() + TELEGRAM_API_TIMEOUT_MS;
    while (client.available() == 0) {
        if (millis() > timeout) {
            DEBUG_PRINTLN("[Telegram] ERROR: Request timeout");
            client.stop();
            return "";
        }
        delay(10);
    }

    // Read response (skip headers like in GET)
    String response = "";
    bool headersEnded = false;

    while (client.available()) {
        String line = client.readStringUntil('\n');

        if (!headersEnded) {
            if (line == "\r") {
                headersEnded = true;
            }
            continue;
        }

        response += line;
    }

    client.stop();

    return response;
}

bool TelegramBot::getBotInfo() {
    DEBUG_PRINTLN("[Telegram] Getting bot info...");

    String response = makeRequest("getMe", "");

    if (response.length() == 0) {
        return false;
    }

    JsonDocument doc;
    if (!parseResponse(response, doc)) {
        return false;
    }

    // Extract bot username
    botUsername = doc["result"]["username"].as<String>();

    DEBUG_PRINTF("[Telegram] Bot username: @%s\n", botUsername.c_str());

    return true;
}

bool TelegramBot::parseResponse(const String& response, JsonDocument& doc) {
    // Parse JSON
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        DEBUG_PRINTF("[Telegram] JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Check if API call was successful
    bool ok = doc["ok"].as<bool>();

    if (!ok) {
        DEBUG_PRINTF("[Telegram] API error: %s\n",
                   doc["description"].as<String>().c_str());
        return false;
    }

    return true;
}

void TelegramBot::queueMessage(const TelegramMessage& message) {
    if (messageQueueCount >= MESSAGE_QUEUE_SIZE) {
        DEBUG_PRINTLN("[Telegram] WARNING: Message queue full, dropping oldest");
        // Drop oldest message (move tail forward)
        messageQueueTail = (messageQueueTail + 1) % MESSAGE_QUEUE_SIZE;
        messageQueueCount--;
    }

    // Add new message to queue head
    messageQueue[messageQueueHead] = message;
    messageQueueHead = (messageQueueHead + 1) % MESSAGE_QUEUE_SIZE;
    messageQueueCount++;
}

bool TelegramBot::isAuthorized(int64_t userId) const {
    return (userId == authorizedUserId);
}

void TelegramBot::updateStatus(TelegramBotStatus newStatus) {
    if (newStatus == status) {
        return;  // No change
    }

    TelegramBotStatus oldStatus = status;
    status = newStatus;

    DEBUG_PRINTF("[Telegram] Status changed: %d -> %d\n", oldStatus, newStatus);

    // Trigger callbacks
    if (newStatus == BOT_ONLINE && callbackOnline != nullptr) {
        callbackOnline();
    } else if (oldStatus == BOT_ONLINE && newStatus != BOT_ONLINE) {
        if (callbackOffline != nullptr) {
            callbackOffline();
        }
    }
}

bool TelegramBot::validateTokenFormat(const String& token) const {
    // Bot tokens have format: "123456789:ABC-DEF1234ghIkl-zyx57W2v1u123ew11"
    // Must contain ':' separator
    if (token.indexOf(':') == -1) {
        DEBUG_PRINTLN("[Telegram] Invalid token format (missing ':')");
        return false;
    }

    // Must be at least 20 characters
    if (token.length() < 20) {
        DEBUG_PRINTLN("[Telegram] Invalid token format (too short)");
        return false;
    }

    return true;
}

/*
 * ===============================================================
 * IMPLEMENTATION NOTES:
 * ===============================================================
 *
 * LONG POLLING vs WEBHOOKS:
 * This implementation uses "long polling" mode where we repeatedly
 * ask Telegram "any new messages?" every 5 seconds.
 *
 * Alternative is "webhooks" where Telegram pushes messages to our server.
 * But webhooks require:
 * - Public IP address or domain name
 * - SSL certificate (HTTPS)
 * - Port forwarding
 *
 * Long polling is MUCH simpler for home devices behind NAT/firewall!
 *
 * ===============================================================
 *
 * SECURITY CONSIDERATIONS:
 * 1. User ID Authorization: Only authorized user can send commands
 * 2. Rate Limiting: Prevents spam if token is leaked
 * 3. HTTPS: All communication encrypted (even though we use setInsecure())
 * 4. No Certificate Validation: Trade-off for simplicity and memory savings
 *
 * ===============================================================
 *
 * ERROR HANDLING:
 * - Network failures: Return false, caller can retry
 * - Invalid JSON: Log error, return false
 * - API errors: Log description from Telegram, return false
 * - Queue overflow: Drop oldest message (FIFO)
 *
 * ===============================================================
 */
