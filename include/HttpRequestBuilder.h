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

#endif // __HTTP_REQUEST_BUILDER_H__
