/*
  ESP32 Dual Foot Pedal — BLE Keyboard

  Features:
  - Two independent pedals
  - Pedal 1: GPIO14 -> switch -> GND
  - Pedal 2: GPIO25 -> switch -> GND
  - BLE keyboard output
  - Separate active keybind for each pedal
  - Five saved presets per pedal
  - Modifier combinations:
      CTRL+ALT+SHIFT+F12
      CTRL+SHIFT+t
      ALT+SPACE
  - Preserves letter case:
      t sends lowercase t
      T sends uppercase T
  - Web configuration interface
  - OTA firmware updates
  - Settings stored in ESP32 Preferences/NVS
  - Handles both pedals being held simultaneously
  - First-boot Wi-Fi provisioning via WiFiManager captive portal
    (no Wi-Fi credentials are compiled into the firmware — each
    friend enters their own network's SSID/password on first setup)
  - Hold both pedals at boot for ~5 seconds to erase saved
    Wi-Fi credentials and re-open the setup portal

  Required libraries:
  - ESP32 BLE Keyboard by T-vK
  - WiFiManager by tzapu

  Board:
  - ESP32 Dev Module
*/

#include <BleKeyboard.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

// ============================================================
// USER SETTINGS
// ============================================================

// Name of the Wi-Fi setup access point a friend connects to on
// first boot (or after a credentials reset) to enter their own
// Wi-Fi network details. No SSID/password ships in the firmware.
const char* WIFI_SETUP_AP_NAME = "ESP32-Pedal-Setup";

// How long the setup portal stays open waiting for input before
// giving up and continuing offline (BLE keeps working regardless).
const unsigned long WIFI_PORTAL_TIMEOUT_SECONDS = 180;

// Hold both pedals at boot for this long to wipe saved Wi-Fi
// credentials and force the setup portal to reopen.
const unsigned long WIFI_RESET_HOLD_MS = 5000;

// Classic ESP32-WROOM has no GPIO24.
// Change PEDAL_2_PIN only if your second switch uses another valid GPIO.
const uint8_t PEDAL_1_PIN = 14;
const uint8_t PEDAL_2_PIN = 25;

const unsigned long DEBOUNCE_MS = 25;
const int MAX_PRESETS = 5;

// ============================================================
// GLOBAL OBJECTS
// ============================================================

BleKeyboard bleKeyboard(
  "ESP32 Foot Pedal",
  "Nuggetron",
  100
);

WebServer server(80);
Preferences prefs;

// ============================================================
// PEDAL STATE
// ============================================================

struct PedalState {
  uint8_t pin;
  bool stableState;
  bool lastReading;
  unsigned long lastDebounceTime;
  bool pressed;
};

PedalState pedals[2] = {
  {PEDAL_1_PIN, HIGH, HIGH, 0, false},
  {PEDAL_2_PIN, HIGH, HIGH, 0, false}
};

// Active keybinds
String configuredKeys[2] = {
  "t",
  "CTRL+ALT+SHIFT+F12"
};

// Preset banks
String presetNames[2][MAX_PRESETS];
String presetKeys[2][MAX_PRESETS];

// ============================================================
// STRING UTILITIES
// ============================================================

String trimCopy(String value) {
  value.trim();
  return value;
}

String uppercaseCopy(String value) {
  value.toUpperCase();
  return value;
}

String htmlEscape(const String& value) {
  String escaped = value;

  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  escaped.replace("'", "&#39;");

  return escaped;
}

bool isSinglePrintableCharacter(const String& value) {
  if (value.length() != 1) {
    return false;
  }

  const unsigned char character = value[0];
  return character >= 32 && character <= 126;
}

// ============================================================
// KEY MAPPING
// ============================================================

uint8_t getFunctionKeyCode(const String& keyUpper) {
  if (keyUpper == "F1")  return KEY_F1;
  if (keyUpper == "F2")  return KEY_F2;
  if (keyUpper == "F3")  return KEY_F3;
  if (keyUpper == "F4")  return KEY_F4;
  if (keyUpper == "F5")  return KEY_F5;
  if (keyUpper == "F6")  return KEY_F6;
  if (keyUpper == "F7")  return KEY_F7;
  if (keyUpper == "F8")  return KEY_F8;
  if (keyUpper == "F9")  return KEY_F9;
  if (keyUpper == "F10") return KEY_F10;
  if (keyUpper == "F11") return KEY_F11;
  if (keyUpper == "F12") return KEY_F12;

  return 0;
}

