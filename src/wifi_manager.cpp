#include "wifi_manager.h"

#include <WiFi.h>

#include "config.h"

namespace {
uint32_t connectionStartedAt = 0;
uint32_t retryAt = 0;
bool connectionPending = false;
bool stackStarted = false;

void startConnection() {
  if (WIFI_SSID[0] == '\0') {
    return;
  }
  WiFi.mode(WIFI_STA);
  stackStarted = true;
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  connectionStartedAt = millis();
  connectionPending = true;
  Serial.println("[WiFi] Connecting in background...");
}
}

void beginWifi() {
  if (WIFI_SSID[0] == '\0') {
    Serial.println("[WiFi] No credentials; continuing offline");
    return;
  }
  startConnection();
}

void handleWifi() {
  if (!stackStarted) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (connectionPending && WiFi.status() == WL_CONNECTED) {
      connectionPending = false;
      Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }

  const uint32_t now = millis();
  if (connectionPending && now - connectionStartedAt >= AppConfig::WIFI_CONNECT_TIMEOUT_MS) {
    WiFi.disconnect();
    connectionPending = false;
    retryAt = now + 30000;
    Serial.println("[WiFi] Connection timed out; offline playback continues");
  } else if (!connectionPending && retryAt != 0 && static_cast<int32_t>(now - retryAt) >= 0) {
    startConnection();
  }
}

bool wifiConnected() {
  return stackStarted && WiFi.status() == WL_CONNECTED;
}

bool wifiStackStarted() {
  return stackStarted;
}

String wifiIpAddress() {
  return wifiConnected() ? WiFi.localIP().toString() : String();
}