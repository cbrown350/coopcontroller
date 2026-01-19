#include "HAL_ESP32.h"

// ESP32-specific includes
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoOTA.h>
#include <esp_system.h>  // For esp_reset_reason()
#include <esp_task_wdt.h>  // For esp_task_wdt_reset()
#include <esp32-hal-ledc.h> // For LEDC functions

/**
 * @brief Constructor
 */
HAL_ESP32::HAL_ESP32() : server_(nullptr) {
}

/**
 * @brief Destructor
 */
HAL_ESP32::~HAL_ESP32() {
  if (server_ != nullptr) {
    delete server_;
    server_ = nullptr;
  }
}

// ========================================================================
// EXISTING METHODS - DO NOT MODIFY
// ========================================================================

bool HAL_ESP32::begin() {
  // Initialize items not explicitly handled elsewhere
  bool success = true;
  return success;
}

unsigned long HAL_ESP32::getTime() {
    time_t now;
    if (struct tm timeinfo; !getLocalTime(&timeinfo, 200)) { // Add timeout to prevent hanging
        if (static unsigned long warned = 0; millis() - warned > 30000 || warned == 0) { // Warn at most once per X
            warned = millis();
            Serial.println("Failed to obtain time"); // Can't call logger since it may call getTime()
        }
        return millis();
    }
    time(&now);
    return now;
}

uint32_t HAL_ESP32::getFreeHeap() {
  return ESP.getFreeHeap();
}

bool HAL_ESP32::WiFiIsConnected() {
  return WiFi.status() == WL_CONNECTED; // NOSONAR - standard way to call status()
}

void HAL_ESP32::SerialPrintf(const char* format, ...) { // NOSONAR - intentional
  char buffer[512]; // NOSONAR
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args); // NOSONAR
  va_end(args);
  Serial.print(String(buffer));
}

void HAL_ESP32::SerialPrintln(const char* message) {
  Serial.println(message);
}

void HAL_ESP32::SerialPrint(const char* message) {
  Serial.print(message);
}

// ========================================================================
// ESP32 SYSTEM FUNCTIONS
// ========================================================================

void HAL_ESP32::restart() {
  ESP.restart();
}

uint32_t HAL_ESP32::getHeapSize() {
  return ESP.getHeapSize();
}

const char* HAL_ESP32::getChipModel() {
  return ESP.getChipModel();
}

uint8_t HAL_ESP32::getResetReason() {
  return static_cast<uint8_t>(esp_reset_reason());
}

void HAL_ESP32::taskWdtReset() {
  esp_task_wdt_reset();
}

// ========================================================================
// WIFI MANAGEMENT
// ========================================================================

bool HAL_ESP32::wifiBegin(const char* ssid, const char* password) {
  WiFiClass::mode(WIFI_STA);
  WiFi.begin(ssid, password);
  return true;
}

bool HAL_ESP32::wifiBeginAP(const char* ssid, const char* password) {
  WiFiClass::mode(WIFI_AP);
  if (password != nullptr) {
    WiFi.softAP(ssid, password);
  } else {
    WiFi.softAP(ssid);
  }
  return true;
}

bool HAL_ESP32::wifiSetHostname(const char* hostName_) {
  return WiFiClass::setHostname(hostName_) && WiFi.softAPsetHostname(hostName_);
}

bool HAL_ESP32::wifiIsConnected() {
  return WiFiClass::status() == WL_CONNECTED;
}

String HAL_ESP32::wifiGetSSID() {
  return WiFi.SSID();
}

String HAL_ESP32::wifiGetBSSID() {
  return WiFi.BSSIDstr();
}

String HAL_ESP32::wifiGetMacAddress() {
  return WiFi.macAddress();
}

String HAL_ESP32::wifiGetLocalIP() {
  return WiFi.localIP().toString();
}

String HAL_ESP32::wifiGetAPIP() {
  return WiFi.softAPIP().toString();
}

int HAL_ESP32::wifiGetRSSI() {
  return WiFi.RSSI();
}

void HAL_ESP32::wifiDisconnect() {
  WiFi.disconnect();
}

void HAL_ESP32::wifiSetAutoReconnect(bool autoReconnect) {
  WiFi.persistent(false); // Fix for issues with reconnection, credentials are stored in settingsManager
  WiFi.setAutoReconnect(autoReconnect);
}

