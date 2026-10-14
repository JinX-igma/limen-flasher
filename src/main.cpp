#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "../include/gateway_root_ca.h"

namespace {

constexpr char kFirmwareVersion[] = "0.2.0";
constexpr char kSetupPassword[] = "limen-codex";
constexpr char kConversationId[] = "device-main";
constexpr uint16_t kDnsPort = 53;
constexpr uint32_t kWifiTimeoutMs = 20000;
constexpr uint32_t kRestartDelayMs = 1200;
constexpr size_t kMaxResponseBytes = 64 * 1024;

Preferences preferences;
WebServer server(80);
DNSServer dnsServer;

String wifiSsid;
String wifiPassword;
String deviceToken;
String gatewayUrl;
bool allowInsecureTls = false;
String serialBuffer;
String setupApName;
bool setupMode = false;
bool clockReady = false;
uint32_t restartAt = 0;

String htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 16);
  for (char c : value) {
    switch (c) {
      case '&': escaped += F("&amp;"); break;
      case '<': escaped += F("&lt;"); break;
      case '>': escaped += F("&gt;"); break;
      case '"': escaped += F("&quot;"); break;
      case '\'': escaped += F("&#39;"); break;
      default: escaped += c;
    }
  }
  return escaped;
}

String maskedToken() {
  if (deviceToken.length() < 8) return deviceToken.isEmpty() ? "not set" : "set";
  return deviceToken.substring(0, 3) + "..." + deviceToken.substring(deviceToken.length() - 4);
}

String normalizedGatewayUrl() {
  String url = gatewayUrl;
  while (url.endsWith("/")) url.remove(url.length() - 1);
  return url;
}

void loadSettings() {
  preferences.begin("limen-codex", false);
  wifiSsid = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("wifi_password", "");
  deviceToken = preferences.getString("device_token", "");
  gatewayUrl = preferences.getString("gateway_url", "");
  allowInsecureTls = preferences.getBool("allow_insecure", false);
}

bool settingsReady() {
  return !wifiSsid.isEmpty() && !deviceToken.isEmpty() && !gatewayUrl.isEmpty();
}

void printHelp() {
  Serial.println();
  Serial.println(F("Limen commands:"));
  Serial.println(F("  <text>       send a prompt to the gateway"));
  Serial.println(F("  /status      show connection and gateway status"));
  Serial.println(F("  /setup       start the local setup portal"));
  Serial.println(F("  /clear yes   erase Wi-Fi and device credentials"));
  Serial.println(F("  /help        show this help"));
  Serial.println();
}

