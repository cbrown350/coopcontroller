#ifndef MOCK_HAL_H__
#define MOCK_HAL_H__

#include <cstdarg>
#include <map>
#include "../HAL/IHAL.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>
#include <functional>
#include <cstring>

// ============================================================================
// MOCK WEB REQUEST IMPLEMENTATION
// ============================================================================

class MockWebRequest : public IWebRequest {
public:
    virtual ~MockWebRequest() = default;

    String url() const override {
        return mockUrl;
    }

    HAL_WebRequestMethod method() const override {
        return mockMethod;
    }

    bool hasParam(const char* name, bool post = false) const override {
        (void)name; (void)post;
        return false;
    }

    String param(const char* name, bool post = false) const override {
        (void)name; (void)post;
        return String();
    }

    void setJsonBody(const JsonVariant& json) override {
        (void)json;
    }

    const JsonVariant& jsonBody() const override {
        return mockJsonBody;
    }

    String body() const override {
        return mockBody;
    }

    bool hasHeader(const char* name) const override {
        return mockHeaders.find(name) != mockHeaders.end();
    }

    String header(const char* name) const override {
        auto it = mockHeaders.find(name);
        return (it != mockHeaders.end()) ? it->second : String();
    }

    // Test helpers
    void setUrl(const String& url) { mockUrl = url; }
    void setMethod(HAL_WebRequestMethod method) { mockMethod = method; }
    void setBody(const String& body) { mockBody = body; }
    void setHeader(const String& name, const String& value) { mockHeaders[name] = value; }

private:
    String mockUrl;
    HAL_WebRequestMethod mockMethod;
    JsonVariant mockJsonBody;
    String mockBody;
    std::map<String, String> mockHeaders;
};

// ============================================================================
// MOCK WEB RESPONSE IMPLEMENTATION
// ============================================================================

class MockWebResponse : public IWebResponse {
public:
    virtual ~MockWebResponse() = default;

    void send(int code, const char* contentType, const char* body) override {
        lastCode = code;
        lastContentType = contentType;
        lastBody = body;
    }

    void sendFile(const char* path, const char* contentType) override {
        (void)path; (void)contentType;
    }

    void setContentLength(size_t len) override {
        (void)len;
    }

    void setContentType(const char* type) override {
        (void)type;
    }

    void addHeader(const char* name, const char* value) override {
        responseHeaders[name] = value;
    }

    // Test helpers
    int getLastCode() const { return lastCode; }
    const char* getLastContentType() const { return lastContentType; }
    const char* getLastBody() const { return lastBody; }
    const std::map<String, String>& getResponseHeaders() const { return responseHeaders; }

private:
    std::map<String, String> responseHeaders;
    int lastCode = 200;
    const char* lastContentType = "";
    const char* lastBody = "";
};

// ============================================================================
// MOCK HAL IMPLEMENTATION
// ============================================================================

class MockHAL : public IHAL {
public:
    // ========================================================================
    // EXISTING METHODS - DO NOT MODIFY
    // ========================================================================
    
    bool begin() override {
        return true;
    }

    unsigned long getTime() override {
        return mockTime;
    }

    uint32_t getFreeHeap() override {
        return mockFreeHeap;
    }

    bool WiFiIsConnected() override {
        return mockWiFiConnected;
    }

    void SerialPrintf(const char* format, ...) override {
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        serialOutput += String(buffer);
    }

    void SerialPrintln(const char* message) override {
        serialOutput += String(message);
        serialOutput += "\n";
    }

    void SerialPrint(const char* message) override {
        serialOutput += String(message);
    }

    // ========================================================================
    // ESP32 SYSTEM FUNCTIONS
    // ========================================================================
    
    void restart() override {
        mockRestarted = true;
    }

    uint32_t getHeapSize() override {
        return mockHeapSize;
    }

    const char* getChipModel() override {
        return "ESP32-D0WDQ6";
    }

    uint8_t getResetReason() override {
        return mockResetReason;
    }

    uint32_t getCpuFreqMHz() override {
        return mockCpuFreqMHz;
    }

    uint32_t getFlashChipSize() override {
        return mockFlashChipSize;
    }

    void taskWdtReset() override {
        mockWdtReset = true;
    }

    // ========================================================================
    // WIFI MANAGEMENT
    // ========================================================================
    
    bool wifiBegin(const char* ssid, const char* password) override {
        mockSSID = String(ssid);
        mockWiFiPassword = String(password);
        return true;
    }

