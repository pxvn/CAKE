#include <SCServo.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> // Required for JSON status updates

// --- Robot Configuration ---
// Servo IDs
#define LEFT_WHEEL_ID 1
#define RIGHT_WHEEL_ID 3
#define LEFT_FOOT_ID 4
#define RIGHT_FOOT_ID 2

// Standing Positions (Feet Only)
#define LEFT_FOOT_STANDING_POS 490
#define RIGHT_FOOT_STANDING_POS 530

// Wheel Mode Positions (Feet Only)
#define LEFT_FOOT_WHEEL_POS 764
#define RIGHT_FOOT_WHEEL_POS 250

// Tilt Positions (Feet Only)
#define TILT_L_RIGHT_FOOT_STEP1 650
#define TILT_L_LEFT_FOOT_STEP2  570
#define TILT_L_RIGHT_FOOT_STEP3 650 // Repeat for stability? Original code did this.

#define TILT_R_LEFT_FOOT_STEP1  390
#define TILT_R_RIGHT_FOOT_STEP2 470
#define TILT_R_LEFT_FOOT_STEP3  350 // Corrected: Left foot moves again? Adjust if needed.

// Robot Modes / States
enum RobotState {
  STANDING,
  WHEEL_MODE,
  TILTING_LEFT,
  TILTING_RIGHT,
  PROCESSING_TILT // Intermediate state for non-blocking tilt
};
RobotState currentState = STANDING;

// Tilt State Machine (for non-blocking tilt)
enum TiltStep {
  TILT_IDLE,
  TILT_L_START,
  TILT_L_STEP1_WAIT,
  TILT_L_STEP2_WAIT,
  TILT_L_FINISH,
  TILT_R_START,
  TILT_R_STEP1_WAIT,
  TILT_R_STEP2_WAIT,
  TILT_R_FINISH
};
TiltStep currentTiltStep = TILT_IDLE;
unsigned long tiltStepTimer = 0;
const unsigned long TILT_STEP_DELAY = 150; // ms between tilt steps

// Speed Presets (PWM values for sc.WritePWM)
#define SLOW_PWM 100
#define MEDIUM_PWM 300
#define FAST_PWM 500
int currentSpeedPWM = MEDIUM_PWM; // Renamed for clarity

// Battery Config
const float BATT_VOLT_MIN = 6.0; // Adjust for your battery (2S Li-ion typically ~6.0V min)
const float BATT_VOLT_MAX = 8.4; // Adjust for your battery (2S Li-ion 8.4V max)

// --- Web Server & Servo Objects ---
AsyncWebServer server(80);
SCSCL sc; // Servo Control Object

// --- HTML Web Interface --- (Improved Layout)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>CAKE Robot Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
        body {
            font-family: Arial, sans-serif;
            background: #111;
            color: #eee;
            text-align: center;
            max-width: 500px;
            margin: 0 auto;
            padding: 15px;
        }
        h3 {
            color: #4CAF50; /* Green */
            font-size: 2.0rem;
        }
        p {
            margin-bottom: 15px; /* Spacing */
        }
        .button-grid {
            display: grid;
            grid-template-columns: 1fr 1fr; /* Two equal columns */
            gap: 10px; /* Space between buttons */
            margin-bottom: 15px;
        }
         .button-grid-single {
            display: grid;
            grid-template-columns: 1fr; /* Single column */
            gap: 10px;
            margin-bottom: 15px;
            justify-items: center; /* Center button */
        }
        button, select {
            display: block; /* Ensure buttons take full grid cell width */
            padding: 12px 15px;
            border: none;
            line-height: 1.5;
            cursor: pointer;
            color: #fff;
            background: #007bff; /* Blue */
            border-radius: 5px;
            font-size: 1.1rem;
            outline: 0;
            width: 100%; /* Make buttons fill container */
            box-sizing: border-box; /* Include padding in width */
            transition: background-color 0.2s ease;
            -webkit-touch-callout: none; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none;
        }
        button:hover, select:hover {
            background: #0056b3; /* Darker Blue */
        }
        button:active, select:active {
            background: #ff494d; /* Red */
        }
        #status {
            margin-top: 20px;
            padding: 15px;
            background: #333;
            border-radius: 5px;
            text-align: left;
            font-size: 1.0rem;
            line-height: 1.6;
            white-space: pre-wrap; /* Preserve line breaks */
        }
        label {
            display: block; /* Ensure labels take full width */
        }
        .status-label {
            font-weight: bold;
            color: #4CAF50; /* Green */
        }
    </style>