uint8_t getSpecialKeyCode(const String& keyUpper) {
  if (keyUpper == "ENTER" || keyUpper == "RETURN") {
    return KEY_RETURN;
  }

  if (keyUpper == "TAB") {
    return KEY_TAB;
  }

  if (keyUpper == "ESC" || keyUpper == "ESCAPE") {
    return KEY_ESC;
  }

  if (keyUpper == "SPACE") {
    return ' ';
  }

  if (keyUpper == "BACKSPACE") {
    return KEY_BACKSPACE;
  }

  if (keyUpper == "DELETE" || keyUpper == "DEL") {
    return KEY_DELETE;
  }

  if (keyUpper == "INSERT" || keyUpper == "INS") {
    return KEY_INSERT;
  }

  if (keyUpper == "UP") {
    return KEY_UP_ARROW;
  }

  if (keyUpper == "DOWN") {
    return KEY_DOWN_ARROW;
  }

  if (keyUpper == "LEFT") {
    return KEY_LEFT_ARROW;
  }

  if (keyUpper == "RIGHT") {
    return KEY_RIGHT_ARROW;
  }

  if (keyUpper == "HOME") {
    return KEY_HOME;
  }

  if (keyUpper == "END") {
    return KEY_END;
  }

  if (keyUpper == "PAGEUP" || keyUpper == "PGUP") {
    return KEY_PAGE_UP;
  }

  if (keyUpper == "PAGEDOWN" || keyUpper == "PGDN") {
    return KEY_PAGE_DOWN;
  }

  return getFunctionKeyCode(keyUpper);
}

bool isModifier(const String& keyUpper) {
  return keyUpper == "CTRL" ||
         keyUpper == "CONTROL" ||
         keyUpper == "SHIFT" ||
         keyUpper == "ALT" ||
         keyUpper == "WIN" ||
         keyUpper == "WINDOWS" ||
         keyUpper == "GUI";
}

// ============================================================
// KEYBIND VALIDATION
// ============================================================

bool isValidKeyToken(String token) {
  token.trim();

  if (token.length() == 0) {
    return false;
  }

  // Preserve exact case for individual characters.
  if (isSinglePrintableCharacter(token)) {
    return true;
  }

  const String tokenUpper = uppercaseCopy(token);

  if (isModifier(tokenUpper)) {
    return true;
  }

  if (getSpecialKeyCode(tokenUpper) != 0) {
    return true;
  }

  return false;
}

bool isValidKeybind(String keybind) {
  keybind.trim();

  if (keybind.length() == 0) {
    return false;
  }

  int tokenStart = 0;

  while (tokenStart < keybind.length()) {
    const int plusPosition = keybind.indexOf('+', tokenStart);

    String token;

    if (plusPosition >= 0) {
      token = keybind.substring(tokenStart, plusPosition);
      tokenStart = plusPosition + 1;
    } else {
      token = keybind.substring(tokenStart);
      tokenStart = keybind.length();
    }

    token.trim();

    if (!isValidKeyToken(token)) {
      return false;
    }
  }

  return true;
}

// ============================================================
// BLE KEYBOARD HANDLING
// ============================================================

