/*
  ESP32 Audio Player + PWA Remote Control
  ========================================
  Gabungan DFPlayer Mini dengan PWA sedia ada

  Komponen:
  - ESP32
  - DFPlayer Mini
  - PAM8403 Amplifier
  - Super Tweeter AXC1000
  - 3 Push Buttons: ON/OFF, SKIP (Next), BACK (Previous)

  MQTT Topics:
  - audio/control : Terima arahan (ON, OFF, play, pause, next, prev, vol_XX)
  - audio/status  : Hantar status JSON
  - audio/sound   : Pilih track (1, 2, 3, dll)
*/

#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <PubSubClient.h>
#include <WiFi.h>


// ==================================================
// ============ KONFIGURASI - TUKAR INI =============
// ==================================================
const char *WIFI_SSID = "NAMA_WIFI_ANDA";    // << TUKAR INI
const char *WIFI_PASSWORD = "PASSWORD_WIFI"; // << TUKAR INI
const char *MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char *MQTT_USERNAME = ""; // Kosongkan jika tidak perlu
const char *MQTT_PASSWORD = ""; // Kosongkan jika tidak perlu
const char *MQTT_CLIENT_ID = "ESP32_AudioPlayer";

// MQTT Topics (selaras dengan PWA)
const char *MQTT_TOPIC_CONTROL = "audio/control";
const char *MQTT_TOPIC_STATUS = "audio/status";
const char *MQTT_TOPIC_SOUND = "audio/sound";

// === Pin Configuration ===
#define DFPLAYER_RX_PIN 16
#define DFPLAYER_TX_PIN 17
#define BTN_PLAY_PAUSE 4
#define BTN_NEXT 5
#define BTN_PREV 18
#define LED_PIN 2

// === Variables ===
HardwareSerial dfPlayerSerial(2);
DFRobotDFPlayerMini dfPlayer;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool isPlaying = false;
bool audioOn = false;
int currentVolume = 25;
int currentTrack = 1;
String currentMode = "MANUAL";

unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 250;
bool lastPlayPauseState = HIGH;
bool lastNextState = HIGH;
bool lastPrevState = HIGH;

unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 10000;

// === Function Prototypes ===
void setupWiFi();
void setupMQTT();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void reconnectMQTT();
void publishStatus();
void handlePlayPause();
void handleNext();
void handlePrevious();
void setVolume(int vol);
void playTrack(int track);

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n============================================="));
  Serial.println(F("  ESP32 Audio Player + PWA Control"));
  Serial.println(F("============================================="));

  pinMode(BTN_PLAY_PAUSE, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize DFPlayer
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  Serial.println(F("Initializing DFPlayer Mini..."));
  delay(1000);

  if (!dfPlayer.begin(dfPlayerSerial)) {
    Serial.println(F("ERROR: DFPlayer tidak dikesan!"));
    while (true) {
      delay(1000);
    }
  }

  Serial.println(F("DFPlayer OK!"));
  dfPlayer.setTimeOut(500);
  dfPlayer.volume(currentVolume);
  dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
  dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  delay(500);
  int fileCount = dfPlayer.readFileCounts();
  Serial.print(F("MP3 files: "));
  Serial.println(fileCount);

  // Setup WiFi & MQTT
  setupWiFi();
  setupMQTT();

  Serial.println(F("\nSistem sedia!"));
  Serial.println(F("MQTT Topics:"));
  Serial.println(F("  Control: audio/control"));
  Serial.println(F("  Status: audio/status"));
  Serial.println(F("  Sound: audio/sound"));
}

void loop() {
  // MQTT handling
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnectMQTT();
    }
  } else {
    mqttClient.loop();
  }

  // Button handling
  bool playPauseState = digitalRead(BTN_PLAY_PAUSE);
  bool nextState = digitalRead(BTN_NEXT);
  bool prevState = digitalRead(BTN_PREV);
  unsigned long currentTime = millis();

  if (playPauseState == LOW && lastPlayPauseState == HIGH) {
    if (currentTime - lastButtonPress > debounceDelay) {
      handlePlayPause();
      lastButtonPress = currentTime;
    }
  }

  if (nextState == LOW && lastNextState == HIGH) {
    if (currentTime - lastButtonPress > debounceDelay) {
      handleNext();
      lastButtonPress = currentTime;
    }
  }

  if (prevState == LOW && lastPrevState == HIGH) {
    if (currentTime - lastButtonPress > debounceDelay) {
      handlePrevious();
      lastButtonPress = currentTime;
    }
  }

  lastPlayPauseState = playPauseState;
  lastNextState = nextState;
  lastPrevState = prevState;

  // Periodic status update
  if (millis() - lastStatusUpdate > statusInterval) {
    publishStatus();
    lastStatusUpdate = millis();
  }

  // DFPlayer messages
  if (dfPlayer.available()) {
    uint8_t type = dfPlayer.readType();
    if (type == DFPlayerPlayFinished) {
      Serial.println(F("Track selesai, main seterusnya..."));
      dfPlayer.next();
    }
  }

  delay(10);
}

