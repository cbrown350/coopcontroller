#ifndef __IHAL_H__
#define __IHAL_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstddef>
#include <functional>
#include <stdint.h>

// Forward declaration for Arduino String class
// This allows interface to be platform-agnostic
class String;

// ============================================================================
// WEB SERVER INTERFACES
// ============================================================================

/**
 * @brief HTTP Request Method Enumeration
 *
 * Represents HTTP request methods (GET, POST, etc.)
 */
enum class HAL_WebRequestMethod {
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_PATCH,
  HTTP_ANY
};

/**
 * @brief Abstract Web Request Interface
 *
 * Provides platform-agnostic access to HTTP request properties.
 * This interface abstracts AsyncWebServerRequest for desktop testing.
 */
class IWebRequest {
public:
  virtual ~IWebRequest() = default;

  /**
   * @brief Get request URL path
   * @return URL path as string
   */
  virtual String url() const = 0;

  /**
   * @brief Get HTTP request method
   * @return HTTP method enumeration
   */
  virtual HAL_WebRequestMethod method() const = 0;

  /**
   * @brief Check if parameter exists
   * @param name Parameter name
   * @param post true to check POST parameters, false for GET/URL parameters
   * @return true if parameter exists
   */
  virtual bool hasParam(const char *name, bool post = false) const = 0;

  /**
   * @brief Get parameter value
   * @param name Parameter name
   * @param post true to get POST parameter, false for GET/URL parameter
   * @return Parameter value as string
   */
  virtual String param(const char *name, bool post = false) const = 0;

  /**
   * @brief Set JSON body content
   * @param json JSON variant representing the body
   */
  virtual void setJsonBody(const JsonVariant &json) = 0;

  /**
   * @brief Get JSON body content
   * @return JSON variant representing the body
   */
  virtual const JsonVariant &jsonBody() const = 0;

  /**
   * @brief Get request body as string
   * @return Request body content as string
   */
  virtual String body() const = 0;

  /**
   * @brief Check if request has a specific header
   * @param name Header name (case-insensitive)
   * @return true if header exists
   */
  virtual bool hasHeader(const char *name) const = 0;

  /**
   * @brief Get header value
   * @param name Header name (case-insensitive)
   * @return Header value as string (empty if not found)
   */
  virtual String header(const char *name) const = 0;
};

/**
 * @brief Abstract Web Response Interface
 *
 * Provides platform-agnostic HTTP response methods.
 * This interface abstracts AsyncWebServerResponse for desktop testing.
 */
class IWebResponse {
public:
  virtual ~IWebResponse() = default;

  /**
   * @brief Send HTTP response with content
   * @param code HTTP status code
   * @param contentType Content type (e.g., "text/plain", "application/json")
   * @param body Response body content
   */
  virtual void send(int code, const char *contentType, const char *body) = 0;

  /**
   * @brief Send file from filesystem as HTTP response
   * @param path File path in filesystem
   * @param contentType Content type (e.g., "text/html")
   */
  virtual void sendFile(const char *path, const char *contentType) = 0;

  /**
   * @brief Set response content length
   * @param len Content length in bytes
   */
  virtual void setContentLength(size_t len) = 0;

  /**
   * @brief Set response content type
   * @param type Content type string (e.g., "text/html")
   */
  virtual void setContentType(const char *type) = 0;

  /**
   * @brief Add response header
   * @param name Header name
   * @param value Header value
   */
  virtual void addHeader(const char *name, const char *value) = 0;

  /**
   * @brief Callback type for chunked response filling
   * @param buffer Output buffer to write data into
   * @param maxLen Maximum bytes to write
   * @param index Cumulative bytes sent so far
   * @return Number of bytes written, 0 when complete
   */
  typedef std::function<size_t(uint8_t* buffer, size_t maxLen, size_t index)> ChunkedFillCallback;

  /**
   * @brief Send chunked HTTP response using a fill callback
   *
   * For large responses that cannot fit in memory at once.
   * The callback is called repeatedly to fill the response buffer.
   * Return 0 from the callback to signal end of data.
   *
   * @param code HTTP status code
   * @param contentType Content type (e.g., "application/json")
   * @param callback Function called to fill each chunk
   */
  virtual void sendChunked(int code, const char* contentType, ChunkedFillCallback callback) = 0;
};

