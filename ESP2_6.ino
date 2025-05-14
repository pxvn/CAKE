/**
 * =============================================================================
 * Project Name : CAKE – The Desktop Companion Robot
 * File         : main.ino
 * Author       : Pavan K.
 * Iteration    : 35
 * Date         : May 13, 2025
 * =============================================================================
 * Description:
 * Firmware for CAKE, a desktop companion robot with expressive OLED eyes,
 * NeoPixel LED effects, and WiFi-enabled controls. Manages boot animations,
 * eye expressions, LED moods, and user commands via a web interface.
 *
 * Main Functionalities:
 * - OLED Display: Boot messages, eye animations, and text (date, time, notes).
 * - RoboEyes: Expressive eye states (happy, sleepy, angry, normal) with flicker.
 * - NeoPixel LEDs: Boot chaser, mood-based pulses (angry, calm, blue, white).
 * - WiFi & Web Server: Handles commands for date/time, sticky notes, reminders.
 * - State Machine: Smooth transitions from boot to interactive modes.
 *
 * Dependencies:
 * - Adafruit_SSD1306
 * - Adafruit_GFX
 * - FluxGarage_RoboEyes
 * - Adafruit_NeoPixel
 * - WiFi (ESP32)
 * - WebServer (ESP32)
 *
 * Hardware:
 * - ESP32 MCU
 * - 2.4" OLED Display (SSD1306)
 * - NeoPixel RGB LEDs (x4)
 *
 * Notes:
 * - Optimized for performance and smooth animations.
 * - Web interface accessible at ESP32’s IP address.
 * - Error handling for WiFi and command parsing.
 * =============================================================================
 */

#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

// WiFi credentials
#define CAKE_SSID      "CAKE_ROBOT"
#define CAKE_PASSWORD  "12121212"
#define RECONNECT_INTERVAL 10000
#define WIFI_MAX_ATTEMPTS 3

// OLED configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI 23
#define OLED_CLK 18
#define OLED_DC 2
#define OLED_CS 5
#define OLED_RESET 4
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

#include <FluxGarage_RoboEyes.h>

// RoboEyes instance
roboEyes eyes;

// NeoPixel configuration
#define LED_PIN 27
#define LED_COUNT 4
Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// State machine enums
enum AnimationState {
  STATE_BOOT_LEDS,
  STATE_BOOT_MSG1,
  STATE_BOOT_MSG2,
  STATE_WAIT_CONNECTION,
  STATE_EYES_CLOSED,
  STATE_EYES_OPEN,
  STATE_BLINK_PHASE,
  STATE_ANGRY_MODE,
  STATE_SHOW_DATE,
  STATE_SHOW_TIME,
  STATE_SHOW_NOTE,
  STATE_NOTE_EXIT,
  STATE_SHOW_REMINDER,
  STATE_LOW_BATTERY,
  STATE_DISCONNECTED,
  STATE_ESP_DISCONNECTED,
  STATE_ERROR
};

enum LedMode {
  LED_ANGRY,
  LED_CALM,
  LED_PULSE_BLUE,
  LED_PULSE_WHITE
};

enum EyeMode {
  EYE_ANGRY,
  EYE_HAPPY,
  EYE_SLEEPY,
  EYE_NORMAL
};

// Global state variables
AnimationState currentState = STATE_BOOT_LEDS;
AnimationState previousState = STATE_ANGRY_MODE;
LedMode currentLedMode = LED_ANGRY;
EyeMode currentEyeMode = EYE_ANGRY;
bool hFlicker = false;
bool vFlicker = true; // Default for angry mode
unsigned long stateTimer = 0;
String stickyNote = "";
unsigned long noteStartTime = 0;
unsigned long noteDuration = 0;
String displayDate = "25 May";
String displayTime = "1:05 PM";
unsigned long reminderInterval = 0;
unsigned long lastReminderTime = 0;
bool wifiConnected = false;
unsigned long lastReconnectAttempt = 0;
bool esp1Connected = false;
unsigned long lastDateTimeDisplay = 0;
uint8_t brightness = 100;
unsigned long bootStartTime = 0;
uint8_t ledSequence = 0;
bool ledsAllOn = false;

// Web server
WebServer server(80);

/**
 * Updates eye animations based on the current eye mode.
 * Handles display buffer clearing and rendering to work around FluxGarage_RoboEyes limitations.
 */
