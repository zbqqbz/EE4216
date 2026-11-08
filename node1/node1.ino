#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include "secrets.h"

const char* AP_SSID = "IoT-GW";

IPAddress nodeIP(192, 168, 4, 11);
IPAddress gatewayIP(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

const int SOIL_PIN = 4;
const int RAW_DRY = 2715;
const int RAW_WET = 1600;

const int RELAY_PIN = 5;
const int RELAY_ON  = LOW;
const int RELAY_OFF = HIGH;

WebServer server(80);

const unsigned long SEND_INTERVAL = 5000;
unsigned long lastSend = 0;

void handleCmd() {
  String value = server.arg("pump");

  if (value == "1") {
    digitalWrite(RELAY_PIN, RELAY_ON);
  } else if (value == "0") {
    digitalWrite(RELAY_PIN, RELAY_OFF);
  } else {
    server.send(400, "text/plain", "bad value");
    Serial.printf("CMD   pump=%s -> 400\n", value.c_str());
    return;
  }

  server.send(200, "text/plain", "pump=" + value);
  Serial.printf("CMD   pump=%s -> 200\n", value.c_str());
}

void setup() {
  Serial.begin(115200);

  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.config(nodeIP, gatewayIP, subnet);
  WiFi.begin(AP_SSID, AP_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected, IP = ");
  Serial.println(WiFi.localIP());

  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.begin();
}

void loop() {
  server.handleClient();

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    int raw = analogRead(SOIL_PIN);
    float soil = (raw - RAW_DRY) * 100.0 / (RAW_WET - RAW_DRY);

    String body = "id=node1&soil=" + String(soil, 1) + "&unit=percent";

    HTTPClient http;
    http.begin("http://192.168.4.1/api/data");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST(body);
    http.end();

    Serial.printf("raw=%d soil=%.1f%%  POST -> %d\n", raw, soil, code);
  }
}
