/*
 * ═══════════════════════════════════════════════════════════════════
 *  AUDIO CONTROLLER - With Sound Effects
 *  ESP32 + PAM8403 + AXC1000
 *
 *  Features:
 *  - Manual ON/OFF via MQTT (Website/App)
 *  - Timer Schedule (Auto ON/OFF at set times)
 *  - Multiple Sound Effects (Siren, Alarm, Thunder, etc.)
 * ═══════════════════════════════════════════════════════════════════
 */

#include <PubSubClient.h>
#include <WiFi.h>
#include <time.h>

// ==================== CONFIGURATION ====================
const char *ssid = "muhaimin";
const char *password = "hehe2233";

// MQTT Broker settings
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_topic_control = "audio/control";
const char *mqtt_topic_status = "audio/status";
const char *mqtt_topic_schedule = "audio/schedule";
const char *mqtt_topic_sound = "audio/sound"; // New: select sound effect
const char *mqtt_client_id = "ESP32_Audio";

// Pin definitions
#define AUDIO_PIN 14
#define LED_PIN 2

// PWM Configuration
const int PWM_CHANNEL = 0;
const int PWM_RESOLUTION = 8;

// Sound Effect Types
enum SoundType {
  SOUND_NONE = 0,
  SOUND_SIREN = 1,
  SOUND_ALARM = 2,
  SOUND_THUNDER = 3,
  SOUND_BEEP = 4,
  SOUND_SWEEP = 5
};

// Timer Schedules (up to 5 schedules)
#define MAX_SCHEDULES 5
int scheduleCount = 0;
int scheduleOnHour[MAX_SCHEDULES] = {0};
int scheduleOnMinute[MAX_SCHEDULES] = {0};
int scheduleDuration[MAX_SCHEDULES] = {0}; // Duration in minutes
bool scheduleEnabled[MAX_SCHEDULES] = {false};

// NTP Time settings
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

// Variables
bool audioOn = false;
bool manualOverride = false;
SoundType currentSound = SOUND_SIREN;
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_INTERVAL = 30000;

WiFiClient espClient;
PubSubClient client(espClient);

// ==================== SOUND EFFECTS ====================

// Siren - Police/Ambulance style (rising and falling)
void playSiren() {
  // Rising
  for (int freq = 400; freq <= 1200; freq += 20) {
    ledcSetup(PWM_CHANNEL, freq, PWM_RESOLUTION);
    ledcWrite(PWM_CHANNEL, 128);
    delay(10);
  }
  // Falling
  for (int freq = 1200; freq >= 400; freq -= 20) {
    ledcSetup(PWM_CHANNEL, freq, PWM_RESOLUTION);
    ledcWrite(PWM_CHANNEL, 128);
    delay(10);
  }
}

// Alarm - Rapid beeping
void playAlarm() {
  for (int i = 0; i < 5; i++) {
    ledcSetup(PWM_CHANNEL, 2000, PWM_RESOLUTION);
    ledcWrite(PWM_CHANNEL, 128);
    delay(100);
    ledcWrite(PWM_CHANNEL, 0);
    delay(100);
  }
}

// Thunder - Random frequency bursts
void playThunder() {
  for (int burst = 0; burst < 3; burst++) {
    int burstLength = random(50, 200);
    for (int i = 0; i < burstLength; i++) {
      int freq = random(50, 500);
      ledcSetup(PWM_CHANNEL, freq, PWM_RESOLUTION);
      ledcWrite(PWM_CHANNEL, random(64, 255));
      delay(10);
    }
    ledcWrite(PWM_CHANNEL, 0);
    delay(random(100, 500));
  }
}

// Beep - Simple beep pattern
void playBeep() {
  ledcSetup(PWM_CHANNEL, 1000, PWM_RESOLUTION);
  ledcWrite(PWM_CHANNEL, 128);
  delay(200);
  ledcWrite(PWM_CHANNEL, 0);
  delay(200);
  ledcWrite(PWM_CHANNEL, 128);
  delay(200);
  ledcWrite(PWM_CHANNEL, 0);
  delay(500);
}