void updateEyes() {
  if (currentState < STATE_EYES_CLOSED) return;

  display.clearDisplay();
  
  switch (currentEyeMode) {
    case EYE_HAPPY:
      eyes.setMood(HAPPY);
      eyes.setVFlicker(OFF);
      eyes.setHFlicker(OFF);
      break;
    case EYE_SLEEPY:
      eyes.setMood(TIRED);
      eyes.setVFlicker(OFF);
      eyes.setHFlicker(OFF);
      break;
    case EYE_NORMAL:
      eyes.setMood(DEFAULT);
      eyes.setVFlicker(OFF);
      eyes.setHFlicker(OFF);
      break;
    default:
      eyes.setMood(ANGRY);
      eyes.setVFlicker(vFlicker ? ON : OFF, 3);
      eyes.setHFlicker(hFlicker ? ON : OFF, 3);
  }
  eyes.update();
  eyes.drawEyes();
  display.display();
}

/**
 * Handles incoming web server commands for controlling CAKE.
 */
void handleCommand() {
  if (!server.hasArg("do")) {
    Serial.println("[CAKE] Error: Missing 'do' parameter");
    displayError("Invalid Command");
    server.send(400, "text/plain", "Missing 'do' parameter");
    return;
  }

  String cmd = server.arg("do");
  Serial.println("[CAKE] Received command: " + cmd);

  if (cmd.startsWith("datetime:")) {
    String data = cmd.substring(9);
    int commaIndex = data.indexOf(',');
    if (commaIndex != -1) {
      String datePart = data.substring(0, commaIndex);
      String timePart = data.substring(commaIndex + 1);
      if (datePart.length() >= 5 && timePart.length() >= 5) {
        int day = datePart.substring(0, 2).toInt();
        int month = datePart.substring(3, 5).toInt();
        String monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        if (month >= 1 && month <= 12) {
          displayDate = String(day) + " " + monthNames[month - 1];
        }
        int hour = timePart.substring(0, 2).toInt();
        String period = hour >= 12 ? "PM" : "AM";
        hour = hour % 12;
        hour = hour ? hour : 12;
        displayTime = String(hour) + ":" + timePart.substring(3, 5) + " " + period;
        Serial.println("[CAKE] DateTime set: " + displayDate + ", " + displayTime);
      } else {
        Serial.println("[CAKE] Error: Invalid DateTime format");
        displayError("Invalid DateTime");
      }
    }
    esp1Connected = true;
    currentState = STATE_EYES_CLOSED;
    stateTimer = millis();
  } else if (cmd.startsWith("sticky:")) {
    String data = cmd.substring(7);
    int pipeIndex = data.indexOf('|');
    if (pipeIndex != -1) {
      stickyNote = data.substring(0, pipeIndex);
      noteDuration = data.substring(pipeIndex + 1).toInt();
      noteStartTime = millis();
      currentState = STATE_SHOW_NOTE;
      currentEyeMode = EYE_HAPPY;
      currentLedMode = LED_CALM;
      Serial.println("[CAKE] Sticky note set: " + stickyNote + ", duration: " + String(noteDuration) + "ms");
    } else {
      Serial.println("[CAKE] Error: Invalid sticky note format");
      displayError("Invalid Note");
    }
  } else if (cmd.startsWith("reminder:")) {
    String data = cmd.substring(9);
    int pipeIndex = data.indexOf('|');
    if (pipeIndex != -1) {
      String interval = data.substring(0, pipeIndex);
      String unit = data.substring(pipeIndex + 1);
      reminderInterval = unit == "hours" ? interval.toInt() * 3600000 : interval.toInt() * 60000;
      lastReminderTime = millis();
      Serial.println("[CAKE] Reminder set: " + interval + " " + unit);
    } else {
      Serial.println("[CAKE] Error: Invalid reminder format");
      displayError("Invalid Reminder");
    }
  } else if (cmd == "cancel_reminder") {
    reminderInterval = 0;
    lastReminderTime = 0;
    Serial.println("[CAKE] Reminder cancelled");
  } else if (cmd.startsWith("eyemode:")) {
    String mode = cmd.substring(8);
    if (mode == "happy") currentEyeMode = EYE_HAPPY;
    else if (mode == "sleepy") currentEyeMode = EYE_SLEEPY;
    else if (mode == "normal") currentEyeMode = EYE_NORMAL;
    else currentEyeMode = EYE_ANGRY;
    Serial.println("[CAKE] Eye mode set: " + mode);
    if (currentState == STATE_ANGRY_MODE) updateEyes();
  } else if (cmd.startsWith("ledmode:")) {
    String mode = cmd.substring(8);
    if (mode == "angry") currentLedMode = LED_ANGRY;
    else if (mode == "calm") currentLedMode = LED_CALM;
    else if (mode == "pulse_blue") currentLedMode = LED_PULSE_BLUE;
    else if (mode == "pulse_white") currentLedMode = LED_PULSE_WHITE;
    Serial.println("[CAKE] LED mode set: " + mode);
  } else if (cmd.startsWith("brightness:")) {
    brightness = cmd.substring(11).toInt();
    leds.setBrightness(brightness);
    Serial.println("[CAKE] Brightness set: " + String(brightness));
  } else if (cmd.startsWith("flicker:")) {
    String type = cmd.substring(8);
    if (type == "h") {
      hFlicker = !hFlicker;
      Serial.println("[CAKE] H-Flicker " + String(hFlicker ? "enabled" : "disabled"));
    } else if (type == "v") {
      vFlicker = !vFlicker;
      Serial.println("[CAKE] V-Flicker " + String(vFlicker ? "enabled" : "disabled"));
    }
    if (currentState == STATE_ANGRY_MODE) updateEyes();
  } else if (cmd == "lowbattery") {
    currentState = STATE_LOW_BATTERY;
    currentEyeMode = EYE_SLEEPY;
    currentLedMode = LED_ANGRY;
    stateTimer = millis();
    Serial.println("[CAKE] Low battery warning received");
  } else {
    Serial.println("[CAKE] Error: Unknown command: " + cmd);
    displayError("Unknown Command");
  }

  server.send(200, "text/plain", "OK");
}