bool pressKeybind(String keybind) {
  if (!bleKeyboard.isConnected()) {
    return false;
  }

  keybind.trim();

  int tokenStart = 0;
  bool sentAtLeastOneKey = false;

  while (tokenStart < keybind.length()) {
    const int plusPosition = keybind.indexOf('+', tokenStart);

    String token;

    if (plusPosition >= 0) {
      token = keybind.substring(tokenStart, plusPosition);
      tokenStart = plusPosition + 1;
    } else {
      token = keybind.substring(tokenStart);
      tokenStart = keybind.length();
    }

    token.trim();

    if (token.length() == 0) {
      continue;
    }

    const String tokenUpper = uppercaseCopy(token);

    if (tokenUpper == "CTRL" || tokenUpper == "CONTROL") {
      bleKeyboard.press(KEY_LEFT_CTRL);
      sentAtLeastOneKey = true;
      continue;
    }

    if (tokenUpper == "SHIFT") {
      bleKeyboard.press(KEY_LEFT_SHIFT);
      sentAtLeastOneKey = true;
      continue;
    }

    if (tokenUpper == "ALT") {
      bleKeyboard.press(KEY_LEFT_ALT);
      sentAtLeastOneKey = true;
      continue;
    }

    if (
      tokenUpper == "WIN" ||
      tokenUpper == "WINDOWS" ||
      tokenUpper == "GUI"
    ) {
      bleKeyboard.press(KEY_LEFT_GUI);
      sentAtLeastOneKey = true;
      continue;
    }

    const uint8_t specialKey = getSpecialKeyCode(tokenUpper);

    if (specialKey != 0) {
      bleKeyboard.press(specialKey);
      sentAtLeastOneKey = true;
      continue;
    }

    if (isSinglePrintableCharacter(token)) {
      /*
        Pass the original character to BleKeyboard.

        This deliberately preserves its case:
        - "t" is passed as 't'
        - "T" is passed as 'T'

        The BLE keyboard library performs the required HID translation.
      */
      bleKeyboard.press(static_cast<uint8_t>(token[0]));
      sentAtLeastOneKey = true;
      continue;
    }

    Serial.print("Unsupported key token: ");
    Serial.println(token);
  }

  return sentAtLeastOneKey;
}

/*
  Rebuild all currently held keys.

  This is safer than releasing only one pedal's keys because two keybinds
  may share modifiers. For example:

    Pedal 1: CTRL+ALT+F11
    Pedal 2: CTRL+ALT+F12

  Releasing either pedal should not leave modifiers stuck or accidentally
  release the keybind still held by the other pedal.
*/
void rebuildHeldKeys() {
  if (!bleKeyboard.isConnected()) {
    return;
  }

  bleKeyboard.releaseAll();

  for (int pedalIndex = 0; pedalIndex < 2; pedalIndex++) {
    if (pedals[pedalIndex].pressed) {
      pressKeybind(configuredKeys[pedalIndex]);
    }
  }
}

// ============================================================
// PREFERENCES / STORAGE
// ============================================================

String presetNamePreferenceKey(int pedalIndex, int presetIndex) {
  // NVS preference keys must remain short.
  return "p" + String(pedalIndex + 1) +
         "n" + String(presetIndex);
}

String presetKeyPreferenceKey(int pedalIndex, int presetIndex) {
  return "p" + String(pedalIndex + 1) +
         "k" + String(presetIndex);
}

void loadSettings() {
  configuredKeys[0] = prefs.getString("key1", "t");
  configuredKeys[1] = prefs.getString(
    "key2",
    "CTRL+ALT+SHIFT+F12"
  );

  for (int pedalIndex = 0; pedalIndex < 2; pedalIndex++) {
    for (int presetIndex = 0;
         presetIndex < MAX_PRESETS;
         presetIndex++) {

      const String nameKey = presetNamePreferenceKey(
        pedalIndex,
        presetIndex
      );

      const String keyKey = presetKeyPreferenceKey(
        pedalIndex,
        presetIndex
      );

      presetNames[pedalIndex][presetIndex] =
        prefs.getString(nameKey.c_str(), "");

      presetKeys[pedalIndex][presetIndex] =
        prefs.getString(keyKey.c_str(), "");
    }
  }
}

void saveConfiguredKey(int pedalIndex, const String& keybind) {
  if (pedalIndex < 0 || pedalIndex > 1) {
    return;
  }

  configuredKeys[pedalIndex] = keybind;

  const char* preferenceKey =
    pedalIndex == 0 ? "key1" : "key2";

  prefs.putString(preferenceKey, keybind);
}

void savePreset(
  int pedalIndex,
  int presetIndex,
  const String& name,
  const String& keybind
) {
  if (
    pedalIndex < 0 ||
    pedalIndex > 1 ||
    presetIndex < 0 ||
    presetIndex >= MAX_PRESETS
  ) {
    return;
  }

  presetNames[pedalIndex][presetIndex] = name;
  presetKeys[pedalIndex][presetIndex] = keybind;

  const String nameKey = presetNamePreferenceKey(
    pedalIndex,
    presetIndex
  );

  const String keyKey = presetKeyPreferenceKey(
    pedalIndex,
    presetIndex
  );

  prefs.putString(nameKey.c_str(), name);
  prefs.putString(keyKey.c_str(), keybind);
}

