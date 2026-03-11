/*
  ESP32 Foot Pedal Firmware

  Features
  - BLE keyboard output
  - HTTP press/release triggers
  - BLE / HTTP / BOTH modes
  - Web UI configuration
  - OTA updates
  - Modifier combos supported
  - Lowercase preserved
  - 5 presets stored in flash

  Pedal wiring:
  GPIO14 -> switch -> GND
*/

#include <BleKeyboard.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ctype.h>

const char* WIFI_SSID = "SSID";
const char* WIFI_PASS = "PASSWORD";

const int PEDAL_PIN = 14;
const unsigned long DEBOUNCE_MS = 25;

BleKeyboard bleKeyboard("ESP32 Foot Pedal", "Generic PnP", 100);
WebServer server(80);
Preferences prefs;

enum OutputMode {
  MODE_BLE,
  MODE_HTTP,
  MODE_BOTH
};

OutputMode outputMode = MODE_BLE;

String configuredKey = "t";
String pressUrl = "";
String releaseUrl = "";
String pressMethod = "GET";
String releaseMethod = "GET";

bool pedalStableState = HIGH;
bool pedalLastReading = HIGH;
unsigned long lastDebounceTime = 0;
bool actionIsHeld = false;

const int MAX_PRESETS = 5;
String presetNames[MAX_PRESETS];
String presetKeys[MAX_PRESETS];

bool isSinglePrintableChar(const String& s) {
  return s.length() == 1 && s[0] >= 32 && s[0] <= 126;
}

String trimCopy(String s) {
  s.trim();
  return s;
}

String toUpperCopy(String s) {
  s.toUpperCase();
  return s;
}

uint8_t functionKeyCode(String key) {
  if (key == "F1") return KEY_F1;
  if (key == "F2") return KEY_F2;
  if (key == "F3") return KEY_F3;
  if (key == "F4") return KEY_F4;
  if (key == "F5") return KEY_F5;
  if (key == "F6") return KEY_F6;
  if (key == "F7") return KEY_F7;
  if (key == "F8") return KEY_F8;
  if (key == "F9") return KEY_F9;
  if (key == "F10") return KEY_F10;
  if (key == "F11") return KEY_F11;
  if (key == "F12") return KEY_F12;
  return 0;
}

uint8_t specialKeyCode(String key) {

  if (key == "ENTER") return KEY_RETURN;
  if (key == "TAB") return KEY_TAB;
  if (key == "ESC") return KEY_ESC;
  if (key == "BACKSPACE") return KEY_BACKSPACE;
  if (key == "DELETE") return KEY_DELETE;

  if (key == "UP") return KEY_UP_ARROW;
  if (key == "DOWN") return KEY_DOWN_ARROW;
  if (key == "LEFT") return KEY_LEFT_ARROW;
  if (key == "RIGHT") return KEY_RIGHT_ARROW;

  if (key == "HOME") return KEY_HOME;
  if (key == "END") return KEY_END;

  if (key == "PAGEUP") return KEY_PAGE_UP;
  if (key == "PAGEDOWN") return KEY_PAGE_DOWN;

  if (key == "SPACE") return ' ';

  return functionKeyCode(key);
}

bool blePressConfiguredKey() {

  if (!bleKeyboard.isConnected()) return false;

  String keyString = trimCopy(configuredKey);

  while (keyString.length()) {

    int plusIndex = keyString.indexOf('+');
    String part;

    if (plusIndex >= 0) {
      part = keyString.substring(0, plusIndex);
      keyString = keyString.substring(plusIndex + 1);
    } else {
      part = keyString;
      keyString = "";
    }

    part = trimCopy(part);
    String partUpper = toUpperCopy(part);

    if (partUpper == "CTRL") bleKeyboard.press(KEY_LEFT_CTRL);
    else if (partUpper == "SHIFT") bleKeyboard.press(KEY_LEFT_SHIFT);
    else if (partUpper == "ALT") bleKeyboard.press(KEY_LEFT_ALT);
    else if (partUpper == "WIN") bleKeyboard.press(KEY_LEFT_GUI);
    else {

      uint8_t special = specialKeyCode(partUpper);

      if (special) bleKeyboard.press(special);

      else if (isSinglePrintableChar(part)) {

        char c = part[0];

        if (isupper(c)) {
          bleKeyboard.press(KEY_LEFT_SHIFT);
          bleKeyboard.press(tolower(c));
        } else {
          bleKeyboard.press(c);
        }
      }
    }
  }

  return true;
}

bool bleReleaseConfiguredKey() {

  if (!bleKeyboard.isConnected()) return false;

  bleKeyboard.releaseAll();
  return true;
}

bool sendHttpRequest(String url, String method) {

  if (WiFi.status() != WL_CONNECTED) return false;
  if (url.length() == 0) return false;

  HTTPClient http;
  http.begin(url);

  int code;

  if (method == "POST")
    code = http.POST("");
  else
    code = http.GET();

  http.end();

  return code > 0;
}

void runPressAction() {

  if (outputMode == MODE_BLE || outputMode == MODE_BOTH)
    blePressConfiguredKey();

  if (outputMode == MODE_HTTP || outputMode == MODE_BOTH)
    sendHttpRequest(pressUrl, pressMethod);
}

void runReleaseAction() {

  if (outputMode == MODE_BLE || outputMode == MODE_BOTH)
    bleReleaseConfiguredKey();

  if (outputMode == MODE_HTTP || outputMode == MODE_BOTH)
    sendHttpRequest(releaseUrl, releaseMethod);
}

