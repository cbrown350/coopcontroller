#include "HAL_ESP32.h"

// ESP32-specific includes
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h> // For HTTPS client
#include <esp32-hal-ledc.h> // For LEDC functions
#include <esp_system.h>     // For esp_reset_reason()
#include <esp_task_wdt.h>   // For esp_task_wdt_reset()
#include <Preferences.h>      // For NVS access
#include <mbedtls/sha256.h>  // For SHA256 verification
#include <freertos/semphr.h>  // For FreeRTOS mutex
#include <exception>          // For std::exception in web-handler guards
#include "HttpRequestBuilder.h"  // Shared GET request builder (adds User-Agent)

/**
 * @brief Constructor
 */
HAL_ESP32::HAL_ESP32() : server_(nullptr), sharedStateMutex_(nullptr) {
  sharedStateMutex_ = xSemaphoreCreateMutex();
}

/**
 * @brief Destructor
 */
HAL_ESP32::~HAL_ESP32() {
  if (server_ != nullptr) {
    delete server_;
    server_ = nullptr;
  }
  if (sharedStateMutex_ != nullptr) {
    vSemaphoreDelete(static_cast<SemaphoreHandle_t>(sharedStateMutex_));
    sharedStateMutex_ = nullptr;
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
  // If NTP time hasn't been obtained yet, skip the expensive getLocalTime()
  // call (which blocks for its full timeout) and just use millis().
  // Re-check periodically in case NTP synced after WiFi connected.
  if (!ntpTimeAvailable_) {
    if (millis() - lastNtpCheckMs_ < NTP_RECHECK_INTERVAL_MS) {
      return millis();
    }
    lastNtpCheckMs_ = millis();
  }

  time_t now;
  if (struct tm timeinfo;
      !getLocalTime(&timeinfo, 10)) { // Short timeout - NTP sync is fast when available
    ntpTimeAvailable_ = false;
    if (static unsigned long warned = 0;
        millis() - warned > 30000 || warned == 0) { // Warn at most once per 30s
      warned = millis();
      Serial.println("Failed to obtain time"); // Can't call logger since it may
                                               // call getTime()
    }
    return millis();
  }
  ntpTimeAvailable_ = true;
  time(&now);
  return now;
}

uint32_t HAL_ESP32::getFreeHeap() { return ESP.getFreeHeap(); }

bool HAL_ESP32::WiFiIsConnected() {
  return WiFi.status() ==
         WL_CONNECTED; // NOSONAR - standard way to call status()
}

void HAL_ESP32::SerialPrintf(const char *format, ...) { // NOSONAR - intentional
  char buffer[512];                                     // NOSONAR
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args); // NOSONAR
  va_end(args);
  Serial.print(String(buffer));
}

void HAL_ESP32::SerialPrintln(const char *message) { Serial.println(message); }

void HAL_ESP32::SerialPrint(const char *message) { Serial.print(message); }

// ========================================================================
// ESP32 SYSTEM FUNCTIONS
// ========================================================================

void HAL_ESP32::restart() { ESP.restart(); }

uint32_t HAL_ESP32::getHeapSize() { return ESP.getHeapSize(); }

uint32_t HAL_ESP32::getMinFreeHeap() { return ESP.getMinFreeHeap(); }

const char *HAL_ESP32::getChipModel() { return ESP.getChipModel(); }

uint8_t HAL_ESP32::getResetReason() {
  return static_cast<uint8_t>(esp_reset_reason());
}

uint32_t HAL_ESP32::getCpuFreqMHz() { return ESP.getCpuFreqMHz(); }

uint32_t HAL_ESP32::getFlashChipSize() { return ESP.getFlashChipSize(); }

void HAL_ESP32::taskWdtReset() { esp_task_wdt_reset(); }

// ========================================================================
// TIME FUNCTIONS
// ========================================================================

bool HAL_ESP32::getLocalTime(struct tm *timeinfo, unsigned long ms) {
  return ::getLocalTime(timeinfo, ms);
}

// ========================================================================
// WIFI MANAGEMENT
// ========================================================================

bool HAL_ESP32::wifiBegin(const char *ssid, const char *password) {
  // Disable modem sleep: the ESP32 SDK default (WIFI_PS_MIN_MODEM) powers the
  // radio down between DTIM beacons, which causes transient dropouts on
  // mesh/multi-AP networks even at strong RSSI. The coop controller is
  // mains-powered, so the ~30 mA savings is irrelevant. WIFI_PS_NONE keeps the
  // radio continuously awake and eliminates that dropout source. Idempotent
  // and harmless to call on every begin.
  WiFi.setSleep(WIFI_PS_NONE);

  // Only cycle the STA interface off when it's actually in a bad state.
  // Unconditionally doing mode(OFF)->mode(STA) on every reconnect calls
  // esp_wifi_stop, which races mbedtls_x509_crt_free on the async_tcp task
  // and panics (decoded backtraces end in esp_wifi_stop/deauth_sta). If STA
  // is already up and associated, just re-issue the connect. Only tear down
  // when stuck in CONNECT_FAILED/DISCONNECTED, where the radio genuinely
  // needs a clean restart to associate again.
  if (WiFiClass::getMode() & WIFI_MODE_STA) {
    if (WiFiClass::status() == WL_CONNECT_FAILED ||
        WiFiClass::status() == WL_CONNECTION_LOST ||
        WiFiClass::status() == WL_DISCONNECTED) {
      WiFiClass::mode(WIFI_OFF);
    }
  }
  WiFiClass::mode(WIFI_STA);
  WiFi.begin(ssid, password);
  return true;
}

bool HAL_ESP32::wifiBeginWithBSSID(const char *ssid, const char *password, const uint8_t *bssid) {
  WiFi.setSleep(WIFI_PS_NONE);
  if (WiFiClass::getMode() & WIFI_MODE_STA) {
    if (WiFiClass::status() == WL_CONNECT_FAILED ||
        WiFiClass::status() == WL_CONNECTION_LOST ||
        WiFiClass::status() == WL_DISCONNECTED) {
      WiFiClass::mode(WIFI_OFF);
    }
  }
  WiFiClass::mode(WIFI_STA);
  WiFi.begin(ssid, password, 0, bssid);
  return true;
}

bool HAL_ESP32::wifiBeginAP(const char *ssid, const char *password) {
  WiFiClass::mode(WIFI_AP);
  if (password != nullptr) {
    WiFi.softAP(ssid, password);
  } else {
    WiFi.softAP(ssid);
  }
  return true;
}

bool HAL_ESP32::wifiSetHostname(const char *hostName_) {
  return WiFiClass::setHostname(hostName_) && WiFi.softAPsetHostname(hostName_);
}