// ============================================================
// WEB INTERFACE
// ============================================================

String buildPresetSection(int pedalIndex) {
  String section;

  const int displayNumber = pedalIndex + 1;

  section += "<section class='card'>";
  section += "<h2>Pedal " + String(displayNumber) + "</h2>";

  section += "<p class='status'>";
  section += "<strong>Pin:</strong> GPIO";
  section += String(pedals[pedalIndex].pin);
  section += "<br><strong>Status:</strong> ";
  section += pedals[pedalIndex].pressed ?
    "Pressed" :
    "Released";
  section += "</p>";

  section +=
    "<form method='POST' action='/key/save' class='main-form'>";

  section +=
    "<input type='hidden' name='pedal' value='" +
    String(pedalIndex) +
    "'>";

  section +=
    "<label for='key" +
    String(pedalIndex) +
    "'>Active keybind</label>";

  section +=
    "<input id='key" +
    String(pedalIndex) +
    "' name='key' value='" +
    htmlEscape(configuredKeys[pedalIndex]) +
    "' required>";

  section +=
    "<button type='submit'>Save Active Keybind</button>";

  section += "</form>";

  section += "<h3>Saved Presets</h3>";

  for (
    int presetIndex = 0;
    presetIndex < MAX_PRESETS;
    presetIndex++
  ) {
    const String savedName =
      presetNames[pedalIndex][presetIndex];

    const String savedKey =
      presetKeys[pedalIndex][presetIndex];

    section += "<div class='preset'>";

    section += "<div class='preset-heading'>";
    section += "<strong>";

    if (savedName.length() > 0) {
      section += htmlEscape(savedName);
    } else {
      section += "Preset " + String(presetIndex + 1);
    }

    section += "</strong>";

    section += "<code>";

    if (savedKey.length() > 0) {
      section += htmlEscape(savedKey);
    } else {
      section += "Empty";
    }

    section += "</code>";
    section += "</div>";

    section +=
      "<form method='POST' action='/preset/load' "
      "class='inline-form'>";

    section +=
      "<input type='hidden' name='pedal' value='" +
      String(pedalIndex) +
      "'>";

    section +=
      "<input type='hidden' name='preset' value='" +
      String(presetIndex) +
      "'>";

    section += "<button type='submit'";

    if (savedKey.length() == 0) {
      section += " disabled";
    }

    section += ">Activate</button>";
    section += "</form>";

    section +=
      "<form method='POST' action='/preset/save' "
      "class='preset-edit'>";

    section +=
      "<input type='hidden' name='pedal' value='" +
      String(pedalIndex) +
      "'>";

    section +=
      "<input type='hidden' name='preset' value='" +
      String(presetIndex) +
      "'>";

    section +=
      "<input name='name' placeholder='Preset name' value='" +
      htmlEscape(savedName) +
      "'>";

    section +=
      "<input name='key' placeholder='Key or combo' value='" +
      htmlEscape(savedKey) +
      "' required>";

    section += "<button type='submit'>Save Preset</button>";
    section += "</form>";

    section += "</div>";
  }

  section += "</section>";

  return section;
}