// Sweep - Frequency sweep up and down
void playSweep() {
  // Sweep up
  for (int freq = 200; freq <= 5000; freq += 100) {
    ledcSetup(PWM_CHANNEL, freq, PWM_RESOLUTION);
    ledcWrite(PWM_CHANNEL, 128);
    delay(5);
  }
  // Sweep down
  for (int freq = 5000; freq >= 200; freq -= 100) {
    ledcSetup(PWM_CHANNEL, freq, PWM_RESOLUTION);
    ledcWrite(PWM_CHANNEL, 128);
    delay(5);
  }
  ledcWrite(PWM_CHANNEL, 0);
  delay(200);
}

// Play current sound effect
void playCurrentSound() {
  switch (currentSound) {
  case SOUND_SIREN:
    playSiren();
    break;
  case SOUND_ALARM:
    playAlarm();
    break;
  case SOUND_THUNDER:
    playThunder();
    break;
  case SOUND_BEEP:
    playBeep();
    break;
  case SOUND_SWEEP:
    playSweep();
    break;
  default:
    break;
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║  AUDIO CONTROLLER - Sound Effects         ║");
  Serial.println("║  ESP32 + PAM8403 + AXC1000                ║");
  Serial.println("╚═══════════════════════════════════════════╝\n");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  ledcSetup(PWM_CHANNEL, 1000, PWM_RESOLUTION);
  ledcAttachPin(AUDIO_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);

  Serial.println("✓ PWM Audio initialized");
  Serial.println("✓ Sound effects: Siren, Alarm, Thunder, Beep, Sweep");

  setupWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  Serial.println("\n✓ System ready!\n");
  blinkLED(3, 200);
}

// ==================== WIFI SETUP ====================
void setupWiFi() {
  Serial.print("📶 Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected!");
    Serial.print("  IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n✗ WiFi failed!");
  }
}

