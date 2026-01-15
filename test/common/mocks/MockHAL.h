#ifndef __MOCKHAL_H__
#define __MOCKHAL_H__

#include "IHAL.h"
#include <Arduino.h>

#include <stdint.h>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <string>
#include <cstring>
#include <vector>

// On desktop builds Arduino 'String' type isn't available; alias it to std::string
// using String = std::string;

/**
 * @brief Mock Web Request Implementation
 *
 * Implements IWebRequest interface for desktop testing.
 * Stores request properties (URL, method, parameters) for test verification.
 */
class MockWebRequest : public IWebRequest {
public:
    /**
     * @brief Constructor
     * @param url Request URL path
     * @param method HTTP request method
     */
    MockWebRequest(const char* url, HAL_WebRequestMethod method)
        : url_(url), method_(method) {}

    /**
     * @brief Get request URL path
     * @return URL path as string
     */
    String url() const override {
        return String(url_.c_str());
    }

    /**
     * @brief Get HTTP request method
     * @return HTTP method enumeration
     */
    HAL_WebRequestMethod method() const override {
        return method_;
    }

    /**
     * @brief Get request body (POST/PUT content)
     * @return Empty string for mock implementation
     */
    String body() const override {
        return String("");
    }

    /**
     * @brief Check if parameter exists
     * @param name Parameter name
     * @param post true to check POST parameters, false for GET/URL parameters
     * @return true if parameter exists
     */
    bool hasParam(const char* name, bool post = false) const override {
        (void)post; // Suppress unused parameter warning
        return params_.find(name) != params_.end();
    }

    /**
     * @brief Get parameter value
     * @param name Parameter name
     * @param post true to get POST parameter, false for GET/URL parameter
     * @return Parameter value as string
     */
    String param(const char* name, bool post = false) const override {
        (void)post; // Suppress unused parameter warning
        auto it = params_.find(name);
        if (it != params_.end()) {
            return String(it->second.c_str());
        }
        return String("");
    }

    /**
     * @brief Set JSON body for request
     * @param json JsonVariant containing JSON data
     */
    void setJsonBody(const JsonVariant& json) override {
        (void)json; // Suppress unused parameter warning
        // Store JSON body for testing (mock implementation)
        jsonBody_ = json;
    }

    /**
     * @brief Get JSON body
     * @return JsonVariant containing JSON data
     */
    const JsonVariant& jsonBody() const override {
        return jsonBody_;
    }

    /**
     * @brief Add a parameter to request (for testing)
     * @param name Parameter name
     * @param value Parameter value
     */
    void addParam(const char* name, const char* value) {
        params_[name] = value;
    }

private:
    std::string url_;
    HAL_WebRequestMethod method_;
    std::map<std::string, std::string> params_;
    JsonVariant jsonBody_;
};

/**
 * @brief Mock Web Response Implementation
 *
 * Implements IWebResponse interface for desktop testing.
 * Stores response data for test verification.
 */
class MockWebResponse : public IWebResponse {
public:
    /**
     * @brief Default constructor
     */
    MockWebResponse() = default;

    /**
     * @brief Get HTTP status code
     * @return HTTP status code
     */
    int getCode() const { return code_; }

    /**
     * @brief Get content type
     * @return Content type string
     */
    String getContentType() const { return String(contentType_.c_str()); }

    /**
     * @brief Get response body content
     * @return Response body string
     */
    String getContent() const { return String(content_.c_str()); }

    /**
     * @brief Get file path (for sendFile calls)
     * @return File path string
     */
    String getFilePath() const { return String(filePath_.c_str()); }

    /**
     * @brief Get content length
     * @return Content length in bytes
     */
    size_t getContentLength() const { return contentLength_; }

    /**
     * @brief Send HTTP response with content
     * @param code HTTP status code
     * @param contentType Content type (e.g., "text/plain", "application/json")
     * @param body Response body content
     */
    void send(int code, const char* contentType, const char* body) override {
        code_ = code;
        contentType_ = contentType ? contentType : "";
        content_ = body ? body : "";
    }

