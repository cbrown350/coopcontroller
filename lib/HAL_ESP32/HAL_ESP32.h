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

private:
  AsyncWebServer *server_;
};

#endif // __HAL_ESP32_H__