// ==================== MQTT ====================
void reconnectMQTT() {
  int retries = 0;
  while (!client.connected() && retries < 3) {
    Serial.print("🔌 MQTT connecting... ");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected!");
      client.subscribe(mqtt_topic_control);
      client.subscribe(mqtt_topic_schedule);
      client.subscribe(mqtt_topic_sound);
      sendStatus("online");
    } else {
      Serial.println("failed");
      retries++;
      delay(3000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.printf("📩 [%s]: %s\n", topic, message.c_str());

  // Control commands
  if (String(topic) == mqtt_topic_control) {
    if (message == "ON" || message == "on") {
      manualOverride = true;
      audioOn = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("🔊 Audio ON");
    } else if (message == "OFF" || message == "off") {
      manualOverride = true;
      audioOn = false;
      ledcWrite(PWM_CHANNEL, 0);
      digitalWrite(LED_PIN, LOW);
      Serial.println("🔇 Audio OFF");
    } else if (message == "AUTO" || message == "auto") {
      manualOverride = false;
      Serial.println("🔄 AUTO mode");
    }
  }

  // Sound selection
  if (String(topic) == mqtt_topic_sound) {
    if (message == "siren" || message == "1") {
      currentSound = SOUND_SIREN;
      Serial.println("🎵 Sound: Siren");
    } else if (message == "alarm" || message == "2") {
      currentSound = SOUND_ALARM;
      Serial.println("🎵 Sound: Alarm");
    } else if (message == "thunder" || message == "3") {
      currentSound = SOUND_THUNDER;
      Serial.println("🎵 Sound: Thunder");
    } else if (message == "beep" || message == "4") {
      currentSound = SOUND_BEEP;
      Serial.println("🎵 Sound: Beep");
    } else if (message == "sweep" || message == "5") {
      currentSound = SOUND_SWEEP;
      Serial.println("🎵 Sound: Sweep");
    }
  }

  // Schedule
  if (String(topic) == mqtt_topic_schedule) {
    parseSchedule(message);
  }
}

void parseSchedule(String schedule) {
  // Format: "HH:MM,D;HH:MM,D;HH:MM,D" for multiple schedules
  // Example: "08:00,2;12:00,2;20:00,2" = 3 schedules

  // Clear existing schedules
  scheduleCount = 0;
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    scheduleEnabled[i] = false;
  }

  int startIdx = 0;
  int schedIdx = 0;

  while (startIdx < schedule.length() && schedIdx < MAX_SCHEDULES) {
    int endIdx = schedule.indexOf(';', startIdx);
    if (endIdx == -1)
      endIdx = schedule.length();

    String single = schedule.substring(startIdx, endIdx);
    single.trim();

    if (single.length() > 0) {
      int commaPos = single.indexOf(',');
      if (commaPos > 0) {
        String onTime = single.substring(0, commaPos);
        String duration = single.substring(commaPos + 1);
        int colonPos = onTime.indexOf(':');
        if (colonPos > 0) {
          scheduleOnHour[schedIdx] = onTime.substring(0, colonPos).toInt();
          scheduleOnMinute[schedIdx] = onTime.substring(colonPos + 1).toInt();
          int dur = duration.toInt();
          if (dur < 1)
            dur = 1;
          if (dur > 60)
            dur = 60;
          scheduleDuration[schedIdx] = dur;
          scheduleEnabled[schedIdx] = true;

          Serial.printf("📅 Schedule %d: ON at %02d:%02d for %d min\n",
                        schedIdx + 1, scheduleOnHour[schedIdx],
                        scheduleOnMinute[schedIdx], scheduleDuration[schedIdx]);
          schedIdx++;
        }
      }
    }
    startIdx = endIdx + 1;
  }

  scheduleCount = schedIdx;
  Serial.printf("📅 Total schedules: %d\n", scheduleCount);
}

void sendStatus(String status) {
  if (!client.connected())
    return;

  String soundName;
  switch (currentSound) {
  case SOUND_SIREN:
    soundName = "siren";
    break;
  case SOUND_ALARM:
    soundName = "alarm";
    break;
  case SOUND_THUNDER:
    soundName = "thunder";
    break;
  case SOUND_BEEP:
    soundName = "beep";
    break;
  case SOUND_SWEEP:
    soundName = "sweep";
    break;
  default:
    soundName = "none";
    break;
  }

  struct tm timeinfo;
  getLocalTime(&timeinfo);

  String msg = "{\"status\":\"" + status + "\",\"audio\":\"" +
               (audioOn ? "ON" : "OFF") + "\",\"sound\":\"" + soundName +
               "\",\"mode\":\"" + (manualOverride ? "MANUAL" : "AUTO") +
               "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";

  client.publish(mqtt_topic_status, msg.c_str());
}

// ==================== SCHEDULE CHECK ====================
void checkSchedule() {
  if (manualOverride)
    return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    return;

  int now = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  bool shouldBeOn = false;

  // Check all schedules
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    if (!scheduleEnabled[i])
      continue;

    int on = scheduleOnHour[i] * 60 + scheduleOnMinute[i];
    int off = on + scheduleDuration[i];

    // Handle midnight wrap-around
    if (off >= 1440)
      off -= 1440;

    if (on <= off) {
      // Normal case: ON and OFF on same day
      if (now >= on && now < off)
        shouldBeOn = true;
    } else {
      // Wrap-around case: duration crosses midnight
      if (now >= on || now < off)
        shouldBeOn = true;
    }
  }

  if (shouldBeOn && !audioOn) {
    audioOn = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("⏰ Schedule ON");
  } else if (!shouldBeOn && audioOn) {
    audioOn = false;
    ledcWrite(PWM_CHANNEL, 0);
    digitalWrite(LED_PIN, LOW);
    Serial.println("⏰ Schedule OFF");
  }
}

// ==================== UTILITIES ====================
void blinkLED(int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
    digitalWrite(LED_PIN, LOW);
    delay(ms);
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  if (WiFi.status() != WL_CONNECTED)
    setupWiFi();
  if (!client.connected())
    reconnectMQTT();
  client.loop();

  checkSchedule();

  // Play sound effect if audio is ON
  if (audioOn) {
    playCurrentSound();
  }

  // Send status periodically
  if (millis() - lastStatusUpdate > STATUS_INTERVAL) {
    sendStatus("heartbeat");
    lastStatusUpdate = millis();
  }
}