/**
 * Attempts to connect to WiFi with retry logic.
 * @return true if connected, false otherwise.
 */
bool connectWiFi() {
  WiFi.begin(CAKE_SSID, CAKE_PASSWORD);
  Serial.print("[CAKE] Connecting to WiFi");
  unsigned long start = millis();
  int attempts = 0;
  while (attempts < WIFI_MAX_ATTEMPTS && WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= 10000) {
      Serial.println("\n[CAKE] WiFi attempt " + String(attempts + 1) + " failed");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(CAKE_SSID, CAKE_PASSWORD);
      start = millis();
      attempts++;
    }
    delay(100);
    Serial.print(".");
  }
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if (wifiConnected) {
    Serial.println("\n[CAKE] WiFi connected, IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[CAKE] WiFi connection failed after " + String(attempts) + " attempts");
  }
  return wifiConnected;
}

/**
 * Monitors and maintains WiFi connection.
 */
void checkConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      Serial.println("[CAKE] Reconnecting to WiFi...");
      connectWiFi();
      lastReconnectAttempt = millis();
    }
  } else if (!wifiConnected) {
    wifiConnected = true;
    Serial.println("[CAKE] WiFi reconnected, IP: " + WiFi.localIP().toString());
  }
}

/**
 * Displays an error message on the OLED.
 * @param message The error message to display.
 */
void displayError(String message) {
  currentState = STATE_ERROR;
  stickyNote = message;
  stateTimer = millis();
}

/**
 * Initializes hardware and network.
 */
void setup() {
  Serial.begin(115200);
  Serial.println("[CAKE] Starting setup");

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("[CAKE] Error: SSD1306 allocation failed");
    for(;;);
  }
  display.dim(true);
  display.setTextWrap(false);
  display.clearDisplay();
  display.display();
  Serial.println("[CAKE] OLED initialized");

  // Initialize NeoPixels
  leds.begin();
  leds.setBrightness(brightness);
  leds.clear();
  leds.show();
  Serial.println("[CAKE] NeoPixels initialized");

  // Initialize RoboEyes
  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60);
  eyes.setHFlicker(OFF);
  eyes.setVFlicker(OFF);
  eyes.close();
  Serial.println("[CAKE] RoboEyes initialized");

  // Initialize WiFi and web server
  connectWiFi();
  server.on("/cmd", handleCommand);
  server.begin();
  Serial.println("[CAKE] Web server started");

  stateTimer = millis();
  bootStartTime = millis();
  lastDateTimeDisplay = millis();
}

/**
 * Main loop for state machine and updates.
 */
