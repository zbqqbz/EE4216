#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include "lab_page.h"

#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_PIN 16
#define LED_COUNT 8

const char* AP_SSID = "A0311911U";
const char* AP_PASSWORD = "12345678";
WebServer server(80);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

struct LedState { uint8_t r, g, b, brightness; };
LedState leds[LED_COUNT] = {};

void sendJson(int code, const String& payload) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", payload);
}
void sendError(int code, const char* message) {
  sendJson(code, String("{\"error\":\"") + message + "\"}");
}
String ledJson(int id) {
  const LedState& s = leds[id];
  return String("{\"id\":") + id + ",\"r\":" + s.r + ",\"g\":" + s.g +
         ",\"b\":" + s.b + ",\"brightness\":" + s.brightness + "}";
}
void applyLed(int id) {
  const LedState& s = leds[id];
  uint8_t r = (uint16_t(s.r) * s.brightness) / 255;
  uint8_t g = (uint16_t(s.g) * s.brightness) / 255;
  uint8_t b = (uint16_t(s.b) * s.brightness) / 255;
  strip.setPixelColor(id, strip.Color(r, g, b));
  strip.show();
}
void handleDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long t = pulseIn(ECHO_PIN, HIGH, 30000);
  if (t == 0) {
    sendJson(200, "{\"distance_cm\":null,\"valid\":false}");
  } else {
    float cm = t * 0.0343f / 2.0f;
    sendJson(200, String("{\"distance_cm\":") + String(cm, 1) + ",\"valid\":true}");
  }
}
void handleGetLeds() {
  String result = "{\"leds\":[";
  for (int i = 0; i < LED_COUNT; ++i) {
    if (i) result += ',';
    result += ledJson(i);
  }
  result += "]}";
  sendJson(200, result);
}
// Strict parser for exactly four integer JSON fields, each in [0,255].
// No ArduinoJson dependency is needed.
bool parseLedBody(const String& body, LedState& out) {
  const char* keys[4] = {"r", "g", "b", "brightness"};
  uint8_t values[4];
  String s = body;
  s.trim();
  if (!s.startsWith("{") || !s.endsWith("}")) return false;
  // Parse the four JSON fields without assuming key order.
  String work = body;
  work.trim();
  work = work.substring(1, work.length() - 1);
  bool found[4] = {false,false,false,false};
  int fields = 0;
  while (work.length()) {
    int comma = work.indexOf(',');
    String part = comma < 0 ? work : work.substring(0, comma);
    work = comma < 0 ? "" : work.substring(comma + 1);
    int colon = part.indexOf(':');
    if (colon < 0) return false;
    String key = part.substring(0, colon), value = part.substring(colon + 1);
    key.trim(); value.trim();
    int index = -1;
    for (int i = 0; i < 4; ++i) if (key == String("\"") + keys[i] + "\"") index = i;
    if (index < 0 || found[index] || value.length() == 0 || value.length() > 3) return false;
    for (unsigned int j = 0; j < value.length(); ++j) if (!isDigit(value[j])) return false;
    int n = value.toInt();
    if (n > 255) return false;
    values[index] = (uint8_t)n;
    found[index] = true;
    ++fields;
  }
  if (fields != 4) return false;
  out = {values[0], values[1], values[2], values[3]};
  return true;
}
void handlePutLed() {
  const String prefix = "/api/leds/";
  String path = server.uri();
  if (!path.startsWith(prefix)) { sendError(404, "Unknown endpoint"); return; }
  String idString = path.substring(prefix.length());
  if (idString.length() != 1 || idString[0] < '0' || idString[0] > '7') {
    sendError(404, "Unknown LED id"); return;
  }
  int id = idString[0] - '0';
  LedState candidate;
  if (!server.hasArg("plain") || !parseLedBody(server.arg("plain"), candidate)) {
    sendError(400, "Invalid LED JSON: require integer r,g,b,brightness in 0..255");
    return;
  }
  leds[id] = candidate;
  applyLed(id);
  sendJson(200, ledJson(id));
}
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  strip.begin();
  strip.clear();
  strip.show();

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 2)) {
    Serial.println("AP start failed");
    return;
  }
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html", LAB_PAGE); });
  server.on("/api/distance", HTTP_GET, handleDistance);
  server.on("/api/leds", HTTP_GET, handleGetLeds);
  server.onNotFound([](){
    if (server.method() == HTTP_PUT && server.uri().startsWith("/api/leds/")) handlePutLed();
    else sendError(404, "Unknown endpoint");
  });
  server.begin();
  Serial.println("Dashboard: http://192.168.4.1/");
}
void loop() { server.handleClient(); }
