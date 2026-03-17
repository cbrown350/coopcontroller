#ifndef __HAL_ESP32_H__
#define __HAL_ESP32_H__

#include "../HAL/IHAL.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "ElegantOTA.h"

struct HalFileHandle {
  File fileHandle;
};

// Forward declarations
class AsyncWebServer;

/**
 * @brief ESP32 Hardware Abstraction Layer Implementation
 *
 * Provides concrete implementations of IHAL interface for ESP32 hardware.
 * This class wraps ESP32-specific APIs including WiFi, filesystem,
 * and web server functionality.
 */
class HAL_ESP32 : public IHAL { // NOSONAR - complexity ok
public:
  /**
   * @brief Constructor
   */
  HAL_ESP32();

  /**
   * @brief Destructor
   */
  ~HAL_ESP32() override;

private:
  // NTP time caching to avoid 200ms getLocalTime() timeout on every getTime() call
  static constexpr unsigned long NTP_RECHECK_INTERVAL_MS = 5000; ///< Re-check NTP every 5s
  bool ntpTimeAvailable_ = false;          ///< Whether NTP time has been successfully obtained
  unsigned long lastNtpCheckMs_ = 0;       ///< Last time we attempted NTP check

public:

  // ========================================================================
  // EXISTING METHODS - DO NOT MODIFY
  // ========================================================================

  bool begin() override;
  unsigned long getTime() override;
  uint32_t getFreeHeap() override;
  bool WiFiIsConnected() override;
  void SerialPrintf(const char *format, ...) override;
  void SerialPrintln(const char *message) override;
  void SerialPrint(const char *message) override;

  // ========================================================================
  // ESP32 SYSTEM FUNCTIONS
  // ========================================================================

  void restart() override;
  uint32_t getHeapSize() override;
  uint32_t getMinFreeHeap() override;
  const char *getChipModel() override;
  uint8_t getResetReason() override;
  uint32_t getCpuFreqMHz() override;
  uint32_t getFlashChipSize() override;

  void taskWdtReset() override;

  // ========================================================================
  // TIME FUNCTIONS
  // ========================================================================

  bool getLocalTime(struct tm *timeinfo, unsigned long ms) override;

  // ========================================================================
  // WIFI MANAGEMENT
  // ========================================================================

  bool wifiBegin(const char *ssid, const char *password) override;
  bool wifiBeginWithBSSID(const char *ssid, const char *password, const uint8_t *bssid) override;
  bool wifiBeginAP(const char *ssid, const char *password = nullptr) override;
  bool wifiSetHostname(const char *hostName_) override;
  bool wifiIsConnected() override;
  String wifiGetSSID() override;
  String wifiGetBSSID() override;
  String wifiGetMacAddress() override;
  String wifiGetLocalIP() override;
  String wifiGetAPIP() override;
  int wifiGetRSSI() override;
  void wifiDisconnect() override;
  void wifiSetAutoReconnect(bool autoReconnect) override;
  int wifiGetStatus() override;

  // ========================================================================
  // MDNS METHODS
  // ========================================================================

  bool mdnsBegin(const char *hostname) override;

  // ========================================================================
  // FILESYSTEM - LittleFS
  // ========================================================================

  bool fsBegin(bool formatOnFail = false) override;
  void fsEnd() override;
  bool fsExists(const char *path) override;
  bool fsRemove(const char *path) override;
  bool fsRename(const char *pathFrom, const char *pathTo) override;
  HalFile fsOpen(const char *path, const char *mode) override;
  void fsClose(HalFile file) override;
  size_t fsRead(HalFile file, uint8_t *buf, size_t size) override;
  size_t fsWrite(HalFile file, const uint8_t *buf, size_t size) override;
  int fsAvailable(HalFile file) override;
  bool fsSeek(HalFile file, size_t pos) override;
  size_t fsPosition(HalFile file) override;
  size_t fsSize(HalFile file) override;

  // ========================================================================
  // WEB SERVER - AsyncWebServer Abstraction
  // ========================================================================

  bool webServerBegin(uint16_t port) override;
  void webServerOn(const char *uri, HAL_WebRequestMethod method,
                   WebServerHandler handler) override;
  void webServerAddHandler(WebServerHandler handler) override;
  void webServerAddRewrite(const char *from, const char *to) override;
  void webServerServeStatic(const char *uri, const char *path) override;
  void webServerOnNotFound(WebServerHandler handler) override;
  void webServerLoop() override;
  void webServerAddElegantOTA() override;

  // ========================================================================
  // LEDC (LED Control) PWM Functions
  // ========================================================================

  /**
   * @brief Setup LEDC PWM channel
   * @param channel PWM channel number (0-15 on ESP32)
   * @param freq PWM frequency in Hz
   * @param resolution PWM resolution in bits (1-16)
   */
  void pwmSetup(uint8_t channel, uint32_t freq, uint8_t resolution) override;

  /**
   * @brief Attach LEDC PWM channel to GPIO pin
   * @param pin GPIO pin number
   * @param channel PWM channel number
   */
  void pwmAttachPin(uint8_t pin, uint8_t channel) override;

  /**
   * @brief Write PWM duty cycle to channel
   * @param channel PWM channel number
   * @param duty Duty cycle value (0 to 2^resolution-1)
   */
  void pwmWrite(uint8_t channel, uint32_t duty) override;

  // ========================================================================
  // GPIO FUNCTIONS
  // ========================================================================

  void pinMode(uint8_t pin, uint8_t mode) override;
  void digitalWrite(uint8_t pin, uint8_t value) override;

  // ========================================================================
  // FREERTOS FUNCTIONS
  // ========================================================================

  int getCoreID() override;
  void *getCurrentTaskHandle() override;

  // ========================================================================
  // HTTP CLIENT FUNCTIONS - For OTA Updates
  // ========================================================================

  String httpGet(const String& url, unsigned long timeout_ms = 10000) override;
  bool httpGetStream(const String& url, HttpDataCallback on_data,
                     unsigned long timeout_ms = 60000) override;
  String httpPost(const String& url, const String& jsonBody, unsigned long timeout_ms = 10000) override;
  String smtpSend(const String& host, uint16_t port,
                   const String& username, const String& password,
                   const String& from, const String& to,
                   const String& subject, const String& body,
                   unsigned long timeout_ms = 15000) override;
  bool sha256Verify(const uint8_t *data, size_t data_length,
                    const String& expected_hash) override;

  // OTA Update functions
  bool otaBegin(size_t size, int command = 0) override;
  size_t otaWrite(const uint8_t* data, size_t len) override;
  bool otaEnd(bool evenIfRemaining = false) override;
  void otaAbort() override;
  String otaGetError() override;

  unsigned long millis() override;

  // ========================================================================
  // THREAD SAFETY - Shared State Mutex
  // ========================================================================

  bool lockSharedState(unsigned long timeout_ms = 1000) override;
  void unlockSharedState() override;

  // ========================================================================
  // NVS (Non-Volatile Storage) FUNCTIONS
  // ========================================================================

  bool nvsWriteString(const char* ns, const char* key, const String& value) override;
  String nvsReadString(const char* ns, const char* key) override;
  bool nvsRemove(const char* ns, const char* key) override;

private:
  AsyncWebServer *server_;
  void *sharedStateMutex_;  ///< FreeRTOS mutex for thread-safe shared state access (SemaphoreHandle_t)
};

#endif // __HAL_ESP32_H__
