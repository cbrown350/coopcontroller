#ifndef __HTTP_REQUEST_BUILDER_H__
#define __HTTP_REQUEST_BUILDER_H__

#include <Arduino.h>

/**
 * @brief Build a minimal HTTP/1.1 GET request for the outbound TLS clients.
 *
 * Includes a User-Agent header: GitHub's REST API (used for the redirect-free
 * OTA manifest check at api.github.com) returns HTTP 403 to any request without
 * one. Kept as a pure, header-only helper so both HAL HTTP paths share one
 * request shape and it stays desktop-testable.
 *
 * @param host  Host header value (e.g. "api.github.com").
 * @param path  Absolute request path beginning with '/'.
 * @return Full request text terminated by a blank line.
 */
inline String buildHttpGetRequest(const String& host, const String& path) {
    String request = "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "User-Agent: CoopController-OTA\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    return request;
}

/**
 * @brief Build an HTTP/1.1 GET request with a Range header.
 *
 * Used by the OTA firmware/filesystem downloader to fetch large release assets
 * in small chunks. Sustained single-connection TLS downloads of multi-hundred-KB
 * bodies stall mid-transfer on the pioarduino 3.x / IDF 5.5.4 lwIP stack (the
 * peer sends one or a few segments then stops while connected() stays true).
 * Ranged downloads sidestep this: each chunk is small enough to arrive in a
 * burst before the stall, and each uses a fresh connection.
 *
 * @param host  Host header value.
 * @param path  Absolute request path beginning with '/'.
 * @param startByte  First byte offset (inclusive) of the range.
 * @param endByte    Last byte offset (inclusive) of the range.
 * @return Full request text terminated by a blank line.
 */
inline String buildHttpGetRangeRequest(const String& host, const String& path,
                                       uint32_t startByte, uint32_t endByte) {
    String request = "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "User-Agent: CoopController-OTA\r\n";
    request += "Range: bytes=" + String(startByte) + "-" + String(endByte) + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    return request;
}

#endif // __HTTP_REQUEST_BUILDER_H__