    /**
     * @brief Send file from filesystem as HTTP response
     * @param path File path in filesystem
     * @param contentType Content type (e.g., "text/html")
     */
    void sendFile(const char* path, const char* contentType) override {
        filePath_ = path ? path : "";
        contentType_ = contentType ? contentType : "";
    }

    /**
     * @brief Set response content length
     * @param len Content length in bytes
     */
    void setContentLength(size_t len) override {
        contentLength_ = len;
    }

    /**
     * @brief Set response content type
     * @param type Content type string (e.g., "text/html")
     */
    void setContentType(const char* type) override {
        contentType_ = type ? type : "";
    }

    /**
     * @brief Clear response data
     */
    void clear() {
        code_ = 0;
        contentType_.clear();
        content_.clear();
        filePath_.clear();
        contentLength_ = 0;
    }

private:
    int code_ = 0;
    std::string contentType_;
    std::string content_;
    std::string filePath_;
    size_t contentLength_ = 0;
};

/**
 * @brief PWM Channel State Structure for Testing
 *
 * Stores the state of a PWM channel for test verification.
 */
struct PWMChannelState {
    bool configured = false;
    uint8_t pin = 0;
    uint32_t freq = 0;
    uint8_t resolution = 0;
    uint32_t duty = 0;
};

/**
 * @brief Mock Hardware Abstraction Layer for Desktop Testing
 *
 * Implements IHAL interface with in-memory mock implementations
 * for ESP32 system, WiFi, filesystem, web server and PWM functionality.
 */
class MockHAL : public IHAL // NOSONAR
{
public:
    bool wifiConnected = true;

    // ========================================================================
    // EXISTING METHODS - DO NOT MODIFY
    // ========================================================================

    void SerialPrintf(const char* format, ...) override { // NOSONAR
        va_list args;
        va_start(args, format);
        vprintf(format, args); // NOSONAR
        va_end(args);
    }

    void SerialPrintln(const char* message) override {
        printf("%s\n", message);
    }

    void SerialPrint(const char* message) override {
        printf("%s", message);
    }

    bool begin() override {
        printf("MockHAL initialized\n");
        return true;
    }

    unsigned long getTime() override {
        return 123456789; // Return a fixed timestamp for testing
    }

    uint32_t getFreeHeap() override {
        return 102400; // Return a fixed heap size for testing
    }

    bool WiFiIsConnected() override {
        return wifiConnected;
    }

    // ========================================================================
    // ESP32 SYSTEM FUNCTIONS
    // ========================================================================

    void restart() override {
        printf("MockHAL: System restart requested\n");
    }

    uint32_t getHeapSize() override {
        return 524288; // Return a fixed total heap size for testing
    }

    const char* getChipModel() override {
        return "ESP32-D0WDQ6"; // Mock chip model
    }

    uint8_t getResetReason() override {
        return 12; // Mock reset reason (ESP_RST_POWERON)
    }

    // ========================================================================
    // WIFI MANAGEMENT
    // ========================================================================

    bool wifiBegin(const char* ssid, const char* password) override {
        mockWifiConnected = true;
        printf("MockHAL: WiFi connecting to %s\n", ssid);
        return true;
    }

    bool wifiBeginAP(const char* ssid, const char* password = nullptr) override {
        mockWifiAP = true;
        printf("MockHAL: WiFi AP mode started with SSID %s\n", ssid);
        return true;
    }

    bool wifiIsConnected() override {
        return mockWifiConnected;
    }

    String wifiGetSSID() override {
        return String("MockSSID");
    }

    String wifiGetLocalIP() override {
        return String("192.168.1.100");
    }

    int wifiGetRSSI() override {
        return -50;  // Mock signal strength
    }

    void wifiDisconnect() override {
        mockWifiConnected = false;
        printf("MockHAL: WiFi disconnected\n");
    }

    void wifiSetAutoReconnect(bool autoReconnect) override {
        // No-op for mock
        (void)autoReconnect;
    }