</head>
<body>
    <h3>CAKE Control Panel</h3>

    <div class="button-grid-single">
        <button onclick="sendCommand('forward');">Forward</button>
    </div>
    <div class="button-grid">
        <button onclick="sendCommand('left');">Left</button>
        <button onclick="sendCommand('right');">Right</button>
    </div>
     <div class="button-grid-single">
         <button onclick="sendCommand('backward');">Backward</button>
    </div>
    <div class="button-grid-single">
         <button onclick="sendCommand('stop');">Stop</button>
    </div>

    <p>--- Tilt ---</p>
    <div class="button-grid">
        <button onclick="sendCommand('tilt_left');">Tilt Left</button>
        <button onclick="sendCommand('tilt_right');">Tilt Right</button>
    </div>

    <p>--- Mode ---</p>
    <div class="button-grid">
        <button onclick="sendCommand('stand');">Stand</button>
        <button onclick="sendCommand('wheel');">Wheel</button>
    </div>

    <p>--- Speed ---</p>
     <div class="button-grid-single">
        <label><select id="speedSelect" onchange="changeSpeed(this.value);">
            <option value="slow">Slow</option>
            <option value="medium" selected>Medium</option>
            <option value="fast">Fast</option>
        </select></label>
    </div>

    <div id="status">Fetching status...</div>

    <script>
        function sendCommand(action) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/cmd?action=" + action, true);
            // Optional: Add response handling if needed
            xhr.onload = function() {
                 if (xhr.status === 200) {
                    console.log("Command Response:", xhr.responseText); // Log server response
                 }
             };
            xhr.send();
        }

        function changeSpeed(speed) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/speed?preset=" + speed, true);
            // Optional: Add response handling if needed
             xhr.onload = function() {
                 if (xhr.status === 200) {
                    console.log("Speed Response:", xhr.responseText); // Log server response
                 }
             };
            xhr.send();
        }

        function updateStatus() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (this.readyState == 4 && this.status == 200) {
                    try {
                        var data = JSON.parse(this.responseText);
                        var statusHTML =
                            `<span class="status-label">Mode:</span> ${data.mode}<br>` +
                            `<span class="status-label">L Wheel Pos:</span> ${data.lw_pos}<br>` +
                            `<span class="status-label">R Wheel Pos:</span> ${data.rw_pos}<br>` +
                            `<span class="status-label">L Foot Pos:</span> ${data.lf_pos}<br>` +
                            `<span class="status-label">R Foot Pos:</span> ${data.rf_pos}<br>` +
                            `<span class="status-label">Battery:</span> ${data.batt_v} V (${data.batt_p}%)`;
                        document.getElementById("status").innerHTML = statusHTML;
                    } catch (e) {
                        document.getElementById("status").innerHTML = "Error parsing status.";
                        console.error("Error parsing JSON:", e, this.responseText);
                    }
                } else if (this.readyState == 4) {
                     document.getElementById("status").innerHTML = "Error fetching status.";
                }
            };
            xhr.open("GET", "/status", true);
            xhr.send();
        }

        // Update status every 2 seconds
        setInterval(updateStatus, 2000);
        // Initial status update on load
        document.addEventListener('DOMContentLoaded', updateStatus);
    </script>
</body>
</html>
)rawliteral";


// --- Function Prototypes ---
void setup();
void loop();
void configureServos();
void moveWheels(int left_speed, int right_speed);
void startTiltLeft(); // Changed to start non-blocking tilt
void startTiltRight(); // Changed to start non-blocking tilt
void handleTiltSequence(); // Manages non-blocking tilt steps
void setStandingMode();
void setWheelMode();
void smoothRampPWM(int left_start, int right_start, int left_end, int right_end, int duration);
float easeInOutCubic(float t); // Helper for smooth ramp
String getStatusJson(); // Changed to return JSON
void changeSpeed(String preset);
float getBatteryVoltage();
float getBatteryPercentage(float voltage);