bool HAL_ESP32::wifiIsConnected() {
  return WiFiClass::status() == WL_CONNECTED;
}

String HAL_ESP32::wifiGetSSID() { return WiFi.SSID(); }

String HAL_ESP32::wifiGetBSSID() { return WiFi.BSSIDstr(); }

String HAL_ESP32::wifiGetMacAddress() { return WiFi.macAddress(); }

String HAL_ESP32::wifiGetLocalIP() { return WiFi.localIP().toString(); }

String HAL_ESP32::wifiGetAPIP() { return WiFi.softAPIP().toString(); }

int HAL_ESP32::wifiGetRSSI() { return WiFi.RSSI(); }

void HAL_ESP32::wifiDisconnect() { WiFi.disconnect(); }

void HAL_ESP32::wifiSetAutoReconnect(bool autoReconnect) {
  WiFi.persistent(false); // Fix for issues with reconnection, credentials are
                          // stored in settingsManager
  WiFi.setAutoReconnect(autoReconnect);
}

int HAL_ESP32::wifiGetStatus() { return WiFiClass::status(); }

int HAL_ESP32::tlsClientsInFlight() {
  return tls_in_flight_.load(std::memory_order_acquire);
}

// ========================================================================
// MDNS METHODS
// ========================================================================

bool HAL_ESP32::mdnsBegin(const char *hostname) {
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
    if (formatOnFail) {
      Serial.println("[HAL_ESP32] LittleFS failed to start, checking if "
                     "formatting successful...");
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

void HAL_ESP32::fsEnd() { LittleFS.end(); }

bool HAL_ESP32::fsExists(const char *path) { return LittleFS.exists(path); }

bool HAL_ESP32::fsRemove(const char *path) { return LittleFS.remove(path); }

bool HAL_ESP32::fsRename(const char *pathFrom, const char *pathTo) {
  return LittleFS.rename(pathFrom, pathTo);
}

HalFile HAL_ESP32::fsOpen(const char *path, const char *mode) {
  File file = LittleFS.open(path, mode);
  if (!file) {
    return nullptr;
  }

  auto *h = new HalFileHandle{std::move(file)};
  return h;
}

void HAL_ESP32::fsClose(HalFile file) {
  if (!file)
    return;
  file->fileHandle.close();
  delete file;
}

size_t HAL_ESP32::fsRead(HalFile file, uint8_t *buf, size_t size) {
  return file && file->fileHandle ? file->fileHandle.read(buf, size) : 0;
}

size_t HAL_ESP32::fsWrite(HalFile file, const uint8_t *buf, size_t size) {
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
  AsyncWebServerRequest *request_;
  JsonDocument jsonDoc_;
  JsonVariant jsonBody_;

public:
  explicit ESP32WebRequestWrapper(AsyncWebServerRequest *request)
      : request_(request) {}

  void parseJsonBody(const char *body) {
    DeserializationError error = deserializeJson(jsonDoc_, body);
    if (!error) {
      jsonBody_ = jsonDoc_.as<JsonVariant>();
    }
  }

  String url() const override { return request_->url(); }

  HAL_WebRequestMethod method() const override {
    WebRequestMethodComposite m = request_->method();
    switch (m) {
    case HTTP_GET:
      return HAL_WebRequestMethod::HTTP_GET;
    case HTTP_POST:
      return HAL_WebRequestMethod::HTTP_POST;
    case HTTP_PUT:
      return HAL_WebRequestMethod::HTTP_PUT;
    case HTTP_DELETE:
      return HAL_WebRequestMethod::HTTP_DELETE;
    case HTTP_PATCH:
      return HAL_WebRequestMethod::HTTP_PATCH;
    default:
      return HAL_WebRequestMethod::HTTP_ANY;
    }
  }

  bool hasParam(const char *name, bool post = false) const override {
    return request_->hasParam(name, post);
  }

  String param(const char *name, bool post = false) const override {
    const AsyncWebParameter *p = request_->getParam(name, post);
    return p ? p->value() : String();
  }

  void setJsonBody(const JsonVariant &json) override { jsonBody_ = json; }

  const JsonVariant &jsonBody() const override { return jsonBody_; }

  String body() const override {
    // The JSON body was parsed by AsyncCallbackJsonWebHandler and stored in jsonBody_
    // Serialize it back to a string for endpoints that need raw body access
    if (!jsonBody_.isNull()) {
      String bodyStr;
      serializeJson(jsonBody_, bodyStr);
      return bodyStr;
    }

    // Fallback: check for form-encoded "msg" parameter (legacy support)
    if (hasParam("msg", true)) {
      return param("msg", true);
    }

    return String();
  }

  bool hasHeader(const char *name) const override {
    return request_->hasHeader(name);
  }

  String header(const char *name) const override {
    if (request_->hasHeader(name)) {
      return request_->header(name);
    }
    return String();
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
  AsyncWebServerRequest *request_;
  std::vector<std::pair<String, String>> headers_;
  bool responseSent_ = false;

public:
  explicit ESP32WebResponseWrapper(AsyncWebServerRequest *request)
      : request_(request) {}

  // True once any send*/sendFile/sendChunked path has handed a response to
  // AsyncWebServer. The webServerOn exception guard uses this to avoid a
  // double-send (which itself crashes AsyncWebServer) when a handler throws
  // after it already responded.
  bool responseSent() const { return responseSent_; }

  void send(int code, const char *contentType, const char *body) override {
    if (request_ == nullptr || responseSent_) return;
    AsyncWebServerResponse *response = request_->beginResponse(code, contentType, body);
    if (response == nullptr) return;
    // Add any custom headers
    for (const auto& header : headers_) {
      response->addHeader(header.first, header.second);
    }
    request_->send(response);
    responseSent_ = true;
  }

  void sendFile(const char *path, const char *contentType) override {
    if (request_ == nullptr || responseSent_) return;
    // For file sending, headers need to be added differently
    // AsyncWebServer's send(LittleFS, path) doesn't support custom headers easily
    // We'll create a response manually if we have custom headers
    if (headers_.empty()) {
      request_->send(LittleFS, path, contentType);
    } else {
      AsyncWebServerResponse *response = request_->beginResponse(LittleFS, path, contentType);
      if (response == nullptr) return;
      for (const auto& header : headers_) {
        response->addHeader(header.first, header.second);
      }
      request_->send(response);
    }
    responseSent_ = true;
  }

  void setContentLength(size_t len) override {
    // AsyncWebServer handles content length automatically
    // This method is provided for interface compatibility
  }

  void setContentType(const char *type) override {
    // AsyncWebServer handles content type automatically
    // This method is provided for interface compatibility
  }

  void addHeader(const char *name, const char *value) override {
    headers_.push_back(std::make_pair(String(name), String(value)));
  }

  void sendChunked(int code, const char* contentType, ChunkedFillCallback callback) override {
    if (request_ == nullptr) return;
    auto response = request_->beginChunkedResponse(contentType,
        [callback](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
            // This filler runs on the async_tcp task per chunk; a throw here
            // would abort the task and reboot. Return 0 to end the stream cleanly.
            try {
              return callback(buffer, maxLen, index);
            } catch (...) {
              Serial.println("[HAL_ESP32] chunked fill callback threw; ending stream");
              return 0;
            }
        });
    if (response == nullptr) return;
    response->setCode(code);
    for (const auto& header : headers_) {
      response->addHeader(header.first, header.second);
    }
    request_->send(response);
    responseSent_ = true;
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

void HAL_ESP32::webServerOn(const char *uri, HAL_WebRequestMethod method,
                            WebServerHandler handler) {
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

  // Capture mutex handle for use in lambda (web handlers run on async_tcp task, core 0)
  SemaphoreHandle_t mutex = static_cast<SemaphoreHandle_t>(sharedStateMutex_);

  server_->on(uri, httpMethod,
              // Request handler - called when request is complete
              [handler, mutex](AsyncWebServerRequest *request) {
                // Acquire shared state mutex to prevent races with main loop (core 1)
                bool locked = false;
                if (mutex != nullptr) {
                  locked = (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE);
                  if (!locked) {
                    request->send(503, "text/plain", "Server busy, try again");
                    return;
                  }
                }

                ESP32WebRequestWrapper wrappedRequest(request);
                ESP32WebResponseWrapper wrappedResponse(request);

                // If body was collected, try to parse as JSON
                if (request->_tempObject != nullptr) {
                  wrappedRequest.parseJsonBody((const char *)request->_tempObject);
                  free(request->_tempObject);
                  request->_tempObject = nullptr;
                }

                // Guard the user handler: it runs on the async_tcp task where any
                // uncaught exception (String/JSON allocation under fragmentation, an
                // empty std::function call, etc.) propagates to std::terminate ->
                // abort -> panic reboot. Catch here so a failed request returns HTTP
                // 500 instead of taking down the whole device (issue #4 follow-up:
                // v0.4.5 guarded only the outbound TLS path, not this inbound one).
                try {
                  handler(&wrappedRequest, &wrappedResponse);
                } catch (const std::exception &e) {
                  Serial.printf("[HAL_ESP32] webServerOn handler threw: %s\r\n", e.what());
                  if (!wrappedResponse.responseSent()) {
                    request->send(500, "text/plain", "Internal server error");
                  }
                } catch (...) {
                  Serial.println("[HAL_ESP32] webServerOn handler threw unknown exception");
                  if (!wrappedResponse.responseSent()) {
                    request->send(500, "text/plain", "Internal server error");
                  }
                }

                if (locked) {
                  xSemaphoreGive(mutex);
                }
              },
              nullptr, // upload handler
              // Body handler - collect POST/PUT body data
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
                 size_t index, size_t total) {
                if (total == 0 || total > 16384)
                  return;
                if (index == 0) {
                  // Free any previous allocation to prevent double-alloc
                  if (request->_tempObject != nullptr) {
                    free(request->_tempObject);
                  }
                  request->_tempObject = malloc(total + 1);
                }
                if (request->_tempObject != nullptr) {
                  memcpy((uint8_t *)request->_tempObject + index, data, len);
                  if (index + len == total) {
                    ((char *)request->_tempObject)[total] = '\0';
                  }
                }
              });

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

void HAL_ESP32::webServerAddRewrite(const char *from, const char *to) {
  if (server_ == nullptr) {
    Serial.println("[HAL_ESP32] webServerAddRewrite: server not initialized");
    return;
  }

  // Add URL rewrite rule for SPA routing
  server_->addRewrite(new AsyncWebRewrite(from, to));
  Serial.printf("[HAL_ESP32] Added rewrite: %s -> %s\n", from, to);
}

void HAL_ESP32::webServerServeStatic(const char *uri, const char *path) {
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

  // Register 404 handler with AsyncWebServer, wrapping with IWebRequest
  // interface
  server_->onNotFound([handler](AsyncWebServerRequest *request) {
    ESP32WebRequestWrapper wrappedRequest(request);
    ESP32WebResponseWrapper wrappedResponse(request);
    // Same async_tcp exception guard as webServerOn: never let a throwing
    // 404 handler abort the task and reboot the device.
    try {
      handler(&wrappedRequest, &wrappedResponse);
    } catch (const std::exception &e) {
      Serial.printf("[HAL_ESP32] onNotFound handler threw: %s\r\n", e.what());
      if (!wrappedResponse.responseSent()) {
        request->send(500, "text/plain", "Internal server error");
      }
    } catch (...) {
      Serial.println("[HAL_ESP32] onNotFound handler threw unknown exception");
      if (!wrappedResponse.responseSent()) {
        request->send(500, "text/plain", "Internal server error");
      }
    }
  });
  Serial.println("[HAL_ESP32] webServerOnNotFound: handler registered");
}

void HAL_ESP32::webServerLoop() {
  // Currently no loop processing needed for AsyncWebServer
}

void HAL_ESP32::webServerAddElegantOTA() {
  if (server_ == nullptr) {
    Serial.println(
        "[HAL_ESP32] webServerAddElegantOTA: server not initialized");
    return;
  }

  ElegantOTA.begin(server_);
  Serial.println("[HAL_ESP32] ElegantOTA support added to web server");
}

void HAL_ESP32::webServerEnd() {
  if (server_ == nullptr) return;
  server_->end();
  Serial.println("[HAL_ESP32] AsyncWebServer stopped");
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

// ========================================================================
// GPIO FUNCTIONS
// ========================================================================

void HAL_ESP32::pinMode(uint8_t pin, uint8_t mode) { ::pinMode(pin, mode); }

void HAL_ESP32::digitalWrite(uint8_t pin, uint8_t value) {
  ::digitalWrite(pin, value);
}

// ========================================================================
// FREERTOS FUNCTIONS
// ========================================================================

int HAL_ESP32::getCoreID() { return xPortGetCoreID(); }

void *HAL_ESP32::getCurrentTaskHandle() { return xTaskGetCurrentTaskHandle(); }

// ========================================================================
// THREAD SAFETY - Shared State Mutex
// ========================================================================

bool HAL_ESP32::lockSharedState(unsigned long timeout_ms) {
  if (sharedStateMutex_ == nullptr) return false;
  return xSemaphoreTake(static_cast<SemaphoreHandle_t>(sharedStateMutex_),
                        pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void HAL_ESP32::unlockSharedState() {
  if (sharedStateMutex_ == nullptr) return;
  xSemaphoreGive(static_cast<SemaphoreHandle_t>(sharedStateMutex_));
}

// ========================================================================
// HTTP CLIENT FUNCTIONS - For OTA Updates
// ========================================================================

// Read a single HTTP line (up to and including '\n') byte-by-byte from a TLS
// client, bounded by an overall deadline. Returns the line WITHOUT the
// trailing newline (CR/LF stripped).
//
// Why not readStringUntil('\n'): Arduino's Stream::readStringUntil() uses the
// stream's internal setTimeout() and timedRead() per character. When
// client.available() reports data but the full line hasn't all arrived yet
// (common on TLS for long header lines — e.g. GitHub's ~750-char signed
// release-asset redirect Location), readStringUntil() can return a partial
// line or empty string when its internal timeout fires mid-line. That made
// the GitHub manifest fetch intermittent (sometimes the redirect Location was
// truncated/missed, so the redirect chain broke and httpGet returned "").
// This reader keeps the overall deadline only and blocks on read bytes until
// the newline or the connection closes, so long header lines are never split.
static String readHttpLine(WiFiClientSecure& client, unsigned long deadlineMs) {
  String line;
  while (::millis() < deadlineMs) {
    int c = client.read();
    if (c < 0) {
      // No data available right now; wait for more or connection close
      if (!client.connected() && client.available() == 0) break;
      delay(1);
      continue;
    }
    if (c == '\n') break;
    if (c != '\r') {
      line += (char)c;
      // Guard against a runaway header line (no newline in 8 KB)
      if (line.length() > 8192) break;
    }
  }
  return line;
}

HAL_ESP32::SecureClientPtr HAL_ESP32::createSecureClient(unsigned long timeout_ms) {
  // Refuse to start a TLS handshake when WiFi isn't associated. Firing a TLS
  // connect while the STA is down/reconnecting is a documented trigger for the
  // mbedtls StoreProhibited panic (esp-idf #8895, #11131) on the async_tcp task,
  // because the TLS teardown races the net80211 connection state machine and
  // fault on a null context. All outbound TLS (httpGet / httpGetStream /
  // httpPost / httpPostAuth) funnels through here, so gating here covers every
  // caller (LLM decider, weather, OTA check/install, Telegram, email) uniformly.
  // Note: deliberately NOT gated on RSSI — a weak-but-working link is fine.
  if (!wifiIsConnected()) {
    Serial.println("[HAL_ESP32] createSecureClient: refusing TLS alloc, WiFi not connected");
    return nullptr;
  }

  // Refuse the TLS allocation when free heap is too low or fragmented to hold
  // the mbedtls context. The constructor's internal `new` would otherwise throw
  // std::bad_alloc; with -fexceptions enabled and no catch handler on the loop
  // or async_tcp tasks, that aborts the task and reboots (issue #4 root cause).
  if (ESP.getFreeHeap() < TLS_CLIENT_MIN_FREE_HEAP) {
    Serial.printf("[HAL_ESP32] createSecureClient: refusing TLS alloc, free heap %u < %u\r\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)TLS_CLIENT_MIN_FREE_HEAP);
    return nullptr;
  }

  try {
    // Construct inside try: WiFiClientSecure allocates the sslclient context
    // with `new`, which can throw even above the threshold under fragmentation.
    auto client = std::make_unique<WiFiClientSecure>();
    client->setInsecure();
    // Bound both the socket connect and the TLS handshake (defaults are 30 s /
    // 120 s, which alone can stall the loop task past the 30 s watchdog).
    // Clamp handshake to the same budget as the overall request timeout.
    client->setTimeout(timeout_ms);
    client->setHandshakeTimeout((timeout_ms + 999) / 1000);  // ms -> seconds, round up

    // RAII: bump the in-flight counter for the lifetime of this client. The
    // custom deleter (TlsClientDeleter) decrements it when the unique_ptr is
    // destroyed — scope exit, move, or exception. This lets the WiFi reconnect
    // path avoid tearing the radio down while a TLS session is live. Atomic,
    // lock-free, and the deleter runs exactly once per allocation — no way to
    // stick high unless a client is genuinely alive.
    tls_in_flight_.fetch_add(1, std::memory_order_acq_rel);
    auto* raw = client.release();
    return SecureClientPtr(raw, TlsClientDeleter(&tls_in_flight_));
  } catch (...) {
    Serial.println("[HAL_ESP32] createSecureClient: exception during TLS client allocation");
    return nullptr;
  }
}

String HAL_ESP32::httpGet(const String& url, unsigned long timeout_ms) {
  String currentUrl = url;
  int maxRedirects = 5;

  for (int redirectCount = 0; redirectCount <= maxRedirects; redirectCount++) {
    // Exception-safe TLS client; nullptr if heap too low to allocate safely.
    auto clientHolder = createSecureClient(timeout_ms);
    if (clientHolder == nullptr) {
      Serial.println("[HAL_ESP32] httpGet: unavailable (low memory), aborting");
      return "";
    }
    WiFiClientSecure &client = *clientHolder;

    String host = currentUrl;
    String path = "/";

    int slashIndex = currentUrl.indexOf("://");
    if (slashIndex != -1) {
      host = currentUrl.substring(slashIndex + 3);
    }

    int pathIndex = host.indexOf("/");
    if (pathIndex != -1) {
      path = host.substring(pathIndex);
      host = host.substring(0, pathIndex);
    }

    int port = 443;
    if (currentUrl.startsWith("http://")) {
      port = 80;
    }

    int portIndex = host.indexOf(":");
    if (portIndex != -1) {
      port = host.substring(portIndex + 1).toInt();
      host = host.substring(0, portIndex);
    }

    unsigned long startTime = ::millis();
    if (!client.connect(host.c_str(), port, timeout_ms)) {
      return "";
    }

    client.print(buildHttpGetRequest(host, path));

    // Read status line (byte-by-byte; see readHttpLine for why not readStringUntil)
    unsigned long deadline = startTime + timeout_ms;
    String statusLine = readHttpLine(client, deadline);

    int statusCode = 0;
    int spaceIdx = statusLine.indexOf(' ');
    if (spaceIdx > 0) {
      statusCode = statusLine.substring(spaceIdx + 1).toInt();
    }

    // Read headers
    String response = "";
    bool headerDone = false;
    String redirectLocation = "";
    long contentLength = -1;  // -1 = header absent

    while (!headerDone && ::millis() < deadline) {
      String line = readHttpLine(client, deadline);
      if (line.startsWith("Location:") || line.startsWith("location:")) {
        redirectLocation = line.substring(9);
        redirectLocation.trim();
      }
      if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
        contentLength = line.substring(15).toInt();
      }
      if (line.length() == 0) {
        headerDone = true;
      }
      // If the connection closed before we finished headers, stop waiting
      if (!client.connected() && client.available() == 0 && line.length() == 0) {
        headerDone = true;
      }
    }

    // Handle redirects
    if (statusCode >= 301 && statusCode <= 308 && redirectLocation.length() > 0) {
      client.stop();
      currentUrl = redirectLocation;
      continue;
    }

    if (statusCode != 200) {
      client.stop();
      return "";
    }

    // Read body in bulk chunks, mirroring the httpGetStream drain loop
    // (`while connected() OR available()`). The old single-byte read had two
    // defects on GitHub's ~14 KB releases/latest JSON (the old ~900-byte static
    // manifest fit in one TLS record and dodged both):
    //   1. Truncation: it broke on `!connected() && available()==0`, but over
    //      TLS connected() can flip false while decrypted records are still
    //      buffering, cutting the body short.
    //   2. Heap churn: growing a String one char at a time repeatedly reallocates
    //      the buffer. Reserving from Content-Length and appending chunks reduces
    //      fragmentation on this memory-constrained device.
    // Content-Length bounds the read exactly when present; the cap protects the
    // loop-task heap for responses that omit it.
    const size_t maxBody = 65535;  // JSON payloads are small; binaries use httpGetStream
    if (contentLength > 0) {
      response.reserve((size_t)contentLength < maxBody ? (size_t)contentLength : maxBody);
    }
    uint8_t buf[512];
    while ((client.connected() || client.available()) && ::millis() < deadline) {
      if (contentLength >= 0 && (long)response.length() >= contentLength) {
        break;  // Full declared body received
      }
      int avail = client.available();
      if (avail > 0) {
        int want = avail < (int)sizeof(buf) ? avail : (int)sizeof(buf);
        int n = client.read(buf, want);
        if (n > 0) {
          response.concat((const char*)buf, (size_t)n);
          if (response.length() >= maxBody) break;
        }
      } else if (!client.connected()) {
        break;  // Server closed and no buffered data remains — body complete
      } else {
        delay(1);
      }
    }

    client.stop();
    return response;
  }

  return ""; // Too many redirects
}

bool HAL_ESP32::httpGetStream(const String& url, HttpDataCallback on_data,
                               unsigned long timeout_ms) {
  String currentUrl = url;
  int maxRedirects = 5;

  for (int redirectCount = 0; redirectCount <= maxRedirects; redirectCount++) {
    // Exception-safe TLS client; nullptr if heap too low to allocate safely.
    auto clientHolder = createSecureClient(timeout_ms);
    if (clientHolder == nullptr) {
      Serial.println("[HAL_ESP32] httpGetStream: unavailable (low memory), aborting");
      return false;
    }
    WiFiClientSecure &client = *clientHolder;

    // Parse URL
    String host = currentUrl;
    String path = "/";

    int slashIndex = currentUrl.indexOf("://");
    if (slashIndex != -1) {
      host = currentUrl.substring(slashIndex + 3);
    }

    int pathIndex = host.indexOf("/");
    if (pathIndex != -1) {
      path = host.substring(pathIndex);
      host = host.substring(0, pathIndex);
    }

    int port = 443;
    if (currentUrl.startsWith("http://")) {
      port = 80;
    }

    int portIndex = host.indexOf(":");
    if (portIndex != -1) {
      port = host.substring(portIndex + 1).toInt();
      host = host.substring(0, portIndex);
    }

    unsigned long startTime = ::millis();
    if (!client.connect(host.c_str(), port, timeout_ms)) {
      return false;
    }

    client.print(buildHttpGetRequest(host, path));

    // Read status line
    String statusLine = "";
    while (client.connected()) {
      if (::millis() - startTime > timeout_ms) { client.stop(); return false; }
      if (client.available()) {
        statusLine = client.readStringUntil('\n');
        break;
      }
      delay(10);
    }

    // Parse HTTP status code
    int statusCode = 0;
    int spaceIdx = statusLine.indexOf(' ');
    if (spaceIdx > 0) {
      statusCode = statusLine.substring(spaceIdx + 1).toInt();
    }

    // Read headers
    bool headerDone = false;
    uint32_t contentLength = 0;
    String redirectLocation = "";

    while (client.connected() && !headerDone) {
      if (::millis() - startTime > timeout_ms) { client.stop(); return false; }
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
          contentLength = line.substring(15).toInt();
        }
        if (line.startsWith("Location:") || line.startsWith("location:")) {
          redirectLocation = line.substring(9);
          redirectLocation.trim();
        }
        if (line.length() == 0) {
          headerDone = true;
        }
      } else {
        delay(10);
      }
    }

    // Handle redirects (301, 302, 303, 307, 308)
    if (statusCode >= 301 && statusCode <= 308 && redirectLocation.length() > 0) {
      client.stop();
      currentUrl = redirectLocation;
      continue;
    }

    if (statusCode != 200) {
      client.stop();
      return false;
    }

    // Download binary data, passing chunks to callback
    uint8_t buffer[1024];
    uint32_t bytesDownloaded = 0;
    while (client.connected() || client.available()) {
      if (::millis() - startTime > timeout_ms) { client.stop(); return false; }
      if (client.available()) {
        int bytesRead = client.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
          bytesDownloaded += bytesRead;
          if (on_data) {
            if (!on_data(buffer, bytesRead, bytesDownloaded, contentLength > 0 ? contentLength : bytesDownloaded)) {
              client.stop();
              return false;
            }
          }
          esp_task_wdt_reset();  // Feed watchdog during long downloads
          yield();  // Allow async web server to handle status requests
        }
      } else {
        delay(10);
      }
    }

    client.stop();

    // Verify complete download if Content-Length was provided
    if (contentLength > 0 && bytesDownloaded != contentLength) {
      Serial.printf("[HAL_ESP32] httpGetStream: incomplete download %u/%u bytes\n",
                    bytesDownloaded, contentLength);
      return false;
    }

    return true;
  }

  return false; // Too many redirects
}

String HAL_ESP32::httpPost(const String& url, const String& jsonBody, unsigned long timeout_ms) {
  // Exception-safe TLS client; nullptr if heap too low to allocate safely.
  auto clientHolder = createSecureClient(timeout_ms);
  if (clientHolder == nullptr) {
    Serial.println("[HAL_ESP32] httpPost: unavailable (low memory), aborting");
    return "";
  }
  WiFiClientSecure &client = *clientHolder;

  String host = url;
  String path = "/";

  int slashIndex = url.indexOf("://");
  if (slashIndex != -1) {
    host = url.substring(slashIndex + 3);
  }

  int pathIndex = host.indexOf("/");
  if (pathIndex != -1) {
    path = host.substring(pathIndex);
    host = host.substring(0, pathIndex);
  }

  int port = 443;
  if (url.startsWith("http://")) {
    port = 80;
  }

  int portIndex = host.indexOf(":");
  if (portIndex != -1) {
    port = host.substring(portIndex + 1).toInt();
    host = host.substring(0, portIndex);
  }

  unsigned long startTime = ::millis();
  if (!client.connect(host.c_str(), port, timeout_ms)) {
    return "";
  }

  String request = "POST " + path + " HTTP/1.1\r\n";
  request += "Host: " + host + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(jsonBody.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += jsonBody;

  client.print(request);

  // Read status line
  String statusLine = "";
  while (client.connected()) {
    if (::millis() - startTime > timeout_ms) { client.stop(); return ""; }
    if (client.available()) {
      statusLine = client.readStringUntil('\n');
      break;
    }
    delay(10);
  }

  int statusCode = 0;
  int spaceIdx = statusLine.indexOf(' ');
  if (spaceIdx > 0) {
    statusCode = statusLine.substring(spaceIdx + 1).toInt();
  }

  // Read headers
  bool headerDone = false;
  while (client.connected() && !headerDone) {
    if (::millis() - startTime > timeout_ms) { client.stop(); return ""; }
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) {
        headerDone = true;
      }
    } else {
      delay(10);
    }
  }

  if (statusCode < 200 || statusCode >= 300) {
    client.stop();
    return "";
  }

  // Read body
  String response = "";
  while (client.connected() || client.available()) {
    if (::millis() - startTime > timeout_ms) break;
    if (client.available()) {
      String line = client.readStringUntil('\n');
      response += line;
    } else {
      delay(10);
    }
  }

  client.stop();
  return response;
}

String HAL_ESP32::httpPostAuth(const String& url, const String& jsonBody,
                               const String& bearerToken, const String& extraHeaders,
                               unsigned long timeout_ms) {
  if (url.length() == 0) return "";

  bool useTls = !url.startsWith("http://");  // https:// (or anything non-plain) => TLS

  // Parse host/path/port from the URL.
  String host = url;
  String path = "/";
  int slashIndex = url.indexOf("://");
  if (slashIndex != -1) host = url.substring(slashIndex + 3);
  int pathIndex = host.indexOf("/");
  if (pathIndex != -1) { path = host.substring(pathIndex); host = host.substring(0, pathIndex); }
  int port = useTls ? 443 : 80;
  int portIndex = host.indexOf(":");
  if (portIndex != -1) { port = host.substring(portIndex + 1).toInt(); host = host.substring(0, portIndex); }

  unsigned long startTime = ::millis();

  // Plain HTTP path uses a non-TLS WiFiClient so LAN endpoints (Rapid-MLX on
  // :8000, local Ollama on :11434) work — httpPost() above is TLS-only.
  std::unique_ptr<WiFiClient> plainHolder;
  SecureClientPtr tlsHolder;
  if (useTls) {
    tlsHolder = createSecureClient(timeout_ms);
    if (tlsHolder == nullptr) {
      Serial.println("[HAL_ESP32] httpPostAuth: TLS client unavailable (low memory), aborting");
      return "";
    }
    if (!tlsHolder->connect(host.c_str(), port, timeout_ms)) return "";
  } else {
    // Same WiFi-connected + low-heap guards as the TLS path: skip the request
    // entirely if the link is down (no point POSTing over a dead STA) or if
    // heap is too low to even buffer the request line.
    if (!wifiIsConnected()) {
      Serial.println("[HAL_ESP32] httpPostAuth: plain-HTTP abort, WiFi not connected");
      return "";
    }
    if (ESP.getFreeHeap() < TLS_CLIENT_MIN_FREE_HEAP) {
      return "";
    }
    try {
      plainHolder = std::make_unique<WiFiClient>();
    } catch (const std::exception&) {
      return "";
    }
    if (!plainHolder->connect(host.c_str(), port, timeout_ms)) return "";
  }

  // Build the request. Both client types expose the same print/stop API.
  String request = "POST " + path + " HTTP/1.1\r\n";
  request += "Host: " + host + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(jsonBody.length()) + "\r\n";
  if (bearerToken.length() > 0) {
    request += "Authorization: Bearer " + bearerToken + "\r\n";
  }
  if (extraHeaders.length() > 0) request += extraHeaders;
  request += "Connection: close\r\n\r\n";
  request += jsonBody;

  if (useTls) {
    tlsHolder->print(request);
  } else {
    plainHolder->print(request);
  }

  // Helper lambda to read "a line or until deadline", independent of client type.
  auto readLine = [&](auto& client) -> String {
    String line;
    unsigned long deadline = startTime + timeout_ms;
    while (::millis() < deadline) {
      int c = client->read();
      if (c < 0) {
        if (!client->connected() && client->available() == 0) break;
        delay(1);
        continue;
      }
      if (c == '\n') break;
      if (c != '\r') { line += (char)c; if (line.length() > 8192) break; }
    }
    return line;
  };

  // Status line + headers
  String statusLine = useTls ? readLine(tlsHolder) : readLine(plainHolder);
  int statusCode = 0;
  int spaceIdx = statusLine.indexOf(' ');
  if (spaceIdx > 0) statusCode = statusLine.substring(spaceIdx + 1).toInt();

  unsigned long deadline = startTime + timeout_ms;
  while (::millis() < deadline) {
    String line = useTls ? readLine(tlsHolder) : readLine(plainHolder);
    line.trim();
    if (line.length() == 0) break;  // end of headers
  }

  if (statusCode < 200 || statusCode >= 300) {
    if (useTls) tlsHolder->stop(); else plainHolder->stop();
    return "";
  }

  // Body. The inter-byte idle gap is the full remaining request budget, NOT a
  // fixed 1 s: an LLM provider (Ollama Cloud gemma4:31b) routinely thinks for
  // 2-5 s before emitting the first token, and a 1 s idle gap made ~32% of real
  // weather-prompt calls return empty ("no usable reply, rule fallback" on the
  // status page) while the trivial test-button prompt still passed. The overall
  // deadline (startTime + timeout_ms) still bounds the call end-to-end.
  String response;
  while (::millis() < deadline) {
    int c = (useTls ? tlsHolder->read() : plainHolder->read());
    if (c < 0) {
      bool connected = useTls ? tlsHolder->connected() : plainHolder->connected();
      int avail = useTls ? tlsHolder->available() : plainHolder->available();
      if (!connected && avail == 0) break;
      delay(1);
      continue;
    }
    response += (char)c;
    if (response.length() > 16384) break;  // cap: don't let a huge reply eat RAM
  }

  if (useTls) tlsHolder->stop(); else plainHolder->stop();
  return response;
}

#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

static String base64Encode(const String& input) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String out;
    int len = input.length();
    const uint8_t* data = (const uint8_t*)input.c_str();
    out.reserve((len + 2) / 3 * 4);
    for (int i = 0; i < len; i += 3) {
        uint32_t a = data[i];
        uint32_t b = (i + 1 < len) ? data[i + 1] : 0;
        uint32_t c = (i + 2 < len) ? data[i + 2] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        int remaining = len - i; // 1, 2, or 3+
        out += b64[(triple >> 18) & 0x3F];
        out += b64[(triple >> 12) & 0x3F];
        out += (remaining < 2) ? '=' : b64[(triple >> 6) & 0x3F];
        out += (remaining < 3) ? '=' : b64[triple & 0x3F];
    }
    return out;
}

// Helper: read SMTP response from a generic send/recv interface
// Returns the full response string; checks for "XXX " end-of-response pattern
typedef std::function<int(uint8_t* buf, size_t len)> SmtpRecvFn;
typedef std::function<int(const uint8_t* buf, size_t len)> SmtpSendFn;

static String smtpRecvResponse(SmtpRecvFn recv, unsigned long timeout_ms, unsigned long startTime) {
    String response = "";
    char buf[256];
    while (::millis() - startTime < timeout_ms) {
        int n = recv((uint8_t*)buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            response += buf;
            // Check if last line has "XXX " pattern (end of multi-line response)
            int lastNewline = response.lastIndexOf('\n', response.length() - 2);
            String lastLine = (lastNewline >= 0) ? response.substring(lastNewline + 1) : response;
            if (lastLine.length() >= 4 && lastLine.charAt(3) == ' ') break;
        } else if (n == 0) {
            break; // Connection closed
        } else {
            delay(10); // MBEDTLS_ERR_SSL_WANT_READ or similar
        }
    }
    return response;
}

static bool smtpCheckResponse(const String& response, const char* expectedCode) {
    return response.startsWith(expectedCode);
}

static int smtpSendLine(SmtpSendFn send, const String& line) {
    String data = line + "\r\n";
    return send((const uint8_t*)data.c_str(), data.length());
}

String HAL_ESP32::smtpSend(const String& host, uint16_t port,
                            const String& username, const String& password,
                            const String& from, const String& to,
                            const String& subject, const String& body,
                            unsigned long timeout_ms) {
    unsigned long startTime = ::millis();
    bool useDirectTLS = (port == 465);

    if (useDirectTLS) {
        // Port 465: Direct TLS (SMTPS) - use WiFiClientSecure directly
        WiFiClientSecure client;
        client.setInsecure();
        if (!client.connect(host.c_str(), port, timeout_ms)) {
            return "Connection failed to " + host + ":" + String(port);
        }

        auto recv = [&](uint8_t* buf, size_t len) -> int {
            unsigned long wait_start = ::millis();
            while (!client.available() && client.connected() && ::millis() - wait_start < 5000) delay(10);
            if (!client.available()) return -1;
            return client.read(buf, len);
        };
        auto send = [&](const uint8_t* buf, size_t len) -> int {
            return client.write(buf, len);
        };

        // SMTP conversation over TLS
        String resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "220")) { client.stop(); return "Bad greeting: " + resp; }

        smtpSendLine(send, "EHLO coop-controller");
        resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) { client.stop(); return "EHLO failed: " + resp; }

        smtpSendLine(send, "AUTH LOGIN");
        resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "334")) { client.stop(); return "AUTH LOGIN failed: " + resp; }

        smtpSendLine(send, base64Encode(username));
        resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "334")) { client.stop(); return "Username rejected: " + resp; }

        smtpSendLine(send, base64Encode(password));
        resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "235")) { client.stop(); return "Auth failed: " + resp; }

        smtpSendLine(send, "MAIL FROM:<" + from + ">");
        resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) { client.stop(); return "MAIL FROM rejected: " + resp; }

        smtpSendLine(send, "RCPT TO:<" + to + ">");
        resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) { client.stop(); return "RCPT TO rejected: " + resp; }

        smtpSendLine(send, "DATA");
        resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "354")) { client.stop(); return "DATA rejected: " + resp; }

        smtpSendLine(send, "From: " + from);
        smtpSendLine(send, "To: " + to);
        smtpSendLine(send, "Subject: " + subject);
        smtpSendLine(send, "MIME-Version: 1.0");
        smtpSendLine(send, "Content-Type: text/plain; charset=UTF-8");
        smtpSendLine(send, "");
        smtpSendLine(send, body);
        smtpSendLine(send, ".");

        resp = smtpRecvResponse(recv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) { client.stop(); return "Send failed: " + resp; }

        smtpSendLine(send, "QUIT");
        client.stop();
        return "";

    } else {
        // Port 587 (or other): STARTTLS - plain connect then TLS upgrade via mbedtls
        WiFiClient plainClient;
        if (!plainClient.connect(host.c_str(), port, timeout_ms)) {
            return "Connection failed to " + host + ":" + String(port);
        }
        int sockfd = plainClient.fd();

        // Plain-text recv/send using raw socket via WiFiClient
        auto plainRecv = [&](uint8_t* buf, size_t len) -> int {
            unsigned long wait_start = ::millis();
            while (!plainClient.available() && plainClient.connected() && ::millis() - wait_start < 5000) delay(10);
            if (!plainClient.available()) return -1;
            return plainClient.read(buf, len);
        };
        auto plainSend = [&](const uint8_t* buf, size_t len) -> int {
            return plainClient.write(buf, len);
        };

        // Read greeting
        String resp = smtpRecvResponse(plainRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "220")) { plainClient.stop(); return "Bad greeting: " + resp; }

        // EHLO
        smtpSendLine(plainSend, "EHLO coop-controller");
        resp = smtpRecvResponse(plainRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) { plainClient.stop(); return "EHLO failed: " + resp; }

        // STARTTLS
        smtpSendLine(plainSend, "STARTTLS");
        resp = smtpRecvResponse(plainRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "220")) { plainClient.stop(); return "STARTTLS failed: " + resp; }

        // TLS upgrade using mbedtls on existing socket
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config conf;
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context ctr_drbg;

        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);

        int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
        if (ret != 0) {
            plainClient.stop();
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            return "TLS seed failed";
        }

        mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE); // Skip cert verification
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

        mbedtls_ssl_setup(&ssl, &conf);
        mbedtls_ssl_set_hostname(&ssl, host.c_str());
        mbedtls_ssl_set_bio(&ssl, &sockfd, mbedtls_net_send, mbedtls_net_recv, NULL);

        // Perform TLS handshake on existing socket
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                char errbuf[128];
                mbedtls_strerror(ret, errbuf, sizeof(errbuf));
                String err = "TLS handshake failed: " + String(errbuf);
                mbedtls_ssl_free(&ssl);
                mbedtls_ssl_config_free(&conf);
                mbedtls_entropy_free(&entropy);
                mbedtls_ctr_drbg_free(&ctr_drbg);
                plainClient.stop();
                return err;
            }
            if (::millis() - startTime > timeout_ms) {
                mbedtls_ssl_free(&ssl);
                mbedtls_ssl_config_free(&conf);
                mbedtls_entropy_free(&entropy);
                mbedtls_ctr_drbg_free(&ctr_drbg);
                plainClient.stop();
                return "TLS handshake timeout";
            }
        }

        // TLS send/recv via mbedtls
        auto tlsRecv = [&](uint8_t* buf, size_t len) -> int {
            int n = mbedtls_ssl_read(&ssl, buf, len);
            return n;
        };
        auto tlsSend = [&](const uint8_t* buf, size_t len) -> int {
            return mbedtls_ssl_write(&ssl, buf, len);
        };

        // EHLO again after TLS
        smtpSendLine(tlsSend, "EHLO coop-controller");
        resp = smtpRecvResponse(tlsRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy); mbedtls_ctr_drbg_free(&ctr_drbg);
            plainClient.stop();
            return "EHLO after TLS failed: " + resp;
        }

        // AUTH LOGIN
        smtpSendLine(tlsSend, "AUTH LOGIN");
        resp = smtpRecvResponse(tlsRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "334")) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy); mbedtls_ctr_drbg_free(&ctr_drbg);
            plainClient.stop();
            return "AUTH LOGIN failed: " + resp;
        }

        smtpSendLine(tlsSend, base64Encode(username));
        resp = smtpRecvResponse(tlsRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "334")) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy); mbedtls_ctr_drbg_free(&ctr_drbg);
            plainClient.stop();
            return "Username rejected: " + resp;
        }

        smtpSendLine(tlsSend, base64Encode(password));
        resp = smtpRecvResponse(tlsRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "235")) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy); mbedtls_ctr_drbg_free(&ctr_drbg);
            plainClient.stop();
            return "Auth failed: " + resp;
        }

        // MAIL FROM / RCPT TO / DATA / message
        smtpSendLine(tlsSend, "MAIL FROM:<" + from + ">");
        resp = smtpRecvResponse(tlsRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy); mbedtls_ctr_drbg_free(&ctr_drbg);
            plainClient.stop();
            return "MAIL FROM rejected: " + resp;
        }

        smtpSendLine(tlsSend, "RCPT TO:<" + to + ">");
        resp = smtpRecvResponse(tlsRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy); mbedtls_ctr_drbg_free(&ctr_drbg);
            plainClient.stop();
            return "RCPT TO rejected: " + resp;
        }

        smtpSendLine(tlsSend, "DATA");
        resp = smtpRecvResponse(tlsRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "354")) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy); mbedtls_ctr_drbg_free(&ctr_drbg);
            plainClient.stop();
            return "DATA rejected: " + resp;
        }

        smtpSendLine(tlsSend, "From: " + from);
        smtpSendLine(tlsSend, "To: " + to);
        smtpSendLine(tlsSend, "Subject: " + subject);
        smtpSendLine(tlsSend, "MIME-Version: 1.0");
        smtpSendLine(tlsSend, "Content-Type: text/plain; charset=UTF-8");
        smtpSendLine(tlsSend, "");
        smtpSendLine(tlsSend, body);
        smtpSendLine(tlsSend, ".");

        resp = smtpRecvResponse(tlsRecv, timeout_ms, startTime);
        if (!smtpCheckResponse(resp, "250")) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
            mbedtls_entropy_free(&entropy); mbedtls_ctr_drbg_free(&ctr_drbg);
            plainClient.stop();
            return "Send failed: " + resp;
        }

        smtpSendLine(tlsSend, "QUIT");
        mbedtls_ssl_close_notify(&ssl);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        plainClient.stop();
        return "";
    }
}