    bool wifiBeginWithBSSID(const char* ssid, const char* password, const uint8_t* bssid) override {
        mockSSID = String(ssid);
        mockWiFiPassword = String(password);
        (void)bssid;  // BSSID not used in mock
        return true;
    }

    bool wifiBeginAP(const char* ssid, const char* password = nullptr) override {
        // Store the AP SSID for testing
        if (ssid != nullptr) {
            mockSSID = String(ssid);
        }
        mockWiFiConnected = false;  // AP mode is not "connected" to a network
        mockAPIP = "192.168.4.1";  // Default AP IP
        (void)password;  // Password not used in mock
        return true;
    }

    bool wifiSetHostname(const char* hostName_) override {
        (void)hostName_;
        return true;
    }

    bool wifiIsConnected() override {
        return mockWiFiConnected;
    }

    String wifiGetSSID() override {
        return mockSSID;
    }

    String wifiGetBSSID() override {
        return mockBSSID;
    }

    String wifiGetMacAddress() override {
        return mockMacAddress;
    }

    String wifiGetLocalIP() override {
        return mockLocalIP;
    }

    String wifiGetAPIP() override {
        return mockAPIP;
    }

    int wifiGetRSSI() override {
        return mockRSSI;
    }

    void wifiDisconnect() override {
        mockWiFiConnected = false;
        mockWiFiStatus = 6; // WL_DISCONNECTED
        mockSSID = "";
        mockLocalIP = "0.0.0.0";
    }

    void wifiSetAutoReconnect(bool autoReconnect) override {
        (void)autoReconnect;
    }

    int wifiGetStatus() override {
        return mockWiFiStatus;
    }

    bool mdnsBegin(const char* hostname) override {
        (void)hostname;
        return true;
    }

    // ========================================================================
    // FILESYSTEM - LittleFS
    // ========================================================================
    
    bool fsBegin(bool formatOnFail = false) override {
        (void)formatOnFail;
        return true;
    }

    void fsEnd() override {
        // No-op for mock
    }

    bool fsExists(const char* path) override {
        if (mockFileExistsCallback) {
            return mockFileExistsCallback(path);
        }
        return false;
    }

    bool fsRemove(const char* path) override {
        if (mockFileRemoveCallback) {
            return mockFileRemoveCallback(path);
        }
        return false;
    }

    bool fsRename(const char* pathFrom, const char* pathTo) override {
        (void)pathFrom; (void)pathTo;
        return true;
    }