void loop() {
  server.handleClient();
  checkConnection();
  unsigned long currentMillis = millis();

  // Handle connection states
  if (!wifiConnected && currentState != STATE_BOOT_LEDS && currentState != STATE_BOOT_MSG1 && currentState != STATE_BOOT_MSG2) {
    currentState = STATE_DISCONNECTED;
    stateTimer = currentMillis;
  } else if (!esp1Connected && currentState != STATE_BOOT_LEDS && currentState != STATE_BOOT_MSG1 && currentState != STATE_BOOT_MSG2 && currentState != STATE_WAIT_CONNECTION) {
    currentState = STATE_ESP_DISCONNECTED;
    stateTimer = currentMillis;
  }

  // Timeout for waiting ESP1 connection
  if (currentState == STATE_WAIT_CONNECTION && currentMillis - bootStartTime > 10000) {
    currentState = STATE_EYES_CLOSED;
    stateTimer = currentMillis;
    Serial.println("[CAKE] Timeout waiting for ESP1, proceeding to eyes closed");
  }

  // Handle sticky note expiration
  if (stickyNote.length() > 0 && (currentMillis - noteStartTime > noteDuration)) {
    currentState = STATE_NOTE_EXIT;
    stateTimer = currentMillis;
  }

  // Handle reminders
  if (reminderInterval > 0 && currentMillis - lastReminderTime > reminderInterval) {
    currentState = STATE_SHOW_REMINDER;
    lastReminderTime = currentMillis;
    stateTimer = currentMillis;
  }

  // Periodic date/time display in angry mode
  if (currentState == STATE_ANGRY_MODE && currentMillis - lastDateTimeDisplay > 10000) {
    previousState = currentState;
    currentState = STATE_SHOW_DATE;
    stateTimer = currentMillis;
    lastDateTimeDisplay = currentMillis;
  }

  // State machine
  switch (currentState) {
    case STATE_BOOT_LEDS:
      if (ledSequence >= LED_COUNT && ledsAllOn) {
        if (currentMillis - stateTimer > 1000) {
          currentState = STATE_BOOT_MSG1;
          stateTimer = currentMillis;
          Serial.println("[CAKE] Boot LEDs complete, showing first message");
        }
      }
      break;

    case STATE_BOOT_MSG1:
      if (currentMillis - stateTimer > 1000) {
        currentState = STATE_BOOT_MSG2;
        stateTimer = currentMillis;
        Serial.println("[CAKE] First message complete, showing CAKE");
      }
      break;

    case STATE_BOOT_MSG2:
      if (currentMillis - stateTimer > 2000) {
        currentState = STATE_WAIT_CONNECTION;
        stateTimer = currentMillis;
        Serial.println("[CAKE] Boot message complete, waiting for ESP1");
      }
      break;

    case STATE_WAIT_CONNECTION:
      if (esp1Connected) {
        flashLEDs(0, 255, 0, 2);
        currentState = STATE_EYES_CLOSED;
        stateTimer = currentMillis;
        Serial.println("[CAKE] ESP1 connected, proceeding to eyes closed");
      }
      break;

    case STATE_EYES_CLOSED:
      if (currentMillis - stateTimer > 500) {
        currentState = STATE_EYES_OPEN;
        stateTimer = currentMillis;
        eyes.open();
        Serial.println("[CAKE] Eyes opened");
      }
      break;

    case STATE_EYES_OPEN:
      if (currentMillis - stateTimer > 1000) {
        currentState = STATE_BLINK_PHASE;
        stateTimer = currentMillis;
        eyes.setAutoblinker(true, 2500, 500);
        Serial.println("[CAKE] Started blinking phase");
      }
      break;

    case STATE_BLINK_PHASE:
      if (currentMillis - stateTimer > 5000) {
        currentState = STATE_ANGRY_MODE;
        stateTimer = currentMillis;
        eyes.setAutoblinker(false);
        currentEyeMode = EYE_ANGRY;
        currentLedMode = LED_ANGRY;
        updateEyes();
        Serial.println("[CAKE] Switched to angry mode");
      }
      break;

    case STATE_ANGRY_MODE:
      break;

    case STATE_SHOW_DATE:
      if (currentMillis - stateTimer > 2000) {
        currentState = STATE_SHOW_TIME;
        stateTimer = currentMillis;
        Serial.println("[CAKE] Showing time");
      }
      break;

    case STATE_SHOW_TIME:
      if (currentMillis - stateTimer > 2000) {
        currentState = previousState;
        stateTimer = currentMillis;
        Serial.println("[CAKE] Resumed previous state: " + String(previousState));
      }
      break;

    case STATE_SHOW_NOTE:
      break;

    case STATE_NOTE_EXIT:
      if (currentMillis - stateTimer > 1000) {
        stickyNote = "";
        noteDuration = 0;
        currentState = STATE_ANGRY_MODE;
        currentEyeMode = EYE_ANGRY;
        currentLedMode = LED_ANGRY;
        stateTimer = currentMillis;
        Serial.println("[CAKE] Sticky note expired, returned to angry mode");
      }
      break;

    case STATE_SHOW_REMINDER:
      if (currentMillis - stateTimer > 3000) {
        currentState = STATE_ANGRY_MODE;
        currentEyeMode = EYE_ANGRY;
        currentLedMode = LED_ANGRY;
        stateTimer = currentMillis;
        Serial.println("[CAKE] Reminder expired, returned to angry mode");
      }
      break;

    case STATE_LOW_BATTERY:
      if (currentMillis - stateTimer > 2000) {
        currentState = STATE_ANGRY_MODE;
        currentEyeMode = EYE_ANGRY;
        currentLedMode = LED_ANGRY;
        stateTimer = currentMillis;
        Serial.println("[CAKE] Low battery warning complete");
      }
      break;

    case STATE_DISCONNECTED:
      if (wifiConnected) {
        currentState = STATE_ANGRY_MODE;
        stateTimer = currentMillis;
        Serial.println("[CAKE] WiFi reconnected, returned to angry mode");
      } else if (currentMillis - stateTimer > 2000) {
        currentState = STATE_ANGRY_MODE;
        stateTimer = currentMillis;
        Serial.println("[CAKE] WiFi disconnect warning complete");
      }
      break;

    case STATE_ESP_DISCONNECTED:
      if (esp1Connected) {
        currentState = STATE_ANGRY_MODE;
        stateTimer = currentMillis;
        Serial.println("[CAKE] ESP1 reconnected, returned to angry mode");
      } else if (currentMillis - stateTimer > 2000) {
        currentState = STATE_ANGRY_MODE;
        stateTimer = currentMillis;
        Serial.println("[CAKE] ESP1 disconnect warning complete");
      }
      break;

    case STATE_ERROR:
      if (currentMillis - stateTimer > 2000) {
        currentState = STATE_ANGRY_MODE;
        stickyNote = "";
        stateTimer = currentMillis;
        Serial.println("[CAKE] Error message complete, returned to angry mode");
      }
      break;
  }

  handleDisplay();
  updateLEDs();
}