void setupWiFi() {
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\nWiFi OK!"));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\nWiFi GAGAL! Mod offline."));
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  reconnectMQTT();
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED)
    return;

  Serial.print(F("Connecting to MQTT..."));

  bool connected = false;
  if (strlen(MQTT_USERNAME) > 0 && strlen(MQTT_PASSWORD) > 0) {
    connected =
        mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
  } else {
    connected = mqttClient.connect(MQTT_CLIENT_ID);
  }

  if (connected) {
    Serial.println(F(" OK!"));
    mqttClient.subscribe(MQTT_TOPIC_CONTROL);
    mqttClient.subscribe(MQTT_TOPIC_SOUND);
    publishStatus();
  } else {
    Serial.print(F(" GAGAL: "));
    Serial.println(mqttClient.state());
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.print(F("[MQTT] "));
  Serial.print(topic);
  Serial.print(F(" -> "));
  Serial.println(message);

  // Handle audio/control topic
  if (String(topic) == MQTT_TOPIC_CONTROL) {
    message.toUpperCase();

    if (message == "ON" || message == "PLAY") {
      dfPlayer.start();
      isPlaying = true;
      audioOn = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println(F("[ACTION] Audio ON"));
      publishStatus();
    } else if (message == "OFF" || message == "PAUSE" || message == "STOP") {
      dfPlayer.pause();
      isPlaying = false;
      audioOn = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println(F("[ACTION] Audio OFF"));
      publishStatus();
    } else if (message == "NEXT" || message == "SKIP") {
      handleNext();
    } else if (message == "PREV" || message == "PREVIOUS" ||
               message == "BACK") {
      handlePrevious();
    } else if (message == "VOL_UP") {
      if (currentVolume < 30) {
        currentVolume += 2;
        dfPlayer.volume(currentVolume);
        Serial.print(F("Volume: "));
        Serial.println(currentVolume);
        publishStatus();
      }
    } else if (message == "VOL_DOWN") {
      if (currentVolume > 0) {
        currentVolume -= 2;
        dfPlayer.volume(currentVolume);
        Serial.print(F("Volume: "));
        Serial.println(currentVolume);
        publishStatus();
      }
    } else if (message.startsWith("VOL_")) {
      int vol = message.substring(4).toInt();
      setVolume(vol);
    }
  }

  // Handle audio/sound topic (play specific track)
  if (String(topic) == MQTT_TOPIC_SOUND) {
    int track = message.toInt();
    if (track > 0) {
      playTrack(track);
    }
  }
}

void handlePlayPause() {
  if (isPlaying) {
    dfPlayer.pause();
    isPlaying = false;
    audioOn = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println(F("[BTN] PAUSED"));
  } else {
    dfPlayer.start();
    isPlaying = true;
    audioOn = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println(F("[BTN] PLAYING"));
  }
  publishStatus();
}

void handleNext() {
  dfPlayer.next();
  currentTrack++;
  isPlaying = true;
  audioOn = true;
  digitalWrite(LED_PIN, HIGH);
  Serial.println(F("[ACTION] NEXT TRACK"));
  publishStatus();
}

void handlePrevious() {
  dfPlayer.previous();
  if (currentTrack > 1)
    currentTrack--;
  isPlaying = true;
  audioOn = true;
  digitalWrite(LED_PIN, HIGH);
  Serial.println(F("[ACTION] PREV TRACK"));
  publishStatus();
}

void setVolume(int vol) {
  if (vol >= 0 && vol <= 30) {
    currentVolume = vol;
    dfPlayer.volume(currentVolume);
    Serial.print(F("Volume set: "));
    Serial.println(currentVolume);
    publishStatus();
  }
}

void playTrack(int track) {
  dfPlayer.play(track);
  currentTrack = track;
  isPlaying = true;
  audioOn = true;
  digitalWrite(LED_PIN, HIGH);
  Serial.print(F("Playing track: "));
  Serial.println(track);
  publishStatus();
}

void publishStatus() {
  if (!mqttClient.connected())
    return;

  StaticJsonDocument<256> doc;

  doc["audio"] = audioOn ? "ON" : "OFF";
  doc["status"] = audioOn ? "audio_on" : "audio_off";
  doc["mode"] = currentMode;
  doc["volume"] = currentVolume;
  doc["track"] = currentTrack;
  doc["ip"] = WiFi.localIP().toString();

  char buffer[256];
  serializeJson(doc, buffer);

  mqttClient.publish(MQTT_TOPIC_STATUS, buffer);
  Serial.print(F("[STATUS] "));
  Serial.println(buffer);
}

/*
  === ARAHAN ===

  1. Tukar WIFI_SSID dan WIFI_PASSWORD

  2. MQTT Commands dari PWA (topic: audio/control):
     - "ON" atau "PLAY"   : Main audio
     - "OFF" atau "PAUSE" : Stop audio
     - "NEXT"             : Track seterusnya
     - "PREV"             : Track sebelumnya
     - "VOL_UP"           : Volume naik
     - "VOL_DOWN"         : Volume turun
     - "VOL_25"           : Set volume ke 25 (0-30)

  3. Pilih track (topic: audio/sound):
     - "1" : Main track pertama
     - "2" : Main track kedua
     - dll

  4. Status JSON dihantar ke topic: audio/status
     Format:
  {"audio":"ON","status":"audio_on","mode":"MANUAL","volume":25,"track":1,"ip":"192.168.1.x"}
*/