    int wifiGetStatus() override {
        return mockWifiConnected ? 3 : 0;  // 3 = WL_CONNECTED, 0 = WL_IDLE_STATUS
    }

    bool wifiSetHostname(const char* hostName_) override {
        printf("MockHAL: WiFi hostname set to %s\n", hostName_);
        return true;
    }

    String wifiGetBSSID() override {
        return String("AA:BB:CC:DD:EE:FF");
    }

    String wifiGetMacAddress() override {
        return String("AA:BB:CC:DD:EE:FF");
    }

    String wifiGetAPIP() override {
        return String("192.168.4.1");
    }

    bool mdnsBegin(const char* hostname) override {
        printf("MockHAL: mDNS started for hostname %s\n", hostname);
        return true;
    }

    // ========================================================================
    // FILESYSTEM - LittleFS
    // ========================================================================

    bool fsBegin(bool formatOnFail = false) override {
        (void)formatOnFail;
        printf("MockHAL: Filesystem initialized\n");
        return true;
    }

    void fsEnd() override {
        mockFiles.clear();
        printf("MockHAL: Filesystem ended\n");
    }

    bool fsExists(const char* path) override {
        return mockFiles.find(path) != mockFiles.end();
    }

    bool fsRemove(const char* path) override {
        return mockFiles.erase(path) > 0;
    }

    bool fsRename(const char* pathFrom, const char* pathTo) override {
        auto it = mockFiles.find(pathFrom);
        if (it != mockFiles.end()) {
            mockFiles[pathTo] = it->second;
            mockFiles.erase(it);
            return true;
        }
        return false;
    }

    HalFile fsOpen(const char* path, const char* mode) override {
        HalFile handle;  // Dummy handle
        if (mode[0] == 'r') {
            // Read mode - use existing content or empty
            auto it = mockFiles.find(path);
            mockFileHandles[handle] = (it != mockFiles.end()) ? it->second : "";
        } else {
            // Write mode - create empty
            mockFileHandles[handle] = "";
        }
        mockFilePositions[handle] = 0;
        return handle;
    }

    void fsClose(HalFile file) override {
        if (file) {
            mockFileHandles.erase(file);
            mockFilePositions.erase(file);
            delete (int*)file;
        }
    }

    size_t fsRead(HalFile file, uint8_t* buf, size_t size) override {
        if (!file) return 0;
        auto& content = mockFileHandles[file];
        size_t pos = mockFilePositions[file];
        size_t available = content.size() - pos;
        size_t toRead = (size < available) ? size : available;
        if (toRead > 0) {
            memcpy(buf, content.data() + pos, toRead);
            mockFilePositions[file] += toRead;
        }
        return toRead;
    }

    size_t fsWrite(HalFile file, const uint8_t* buf, size_t size) override {
        if (!file) return 0;
        auto& content = mockFileHandles[file];
        content.append((const char*)buf, size);
        mockFilePositions[file] += size;
        return size;
    }

    int fsAvailable(HalFile file) override {
        if (!file) return 0;
        auto& content = mockFileHandles[file];
        return content.size() - mockFilePositions[file];
    }

    bool fsSeek(HalFile file, size_t pos) override {
        if (!file) return false;
        mockFilePositions[file] = pos;
        return true;
    }

    size_t fsPosition(HalFile file) override {
        return file ? mockFilePositions[file] : 0;
    }

    size_t fsSize(HalFile file) override {
        return file ? mockFileHandles[file].size() : 0;
    }

    // ========================================================================
    // WEB SERVER - Mock Implementation
    // ========================================================================

    /**
     * @brief Initialize mock web server on specified port
     * @param port TCP port to listen on (e.g., 80)
     * @return true if web server started successfully
     */
    bool webServerBegin(uint16_t port) override {
        mockWebServerPort = port;
        mockWebServerHandlers.clear();
        mockWebServerCustomHandlers.clear();
        mockWebServerRewrites.clear();
        mockWebServerStatic.clear();
        mockWebServerNotFoundHandler = nullptr;
        printf("MockHAL: Web server initialized on port %d\n", port);
        return true;
    }