/**
 * @brief Web Server Request Handler Callback Type
 *
 * Function signature for HTTP request handlers.
 * Takes IWebRequest pointer and IWebResponse pointer as parameters.
 * The response pointer allows handlers to send HTTP responses.
 * Uses std::function to support lambdas with captures.
 */
using WebServerHandler = std::function<void(IWebRequest *, IWebResponse *)>;

/**
 * @brief Web Server JSON Handler Callback Type
 *
 * Function signature for HTTP request handlers with JSON body.
 * Takes IWebRequest pointer and JsonVariant pointer as parameters.
 *
 * Note: JsonVariant is passed as pointer to avoid forward declaration
 * conflicts with ArduinoJson template classes.
 */
using WebServerJsonHandler = void (*)(IWebRequest *, void *);

/**
 * @brief Opaque handle to a file in the filesystem
 * Represents a file opened via fsOpen/fsClose methods.
 *
 */
struct HalFileHandle; // opaque to users
using HalFile = HalFileHandle *;

// ============================================================================
// MAIN HAL INTERFACE
// ============================================================================

class IHAL { // NOSONAR
public:
  virtual ~IHAL() = default;

  // ========================================================================
  // EXISTING METHODS - DO NOT MODIFY
  // ========================================================================

  /**
   * @brief Initialize HAL subsystem
   * @return true if initialization successful
   */
  virtual bool begin() = 0;

  /**
   * @brief Get current timestamp (seconds since epoch)
   * @return Current time in seconds
   */
  virtual unsigned long getTime() = 0;

  /**
   * @brief Get free heap memory size
   * @return Free heap size in bytes
   */
  virtual uint32_t getFreeHeap() = 0;

  /**
   * @brief Check if WiFi is connected
   * @return true if WiFi connected
   */
  virtual bool WiFiIsConnected() = 0;

  /**
   * @brief Print formatted string to serial output
   * @param format Printf-style format string
   * @param ... Variable arguments
   */
  virtual void SerialPrintf(const char *format, ...) = 0;

  /**
   * @brief Print string to serial output with newline
   * @param message String to print
   */
  virtual void SerialPrintln(const char *message) = 0;

  /**
   * @brief Print string to serial output without newline
   * @param message String to print
   */
  virtual void SerialPrint(const char *message) = 0;

  // ========================================================================
  // ESP32 SYSTEM FUNCTIONS
  // ========================================================================

  /**
   * @brief Restart system
   */
  virtual void restart() = 0;

  /**
   * @brief Get total heap memory size
   * @return Total heap size in bytes
   */
  virtual uint32_t getHeapSize() = 0;

  /**
   * @brief Get chip model string
   * @return Chip model (e.g., "ESP32-D0WDQ6")
   */
  virtual const char *getChipModel() = 0;

  /**
   * @brief Get reset reason code
   * @return Reset reason code
   */
  virtual uint8_t getResetReason() = 0;

  /**
   * @brief Get CPU frequency in MHz
   * @return CPU frequency in MHz
   */
  virtual uint32_t getCpuFreqMHz() = 0;

  /**
   * @brief Get flash chip size in bytes
   * @return Flash chip size in bytes
   */
  virtual uint32_t getFlashChipSize() = 0;

  /**
   * @brief Reset the task watchdog timer
   *
   * This function resets the watchdog timer for the current task to prevent
   * watchdog timeout. Should be called periodically from long-running
   * operations.
   */
  virtual void taskWdtReset() = 0;

  // ========================================================================
  // TIME FUNCTIONS
  // ========================================================================

  /**
   * @brief Get local time from NTP
   * @param timeinfo Pointer to tm struct to receive time
   * @param ms Timeout in milliseconds
   * @return true if time retrieved successfully
   */
  virtual bool getLocalTime(struct tm *timeinfo, unsigned long ms) = 0;

  // ========================================================================
  // WIFI MANAGEMENT
  // ========================================================================

  /**
   * @brief Connect to WiFi network
   * @param ssid Network SSID
   * @param password Network password
   * @return true if connection initiated successfully
   */
  virtual bool wifiBegin(const char *ssid, const char *password) = 0;

  /**
   * @brief Connect to WiFi network with specific BSSID
   * @param ssid Network SSID
   * @param password Network password
   * @param bssid 6-byte BSSID to connect to (nullptr = auto-select)
   * @return true if connection initiated successfully
   */
  virtual bool wifiBeginWithBSSID(const char *ssid, const char *password, const uint8_t *bssid) = 0;