int HAL_ESP32::wifiGetStatus() {
  return WiFiClass::status();
}

// ========================================================================
// MDNS METHODS
// ========================================================================

bool HAL_ESP32::mdnsBegin(const char* hostname) {
  if (!MDNS.begin(hostname)) {
    Serial.println("[HAL_ESP32] mDNS failed to start");
    return false;
  }
  Serial.println("[HAL_ESP32] mDNS started");
  return true;
}

// ========================================================================
// FILESYSTEM - LittleFS
// ========================================================================

bool HAL_ESP32::fsBegin(bool formatOnFail) {
  if (!LittleFS.begin(formatOnFail)) {
    if(formatOnFail) {
      Serial.println("[HAL_ESP32] LittleFS failed to start, checking if formatting successful...");
      if (!LittleFS.begin()) {
        Serial.println("[HAL_ESP32] LittleFS format failed");
        return false;
      }
      Serial.println("[HAL_ESP32] LittleFS formatted successfully");
    } else {
      Serial.println("[HAL_ESP32] LittleFS failed to start");
      return false;
    }
  }
  Serial.println("[HAL_ESP32] LittleFS mounted successfully");
  return true;
}

void HAL_ESP32::fsEnd() {
  LittleFS.end();
}

bool HAL_ESP32::fsExists(const char* path) {
  return LittleFS.exists(path);
}

bool HAL_ESP32::fsRemove(const char* path) {
  return LittleFS.remove(path);
}

bool HAL_ESP32::fsRename(const char* pathFrom, const char* pathTo) {
  return LittleFS.rename(pathFrom, pathTo);
}

HalFile HAL_ESP32::fsOpen(const char* path, const char* mode) {
  File file = LittleFS.open(path, mode);
  if (!file) {
    return nullptr;
  }

  auto* h = new HalFileHandle{ std::move(file) };
  return h;
}

void HAL_ESP32::fsClose(HalFile file) {
  if (!file) return;
  file->fileHandle.close();
  delete file;
}

size_t HAL_ESP32::fsRead(HalFile file, uint8_t* buf, size_t size) {
    return file && file->fileHandle ? file->fileHandle.read(buf, size) : 0;
}

size_t HAL_ESP32::fsWrite(HalFile file, const uint8_t* buf, size_t size) {
    return file && file->fileHandle ? file->fileHandle.write(buf, size) : 0;
}

int HAL_ESP32::fsAvailable(HalFile file) {
    return file && file->fileHandle ? file->fileHandle.available() : 0;
}

bool HAL_ESP32::fsSeek(HalFile file, size_t pos) {
  if (file == nullptr) {
    return false;
  }
  return file->fileHandle.seek(pos);
}

size_t HAL_ESP32::fsPosition(HalFile file) {
  if (file == nullptr) {
    return 0;
  }
  return file->fileHandle.position();
}

size_t HAL_ESP32::fsSize(HalFile file) {
  if (file == nullptr) {
    return 0;
  }
  return file->fileHandle.size();
}

// ========================================================================
// WEB SERVER - AsyncWebServer Abstraction
// ========================================================================

/**
 * @brief ESP32 Web Request Wrapper
 *
 * Wraps AsyncWebServerRequest* to implement IWebRequest interface.
 * This allows HAL web server methods to work with platform-agnostic interfaces.
 */
class ESP32WebRequestWrapper : public IWebRequest {
private:
  AsyncWebServerRequest* request_;
  JsonVariant jsonBody_;

public:
  explicit ESP32WebRequestWrapper(AsyncWebServerRequest* request) : request_(request) {}

  String url() const override {
    return request_->url();
  }

  HAL_WebRequestMethod method() const override {
    WebRequestMethodComposite m = request_->method();
    switch (m) {
      case HTTP_GET: return HAL_WebRequestMethod::HTTP_GET;
      case HTTP_POST: return HAL_WebRequestMethod::HTTP_POST;
      case HTTP_PUT: return HAL_WebRequestMethod::HTTP_PUT;
      case HTTP_DELETE: return HAL_WebRequestMethod::HTTP_DELETE;
      case HTTP_PATCH: return HAL_WebRequestMethod::HTTP_PATCH;
      default: return HAL_WebRequestMethod::HTTP_ANY;
    }
  }