void setup() {
  Serial.begin(115200);
  Serial.println("CAKE Robot Booting...");

  Serial1.begin(1000000, SERIAL_8N1, 18, 19); // SC servo communication pins
  sc.pSerial = &Serial1;
  if (!Serial1) {
      Serial.println("Failed to start Serial1 for servos!");
      while(1); // Halt
  }
  delay(100); // Allow serial port to initialize

  // Configure servos
  configureServos();
  Serial.println("Servos Configured.");

  // Initialize WiFi AP
  WiFi.softAP("CAKE_ROBOT", "12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // --- Set up web server routes ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    String message = "OK"; // Default response
    if (request->hasParam("action")) {
      String action = request->getParam("action")->value();
      Serial.print("Received action: "); Serial.println(action); // Debug

      // Only allow wheel movement if in wheel mode
      if (currentState == WHEEL_MODE) {
          if (action == "forward") {
            moveWheels(currentSpeedPWM, -currentSpeedPWM); // Note: SC servo PWM might need + for forward on both? Adjust signs if needed.
            message = "OK: Moving Forward";
          } else if (action == "backward") {
            moveWheels(-currentSpeedPWM, currentSpeedPWM);
            message = "OK: Moving Backward";
          } else if (action == "left") {
            moveWheels(-currentSpeedPWM, -currentSpeedPWM); // Adjust based on how servos are mounted/wired
             message = "OK: Turning Left";
          } else if (action == "right") {
            moveWheels(currentSpeedPWM, currentSpeedPWM); // Adjust based on how servos are mounted/wired
             message = "OK: Turning Right";
          } else if (action == "stop") {
            moveWheels(0, 0);
             message = "OK: Stopping";
          }
      } else if (action == "stop"){
          // Allow stop in any mode, but only affects wheels if torque enabled
           moveWheels(0, 0);
           message = "OK: Stopping";
      }

      // Allow mode changes/tilts regardless of current movement
      if (action == "tilt_left") {
         if (currentTiltStep == TILT_IDLE) {
             startTiltLeft();
             message = "OK: Tilting Left";
         } else { message = "Busy: Already Tilting"; }
      } else if (action == "tilt_right") {
         if (currentTiltStep == TILT_IDLE) {
            startTiltRight();
            message = "OK: Tilting Right";
         } else { message = "Busy: Already Tilting"; }
      } else if (action == "stand") {
        setStandingMode();
        message = "OK: Switching to Standing Mode";
      } else if (action == "wheel") {
        setWheelMode();
        message = "OK: Switching to Wheel Mode";
      }
    } else {
        message = "Error: No action specified";
    }
    request->send(200, "text/plain", message);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String statusJson = getStatusJson();
    request->send(200, "application/json", statusJson);
  });

  server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request) {
     String message = "OK";
    if (request->hasParam("preset")) {
      String preset = request->getParam("preset")->value();
      changeSpeed(preset);
       message = "OK: Speed set to " + preset;
    } else {
         message = "Error: No speed preset specified";
    }
     request->send(200, "text/plain", message);
  });

  server.begin();
  Serial.println("Web Server Started.");
  Serial.println("Setup Complete.");
}

void loop() {
  // Handle non-blocking tilt sequences
  handleTiltSequence();

  // Other periodic tasks can go here if needed
  // Note: Avoid blocking code (long loops, delays) in loop()
}