    /**
     * @brief Register HTTP request handler for specific URI and method
     * @param uri URI path (e.g., "/get_settings")
     * @param method HTTP request method
     * @param handler Callback function to handle requests
     */
    void webServerOn(const char* uri, HAL_WebRequestMethod method, WebServerHandler handler) override {
        // Create composite key from URI and method
        std::string key = std::string(uri) + ":" + std::to_string(static_cast<int>(method));
        mockWebServerHandlers[key] = handler;
        printf("MockHAL: Registered handler for %s (method: %d)\n", uri, static_cast<int>(method));
    }

    /**
     * @brief Add custom handler to web server
     * @param handler Custom handler callback
     */
    void webServerAddHandler(WebServerHandler handler) override {
        mockWebServerCustomHandlers.push_back(handler);
        printf("MockHAL: Added custom handler\n");
    }

    /**
     * @brief Add URL rewrite rule for SPA routing
     * @param from Source URI pattern
     * @param to Destination URI path
     */
    void webServerAddRewrite(const char* from, const char* to) override {
        mockWebServerRewrites[from] = to;
        printf("MockHAL: Added rewrite rule: %s -> %s\n", from, to);
    }

    /**
     * @brief Configure static file serving from filesystem
     * @param uri URI prefix for static files (e.g., "/assets/")
     * @param path Filesystem path to serve from (e.g., "/assets/")
     */
    void webServerServeStatic(const char* uri, const char* path) override {
        mockWebServerStatic[uri] = path;
        printf("MockHAL: Configured static serving: %s -> %s\n", uri, path);
    }

    /**
     * @brief Set handler for requests that don't match any registered route
     * @param handler Callback function for 404/not found requests
     */
    void webServerOnNotFound(WebServerHandler handler) override {
        mockWebServerNotFoundHandler = handler;
        printf("MockHAL: Set 404 handler\n");
    }

    /**
     * @brief Add ElegantOTA support to web server
     */
    void webServerAddElegantOTA() override {
        printf("MockHAL: ElegantOTA support added to web server\n");
    }

    /**
     * @brief Process web server events (no-op for mock)
     */
    void webServerLoop() override {
        // No-op for mock implementation
        // In real implementation, this would handle async events, OTA, etc.
    }

    // ========================================================================
    // LEDC (LED Control) PWM Functions
    // ========================================================================

    /**
     * @brief Setup LEDC PWM channel
     * @param channel PWM channel number (0-15 on ESP32)
     * @param freq PWM frequency in Hz
     * @param resolution PWM resolution in bits (1-16)
     */
    void pwmSetup(uint8_t channel, uint32_t freq, uint8_t resolution) override {
        // Store PWM configuration for testing
        _pwmChannels[channel].configured = true;
        _pwmChannels[channel].freq = freq;
        _pwmChannels[channel].resolution = resolution;
        printf("MockHAL: PWM channel %d setup: freq=%dHz, resolution=%d bits\n", 
               channel, freq, resolution);
    }

    /**
     * @brief Attach LEDC PWM channel to GPIO pin
     * @param pin GPIO pin number
     * @param channel PWM channel number
     */
    void pwmAttachPin(uint8_t pin, uint8_t channel) override {
        // Store pin-to-channel mapping for testing
        _pwmChannels[channel].pin = pin;
        printf("MockHAL: PWM channel %d attached to pin %d\n", channel, pin);
    }

    /**
     * @brief Write PWM duty cycle to channel
     * @param channel PWM channel number
     * @param duty Duty cycle value (0 to 2^resolution-1)
     */
    void pwmWrite(uint8_t channel, uint32_t duty) override {
        // Store duty cycle value for testing
        _pwmChannels[channel].duty = duty;
        printf("MockHAL: PWM channel %d duty cycle set to %d\n", channel, duty);
    }

    // ========================================================================
    // TESTING HELPER METHODS
    // ========================================================================

