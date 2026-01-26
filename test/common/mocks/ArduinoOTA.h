// Mock ArduinoOTA library for desktop unit testing
// This provides stub implementations that do nothing in desktop environment

#ifndef ARDUINO_OTA_MOCK_H
#define ARDUINO_OTA_MOCK_H

#include <functional>

// OTA error types - must be defined before the class
enum ota_error_t {
  OTA_AUTH_ERROR,
  OTA_BEGIN_ERROR,
  OTA_CONNECT_ERROR,
  OTA_RECEIVE_ERROR,
  OTA_END_ERROR
};

class ArduinoOTAClass {
public:
  ArduinoOTAClass() {}
  
  void setHostname(const char* hostname) {
    // Stub - does nothing in desktop environment
  }
  
  void setPassword(const char* password) {
    // Stub - does nothing in desktop environment
  }
  
  void onStart(std::function<void()> callback) {
    // Stub - does nothing in desktop environment
  }
  
  void onEnd(std::function<void()> callback) {
    // Stub - does nothing in desktop environment
  }
  
  void onProgress(std::function<void(unsigned int, unsigned int)> callback) {
    // Stub - does nothing in desktop environment
  }
  
  void onError(std::function<void(ota_error_t)> callback) {
    // Stub - does nothing in desktop environment
  }
  
  void begin() {
    // Stub - does nothing in desktop environment
  }
  
  void handle() {
    // Stub - does nothing in desktop environment
  }
  
  int getCommand() {
    return 0;
  }
};

// Global ArduinoOTA instance
extern ArduinoOTAClass ArduinoOTA;

#endif // ARDUINO_OTA_MOCK_H