    HalFile fsOpen(const char* path, const char* mode) override {
        (void)mode;
        (void)path;
        // Return nullptr if no file content is set (simulates file not found)
        if (mockFileSize == 0 || mockFileContent == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<HalFile>(1);  // Mock file handle
    }

    void fsClose(HalFile file) override {
        (void)file;
    }

    size_t fsRead(HalFile file, uint8_t* buf, size_t size) override {
        (void)file;
        if (mockFileContent && buf && size > 0) {
            size_t copySize = (size < mockFileSize) ? size : mockFileSize;
            memcpy(buf, mockFileContent, copySize);
            return copySize;
        }
        return 0;
    }

    size_t fsWrite(HalFile file, const uint8_t* buf, size_t size) override {
        (void)file;
        if (buf && size > 0) {
            delete[] mockFileContent;
            mockFileContent = new uint8_t[size];
            memcpy(mockFileContent, buf, size);
            mockFileSize = size;
        }
        return size;
    }

    int fsAvailable(HalFile file) override {
        (void)file;
        return mockFileSize;
    }

    bool fsSeek(HalFile file, size_t pos) override {
        (void)file; (void)pos;
        return true;
    }

    size_t fsPosition(HalFile file) override {
        (void)file;
        return 0;
    }

    size_t fsSize(HalFile file) override {
        (void)file;
        return mockFileSize;
    }

    // ========================================================================
    // WEB SERVER - AsyncWebServer Abstraction
    // ========================================================================
    
    bool webServerBegin(uint16_t port) override {
        (void)port;
        return true;
    }

    void webServerOn(const char* uri, HAL_WebRequestMethod method, WebServerHandler handler) override {
        (void)uri; (void)method;
        mockWebServerHandler = handler;
    }

    void webServerAddHandler(WebServerHandler handler) override {
        mockWebServerHandler = handler;
    }

    void webServerAddRewrite(const char* from, const char* to) override {
        (void)from; (void)to;
    }

    void webServerServeStatic(const char* uri, const char* path) override {
        (void)uri; (void)path;
    }

    void webServerOnNotFound(WebServerHandler handler) override {
        mockWebServerNotFoundHandler = handler;
    }

    void webServerLoop() override {
        // No-op for mock
    }

    void webServerAddElegantOTA() override {
        // No-op for mock
    }

    // ========================================================================
    // LEDC (LED Control) PWM Functions
    // ========================================================================
    
    void pwmSetup(uint8_t channel, uint32_t freq, uint8_t resolution) override {
        pwmSetupCalled = true;
        pwmSetupChannel = channel;
        pwmSetupFreq = freq;
        pwmSetupResolution = resolution;
        mockPwmChannel = channel;
        mockPwmFreq = freq;
        mockPwmResolution = resolution;
    }

    void pwmAttachPin(uint8_t pin, uint8_t channel) override {
        pwmAttachPinCalled = true;
        pwmAttachPinPin = pin;
        pwmAttachPinChannel = channel;
        mockPwmPin = pin;
    }

    void pwmWrite(uint8_t channel, uint32_t duty) override {
        pwmWriteCalled = true;
        pwmWriteChannel = channel;
        pwmWriteValue = duty;
        mockPwmDuty = duty;
    }

    // ========================================================================
    // GPIO AND FREERTOS FUNCTIONS
    // ========================================================================
    
    void pinMode(uint8_t pin, uint8_t mode) override {
        mockPinModes[pin] = mode;
        mockPinStates[pin] = 0;  // Default to LOW
    }

    void digitalWrite(uint8_t pin, uint8_t value) override {
        mockPinStates[pin] = value;
    }

    int getCoreID() override {
        return mockCoreID;
    }

    void* getCurrentTaskHandle() override {
        return mockTaskHandle;
    }

    bool getLocalTime(struct tm* timeinfo, unsigned long ms) override {
        (void)ms;
        if (timeinfo) {
            *timeinfo = mockTimeInfo;
        }
        return mockGetLocalTimeResult;
    }

    unsigned long millis() {
        return millisValue;
    }

    // ========================================================================
    // TEST HELPERS
    // ========================================================================

    // Reset all mock state
    void reset() {
        // Reset PWM state
        pwmSetupCalled = false;
        pwmSetupChannel = 0;
        pwmSetupFreq = 0;
        pwmSetupResolution = 0;
        pwmAttachPinCalled = false;
        pwmAttachPinPin = 0;
        pwmAttachPinChannel = 0;
        pwmWriteCalled = false;
        pwmWriteChannel = 0;
        pwmWriteValue = 0;

        // Reset time
        millisValue = 0;
        mockTime = 0;
        mockGetLocalTimeResult = false;
        mockTimeInfo = {0};

        // Reset WiFi state
        mockWiFiConnected = false;
        mockWiFiStatus = 0;
        mockSSID = "";
        mockWiFiPassword = "";
        mockBSSID = "";
        mockMacAddress = "";
        mockLocalIP = "";
        mockAPIP = "";
        mockRSSI = -127;

        // Reset system state
        mockFreeHeap = 200000;
        mockHeapSize = 300000;
        mockResetReason = 0;
        mockRestarted = false;
        mockWdtReset = false;

        // Reset serial
        serialOutput = "";
    }

    // Time helpers
    void setTime(unsigned long time) { mockTime = time; }
    void setGetLocalTimeResult(bool result) { mockGetLocalTimeResult = result; }
    void setTimeInfo(const struct tm& timeinfo) { mockTimeInfo = timeinfo; }
    void setMillis(unsigned long time) { millisValue = time; mockTime = time; }
    
    // WiFi helpers
    void setWiFiConnected(bool connected) { mockWiFiConnected = connected; }
    void setWiFiStatus(int status) {
        mockWiFiStatus = status;
        // Auto-sync connected state based on status (WL_CONNECTED = 3)
        mockWiFiConnected = (status == 3);
    }
    void setWiFiSSID(const String& ssid) { mockSSID = ssid; }
    void setWiFiBSSID(const String& bssid) { mockBSSID = bssid; }
    void setWiFiMacAddress(const String& mac) { mockMacAddress = mac; }
    void setWiFiLocalIP(const String& ip) { mockLocalIP = ip; }
    void setWiFiAPIP(const String& ip) { mockAPIP = ip; }
    void setWiFiRSSI(int rssi) { mockRSSI = rssi; }
    
    // System helpers
    void setFreeHeap(uint32_t heap) { mockFreeHeap = heap; }
    void setHeapSize(uint32_t size) { mockHeapSize = size; }
    void setResetReason(uint8_t reason) { mockResetReason = reason; }
    void setRestarted(bool restarted) { mockRestarted = restarted; }
    void setWdtReset(bool reset) { mockWdtReset = reset; }
    
    // Filesystem helpers
    void setFileExistsCallback(std::function<bool(const char*)> callback) {
        mockFileExistsCallback = callback;
    }
    void setFileRemoveCallback(std::function<bool(const char*)> callback) {
        mockFileRemoveCallback = callback;
    }
    void setFileContent(const char* content, size_t size) {
        delete[] mockFileContent;
        mockFileContent = new uint8_t[size];
        memcpy(mockFileContent, content, size);
        mockFileSize = size;
    }

    void clearFileContent() {
        delete[] mockFileContent;
        mockFileContent = nullptr;
        mockFileSize = 0;
    }
    
    // Serial helpers
    String getSerialOutput() const { return serialOutput; }
    void clearSerialOutput() { serialOutput = ""; }
    
    // PWM helpers
    uint8_t getPwmChannel() const { return mockPwmChannel; }
    uint32_t getPwmFreq() const { return mockPwmFreq; }
    uint8_t getPwmResolution() const { return mockPwmResolution; }
    uint8_t getPwmPin() const { return mockPwmPin; }
    uint32_t getPwmDuty() const { return mockPwmDuty; }
    
    // Web server helpers
    WebServerHandler getWebServerHandler() const { return mockWebServerHandler; }
    WebServerHandler getWebServerNotFoundHandler() const { return mockWebServerNotFoundHandler; }

    // ========================================================================
    // PUBLIC TEST FIELDS (accessed directly by tests)
    // ========================================================================

    // PWM test flags
    bool pwmSetupCalled = false;
    uint8_t pwmSetupChannel = 0;
    uint32_t pwmSetupFreq = 0;
    uint8_t pwmSetupResolution = 0;
    bool pwmAttachPinCalled = false;
    uint8_t pwmAttachPinPin = 0;
    uint8_t pwmAttachPinChannel = 0;
    bool pwmWriteCalled = false;
    uint8_t pwmWriteChannel = 0;
    uint32_t pwmWriteValue = 0;

    // Time test field
    unsigned long millisValue = 0;

private:
    // Time state
    unsigned long mockTime = 0;
    
    // WiFi state
    bool mockWiFiConnected = false;
    int mockWiFiStatus = 0;  // WL_DISCONNECTED = 0
    String mockSSID = "";
    String mockWiFiPassword = "";
    String mockBSSID = "";
    String mockMacAddress = "";
    String mockLocalIP = "";
    String mockAPIP = "";
    int mockRSSI = -127;
    
    // System state
    uint32_t mockFreeHeap = 200000;
    uint32_t mockHeapSize = 300000;
    uint8_t mockResetReason = 0;
    uint32_t mockCpuFreqMHz = 240;
    uint32_t mockFlashChipSize = 4194304;  // 4MB
    bool mockRestarted = false;
    bool mockWdtReset = false;
    
    // Filesystem state
    std::function<bool(const char*)> mockFileExistsCallback;
    std::function<bool(const char*)> mockFileRemoveCallback;
    uint8_t* mockFileContent = nullptr;
    size_t mockFileSize = 0;
    
    // Serial state
    String serialOutput = "";
    
    // PWM state
    uint8_t mockPwmChannel = 0;
    uint32_t mockPwmFreq = 1000;
    uint8_t mockPwmResolution = 8;
    uint8_t mockPwmPin = 0;
    uint32_t mockPwmDuty = 0;
    
    // Web server state
    WebServerHandler mockWebServerHandler = nullptr;
    WebServerHandler mockWebServerNotFoundHandler = nullptr;
    
    // GPIO state
    uint8_t mockPinModes[256] = {0};  // Array to store pin modes
    uint8_t mockPinStates[256] = {0};  // Array to store pin states
    
    // FreeRTOS state
    int mockCoreID = 0;           // Mock core ID
    void* mockTaskHandle = nullptr;  // Mock task handle
    
    // Time info state for getLocalTime()
    bool mockGetLocalTimeResult = true;
    struct tm mockTimeInfo = {0};
};

// WiFi status constants (matching ESP32 WiFi.h)
#define WL_NO_SHIELD        255
#define WL_IDLE_STATUS       0
#define WL_NO_SSID_AVAIL     1
#define WL_SCAN_COMPLETED    2
#define WL_CONNECTED         3
#define WL_CONNECT_FAILED     4
#define WL_CONNECTION_LOST   5
#define WL_DISCONNECTED     6

#endif // MOCK_HAL_H__