String htmlPage() {
  String page;

  page.reserve(14000);

  page += "<!doctype html>";
  page += "<html lang='en'>";
  page += "<head>";
  page += "<meta charset='utf-8'>";
  page +=
    "<meta name='viewport' "
    "content='width=device-width,initial-scale=1'>";

  page += "<title>Dual Foot Pedal</title>";

  page += "<style>";
  page +=
    "*{box-sizing:border-box}"
    "body{"
      "margin:0;"
      "background:#f4f5f7;"
      "color:#202124;"
      "font-family:Arial,sans-serif"
    "}"
    "main{"
      "max-width:1050px;"
      "margin:0 auto;"
      "padding:24px"
    "}"
    "header{margin-bottom:22px}"
    "h1{margin:0 0 8px}"
    "h2{margin-top:0}"
    "h3{margin-top:28px}"
    ".summary{"
      "background:#fff;"
      "border-radius:10px;"
      "padding:14px 18px;"
      "box-shadow:0 1px 4px rgba(0,0,0,.12)"
    "}"
    ".grid{"
      "display:grid;"
      "grid-template-columns:repeat(2,minmax(0,1fr));"
      "gap:20px;"
      "margin-top:20px"
    "}"
    ".card{"
      "background:#fff;"
      "border-radius:12px;"
      "padding:20px;"
      "box-shadow:0 1px 5px rgba(0,0,0,.14)"
    "}"
    ".status{line-height:1.7}"
    "label{"
      "display:block;"
      "font-weight:bold;"
      "margin-bottom:7px"
    "}"
    "input{"
      "width:100%;"
      "padding:10px;"
      "border:1px solid #b8bcc2;"
      "border-radius:6px;"
      "font-size:16px"
    "}"
    "button{"
      "padding:10px 14px;"
      "border:0;"
      "border-radius:6px;"
      "cursor:pointer;"
      "font-size:14px"
    "}"
    "button:not(:disabled){"
      "background:#1a73e8;"
      "color:white"
    "}"
    "button:disabled{"
      "background:#d7d9dc;"
      "color:#777;"
      "cursor:not-allowed"
    "}"
    ".main-form button{"
      "margin-top:10px;"
      "width:100%"
    "}"
    ".preset{"
      "border-top:1px solid #e1e3e6;"
      "padding:16px 0"
    "}"
    ".preset-heading{"
      "display:flex;"
      "justify-content:space-between;"
      "gap:10px;"
      "margin-bottom:10px"
    "}"
    "code{"
      "background:#eef0f3;"
      "padding:3px 7px;"
      "border-radius:4px;"
      "overflow-wrap:anywhere"
    "}"
    ".inline-form{margin-bottom:10px}"
    ".preset-edit{"
      "display:grid;"
      "grid-template-columns:1fr 1.4fr auto;"
      "gap:8px"
    "}"
    ".help{"
      "background:#fff;"
      "margin-top:20px;"
      "padding:18px;"
      "border-radius:10px;"
      "line-height:1.6"
    "}"
    "@media(max-width:800px){"
      ".grid{grid-template-columns:1fr}"
      ".preset-edit{grid-template-columns:1fr}"
      ".preset-edit button{width:100%}"
    "}";

  page += "</style>";
  page += "</head>";
  page += "<body>";
  page += "<main>";

  page += "<header>";
  page += "<h1>ESP32 Foot Pedal</h1>";

  page += "<div class='summary'>";
  page += "<strong>Bluetooth:</strong> ";
  page += bleKeyboard.isConnected() ?
    "Connected" :
    "Not connected";

  page += "<br><strong>Wi-Fi:</strong> ";
  page += WiFi.status() == WL_CONNECTED ?
    WiFi.localIP().toString() :
    "Not connected";

  page += "<br><strong>OTA hostname:</strong> ";
  page += "esp32-pedal.local";
  page += "</div>";
  page += "</header>";

  page += "<div class='grid'>";
  page += buildPresetSection(0);
  page += buildPresetSection(1);
  page += "</div>";

  page += "<div class='help'>";
  page += "<strong>Keybind examples</strong><br>";
  page += "<code>t</code> sends lowercase t.<br>";
  page += "<code>T</code> sends uppercase T.<br>";
  page +=
    "<code>CTRL+ALT+SHIFT+F12</code> sends a modifier combo.<br>";
  page +=
    "Other supported names include SPACE, ENTER, TAB, ESC, "
    "BACKSPACE, DELETE, arrows, HOME, END, PAGEUP, PAGEDOWN, "
    "and F1-F12.";
  page += "</div>";

  page += "</main>";
  page += "</body>";
  page += "</html>";

  return page;
}

// ============================================================
// WEB HANDLERS
// ============================================================

bool parsePedalAndPreset(
  int& pedalIndex,
  int& presetIndex
) {
  if (
    !server.hasArg("pedal") ||
    !server.hasArg("preset")
  ) {
    return false;
  }

  pedalIndex = server.arg("pedal").toInt();
  presetIndex = server.arg("preset").toInt();

  return pedalIndex >= 0 &&
         pedalIndex < 2 &&
         presetIndex >= 0 &&
         presetIndex < MAX_PRESETS;
}