  bool hasParam(const char* name, bool post = false) const override {
    return request_->hasParam(name, post);
  }

  String param(const char* name, bool post = false) const override {
    const AsyncWebParameter* p = request_->getParam(name, post);
    return p ? p->value() : String();
  }

  void setJsonBody(const JsonVariant& json) {
    jsonBody_ = json;
  }

  const JsonVariant& jsonBody() const {
    return jsonBody_;
  }

  String body() const override {
    // AsyncWebServerRequest doesn't have a body() method
    // For POST requests, we need to read the body differently
    // This is a limitation - POST body access not fully supported in HAL abstraction

    String msg;
    if (hasParam("msg", true)) {     // true = search in POST body
      msg = param("msg", true);
    }
    return msg;
  }
};

/**
 * @brief ESP32 Web Response Wrapper
 *
 * Wraps AsyncWebServerResponse* to implement IWebResponse interface.
 * This allows HAL web server methods to work with platform-agnostic interfaces.
 */
class ESP32WebResponseWrapper : public IWebResponse {
private:
  AsyncWebServerRequest* request_;

public:
  explicit ESP32WebResponseWrapper(AsyncWebServerRequest* request) : request_(request) {}

  void send(int code, const char* contentType, const char* body) override {
    request_->send(code, contentType, body);
  }

  void sendFile(const char* path, const char* contentType) override {
    request_->send(LittleFS, path, contentType);
  }

  void setContentLength(size_t len) override {
    // AsyncWebServer handles content length automatically
    // This method is provided for interface compatibility
  }

  void setContentType(const char* type) override {
    // AsyncWebServer handles content type automatically
    // This method is provided for interface compatibility
  }
};

bool HAL_ESP32::webServerBegin(uint16_t port) {
  // Create new AsyncWebServer instance
  if (server_ != nullptr) {
    delete server_;
    server_ = nullptr;
  }
  
  server_ = new AsyncWebServer(port);
  
  if (server_ == nullptr) {
    Serial.println("[HAL_ESP32] Failed to create AsyncWebServer");
    return false;
  }
  
  Serial.printf("[HAL_ESP32] AsyncWebServer created on port %d\n", port);

  server_->begin();
  Serial.println("[HAL_ESP32] AsyncWebServer started");

  return true;
}

void HAL_ESP32::webServerOn(const char* uri, HAL_WebRequestMethod method, WebServerHandler handler) {
  if (server_ == nullptr) {
    Serial.println("[HAL_ESP32] webServerOn: server not initialized");
    return;
  }
  
  // Convert HAL_WebRequestMethod to ESPAsyncWebServer HTTP method
  WebRequestMethod httpMethod;
  String methodStr;
  switch (method) {
    case HAL_WebRequestMethod::HTTP_POST:
      httpMethod = HTTP_POST;
      methodStr = "POST";
      break;
    case HAL_WebRequestMethod::HTTP_PUT:
      httpMethod = HTTP_PUT;
      methodStr = "PUT";
      break;
    case HAL_WebRequestMethod::HTTP_DELETE:
      httpMethod = HTTP_DELETE;
      methodStr = "DELETE";
      break;
    case HAL_WebRequestMethod::HTTP_PATCH:
      httpMethod = HTTP_PATCH;
      methodStr = "PATCH";
      break;
    case HAL_WebRequestMethod::HTTP_ANY:
      httpMethod = HTTP_ANY;
      methodStr = "ANY";
      break;
    case HAL_WebRequestMethod::HTTP_GET:
    default:
      httpMethod = HTTP_GET;
      methodStr = "GET";
      break;
  }
  
  // if(httpMethod == HTTP_POST) {
  //   Serial.printf("[HAL_ESP32] Warning: Handling POST requests may not support full body access due to AsyncWebServer limitations.\n");
    
  // //  server_->on(uri, httpMethod, [handler, uri](AsyncWebServerRequest *request) {}, nullptr,
  // //   // onBody
  // //   [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // //     // data points to the current chunk, len is its size
  // //     Serial.print("Chunk len: ");
  // //     Serial.println(len);
  // //     for (size_t i = 0; i < len; i++) {
  // //       Serial.write(data[i]);
  // //     }
  // //     Serial.println();

  // //     if (index + len == total) {  // last chunk
  // //       request->send(200, "text/plain", "Body received\n");
  // //     }
  // //   });
  //   server_->addHandler(new AsyncCallbackJsonWebHandler(uri,
  //     [handler, uri](AsyncWebServerRequest *request, JsonVariant &json) { // NOSONAR
  //       if (json.isNull()) {
  //         Serial.printf("[HAL_ESP32] webServerOn: %s received invalid JSON\n", uri);
  //         request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
  //         return;
  //       }
  //       ESP32WebRequestWrapper wrappedRequest(request);
  //       ESP32WebResponseWrapper wrappedResponse(request);
  //       wrappedRequest.setJsonBody(json);
  //       // Handler signature now takes both request and response
  //       handler(&wrappedRequest, &wrappedResponse);
  //     }));
  // } else {
    // Register handler with AsyncWebServer, wrapping with IWebRequest interface
    server_->on(uri, httpMethod, [handler, uri](AsyncWebServerRequest *request, JsonVariant &json) {
      ESP32WebRequestWrapper wrappedRequest(request);
      ESP32WebResponseWrapper wrappedResponse(request);
      wrappedRequest.setJsonBody(json);
      // Handler signature now takes both request and response
      handler(&wrappedRequest, &wrappedResponse);
    });
  // }
  Serial.printf("[HAL_ESP32] %s webServerOn: %s\n", methodStr.c_str(), uri);
}