  /**
   * @brief Start WiFi Access Point mode
   * @param ssid AP SSID
   * @param password AP password (nullptr for open network)
   * @return true if AP started successfully
   */
  virtual bool wifiBeginAP(const char *ssid,
                           const char *password = nullptr) = 0;

  /**
   * @brief Set WiFi hostname
   * @param hostName_ Hostname string
   * @return true if hostname set successfully
   */
  virtual bool wifiSetHostname(const char *hostName_) = 0;

  /**
   * @brief Check if WiFi is connected
   * @return true if connected
   */
  virtual bool wifiIsConnected() = 0;

  /**
   * @brief Get current WiFi SSID
   * @return SSID string
   */
  virtual String wifiGetSSID() = 0;

  /**
   * @brief Get current WiFi BSSID
   * @return BSSID string
   */
  virtual String wifiGetBSSID() = 0;

  /**
   * @brief Get device MAC address
   * @return MAC address string
   */
  virtual String wifiGetMacAddress() = 0;

  /**
   * @brief Get local IP address
   * @return IP address as string
   */
  virtual String wifiGetLocalIP() = 0;

  /**
   * @brief Get Access Point IP address
   * @return AP IP address as string
   */
  virtual String wifiGetAPIP() = 0;

  /**
   * @brief Get WiFi signal strength
   * @return RSSI in dBm
   */
  virtual int wifiGetRSSI() = 0;

  /**
   * @brief Disconnect from WiFi
   */
  virtual void wifiDisconnect() = 0;

  /**
   * @brief Enable/disable automatic reconnection
   * @param autoReconnect true to enable auto-reconnect
   */
  virtual void wifiSetAutoReconnect(bool autoReconnect) = 0;

  /**
   * @brief Get WiFi connection status
   * @return WiFi status code
   */
  virtual int wifiGetStatus() = 0;

  /**
   * @brief Start mDNS service
   * @param hostname Hostname for mDNS
   * @return true if mDNS started successfully
   */
  virtual bool mdnsBegin(const char *hostname) = 0;

  // ========================================================================
  // FILESYSTEM - LittleFS
  // ========================================================================

  /**
   * @brief Initialize filesystem
   * @param formatOnFail Format filesystem if mount fails
   * @return true if filesystem initialized successfully
   */
  virtual bool fsBegin(bool formatOnFail = false) = 0;

  /**
   * @brief End filesystem
   */
  virtual void fsEnd() = 0;

  /**
   * @brief Check if file exists
   * @param path File path
   * @return true if file exists
   */
  virtual bool fsExists(const char *path) = 0;

  /**
   * @brief Remove file
   * @param path File path
   * @return true if file removed successfully
   */
  virtual bool fsRemove(const char *path) = 0;

  /**
   * @brief Rename file
   * @param pathFrom Source path
   * @param pathTo Destination path
   * @return true if renamed successfully
   */
  virtual bool fsRename(const char *pathFrom, const char *pathTo) = 0;

  /**
   * @brief Open file
   * @param path File path
   * @param mode File mode ("r", "w", "a", etc.)
   * @return File handle as HalFile
   */
  virtual HalFile fsOpen(const char *path, const char *mode) = 0;

  /**
   * @brief Close file
   * @param file File handle from fsOpen
   */
  virtual void fsClose(HalFile file) = 0;

  /**
   * @brief Read from file
   * @param file File handle from fsOpen
   * @param buf Buffer to read into
   * @param size Number of bytes to read
   * @return Number of bytes actually read
   */
  virtual size_t fsRead(HalFile file, uint8_t *buf, size_t size) = 0;

  /**
   * @brief Write to file
   * @param file File handle from fsOpen
   * @param buf Buffer to write from
   * @param size Number of bytes to write
   * @return Number of bytes actually written
   */
  virtual size_t fsWrite(HalFile file, const uint8_t *buf, size_t size) = 0;

  /**
   * @brief Get number of bytes available for reading
   * @param file File handle from fsOpen
   * @return Number of bytes available
   */
  virtual int fsAvailable(HalFile file) = 0;

  /**
   * @brief Seek to position in file
   * @param file File handle from fsOpen
   * @param pos Position to seek to
   * @return true if seek successful
   */
  virtual bool fsSeek(HalFile file, size_t pos) = 0;