void printStatus() {
  Serial.printf("Firmware: %s\n", kFirmwareVersion);
  const int revision = ESP.getChipRevision();
  Serial.printf("Chip: %s, rev ", ESP.getChipModel());
  if (revision >= 100) {
    Serial.printf("%d.%d", revision / 100, (revision % 100) / 10);
  } else {
    Serial.printf("%d", revision);
  }
  Serial.printf(", flash %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("Wi-Fi: %s", WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(" (%s, %s)", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }
  Serial.println();
  Serial.printf("Device token: %s\n", maskedToken().c_str());
  Serial.printf("Gateway: %s\n", gatewayUrl.c_str());
  if (allowInsecureTls) {
    Serial.println(F("WARNING: insecure TLS is enabled (same-LAN/demo only)."));
  }
  if (setupMode) {
    Serial.printf("Setup portal: http://192.168.4.1 on %s\n", setupApName.c_str());
  }
}

String setupPage(const String &message = "") {
  String page;
  page.reserve(4600);
  page += F(R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Limen setup</title><style>
body{font-family:system-ui,sans-serif;background:#0b1020;color:#edf2ff;margin:0;padding:24px}main{max-width:560px;margin:auto;background:#141b31;border:1px solid #2b365a;border-radius:18px;padding:28px}h1{margin-top:0}label{display:block;margin:18px 0 6px}input{box-sizing:border-box;width:100%;padding:12px;border:1px solid #48557d;border-radius:10px;background:#0d1428;color:#fff}.checkbox-row{display:flex;align-items:center;gap:10px;margin-top:18px}.checkbox-row input{width:auto}button{margin-top:24px;width:100%;padding:13px;border:0;border-radius:10px;background:#6d8cff;color:#071029;font-weight:700}.note{color:#aab7db;font-size:.92rem}.warning{color:#ffb4b4;font-size:.9rem}.message{background:#173b30;padding:10px;border-radius:8px}</style></head><body><main><h1>Limen</h1>)HTML");
  if (!message.isEmpty()) page += "<p class=\"message\">" + htmlEscape(message) + "</p>";
  page += F("<p>Configure Wi-Fi and your gateway. This device never holds a real OpenAI API key — only a revocable token your gateway operator issued it.</p><form method=\"post\" action=\"/save\">");
  page += "<label>Wi-Fi SSID</label><input name=\"ssid\" maxlength=\"63\" required value=\"" + htmlEscape(wifiSsid) + "\">";
  page += F("<label>Wi-Fi password</label><input name=\"wifi_password\" type=\"password\" maxlength=\"63\" placeholder=\"Leave blank to keep the saved password\">");
  page += F("<label>Device token (from your gateway operator)</label><input name=\"device_token\" type=\"password\" maxlength=\"220\" placeholder=\"Leave blank to keep the saved token\">");
  page += "<label>Gateway URL</label><input name=\"gateway_url\" maxlength=\"128\" placeholder=\"https://gateway.local:8443\" value=\"" + htmlEscape(gatewayUrl) + "\">";
  page += "<div class=\"checkbox-row\"><input type=\"checkbox\" id=\"allow_insecure\" name=\"allow_insecure\"" + String(allowInsecureTls ? " checked" : "") + "><label for=\"allow_insecure\" style=\"margin:0\">Allow insecure TLS</label></div>";
  page += F("<p class=\"warning\">Only enable insecure TLS for a same-LAN demo gateway whose certificate you have not pinned. Never use it for an internet-facing gateway.</p>");
  page += F("<button type=\"submit\">Save and restart</button></form><p class=\"note\">This token is scoped and revocable by your gateway operator at any time — it is not a real OpenAI API key, and losing it does not expose your account.</p></main></body></html>");
  return page;
}

void redirectToPortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void startSetupPortal(const String &reason) {
  if (setupMode) return;
  setupMode = true;
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);
  const uint64_t mac = ESP.getEfuseMac();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06llX", mac & 0xFFFFFFULL);
  setupApName = "Limen-Codex-" + String(suffix);

  if (!WiFi.softAP(setupApName.c_str(), kSetupPassword)) {
    Serial.println(F("Failed to start setup access point."));
    return;
  }
  dnsServer.start(kDnsPort, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", setupPage()); });
  server.on("/save", HTTP_POST, []() {
    const String newSsid = server.arg("ssid");
    const String newPassword = server.arg("wifi_password");
    const String newToken = server.arg("device_token");
    const String newGatewayUrl = server.arg("gateway_url");
    const bool newAllowInsecure = server.hasArg("allow_insecure");

    if (newSsid.isEmpty() || (newToken.isEmpty() && deviceToken.isEmpty()) || newGatewayUrl.isEmpty()) {
      server.send(400, "text/html; charset=utf-8",
                  setupPage("SSID, gateway URL, and device token are required."));
      return;
    }

    const bool ssidChanged = newSsid != wifiSsid;
    wifiSsid = newSsid;
    if (!newPassword.isEmpty() || wifiPassword.isEmpty() || ssidChanged) wifiPassword = newPassword;
    if (!newToken.isEmpty()) deviceToken = newToken;
    gatewayUrl = newGatewayUrl;
    allowInsecureTls = newAllowInsecure;
    preferences.putString("ssid", wifiSsid);
    preferences.putString("wifi_password", wifiPassword);
    preferences.putString("device_token", deviceToken);
    preferences.putString("gateway_url", gatewayUrl);
    preferences.putBool("allow_insecure", allowInsecureTls);
    server.send(200, "text/html; charset=utf-8", setupPage("Saved. The device is restarting..."));
    restartAt = millis() + kRestartDelayMs;
  });
  server.on("/generate_204", HTTP_ANY, redirectToPortal);
  server.on("/hotspot-detect.html", HTTP_ANY, redirectToPortal);
  server.on("/connecttest.txt", HTTP_ANY, redirectToPortal);
  server.onNotFound(redirectToPortal);
  server.begin();

  Serial.printf("%s\n", reason.c_str());
  Serial.printf("Setup Wi-Fi: %s\n", setupApName.c_str());
  Serial.printf("Setup password: %s\n", kSetupPassword);
  Serial.println(F("Open http://192.168.4.1 in a browser."));
}

bool syncClock() {
  if (clockReady && time(nullptr) > 1700000000) return true;
  configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
  const uint32_t started = millis();
  while (time(nullptr) < 1700000000 && millis() - started < 15000) {
    delay(200);
  }
  clockReady = time(nullptr) >= 1700000000;
  if (!clockReady) Serial.println(F("NTP clock sync failed; TLS validation cannot continue."));
  return clockReady;
}

bool connectStation() {
  if (WiFi.status() == WL_CONNECTED) return syncClock();
  if (wifiSsid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  Serial.printf("Connecting to Wi-Fi %s", wifiSsid.c_str());
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < kWifiTimeoutMs) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("Connected: %s\n", WiFi.localIP().toString().c_str());
  return syncClock();
}

String responseText(JsonDocument &document) {
  String answer;
  for (JsonObject item : document["output"].as<JsonArray>()) {
    if (strcmp(item["type"] | "", "message") != 0) continue;
    for (JsonObject content : item["content"].as<JsonArray>()) {
      if (strcmp(content["type"] | "", "output_text") == 0) {
        if (!answer.isEmpty()) answer += '\n';
        answer += content["text"].as<String>();
      }
    }
  }
  return answer;
}

void callResponsesApi(const String &prompt) {
  if (!settingsReady()) {
    startSetupPortal("Configuration is incomplete.");
    return;
  }
  if (!connectStation()) {
    startSetupPortal("Wi-Fi or clock setup failed. Update settings in the portal.");
    return;
  }

  JsonDocument requestDocument;
  requestDocument["input"] = prompt;
  requestDocument["conversation"] = kConversationId;
  String payload;
  serializeJson(requestDocument, payload);

  WiFiClientSecure client;
  if (allowInsecureTls) {
    client.setInsecure();
    Serial.println(F("WARNING: skipping TLS certificate validation (insecure mode)."));
  } else {
    client.setCACert(kGatewayRootCa);
  }
  client.setHandshakeTimeout(30);
  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(60000);
  const String url = normalizedGatewayUrl() + "/v1/responses";
  if (!http.begin(client, url)) {
    Serial.println(F("Could not initialize the HTTPS request."));
    return;
  }

  http.addHeader("Authorization", "Bearer " + deviceToken);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "limen-esp32/" + String(kFirmwareVersion));
  Serial.println(F("Asking the gateway..."));
  const int status = http.POST(payload);
  payload = "";

  if (status <= 0) {
    Serial.printf("HTTPS error: %s\n", http.errorToString(status).c_str());
    http.end();
    return;
  }
  const int contentLength = http.getSize();
  if (contentLength > static_cast<int>(kMaxResponseBytes)) {
    Serial.printf("Response rejected: %d bytes exceeds the %u-byte limit.\n", contentLength,
                  static_cast<unsigned>(kMaxResponseBytes));
    http.end();
    return;
  }

  String body = http.getString();
  http.end();
  if (body.length() > kMaxResponseBytes) {
    Serial.println(F("Response rejected after download: response too large."));
    return;
  }

  JsonDocument responseDocument;
  const DeserializationError jsonError = deserializeJson(responseDocument, body);
  body = "";
  if (jsonError) {
    Serial.printf("Could not parse gateway JSON: %s\n", jsonError.c_str());
    return;
  }

  if (status < 200 || status >= 300) {
    const char *message = responseDocument["error"]["message"] | "Unknown gateway error";
    Serial.printf("Gateway error (HTTP %d): %s\n", status, message);
    return;
  }

  const String answer = responseText(responseDocument);
  if (answer.isEmpty()) {
    Serial.println(F("No answer text in gateway response."));
    return;
  }
  Serial.println(F("\nGateway:"));
  Serial.println(answer);
  Serial.println();
}

void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;
  if (line == "/help") {
    printHelp();
  } else if (line == "/status") {
    printStatus();
  } else if (line == "/setup") {
    startSetupPortal("Setup requested from serial.");
  } else if (line == "/clear yes") {
    preferences.clear();
    Serial.println(F("Credentials erased. Restarting..."));
    delay(300);
    ESP.restart();
  } else if (line.startsWith("/")) {
    Serial.println(F("Unknown command. Type /help."));
  } else {
    callResponsesApi(line);
  }
}

void readSerial() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      handleCommand(serialBuffer);
      serialBuffer = "";
    } else if (serialBuffer.length() < 4096) {
      serialBuffer += c;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\nLimen ESP32 %s\n", kFirmwareVersion);
  loadSettings();
  if (!settingsReady()) {
    startSetupPortal("First-run configuration required.");
  } else if (!connectStation()) {
    startSetupPortal("Saved Wi-Fi could not be reached.");
  } else {
    printStatus();
    printHelp();
  }
}

void loop() {
  readSerial();
  if (setupMode) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
  if (restartAt != 0 && static_cast<int32_t>(millis() - restartAt) >= 0) {
    ESP.restart();
  }
  delay(2);
}
