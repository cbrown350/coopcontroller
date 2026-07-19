#include <gtest/gtest.h>
#include <ArduinoFake.h>
#include "HttpRequestBuilder.h"

// GitHub's REST API returns HTTP 403 to any request without a User-Agent
// header. The OTA manifest check now targets api.github.com, so every outbound
// GET must carry a User-Agent or the check silently fails to parse.
TEST(HttpRequestBuilderTest, IncludesUserAgentHeader) {
    String req = buildHttpGetRequest("api.github.com", "/repos/x/y/releases/latest");
    EXPECT_TRUE(req.indexOf("User-Agent:") >= 0)
        << "request missing User-Agent (GitHub API returns 403 without it):\n" << req.c_str();
}

TEST(HttpRequestBuilderTest, RequestsGivenHostAndPath) {
    String req = buildHttpGetRequest("api.github.com", "/repos/x/y/releases/latest");
    EXPECT_TRUE(req.startsWith("GET /repos/x/y/releases/latest HTTP/1.1\r\n"));
    EXPECT_TRUE(req.indexOf("Host: api.github.com\r\n") >= 0);
    EXPECT_TRUE(req.indexOf("Connection: close\r\n") >= 0);
    EXPECT_TRUE(req.endsWith("\r\n\r\n"));
}