void redirectHome() {
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleSaveActiveKey() {
  if (
    !server.hasArg("pedal") ||
    !server.hasArg("key")
  ) {
    server.send(400, "text/plain", "Missing pedal or key.");
    return;
  }

  const int pedalIndex = server.arg("pedal").toInt();
  String keybind = server.arg("key");
  keybind.trim();

  if (pedalIndex < 0 || pedalIndex > 1) {
    server.send(400, "text/plain", "Invalid pedal.");
    return;
  }

  if (!isValidKeybind(keybind)) {
    server.send(
      400,
      "text/plain",
      "Invalid keybind. Example: t or CTRL+ALT+SHIFT+F12"
    );
    return;
  }

  saveConfiguredKey(pedalIndex, keybind);

  /*
    If the pedal is currently held while its keybind changes,
    immediately rebuild the held keys using the new setting.
  */
  rebuildHeldKeys();

  redirectHome();
}

void handleSavePreset() {
  int pedalIndex;
  int presetIndex;

  if (!parsePedalAndPreset(pedalIndex, presetIndex)) {
    server.send(400, "text/plain", "Invalid preset request.");
    return;
  }

  if (!server.hasArg("key")) {
    server.send(400, "text/plain", "Missing preset keybind.");
    return;
  }

  String name = server.hasArg("name") ?
    server.arg("name") :
    "";

  String keybind = server.arg("key");

  name.trim();
  keybind.trim();

  if (!isValidKeybind(keybind)) {
    server.send(
      400,
      "text/plain",
      "Invalid preset keybind."
    );
    return;
  }

  if (name.length() == 0) {
    name = "Preset " + String(presetIndex + 1);
  }

  savePreset(
    pedalIndex,
    presetIndex,
    name,
    keybind
  );

  redirectHome();
}

void handleLoadPreset() {
  int pedalIndex;
  int presetIndex;

  if (!parsePedalAndPreset(pedalIndex, presetIndex)) {
    server.send(400, "text/plain", "Invalid preset request.");
    return;
  }

  String keybind = presetKeys[pedalIndex][presetIndex];
  keybind.trim();

  if (keybind.length() == 0) {
    server.send(400, "text/plain", "That preset is empty.");
    return;
  }

  if (!isValidKeybind(keybind)) {
    server.send(
      400,
      "text/plain",
      "The saved preset contains an invalid keybind."
    );
    return;
  }

  saveConfiguredKey(pedalIndex, keybind);
  rebuildHeldKeys();

  redirectHome();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found.");
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);

  server.on(
    "/key/save",
    HTTP_POST,
    handleSaveActiveKey
  );

  server.on(
    "/preset/save",
    HTTP_POST,
    handleSavePreset
  );

  server.on(
    "/preset/load",
    HTTP_POST,
    handleLoadPreset
  );

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started.");
}

// ============================================================
// WI-FI AND OTA
// ============================================================

/*
  Checks whether both pedals are held down at boot. If so, and if
  they're still both held after WIFI_RESET_HOLD_MS, saved Wi-Fi
  credentials are erased so the setup portal reopens.

  This exists because the enclosure has no spare physical button
  for a manual Wi-Fi reset.
*/
void checkForWiFiCredentialReset() {
  const bool pedal1Held = digitalRead(PEDAL_1_PIN) == LOW;
  const bool pedal2Held = digitalRead(PEDAL_2_PIN) == LOW;

  if (!pedal1Held || !pedal2Held) {
    return;
  }

  Serial.println(
    "Both pedals held at boot — hold for 5 seconds to reset "
    "Wi-Fi credentials..."
  );

  const unsigned long holdStartTime = millis();

  while (millis() - holdStartTime < WIFI_RESET_HOLD_MS) {
    if (
      digitalRead(PEDAL_1_PIN) == HIGH ||
      digitalRead(PEDAL_2_PIN) == HIGH
    ) {
      Serial.println("Released early — Wi-Fi credentials kept.");
      return;
    }

    delay(20);
  }

  Serial.println("Erasing saved Wi-Fi credentials...");

  WiFiManager wm;
  wm.resetSettings();

  Serial.println("Wi-Fi credentials erased. Continuing to setup portal.");
}