// ========================================================================
// OTA UPDATE FUNCTIONS
// ========================================================================

bool HAL_ESP32::otaBegin(size_t size, int command) {
  return Update.begin(size, command == 0 ? U_FLASH : U_SPIFFS);
}

size_t HAL_ESP32::otaWrite(const uint8_t* data, size_t len) {
  return Update.write(const_cast<uint8_t*>(data), len);
}

bool HAL_ESP32::otaEnd(bool evenIfRemaining) {
  return Update.end(evenIfRemaining);
}

void HAL_ESP32::otaAbort() {
  Update.abort();
}

String HAL_ESP32::otaGetError() {
  return Update.errorString();
}

bool HAL_ESP32::sha256Verify(const uint8_t *data, size_t data_length,
                              const String& expected_hash) {
  // Use mbedtls for SHA256 calculation
  unsigned char hash[32];  // SHA256 = 32 bytes
  mbedtls_sha256_context ctx;

  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, false);  // false = SHA256 (not SHA224)
  mbedtls_sha256_update_ret(&ctx, data, data_length);
  mbedtls_sha256_finish_ret(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  // Convert hash to hex string
  char hashHex[65];
  for (int i = 0; i < 32; i++) {
    sprintf(hashHex + (i * 2), "%02x", hash[i]);
  }
  hashHex[64] = '\0';

  // Compare (case-insensitive)
  return expected_hash.equalsIgnoreCase(hashHex);
}

unsigned long HAL_ESP32::millis() {
  return ::millis();
}

// ========================================================================
// NVS (Non-Volatile Storage) FUNCTIONS
// ========================================================================

bool HAL_ESP32::nvsWriteString(const char* ns, const char* key, const String& value) {
  Preferences prefs;
  if (!prefs.begin(ns, false)) {  // false = read/write mode
    Serial.println("[HAL_ESP32] NVS: Failed to open namespace for writing");
    return false;
  }
  size_t written = prefs.putString(key, value);
  prefs.end();
  return written > 0;
}

String HAL_ESP32::nvsReadString(const char* ns, const char* key) {
  Preferences prefs;
  if (!prefs.begin(ns, true)) {  // true = read-only mode
    return "";
  }
  String value = prefs.getString(key, "");
  prefs.end();
  return value;
}

bool HAL_ESP32::nvsRemove(const char* ns, const char* key) {
  Preferences prefs;
  if (!prefs.begin(ns, false)) {
    return false;
  }
  bool result = prefs.remove(key);
  prefs.end();
  return result;
}
