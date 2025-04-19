#include <Adafruit_SSD1306.h>

// OLED Configuration
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

// NeoPixel Configuration
#include <Adafruit_NeoPixel.h>
#define LED_PIN 27
#define LED_COUNT 4
Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Animation States
enum AnimationState {
  STATE_BOOT_LEDS,
  STATE_BOOT_MSG1,
  STATE_BOOT_MSG2,
  STATE_EYES_CLOSED,
  STATE_EYES_OPEN,
  STATE_BLINK_PHASE,
  STATE_ANGRY_MODE
};

// Global Variables
AnimationState currentState = STATE_BOOT_LEDS;
unsigned long stateTimer = 0;
uint8_t ledSequence = 0;
bool ledsAllOn = false;

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.dim(true);
  display.setTextWrap(false);
  display.clearDisplay();
  display.display();  // Clear the display initially

  // Initialize eyes
  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60); // Lower framerate for stability
  eyes.setHFlicker(OFF);  // Turn off horizontal flickering
  eyes.setVFlicker(OFF);  // Turn off vertical flickering
  eyes.close();

  // Initialize LEDs
  leds.begin();
  leds.setBrightness(100);
  leds.clear();
  leds.show();

  stateTimer = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // State transitions
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
        display.clearDisplay();  // Clear display before showing eyes
        display.display();
        eyes.close();
      }
      break;
      
    case STATE_EYES_CLOSED:
      if(currentMillis - stateTimer > 500) {
        currentState = STATE_EYES_OPEN;
        stateTimer = millis();
        eyes.setHFlicker(OFF);  // Make sure flickering is off
        eyes.setVFlicker(OFF);  // Make sure vertical flickering is off
        eyes.open();
      }
      break;
      
    case STATE_EYES_OPEN:
      if(currentMillis - stateTimer > 2000) {
        currentState = STATE_BLINK_PHASE;
        stateTimer = millis();
        eyes.setAutoblinker(true, 2500, 500);  // Enable automatic blinking with 2.5s interval and 500ms variation
      }
      break;
      
    case STATE_BLINK_PHASE:
      if(currentMillis - stateTimer > 5000) {
        currentState = STATE_ANGRY_MODE;
        stateTimer = millis();
        eyes.setAutoblinker(OFF);  // Turn off auto-blinker before changing mood
        eyes.setMood(ANGRY);
        eyes.setVFlicker(ON, 3);  // Vertical flickering for angry mode
      }
      break;
  }

  handleDisplay();
  updateLEDs();
  updateEyes();
}

void handleDisplay() {
  static unsigned long lastDraw = 0;
  if(millis() - lastDraw < 50) return; // Stable 20fps
  
  // Only clear display in text states
  if(currentState == STATE_BOOT_MSG1 || currentState == STATE_BOOT_MSG2) {
    display.clearDisplay();
  }

  switch(currentState) {
    case STATE_BOOT_MSG1:
      drawCenteredText("Hi I am,", 2, 0);  // Removed extra space
      break;
      
    case STATE_BOOT_MSG2:
      drawCenteredText("CAKE", 3, 0);
      break;
      
    case STATE_EYES_CLOSED:
    case STATE_EYES_OPEN:
    case STATE_BLINK_PHASE:
    case STATE_ANGRY_MODE:
      // Let RoboEyes handle drawing
      break;
  }

  // Only display in text states or after the eyes update
  if(currentState == STATE_BOOT_MSG1 || currentState == STATE_BOOT_MSG2) {
    display.display();
  }
  
  lastDraw = millis();
}

// Modified to add vertical offset
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
        leds.setPixelColor(ledSequence, leds.Color(0, 0, 150));  // Blue color
        leds.show();
        ledSequence++;
        lastLedChange = millis();
      }
      if(ledSequence >= LED_COUNT && !ledsAllOn) {
        delay(500);
        ledsAllOn = true;
        flashLEDs(0, 150, 0, 3); // Green flash
        flashLEDs(150, 150, 150, 3); // White flash instead of blue
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
      // Maintain neutral white
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
      // Using built-in auto-blinker for smoother blinking
      eyes.update();
      break;
      
    case STATE_ANGRY_MODE:
      eyes.update();
      break;
      
    case STATE_EYES_CLOSED:
    case STATE_EYES_OPEN:
      eyes.setHFlicker(OFF);  // Ensure no horizontal flickering during these states
      eyes.setVFlicker(OFF);  // Ensure no vertical flickering during these states
      eyes.update();
      break;
      
    default:
      // Don't update eyes during text display phases
      break;
  }
  
  // Only display the eyes in eye-related states
  if(currentState >= STATE_EYES_CLOSED) {
    display.display();
  }
}