  /**
   * @brief Get current position in file
   * @param file File handle from fsOpen
   * @return Current position
   */
  virtual size_t fsPosition(HalFile file) = 0;

  /**
   * @brief Get file size
   * @param file File handle from fsOpen
   * @return File size in bytes
   */
  virtual size_t fsSize(HalFile file) = 0;

  // ========================================================================
  // WEB SERVER - AsyncWebServer Abstraction
  // ========================================================================

  /**
   * @brief Initialize and start web server on specified port
   * @param port TCP port to listen on (e.g., 80)
   * @return true if web server started successfully
   */
  virtual bool webServerBegin(uint16_t port) = 0;

  /**
   * @brief Register HTTP request handler for specific URI and method
   * @param uri URI path (e.g., "/get_settings")
   * @param method HTTP request method
   * @param handler Callback function to handle requests
   */
  virtual void webServerOn(const char *uri, HAL_WebRequestMethod method,
                           WebServerHandler handler) = 0;

  /**
   * @brief Add custom handler to web server (e.g., for JSON body parsing)
   * @param handler Custom handler callback
   */
  virtual void webServerAddHandler(WebServerHandler handler) = 0;

  /**
   * @brief Add URL rewrite rule for SPA routing
   * @param from Source URI pattern
   * @param to Destination URI path
   */
  virtual void webServerAddRewrite(const char *from, const char *to) = 0;

  /**
   * @brief Configure static file serving from filesystem
   * @param uri URI prefix for static files (e.g., "/assets/")
   * @param path Filesystem path to serve from (e.g., "/assets/")
   */
  virtual void webServerServeStatic(const char *uri, const char *path) = 0;

  /**
   * @brief Set handler for requests that don't match any registered route
   * @param handler Callback function for 404/not found requests
   */
  virtual void webServerOnNotFound(WebServerHandler handler) = 0;

  /**
   * @brief Process web server events (for OTA, etc.)
   * Call this method in main loop to handle async web server operations
   */
  virtual void webServerLoop() = 0;

  /**
   * @brief Add ElegantOTA support to the web server
   */
  virtual void webServerAddElegantOTA() = 0;

  // ========================================================================
  // LEDC (LED Control) PWM Functions
  // ========================================================================

  /**
   * @brief Setup LEDC PWM channel
   * @param channel PWM channel number (0-15 on ESP32)
   * @param freq PWM frequency in Hz
   * @param resolution PWM resolution in bits (1-16)
   */
  virtual void pwmSetup(uint8_t channel, uint32_t freq, uint8_t resolution) = 0;

  /**
   * @brief Attach LEDC PWM channel to GPIO pin
   * @param pin GPIO pin number
   * @param channel PWM channel number
   */
  virtual void pwmAttachPin(uint8_t pin, uint8_t channel) = 0;

  /**
   * @brief Write PWM duty cycle to channel
   * @param channel PWM channel number
   * @param duty Duty cycle value (0 to 2^resolution-1)
   */
  virtual void pwmWrite(uint8_t channel, uint32_t duty) = 0;

  // ========================================================================
  // GPIO FUNCTIONS
  // ========================================================================

  /**
   * @brief Configure GPIO pin mode
   * @param pin GPIO pin number
   * @param mode Pin mode (INPUT, OUTPUT, INPUT_PULLUP, etc.)
   */
  virtual void pinMode(uint8_t pin, uint8_t mode) = 0;

  /**
   * @brief Write digital value to GPIO pin
   * @param pin GPIO pin number
   * @param value Digital value (HIGH or LOW)
   */
  virtual void digitalWrite(uint8_t pin, uint8_t value) = 0;

  // ========================================================================
  // FREERTOS FUNCTIONS
  // ========================================================================

  /**
   * @brief Get current FreeRTOS task core ID
   * @return Core ID (0 or 1 on ESP32)
   */
  virtual int getCoreID() = 0;

  /**
   * @brief Get current FreeRTOS task handle
   * @return Task handle pointer
   */
  virtual void *getCurrentTaskHandle() = 0;

  // ========================================================================
  // HTTP CLIENT FUNCTIONS - For OTA Updates
  // ========================================================================