/*
  Connects to a previously saved Wi-Fi network, or — if none is
  saved (first boot) or the saved network can't be reached —
  starts a captive-portal access point so the person setting up
  the pedal can enter their own network's credentials.

  No SSID/password is ever compiled into the firmware. Credentials
  entered through the portal are stored by WiFiManager in NVS,
  the same flash storage area used by Preferences elsewhere in
  this sketch.

  BLE keyboard functionality does not depend on Wi-Fi, so pedals
  work immediately regardless of how this resolves.
*/
void connectWiFi() {
  checkForWiFiCredentialReset();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-pedal");

  WiFiManager wm;
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SECONDS);

  Serial.println(
    "Starting Wi-Fi. If no network is saved, connect to the "
    "'" + String(WIFI_SETUP_AP_NAME) + "' Wi-Fi network from a "
    "phone or laptop to enter your home Wi-Fi details."
  );

  const bool connected = wm.autoConnect(WIFI_SETUP_AP_NAME);

  if (connected) {
    Serial.print("Wi-Fi connected. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(
      "No Wi-Fi connection (setup portal timed out or was "
      "skipped). BLE pedal functionality will still work. "
      "Reboot to try Wi-Fi setup again."
    );
  }
}

void setupOTA() {
  ArduinoOTA.setHostname("esp32-pedal");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA update started.");
    bleKeyboard.releaseAll();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA update complete.");
  });

  ArduinoOTA.onProgress(
    [](unsigned int progress, unsigned int total) {
      const unsigned int percentage =
        total > 0 ?
        (progress * 100U) / total :
        0;

      Serial.printf(
        "OTA progress: %u%%\r",
        percentage
      );
    }
  );

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error [%u]: ", error);

    if (error == OTA_AUTH_ERROR) {
      Serial.println("Authentication failed.");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin failed.");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connection failed.");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive failed.");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End failed.");
    } else {
      Serial.println("Unknown OTA error.");
    }
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready.");
}

// ============================================================
// PEDAL INPUT
// ============================================================

void updatePedal(int pedalIndex) {
  PedalState& pedal = pedals[pedalIndex];

  const bool currentReading = digitalRead(pedal.pin);

  if (currentReading != pedal.lastReading) {
    pedal.lastDebounceTime = millis();
    pedal.lastReading = currentReading;
  }

  if (
    millis() - pedal.lastDebounceTime <
    DEBOUNCE_MS
  ) {
    return;
  }

  if (currentReading == pedal.stableState) {
    return;
  }

  pedal.stableState = currentReading;

  // INPUT_PULLUP: LOW means switch pressed.
  pedal.pressed = pedal.stableState == LOW;

  Serial.print("Pedal ");
  Serial.print(pedalIndex + 1);
  Serial.print(
    pedal.pressed ?
    " pressed: " :
    " released: "
  );
  Serial.println(configuredKeys[pedalIndex]);

  rebuildHeldKeys();
}

// ============================================================
// SETUP AND LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PEDAL_1_PIN, INPUT_PULLUP);
  pinMode(PEDAL_2_PIN, INPUT_PULLUP);

  /*
    Read the initial switch states so a pedal held during startup
    is represented correctly.
  */
  for (int pedalIndex = 0; pedalIndex < 2; pedalIndex++) {
    pedals[pedalIndex].stableState =
      digitalRead(pedals[pedalIndex].pin);

    pedals[pedalIndex].lastReading =
      pedals[pedalIndex].stableState;

    pedals[pedalIndex].pressed =
      pedals[pedalIndex].stableState == LOW;
  }

  prefs.begin("footpedal", false);
  loadSettings();

  Serial.println("Starting BLE keyboard...");
  bleKeyboard.begin();

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    setupWebServer();
    setupOTA();
  }

  Serial.println("Dual pedal ready.");

  Serial.print("Pedal 1: GPIO");
  Serial.print(PEDAL_1_PIN);
  Serial.print(" -> ");
  Serial.println(configuredKeys[0]);

  Serial.print("Pedal 2: GPIO");
  Serial.print(PEDAL_2_PIN);
  Serial.print(" -> ");
  Serial.println(configuredKeys[1]);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    ArduinoOTA.handle();
  }

  updatePedal(0);
  updatePedal(1);

  delay(1);
}