void configureServos() {
  Serial.println("Configuring servos...");
  bool success = true;

  // Configure wheels (IDs 1 and 3) for PWM (Speed Control)
  for (int id : {LEFT_WHEEL_ID, RIGHT_WHEEL_ID}) {
    if (sc.Ping(id) != id) { // Check if servo is responding
        Serial.print("Error: Servo ID "); Serial.print(id); Serial.println(" not responding!");
        success = false;
        continue; // Skip configuration for this servo
    }
    sc.unLockEprom(id);
    delay(5); // Small delay after unlock
    sc.PWMMode(id); // Enable PWM mode for speed control
    delay(5); // Small delay after mode change
    sc.LockEprom(id);
    delay(5);
    sc.EnableTorque(id, 1); // Keep torque enabled for wheels initially in PWM mode
    delay(5);
    sc.WritePWM(id, 0); // Ensure wheels are stopped initially
     Serial.print("Servo ID "); Serial.print(id); Serial.println(" configured for PWM.");
  }

  // Configure feet (IDs 4 and 2) for Position Control
  for (int id : {LEFT_FOOT_ID, RIGHT_FOOT_ID}) {
     if (sc.Ping(id) != id) {
        Serial.print("Error: Servo ID "); Serial.print(id); Serial.println(" not responding!");
        success = false;
        continue;
    }
    // Ensure feet are in position mode (default, but let's be explicit)
    // We check if limits are 0, which indicates PWM mode for SC servos
    int minLimit = sc.ReadMode(id); // ReadMode checks angle limits for SC
    if (minLimit == 0) { // If it's in PWM mode (limits are 0)
        Serial.print("Servo ID "); Serial.print(id); Serial.println(" was in PWM mode, switching to Position mode.");
        sc.unLockEprom(id);
        delay(5);
        // Set standard angle limits for position mode (e.g., 0 to 1023 for SC09)
        sc.writeWord(id, SCSCL_MIN_ANGLE_LIMIT_L, 0); // Assuming standard range
        delay(5);
        sc.writeWord(id, SCSCL_MAX_ANGLE_LIMIT_L, 1023); // Assuming standard range
        delay(5);
        sc.LockEprom(id);
        delay(5);
    }
    // Set initial position and enable torque
    uint16_t initialPos = (id == LEFT_FOOT_ID) ? LEFT_FOOT_STANDING_POS : RIGHT_FOOT_STANDING_POS;
    sc.EnableTorque(id, 1);
    delay(5);
    sc.WritePos(id, initialPos, 0, 200); // Speed 200
    Serial.print("Servo ID "); Serial.print(id); Serial.print(" configured for Position. Initial Pos: "); Serial.println(initialPos);
    delay(50); // Allow time for servo to potentially reach position
  }

  if (success) {
     setStandingMode(); // Set initial mode after configuration
  } else {
      Serial.println("!!! Servo configuration failed for one or more servos. Robot might not function correctly. !!!");
      // Maybe blink an LED or enter a safe state here
  }
}

// Non-blocking state machine for tilt sequences
void handleTiltSequence() {
  if (currentTiltStep == TILT_IDLE) {
    return; // Nothing to do
  }

  // Check if the delay for the current step has passed
  if (millis() - tiltStepTimer < TILT_STEP_DELAY) {
    return; // Wait longer
  }

  // --- Tilting Left Sequence ---
  if (currentTiltStep == TILT_L_START) {
    Serial.println("Tilt Left: Step 1");
    sc.WritePos(RIGHT_FOOT_ID, TILT_L_RIGHT_FOOT_STEP1, 0, 200);
    tiltStepTimer = millis(); // Reset timer for next step
    currentTiltStep = TILT_L_STEP1_WAIT;
  } else if (currentTiltStep == TILT_L_STEP1_WAIT) {
     Serial.println("Tilt Left: Step 2");
    sc.WritePos(LEFT_FOOT_ID, TILT_L_LEFT_FOOT_STEP2, 0, 200);
    tiltStepTimer = millis();
    currentTiltStep = TILT_L_STEP2_WAIT;
  } else if (currentTiltStep == TILT_L_STEP2_WAIT) {
     Serial.println("Tilt Left: Step 3");
    sc.WritePos(RIGHT_FOOT_ID, TILT_L_RIGHT_FOOT_STEP3, 0, 200); // Optional stability move
    tiltStepTimer = millis();
    currentTiltStep = TILT_L_FINISH; // Move to finish state
  } else if (currentTiltStep == TILT_L_FINISH) {
     Serial.println("Tilt Left: Finished");
    currentState = TILTING_LEFT; // Update final robot state
    currentTiltStep = TILT_IDLE; // Reset tilt state machine
  }

  // --- Tilting Right Sequence ---
  else if (currentTiltStep == TILT_R_START) {
     Serial.println("Tilt Right: Step 1");
    sc.WritePos(LEFT_FOOT_ID, TILT_R_LEFT_FOOT_STEP1, 0, 200);
    tiltStepTimer = millis();
    currentTiltStep = TILT_R_STEP1_WAIT;
  } else if (currentTiltStep == TILT_R_STEP1_WAIT) {
    Serial.println("Tilt Right: Step 2");
    sc.WritePos(RIGHT_FOOT_ID, TILT_R_RIGHT_FOOT_STEP2, 0, 200);
    tiltStepTimer = millis();
    currentTiltStep = TILT_R_STEP2_WAIT;
  } else if (currentTiltStep == TILT_R_STEP2_WAIT) {
    Serial.println("Tilt Right: Step 3");
    sc.WritePos(LEFT_FOOT_ID, TILT_R_LEFT_FOOT_STEP3, 0, 200); // Adjusted stability move
    tiltStepTimer = millis();
    currentTiltStep = TILT_R_FINISH;
  } else if (currentTiltStep == TILT_R_FINISH) {
     Serial.println("Tilt Right: Finished");
    currentState = TILTING_RIGHT;
    currentTiltStep = TILT_IDLE;
  }
}