/**
 * Manages OLED display updates based on the current state.
 */
void handleDisplay() {
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 50) return;

  display.clearDisplay();

  switch (currentState) {
    case STATE_BOOT_MSG1:
      drawCenteredText("Hi I am,", 2, -10);
      break;

    case STATE_BOOT_MSG2: {
      drawCenteredText("CAKE", 3, 10);
      break;
    }

    case STATE_WAIT_CONNECTION:
      drawCenteredText("Waiting ESP1", 2, 0);
      break;

    case STATE_EYES_CLOSED:
    case STATE_EYES_OPEN:
    case STATE_BLINK_PHASE:
    case STATE_ANGRY_MODE:
      updateEyes();
      break;

    case STATE_SHOW_DATE:
      currentLedMode = LED_CALM;
      drawCenteredText(displayDate.c_str(), displayDate.length() > 10 ? 1 : 2, 0);
      break;

    case STATE_SHOW_TIME:
      currentLedMode = LED_CALM;
      drawCenteredText(displayTime.c_str(), displayTime.length() > 10 ? 1 : 2, 0);
      break;

    case STATE_SHOW_NOTE: {
      int textSize = stickyNote.length() > 10 ? 1 : (stickyNote.length() > 5 ? 2 : 3);
      drawCenteredText(stickyNote.c_str(), textSize, 0);
      break;
    }

    case STATE_NOTE_EXIT:
      if ((millis() / 500) % 2) {
        int textSize = stickyNote.length() > 10 ? 1 : (stickyNote.length() > 5 ? 2 : 3);
        drawCenteredText(stickyNote.c_str(), textSize, 0);
      }
      break;

    case STATE_SHOW_REMINDER:
      if ((millis() / 500) % 2) {
        drawCenteredText("DRINK WATER!", 2, 0);
      }
      break;

    case STATE_LOW_BATTERY:
      drawCenteredText("Low Battery!", 2, 0);
      break;

    case STATE_DISCONNECTED:
      drawCenteredText("No WiFi", 2, 0);
      break;

    case STATE_ESP_DISCONNECTED:
      drawCenteredText("ESP1 Disconnected", 1, 0);
      break;

    case STATE_ERROR:
      drawCenteredText(stickyNote.c_str(), stickyNote.length() > 10 ? 1 : 2, 0);
      break;
  }

  if (currentState != STATE_EYES_CLOSED && currentState != STATE_EYES_OPEN && 
      currentState != STATE_BLINK_PHASE && currentState != STATE_ANGRY_MODE) {
    display.display();
  }

  lastDraw = millis();
}

