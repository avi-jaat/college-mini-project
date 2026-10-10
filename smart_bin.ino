/*
 * Smart Waste Management & Bin-Level Monitoring
 * Board  : ESP32 Dev Module
 * Sensor : HC-SR04 ultrasonic
 * Cloud  : ThingSpeak (dashboard) + IFTTT Webhooks (email/SMS alert)
 *
 * Fill the "USER SETTINGS" section, then upload from Arduino IDE.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ===================== USER SETTINGS =====================
const char* WIFI_SSID    = "YOUR_WIFI_NAME";
const char* WIFI_PASS    = "YOUR_WIFI_PASSWORD";

const char* TS_API_KEY   = "YOUR_THINGSPEAK_WRITE_API_KEY";
const char* IFTTT_KEY    = "YOUR_IFTTT_WEBHOOK_KEY";
const char* IFTTT_EVENT  = "bin_full";      // event name you create in IFTTT

const char* BIN_ID       = "BIN-01";        // change for each bin

// Calibration (measure these with a ruler on your own bin, in cm)
const float EMPTY_DISTANCE_CM = 30.0;       // sensor reading when bin is empty
const float FULL_DISTANCE_CM  = 4.0;        // sensor reading when bin is 100% full

const float ALERT_ON_PERCENT  = 90.0;       // send alert at/above this level
const float ALERT_OFF_PERCENT = 70.0;       // re-arm alert once bin drops below this

const unsigned long LOOP_INTERVAL_MS = 20000; // ThingSpeak free tier: min 15 s
// =========================================================

// Pins
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;   // use a voltage divider here (see wiring notes)

const int SAMPLES = 5;

bool alertSent = false;

// ---------- WiFi ----------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed, will retry.");
  }
}

// ---------- Ultrasonic ----------
float readDistanceOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // 30 ms timeout (~5 m)
  if (duration == 0) return -1;                      // no echo
  return duration * 0.0343f / 2.0f;                  // cm
}

// Median of several readings filters out noise and random spikes
float readDistanceCm() {
  float readings[SAMPLES];
  int n = 0;

  for (int i = 0; i < SAMPLES; i++) {
    float d = readDistanceOnce();
    if (d > 0) readings[n++] = d;
    delay(60);
  }
  if (n == 0) return -1;

  // simple sort
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      if (readings[j] > readings[j + 1]) {
        float t = readings[j];
        readings[j] = readings[j + 1];
        readings[j + 1] = t;
      }
    }
  }
  return readings[n / 2];
}

float distanceToPercent(float distanceCm) {
  float pct = (EMPTY_DISTANCE_CM - distanceCm) /
              (EMPTY_DISTANCE_CM - FULL_DISTANCE_CM) * 100.0f;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

// ---------- ThingSpeak ----------
bool sendToThingSpeak(float fillPercent, float distanceCm, int alertFlag) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = "http://api.thingspeak.com/update?api_key=";
  url += TS_API_KEY;
  url += "&field1=" + String(fillPercent, 1);
  url += "&field2=" + String(distanceCm, 1);
  url += "&field3=" + String(alertFlag);

  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  String body = http.getString();   // ThingSpeak returns entry id, or 0 on failure
  http.end();

  Serial.printf("ThingSpeak -> HTTP %d, entry %s\n", code, body.c_str());
  return (code == 200 && body != "0");
}

// ---------- IFTTT alert (email / SMS) ----------
bool sendAlert(float fillPercent) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = "https://maker.ifttt.com/trigger/";
  url += IFTTT_EVENT;
  url += "/with/key/";
  url += IFTTT_KEY;
  url += "?value1=" + String(BIN_ID);
  url += "&value2=" + String(fillPercent, 0);

  WiFiClientSecure client;
  client.setInsecure();   // OK for a prototype; use a root CA cert in production

  HTTPClient https;
  https.begin(client, url);
  int code = https.GET();
  https.end();

  Serial.printf("IFTTT alert -> HTTP %d\n", code);
  return (code == 200);
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  connectWiFi();
}

void loop() {
  connectWiFi(); // reconnect if the connection dropped

  float distance = readDistanceCm();

  if (distance < 0) {
    Serial.println("Sensor error: no echo. Check wiring.");
    delay(LOOP_INTERVAL_MS);
    return;
  }

  float fill = distanceToPercent(distance);
  Serial.printf("[%s] Distance: %.1f cm | Fill: %.1f %%\n", BIN_ID, distance, fill);

  // Alert logic with hysteresis: sends once when full, re-arms after emptying
  if (fill >= ALERT_ON_PERCENT && !alertSent) {
    if (sendAlert(fill)) alertSent = true;
  } else if (fill <= ALERT_OFF_PERCENT) {
    alertSent = false;
  }

  sendToThingSpeak(fill, distance, alertSent ? 1 : 0);

  delay(LOOP_INTERVAL_MS);
}