// Functions to initiate the non-blocking tilt
void startTiltLeft() {
  if (currentTiltStep == TILT_IDLE) { // Only start if not already tilting
    currentState = PROCESSING_TILT; // Intermediate state
    currentTiltStep = TILT_L_START;
    tiltStepTimer = millis(); // Start the timer now
    handleTiltSequence(); // Execute the first step immediately
  }
}

void startTiltRight() {
   if (currentTiltStep == TILT_IDLE) {
    currentState = PROCESSING_TILT;
    currentTiltStep = TILT_R_START;
    tiltStepTimer = millis();
    handleTiltSequence();
   }
}


// Function to control wheel speeds using smooth ramp
void moveWheels(int left_speed, int right_speed) {
   // Assuming current speed needs to be read or stored globally if ramping from current
   // For simplicity, this example ramps from 0 each time 'move' is called
   // More advanced: store current wheel PWM and ramp from there.
   Serial.print("Ramping wheels to L:"); Serial.print(left_speed); Serial.print(" R:"); Serial.println(right_speed);
   smoothRampPWM(0, 0, left_speed, right_speed, 200); // Ramp from 0 over 200ms
   // If you need continuous movement after ramp, call WritePWM again without ramp:
   // sc.WritePWM(LEFT_WHEEL_ID, left_speed);
   // sc.WritePWM(RIGHT_WHEEL_ID, right_speed);
}

// --- Mode Setting Functions ---
void setStandingMode() {
  Serial.println("Setting Standing Mode...");
  currentState = STANDING;
  // Stop wheels smoothly before changing posture
  smoothRampPWM(0, 0, 0, 0, 100); // Ramp current speed to 0 quickly

  // Move feet to standing position
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_STANDING_POS, 0, 200);
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_STANDING_POS, 0, 200);
  delay(50); // Short delay to allow servos to start moving

  // Disable torque on wheels AFTER feet are in position (or close)
  // Add delay or check position feedback if precise timing is needed
  delay(200); // Wait for feet to likely be stable
  sc.EnableTorque(LEFT_WHEEL_ID, 0);
  sc.EnableTorque(RIGHT_WHEEL_ID, 0);
  Serial.println("Standing Mode Set.");
}

void setWheelMode() {
  Serial.println("Setting Wheel Mode...");
  currentState = WHEEL_MODE;

  // Ensure wheel torque is enabled BEFORE moving feet
  sc.EnableTorque(LEFT_WHEEL_ID, 1);
  sc.EnableTorque(RIGHT_WHEEL_ID, 1);
  delay(10); // Small delay

  // Move feet to wheel mode position
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_WHEEL_POS, 0, 200);
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_WHEEL_POS, 0, 200);
  Serial.println("Wheel Mode Set.");
   // Wheels should be stopped initially when entering wheel mode
   sc.WritePWM(LEFT_WHEEL_ID, 0);
   sc.WritePWM(RIGHT_WHEEL_ID, 0);
}