    /**
     * @brief Get PWM channel configuration for testing
     * @param channel PWM channel number
     * @return Pointer to PWM channel state or nullptr if not configured
     */
    const PWMChannelState* getPWMChannelState(uint8_t channel) const {
        auto it = _pwmChannels.find(channel);
        if (it != _pwmChannels.end() && it->second.configured) {
            return &(it->second);
        }
        return nullptr;
    }

    /**
     * @brief Clear all PWM channel states
     */
    void clearPWMChannels() {
        _pwmChannels.clear();
        printf("MockHAL: All PWM channels cleared\n");
    }

    /**
     * @brief Simulate HTTP request to invoke registered handler
     * @param uri Request URI path
     * @param method HTTP request method
     * @param body Optional request body (for POST requests)
     * @return Pointer to last response (for test verification)
     */
    MockWebResponse* simulateRequest(const char* uri, HAL_WebRequestMethod method, const char* body = nullptr) {
        // Create mock request and response
        MockWebRequest mockRequest(uri, method);
        mockLastResponse.clear();
        
        // Find handler for this URI and method
        std::string key = std::string(uri) + ":" + std::to_string(static_cast<int>(method));
        auto it = mockWebServerHandlers.find(key);
        
        if (it != mockWebServerHandlers.end()) {
            // Invoke handler with both request and response
            it->second(&mockRequest, &mockLastResponse);
        } else if (mockWebServerNotFoundHandler != nullptr) {
            // Invoke 404 handler with both request and response
            mockWebServerNotFoundHandler(&mockRequest, &mockLastResponse);
        }
        
        // Return pointer to last response for verification
        return &mockLastResponse;
    }

    /**
     * @brief Get last response for test verification
     * @return Pointer to last response
     */
    MockWebResponse* getLastResponse() {
        return &mockLastResponse;
    }

    /**
     * @brief Clear stored responses
     */
    void clearResponses() {
        mockLastResponse.clear();
    }

    /**
     * @brief Get registered handler for URI and method
     * @param uri Request URI path
     * @param method HTTP request method
     * @return Handler function or nullptr if not found
     */
    WebServerHandler getHandler(const char* uri, HAL_WebRequestMethod method) {
        std::string key = std::string(uri) + ":" + std::to_string(static_cast<int>(method));
        auto it = mockWebServerHandlers.find(key);
        return (it != mockWebServerHandlers.end()) ? it->second : nullptr;
    }

    /**
     * @brief Get list of all registered URIs
     * @return Vector of registered URI strings
     */
    std::vector<std::string> getRegisteredURIs() {
        std::vector<std::string> uris;
        for (const auto& entry : mockWebServerHandlers) {
            // Extract URI from composite key (before colon)
            size_t colonPos = entry.first.find(':');
            if (colonPos != std::string::npos) {
                uris.push_back(entry.first.substr(0, colonPos));
            }
        }
        return uris;
    }

    /**
     * @brief Get web server port
     * @return Port number
     */
    uint16_t getWebServerPort() const {
        return mockWebServerPort;
    }

    /**
     * @brief Get static file configuration
     * @param uri URI prefix to query
     * @return Filesystem path or empty string if not configured
     */
    std::string getStaticPath(const char* uri) {
        auto it = mockWebServerStatic.find(uri);
        return (it != mockWebServerStatic.end()) ? it->second : "";
    }

private:
    bool mockWifiConnected = false;
    bool mockWifiAP = false;
    std::map<std::string, std::string> mockFiles;
    std::map<HalFile, std::string> mockFileHandles;
    std::map<HalFile, size_t> mockFilePositions;

    // Web server mock storage
    uint16_t mockWebServerPort = 0;
    std::map<std::string, WebServerHandler> mockWebServerHandlers;
    std::vector<WebServerHandler> mockWebServerCustomHandlers;
    std::map<std::string, std::string> mockWebServerRewrites;
    std::map<std::string, std::string> mockWebServerStatic;
    WebServerHandler mockWebServerNotFoundHandler = nullptr;
    MockWebResponse mockLastResponse;

    // PWM channel mock storage
    std::map<uint8_t, PWMChannelState> _pwmChannels;
};

#endif // __MOCKHAL_H__
