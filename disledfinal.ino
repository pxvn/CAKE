/**
 * =============================================================================
 * Project Name : CAKE – The Desktop Companion Robot
 * File         : main.ino AND disledfinal.ino
 * Author       : Pavan K.
 * Iteration    : 34
 * Date         : 19th April 2025
 * =============================================================================
 * Description:
 * This firmware controls the core boot-up animations and expressions of CAKE,
 * a compact desktop companion robot inspired by EMO and HP Otto Invent.
 * 
 * Main Functionalities:
 * ---------------------
 * 1. OLED Display Control:
 *    - Uses Adafruit SSD1306 for displaying boot messages and eye animations.
 *    - Displays boot sequence text followed by animated eye states.
 * 
 * 2. FluxGarage RoboEyes Integration:
 *    - Animates expressive eye states: blink, open, close, angry, etc.
 *    - Controls mood-based flickering and autoblinking behavior.
 * 
 * 3. NeoPixel LED Effects:
 *    - Under-glow RGB effects based on boot phase and mood.
 *    - Sequential boot-up illumination and mood-based lighting (e.g., angry red pulse).
 * 
 * 4. State Machine Architecture:
 *    - Uses an enum-based state machine to control sequential animations:
 *        - STATE_BOOT_LEDS → STATE_BOOT_MSG1 → STATE_BOOT_MSG2
 *        - → STATE_EYES_CLOSED → STATE_EYES_OPEN → STATE_BLINK_PHASE → STATE_ANGRY_MODE
 * 
 * 5. Modular Functions:
 *    - `handleDisplay()`: Manages text drawing and eye updates.
 *    - `updateLEDs()`: Handles RGB animations and LED sequences.
 *    - `updateEyes()`: Updates eye expressions and behavior.
 *    - `drawCenteredText()`: Utility for text alignment on OLED.
 * 
 * Dependencies:
 * -------------
 * - Adafruit_SSD1306
 * - Adafruit_GFX
 * - FluxGarage_RoboEyes (custom library for OLED eye animations)
 * - Adafruit_NeoPixel
 * 
 * Hardware:
 * ---------
 * - ESP32 MCU AND ARDUINO NANO
 * - 2.4" OLED Display (SSD1306)
 * - NeoPixel RGB LEDs (x4)
 * - SC09 Serial Servo Driver 
 
 * Notes:
 * ------
 * - Designed for stable startup animations and characterful interaction.
 * - Flickering and blinking features are optimized for performance.
 * - Intended as the visual layer of CAKE's emotion and boot logic.
 * - Further Utility additions are supposed to be added in further devlopment.
 *   (such as - displaying time-date, calender, setting alarm, sticky ntotes...
 * =============================================================================
 */


#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI 23
#define OLED_CLK 18
#define OLED_DC 2
#define OLED_CS 5
#define OLED_RESET 4

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

#include <FluxGarage_RoboEyes.h>
roboEyes eyes;

#include <Adafruit_NeoPixel.h>
#define LED_PIN 27
#define LED_COUNT 4
Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

enum AnimationState {
  STATE_BOOT_LEDS,
  STATE_BOOT_MSG1,
  STATE_BOOT_MSG2,
  STATE_EYES_CLOSED,
  STATE_EYES_OPEN,
  STATE_BLINK_PHASE,
  STATE_ANGRY_MODE
};

AnimationState currentState = STATE_BOOT_LEDS;
unsigned long stateTimer = 0;
uint8_t ledSequence = 0;
bool ledsAllOn = false;