// --- Helper Functions ---

// Cubic easing function for smoother ramps (t from 0.0 to 1.0)
float easeInOutCubic(float t) {
  return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

// Ramping function with easing (still uses delay)
void smoothRampPWM(int left_start, int right_start, int left_end, int right_end, int duration) {
  int steps = 20; // More steps for smoother ramp
  if (duration <= 0) duration = 1; // Avoid division by zero
  int step_duration = duration / steps;
  if (step_duration <= 0) step_duration = 1; // Ensure minimum delay

  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps; // Normalized time (0.0 to 1.0)
    float ease_factor = easeInOutCubic(t); // Apply easing

    int left_pwm = left_start + (int)((left_end - left_start) * ease_factor);
    int right_pwm = right_start + (int)((right_end - right_start) * ease_factor);

    sc.WritePWM(LEFT_WHEEL_ID, left_pwm);
    sc.WritePWM(RIGHT_WHEEL_ID, right_pwm);
    delay(step_duration);
  }
   // Ensure final value is set exactly
   sc.WritePWM(LEFT_WHEEL_ID, left_end);
   sc.WritePWM(RIGHT_WHEEL_ID, right_end);
}


// Gets status and formats as JSON string
String getStatusJson() {
  // Create a JSON document
  // Calculate size: https://arduinojson.org/v6/assistant/
  StaticJsonDocument doc; // Adjust size if needed

  // Add mode
  switch (currentState) {
    case STANDING:        doc["mode"] = "Standing"; break;
    case WHEEL_MODE:      doc["mode"] = "Wheel"; break;
    case TILTING_LEFT:    doc["mode"] = "Tilted Left"; break;
    case TILTING_RIGHT:   doc["mode"] = "Tilted Right"; break;
    case PROCESSING_TILT: doc["mode"] = "Processing Tilt"; break;
    default:              doc["mode"] = "Unknown"; break;
  }

  // Add servo positions (ReadPos returns -1 on error)
  doc["lw_pos"] = sc.ReadPos(LEFT_WHEEL_ID);
  doc["rw_pos"] = sc.ReadPos(RIGHT_WHEEL_ID);
  doc["lf_pos"] = sc.ReadPos(LEFT_FOOT_ID);
  doc["rf_pos"] = sc.ReadPos(RIGHT_FOOT_ID);

  // Add battery info
  float voltage = getBatteryVoltage();
  float percentage = getBatteryPercentage(voltage);
  doc["batt_v"] = String(voltage, 1); // Voltage with 1 decimal place
  doc["batt_p"] = String(percentage, 0); // Percentage with 0 decimal places

  // Serialize JSON to string
  String output;
  serializeJson(doc, output);
  return output;
}

// Changes the current speed preset
void changeSpeed(String preset) {
  if (preset == "slow") {
    currentSpeedPWM = SLOW_PWM;
  } else if (preset == "medium") {
    currentSpeedPWM = MEDIUM_PWM;
  } else if (preset == "fast") {
    currentSpeedPWM = FAST_PWM;
  }
  Serial.print("Speed set to: "); Serial.println(preset);
}

// Reads battery voltage from a servo
float getBatteryVoltage() {
  // Read voltage from one of the servos (e.g., left wheel)
  // Assumes servos are powered directly from the battery input on the driver board
  int voltage_raw = sc.ReadVoltage(LEFT_WHEEL_ID); // Returns value in 0.1V units
  if (voltage_raw == -1) {
      Serial.println("Error reading voltage from servo!");
      return 0.0; // Return 0 or some error indicator
  }
  return voltage_raw / 10.0; // Convert to Volts
}

// Calculates battery percentage based on voltage
float getBatteryPercentage(float voltage) {
  if (voltage <= 0.1) return 0.0; // Handle error case from getBatteryVoltage
  float percentage = ((voltage - BATT_VOLT_MIN) / (BATT_VOLT_MAX - BATT_VOLT_MIN)) * 100.0;
  return constrain(percentage, 0.0, 100.0); // Clamp between 0% and 100%
}