  /**
   * @brief Data callback type for streaming HTTP downloads
   * Called as data chunks are received during download.
   * Parameters: data pointer, chunk length, bytes_downloaded, total_bytes
   * Return: true to continue, false to abort
   */
  typedef std::function<bool(const uint8_t* data, size_t len, uint32_t bytes_downloaded, uint32_t total_bytes)> HttpDataCallback;

  // Keep old name as alias for backwards compatibility
  typedef HttpDataCallback HttpProgressCallback;

  /**
   * @brief Perform HTTP GET request and return response body
   *
   * Sends HTTP GET request to the specified URL and returns the complete
   * response body as a string. For small responses (manifest JSON, etc).
   *
   * @param url Full URL to request (e.g., "https://example.com/data.json")
   * @param timeout_ms Request timeout in milliseconds
   * @return Response body as string, empty string on failure
   */
  virtual String httpGet(const String& url, unsigned long timeout_ms = 10000) = 0;

  /**
   * @brief Perform streaming HTTP GET request with data callback
   *
   * Sends HTTP GET request and calls data callback with each chunk received.
   * Handles HTTP redirects (301/302) for GitHub release URLs.
   * Useful for large downloads (firmware binaries) to avoid buffering in memory.
   *
   * @param url Full URL to request
   * @param on_data Callback function called with each data chunk
   * @param timeout_ms Request timeout in milliseconds
   * @return true if download completed successfully, false on failure
   */
  virtual bool httpGetStream(const String& url, HttpDataCallback on_data,
                             unsigned long timeout_ms = 60000) = 0;

  /**
   * @brief Verify SHA256 checksum
   *
   * Verifies that the provided data matches the expected SHA256 hash.
   * Uses ESP32 built-in mbedtls crypto.
   *
   * @param data Pointer to data buffer
   * @param data_length Length of data in bytes
   * @param expected_hash Expected SHA256 hash as hex string (64 characters)
   * @return true if hash matches, false otherwise
   */
  virtual bool sha256Verify(const uint8_t *data, size_t data_length,
                            const String& expected_hash) = 0;

  // ========================================================================
  // OTA UPDATE FUNCTIONS
  // ========================================================================

  /**
   * @brief Begin OTA update partition write
   * @param size Total size of update in bytes
   * @param command 0 for firmware (U_FLASH), 1 for filesystem (U_SPIFFS)
   * @return true if OTA partition opened successfully
   */
  virtual bool otaBegin(size_t size, int command = 0) = 0;

  /**
   * @brief Write data to OTA update partition
   * @param data Pointer to data buffer
   * @param len Length of data to write
   * @return Number of bytes written
   */
  virtual size_t otaWrite(const uint8_t* data, size_t len) = 0;

  /**
   * @brief Finalize OTA update
   * @param evenIfRemaining If true, finalize even if not all bytes written
   * @return true if finalization successful
   */
  virtual bool otaEnd(bool evenIfRemaining = false) = 0;

  /**
   * @brief Abort OTA update in progress
   */
  virtual void otaAbort() = 0;

  /**
   * @brief Get last OTA error message
   * @return Error description string
   */
  virtual String otaGetError() = 0;

  /**
   * @brief Get current milliseconds counter
   *
   * Returns milliseconds elapsed since boot. Used for timing and timeouts.
   *
   * @return Milliseconds since boot
   */
  virtual unsigned long millis() = 0;

  // ========================================================================
  // NVS (Non-Volatile Storage) FUNCTIONS
  // ========================================================================

  /**
   * @brief Write a string value to NVS
   *
   * Stores a string in the ESP32 NVS partition under the given namespace and key.
   * NVS survives OTA firmware and filesystem updates.
   *
   * @param ns NVS namespace (max 15 chars)
   * @param key Key name (max 15 chars)
   * @param value String value to store
   * @return true if write successful
   */
  virtual bool nvsWriteString(const char* ns, const char* key, const String& value) = 0;

  /**
   * @brief Read a string value from NVS
   *
   * @param ns NVS namespace (max 15 chars)
   * @param key Key name (max 15 chars)
   * @return Stored string value, or empty string if not found
   */
  virtual String nvsReadString(const char* ns, const char* key) = 0;

  /**
   * @brief Remove a key from NVS
   *
   * @param ns NVS namespace (max 15 chars)
   * @param key Key name to remove
   * @return true if removal successful
   */
  virtual bool nvsRemove(const char* ns, const char* key) = 0;
};

#endif // __IHAL_H__