void setup() {
  Serial.begin(115200);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.dim(true);
  display.setTextWrap(false);
  display.clearDisplay();
  display.display();  

  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60); 
  eyes.setHFlicker(OFF);  
  eyes.setVFlicker(OFF);  
  eyes.close();

  leds.begin();
  leds.setBrightness(100);
  leds.clear();
  leds.show();

  stateTimer = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  
  switch(currentState) {
    case STATE_BOOT_LEDS:
      if(ledSequence >= LED_COUNT) {
        if(currentMillis - stateTimer > 1000) {
          currentState = STATE_BOOT_MSG1;
          stateTimer = millis();
        }
      }
      break;
      
    case STATE_BOOT_MSG1:
      if(currentMillis - stateTimer > 1000) {
        currentState = STATE_BOOT_MSG2;
        stateTimer = millis();
      }
      break;
      
    case STATE_BOOT_MSG2:
      if(currentMillis - stateTimer > 2000) {
        currentState = STATE_EYES_CLOSED;
        stateTimer = millis();
        display.clearDisplay();
        display.display();
        eyes.close();
      }
      break;
      
    case STATE_EYES_CLOSED:
      if(currentMillis - stateTimer > 500) {
        currentState = STATE_EYES_OPEN;
        stateTimer = millis();
        eyes.setHFlicker(OFF);  
        eyes.setVFlicker(OFF);  
        eyes.open();
      }
      break;
      
    case STATE_EYES_OPEN:
      if(currentMillis - stateTimer > 2000) {
        currentState = STATE_BLINK_PHASE;
        stateTimer = millis();
        eyes.setAutoblinker(true, 2500, 500);  
      }
      break;
      
    case STATE_BLINK_PHASE:
      if(currentMillis - stateTimer > 5000) {
        currentState = STATE_ANGRY_MODE;
        stateTimer = millis();
        eyes.setAutoblinker(OFF); 
        eyes.setMood(ANGRY);
        eyes.setVFlicker(ON, 3);  
      }
      break;
  }

  handleDisplay();
  updateLEDs();
  updateEyes();
}

void handleDisplay() {
  static unsigned long lastDraw = 0;
  if(millis() - lastDraw < 50) return; 
  
  if(currentState == STATE_BOOT_MSG1 || currentState == STATE_BOOT_MSG2) {
    display.clearDisplay();
  }

  switch(currentState) {
    case STATE_BOOT_MSG1:
      drawCenteredText("Hi I am,", 2, 0);  
      break;
      
    case STATE_BOOT_MSG2:
      drawCenteredText("CAKE", 3, 0);
      break;
      
    case STATE_EYES_CLOSED:
    case STATE_EYES_OPEN:
    case STATE_BLINK_PHASE:
    case STATE_ANGRY_MODE:
      break;
  }

  if(currentState == STATE_BOOT_MSG1 || currentState == STATE_BOOT_MSG2) {
    display.display();
  }
  
  lastDraw = millis();
}

void drawCenteredText(const char* text, uint8_t textSize, int8_t yOffset) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x, &y, &w, &h);
  display.setCursor((SCREEN_WIDTH - w)/2, (SCREEN_HEIGHT - h)/2 + yOffset);
  display.print(text);
}

void updateLEDs() {
  static unsigned long lastLedChange = 0;
  
  switch(currentState) {
    case STATE_BOOT_LEDS:
      if(millis() - lastLedChange > 200 && ledSequence < LED_COUNT) {
        leds.setPixelColor(ledSequence, leds.Color(0, 0, 150)); 
        leds.show();
        ledSequence++;
        lastLedChange = millis();
      }
      if(ledSequence >= LED_COUNT && !ledsAllOn) {
        delay(500);
        ledsAllOn = true;
        flashLEDs(0, 150, 0, 3); 
        flashLEDs(150, 150, 150, 3);
      }
      break;
      
    case STATE_ANGRY_MODE: {
      float intensity = 0.5 + 0.5 * sin(millis()/150.0);
      for(int i=0; i<LED_COUNT; i++) {
        leds.setPixelColor(i, leds.Color(200 * intensity, 0, 0));
      }
      leds.show();
      break;
    }
      
    default:
      for(int i=0; i<LED_COUNT; i++) {
        leds.setPixelColor(i, leds.Color(100, 100, 100));
      }
      leds.show();
      break;
  }
}

void flashLEDs(uint8_t r, uint8_t g, uint8_t b, uint8_t times) {
  for(int i=0; i<times; i++) {
    leds.fill(leds.Color(r, g, b));
    leds.show();
    delay(100);
    leds.clear();
    leds.show();
    delay(100);
  }
}

void updateEyes() {
  switch(currentState) {
    case STATE_BLINK_PHASE:
      eyes.update();
      break;
      
    case STATE_ANGRY_MODE:
      eyes.update();
      break;
      
    case STATE_EYES_CLOSED:
    case STATE_EYES_OPEN:
      eyes.setHFlicker(OFF);  
      eyes.setVFlicker(OFF); 
      eyes.update();
      break;
      
    default:
      break;
  }
  
  if(currentState >= STATE_EYES_CLOSED) {
    display.display();
  }
}


//if you reached here... thanks for taking a look.. hope you like the project.