void savePreset(int index, String name, String key) {

  if (index < 0 || index >= MAX_PRESETS) return;

  presetNames[index] = name;
  presetKeys[index] = key;

  prefs.putString(("pname" + String(index)).c_str(), name);
  prefs.putString(("pkey" + String(index)).c_str(), key);
}

void handleLoadPreset() {

  int id = server.arg("id").toInt();

  configuredKey = presetKeys[id];
  prefs.putString("key", configuredKey);

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSavePreset() {

  int id = server.arg("id").toInt();
  savePreset(id, server.arg("name"), server.arg("key"));

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetMode() {

  String m = server.arg("mode");

  if (m == "HTTP") outputMode = MODE_HTTP;
  else if (m == "BOTH") outputMode = MODE_BOTH;
  else outputMode = MODE_BLE;

  prefs.putString("mode", m);

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetKey() {

  configuredKey = server.arg("key");
  prefs.putString("key", configuredKey);

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetHttp() {

  pressUrl = server.arg("pressUrl");
  releaseUrl = server.arg("releaseUrl");
  pressMethod = server.arg("pressMethod");
  releaseMethod = server.arg("releaseMethod");

  prefs.putString("pressUrl", pressUrl);
  prefs.putString("releaseUrl", releaseUrl);
  prefs.putString("pressMethod", pressMethod);
  prefs.putString("releaseMethod", releaseMethod);

  server.sendHeader("Location", "/");
  server.send(303);
}

String htmlPage() {

  String page;

  page += "<html><body style='font-family:Arial'>";

  page += "<h2>ESP32 Foot Pedal</h2>";

  page += "<h3>Mode</h3>";

  page += "<form method='POST' action='/mode'>";
  page += "<select name='mode'>";

  page += "<option value='BLE'";
  if (outputMode == MODE_BLE) page += " selected";
  page += ">BLE</option>";

  page += "<option value='HTTP'";
  if (outputMode == MODE_HTTP) page += " selected";
  page += ">HTTP</option>";

  page += "<option value='BOTH'";
  if (outputMode == MODE_BOTH) page += " selected";
  page += ">BLE + HTTP</option>";

  page += "</select>";
  page += "<button>Set</button>";
  page += "</form>";

  page += "<h3>Key</h3>";

  page += "<form method='POST' action='/set'>";
  page += "<input name='key' value='" + configuredKey + "'>";
  page += "<button>Save</button>";
  page += "</form>";

  page += "<h3>HTTP Actions</h3>";

  page += "<form method='POST' action='/http'>";
  page += "Press URL <input name='pressUrl' value='" + pressUrl + "'><br>";
  page += "Release URL <input name='releaseUrl' value='" + releaseUrl + "'><br>";

  page += "Press Method <input name='pressMethod' value='" + pressMethod + "'><br>";
  page += "Release Method <input name='releaseMethod' value='" + releaseMethod + "'><br>";

  page += "<button>Save HTTP</button>";
  page += "</form>";

  page += "<hr><h3>Presets</h3>";

  for (int i = 0; i < MAX_PRESETS; i++) {

    page += "<div style='border:1px solid #ccc;padding:8px;margin:8px;'>";

    page += "<b>" + presetNames[i] + "</b><br>";
    page += presetKeys[i] + "<br><br>";

    page += "<form method='POST' action='/preset/load' style='display:inline'>";
    page += "<input type='hidden' name='id' value='" + String(i) + "'>";
    page += "<button>Load</button></form>";

    page += "<form method='POST' action='/preset/save' style='display:inline'>";
    page += "<input type='hidden' name='id' value='" + String(i) + "'>";
    page += "<input name='name' placeholder='Name'>";
    page += "<input name='key' placeholder='Key'>";
    page += "<button>Save</button></form>";

    page += "</div>";
  }

  page += "</body></html>";

  return page;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void connectWiFi() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void setup() {

  Serial.begin(115200);

  pinMode(PEDAL_PIN, INPUT_PULLUP);

  prefs.begin("footpedal", false);

  configuredKey = prefs.getString("key", "t");

  pressUrl = prefs.getString("pressUrl", "");
  releaseUrl = prefs.getString("releaseUrl", "");

  pressMethod = prefs.getString("pressMethod", "GET");
  releaseMethod = prefs.getString("releaseMethod", "GET");

  String savedMode = prefs.getString("mode", "BLE");

  if (savedMode == "HTTP") outputMode = MODE_HTTP;
  else if (savedMode == "BOTH") outputMode = MODE_BOTH;

  for (int i = 0; i < MAX_PRESETS; i++) {
    presetNames[i] = prefs.getString(("pname" + String(i)).c_str(), "");
    presetKeys[i] = prefs.getString(("pkey" + String(i)).c_str(), "");
  }

  bleKeyboard.begin();

  connectWiFi();

  ArduinoOTA.begin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/mode", HTTP_POST, handleSetMode);
  server.on("/set", HTTP_POST, handleSetKey);
  server.on("/http", HTTP_POST, handleSetHttp);
  server.on("/preset/load", HTTP_POST, handleLoadPreset);
  server.on("/preset/save", HTTP_POST, handleSavePreset);

  server.begin();
}

void loop() {

  server.handleClient();
  ArduinoOTA.handle();

  bool reading = digitalRead(PEDAL_PIN);

  if (reading != pedalLastReading) {
    lastDebounceTime = millis();
    pedalLastReading = reading;
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {

    if (reading != pedalStableState) {

      pedalStableState = reading;

      if (pedalStableState == LOW) {

        if (!actionIsHeld) {
          runPressAction();
          actionIsHeld = true;
        }

      } else {

        if (actionIsHeld) {
          runReleaseAction();
          actionIsHeld = false;
        }
      }
    }
  }
}