void HAL_ESP32::webServerAddHandler(WebServerHandler handler) {
  if (server_ == nullptr) {
    Serial.println("[HAL_ESP32] webServerAddHandler: server not initialized");
    return;
  }
  
  // NOTE: This is a stub implementation.
  // Actual implementation requires AsyncWebHandler* which is incompatible
  // with WebServerHandler. The HAL interface needs to be updated.
  Serial.println("[HAL_ESP32] webServerAddHandler called (stub)");
}

void HAL_ESP32::webServerAddRewrite(const char* from, const char* to) {
  if (server_ == nullptr) {
    Serial.println("[HAL_ESP32] webServerAddRewrite: server not initialized");
    return;
  }
  
  // Add URL rewrite rule for SPA routing
  server_->addRewrite(new AsyncWebRewrite(from, to));
  Serial.printf("[HAL_ESP32] Added rewrite: %s -> %s\n", from, to);
}

void HAL_ESP32::webServerServeStatic(const char* uri, const char* path) {
  if (server_ == nullptr) {
    Serial.println("[HAL_ESP32] webServerServeStatic: server not initialized");
    return;
  }
  
  // Serve static files from LittleFS
  server_->serveStatic(uri, LittleFS, path);
  Serial.printf("[HAL_ESP32] Configured static serving: %s -> %s\n", uri, path);
}

void HAL_ESP32::webServerOnNotFound(WebServerHandler handler) {
  if (server_ == nullptr) {
    Serial.println("[HAL_ESP32] webServerOnNotFound: server not initialized");
    return;
  }
  
  // Register 404 handler with AsyncWebServer, wrapping with IWebRequest interface
  server_->onNotFound([handler](AsyncWebServerRequest *request) {
    ESP32WebRequestWrapper wrappedRequest(request);
    ESP32WebResponseWrapper wrappedResponse(request);
    handler(&wrappedRequest, &wrappedResponse);
  });
  Serial.println("[HAL_ESP32] webServerOnNotFound: handler registered");
}

void HAL_ESP32::webServerLoop() {
  // Currently no loop processing needed for AsyncWebServer
}

void HAL_ESP32::webServerAddElegantOTA() {
  if (server_ == nullptr) {
    Serial.println("[HAL_ESP32] webServerAddElegantOTA: server not initialized");
    return;
  }
  
  ElegantOTA.begin(server_);
  Serial.println("[HAL_ESP32] ElegantOTA support added to web server");
}

// ========================================================================
// LEDC (LED Control) PWM Functions
// ========================================================================

void HAL_ESP32::pwmSetup(uint8_t channel, uint32_t freq, uint8_t resolution) {
  ledcSetup(channel, freq, resolution);
}

void HAL_ESP32::pwmAttachPin(uint8_t pin, uint8_t channel) {
  ledcAttachPin(pin, channel);
}

void HAL_ESP32::pwmWrite(uint8_t channel, uint32_t duty) {
  ledcWrite(channel, duty);
}