/**
 * Draws centered text on the OLED.
 * @param text The text to display.
 * @param textSize Font size (1, 2, or 3).
 * @param yOffset Vertical offset from center.
 */
void drawCenteredText(const char* text, uint8_t textSize, int8_t yOffset) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);

  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x, &y, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2 + yOffset);
  display.print(text);
}

/**
 * Updates NeoPixel LEDs based on the current state and mode.
 */
void updateLEDs() {
  static unsigned long lastLedChange = 0;

  if (currentState == STATE_BOOT_LEDS) {
    if (millis() - lastLedChange > 200 && ledSequence < LED_COUNT) {
      leds.setPixelColor(ledSequence, leds.Color(0, 0, 150 * (brightness / 255.0)));
      leds.show();
      ledSequence++;
      lastLedChange = millis();
    }
    if (ledSequence >= LED_COUNT && !ledsAllOn) {
      delay(500);
      ledsAllOn = true;
      flashLEDs(0, 150, 0, 3);
      flashLEDs(150, 150, 150, 3);
    }
    return;
  }

  if (currentState == STATE_BOOT_MSG1 || currentState == STATE_BOOT_MSG2 || currentState == STATE_WAIT_CONNECTION) {
    static uint8_t chaserIndex = 0;
    if (millis() - lastLedChange > 100) {
      leds.clear();
      leds.setPixelColor(chaserIndex % LED_COUNT, leds.Color(0, 0, 100 * (brightness / 255.0)));
      leds.show();
      chaserIndex++;
      lastLedChange = millis();
    }
    return;
  }

  switch (currentLedMode) {
    case LED_ANGRY:
      if (millis() - lastLedChange > 150) {
        float intensity = 0.5 + 0.5 * sin(millis() / 150.0);
        leds.fill(leds.Color(200 * intensity * (brightness / 255.0), 0, 0));
        lastLedChange = millis();
      }
      break;

    case LED_CALM:
      leds.fill(leds.Color(0, 0, 100 * (brightness / 255.0)));
      break;

    case LED_PULSE_BLUE:
      if (millis() - lastLedChange > 50) {
        float intensity = 0.5 + 0.5 * sin(millis() / 500.0);
        leds.fill(leds.Color(0, 0, 200 * intensity * (brightness / 255.0)));
        lastLedChange = millis();
      }
      break;

    case LED_PULSE_WHITE:
      if (millis() - lastLedChange > 50) {
        float intensity = 0.5 + 0.5 * sin(millis() / 500.0);
        leds.fill(leds.Color(200 * intensity * (brightness / 255.0), 200 * intensity * (brightness / 255.0), 200 * intensity * (brightness / 255.0)));
        lastLedChange = millis();
      }
      break;
  }
  leds.show();
}

/**
 * Flashes LEDs with specified color and number of times.
 * @param r Red component (0-255).
 * @param g Green component (0-255).
 * @param b Blue component (0-255).
 * @param times Number of flashes.
 */
void flashLEDs(uint8_t r, uint8_t g, uint8_t b, uint8_t times) {
  for (int i = 0; i < times; i++) {
    leds.fill(leds.Color(r * (brightness / 255.0), g * (brightness / 255.0), b * (brightness / 255.0)));
    leds.show();
    delay(100);
    leds.clear();
    leds.show();
    delay(100);
  }
  Serial.println("[CAKE] Flashed LEDs: R=" + String(r) + ", G=" + String(g) + ", B=" + String(b));
}
