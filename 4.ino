#include <SCServo.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// HTML Web Interface
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>OttoNinja Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
        html {
            font-family: Arial;
            display: inline-block;
            background: #000000;
            color: #efefef;
            text-align: center;
        }
        h2 {
            font-size: 3.0rem;
        }
        p {
            font-size: 1.0rem;
        }
        body {
            max-width: 600px;
            margin: 0px auto;
            padding-bottom: 25px;
        }
        button {
            display: inline-block;
            margin: 5px;
            padding: 10px 10px;
            border: 0;
            line-height: 21px;
            cursor: pointer;
            color: #fff;
            background: #4247b7;
            border-radius: 5px;
            font-size: 21px;
            outline: 0;
            width: 150px;
            -webkit-touch-callout: none;
            -webkit-user-select: none;
            -khtml-user-select: none;
            -moz-user-select: none;
            -ms-user-select: none;
            user-select: none;
        }
        button:hover {
            background: #ff494d
        }
        button:active {
            background: #f21c21
        }
        select {
            display: inline-block;
            margin: 5px;
            padding: 10px 10px;
            border: 0;
            line-height: 21px;
            cursor: pointer;
            color: #fff;
            background: #4247b7;
            border-radius: 5px;
            font-size: 21px;
            outline: 0;
            width: 150px;
            -webkit-touch-callout: none;
            -webkit-user-select: none;
            -khtml-user-select: none;
            -moz-user-select: none;
            -ms-user-select: none;
            user-select: none;
        }
        select:hover {
            background: #ff494d
        }
        select:active {
            background: #f21c21
        }
    </style>
</head>
<body>
    <h3>OttoNinja Control Panel</h3>
    <p>
    <label align="center"><button class="button" onclick="sendCommand('forward');">Forward</button></label>
    <label align="center"><button class="button" onclick="sendCommand('backward');">Backward</button></label>
    <p>
    <label align="center"><button class="button" onclick="sendCommand('left');">Left</button></label>
    <label align="center"><button class="button" onclick="sendCommand('right');">Right</button></label>
    <p>
    <label align="center"><button class="button" onclick="sendCommand('stop');">Stop</button></label>
    <p>
    <label align="center"><button class="button" onclick="sendCommand('tilt_left');">Tilt Left</button></label>
    <label align="center"><button class="button" onclick="sendCommand('tilt_right');">Tilt Right</button></label>
    <p>
    <label align="center"><button class="button" onclick="sendCommand('stand');">Stand</button></label>
    <label align="center"><button class="button" onclick="sendCommand('wheel');">Wheel</button></label>
    <p>
    <label align="center"><button class="button" onclick="getStatus();">Get Status</button></label>
    <p>
    <label align="center"><select id="speedSelect" onchange="changeSpeed(this.value);">
        <option value="slow">Slow</option>
        <option value="medium" selected>Medium</option>
        <option value="fast">Fast</option>
        <option value="turbo">Turbo</option>

    </select></label>
    <p>
    <div id="status"></div>
    <script>
        function sendCommand(action) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/cmd?action=" + action, true);
            xhr.send();
        }
        function getStatus() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (this.readyState == 4 && this.status == 200) {
                    document.getElementById("status").innerHTML = this.responseText;
                }
            };
            xhr.open("GET", "/status", true);
            xhr.send();
        }
        function changeSpeed(speed) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/speed?preset=" + speed, true);
            xhr.send();
        }
    </script>
</body>
</html>
)rawliteral";

SCSCL sc;

// ========== Configuration Constants ==========
// Servo IDs
#define LEFT_WHEEL_ID 1    // PWM mode
#define RIGHT_WHEEL_ID 3   // PWM mode
#define LEFT_FOOT_ID 4     // Position mode
#define RIGHT_FOOT_ID 2    // Position mode

// Operation Modes
enum RobotMode { 
  MODE_STANDING, 
  MODE_WHEEL, 
  MODE_TILTED_LEFT, 
  MODE_TILTED_RIGHT 
};
RobotMode currentMode = MODE_STANDING;

// Speed Control
enum SpeedPreset { SLOW = 200, MEDIUM = 400, FAST = 700, TURBO = 1000 };
SpeedPreset currentSpeed = MEDIUM;

// Network Configuration
const char* AP_SSID = "CAKE_ROBOT";
const char* AP_PASS = "12345678";
const char* STA_SSID = "0000";
const char* STA_PASS = "12121212";

// Safety Parameters
const unsigned long SAFETY_TIMEOUT = 2000; // 2 seconds
unsigned long lastCommandTime = 0;

// ========== Global State Management ==========
// Movement Control
int targetLeftSpeed = 0;
int targetRightSpeed = 0;
int currentLeftSpeed = 0;
int currentRightSpeed = 0;
unsigned long lastSpeedUpdate = 0;
const int SPEED_RAMP_INTERVAL = 20;

// Tilt State Machine
enum TiltState { 
  NONE, 
  TILTING_LEFT, 
  TILTING_LEFT_STEP2,
  TILTING_RIGHT, 
  TILTING_RIGHT_STEP2 
};
TiltState tiltState = NONE;
unsigned long tiltStartTime = 0;
const int TILT_STEP_DURATION = 100;

// Battery Monitoring
float batteryVoltage = 0.0;
unsigned long lastUpdateTime = 0;
const long UPDATE_INTERVAL = 2000;

// Network Objects
AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial1.begin(1000000, SERIAL_8N1, 18, 19);
  sc.pSerial = &Serial1;

  // ========== Network Setup ==========
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.begin(STA_SSID, STA_PASS);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.print("Connecting to STA");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  if (!MDNS.begin("cake")) {
    Serial.println("mDNS Failed!");
  } else {
    MDNS.addService("http", "tcp", 80);
  }

  Serial.println("\nAP IP: " + WiFi.softAPIP().toString());
  Serial.println("STA IP: " + WiFi.localIP().toString());

  // ========== System Initialization ==========
  configureServos();
  setupServer();
  lastCommandTime = millis();
}

void loop() {
  updateNetworkConnection();
  updateSafetySystem();
  updateBattery();
  updateMovement();
  updateTilt();
}

// ========== Core Functions ==========
void configureServos() {
  // Configure wheels in PWM mode
  for (int id : {LEFT_WHEEL_ID, RIGHT_WHEEL_ID}) {
    sc.unLockEprom(id);
    sc.PWMMode(id);
    sc.LockEprom(id);
    sc.EnableTorque(id, 1);
  }

  // Configure feet in position mode
  for (int id : {LEFT_FOOT_ID, RIGHT_FOOT_ID}) {
    sc.unLockEprom(id);
    sc.WritePos(id, (id == LEFT_FOOT_ID) ? 490 : 530, 0, 400);
    sc.LockEprom(id);
    sc.EnableTorque(id, 1);
  }
}

// ========== Network Management ==========
void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    lastCommandTime = millis();
    if (request->hasParam("action")) handleCommand(request);
    request->send(200, "text/plain", "OK");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getStatus());
  });

  server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request) {
    lastCommandTime = millis();
    if (request->hasParam("preset")) handleSpeedChange(request);
    request->send(200, "text/plain", "Speed changed");
  });

  server.begin();
}

void updateNetworkConnection() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("STA Connection lost! Attempting reconnect...");
      WiFi.reconnect();
    }
    lastCheck = millis();
  }
}

// ========== Safety System ==========
void updateSafetySystem() {
  if (millis() - lastCommandTime > SAFETY_TIMEOUT) {
    emergencyStop();
  }
}

void emergencyStop() {
  targetLeftSpeed = 0;
  targetRightSpeed = 0;
  currentLeftSpeed = 0;
  currentRightSpeed = 0;
  sc.WritePWM(LEFT_WHEEL_ID, 0);
  sc.WritePWM(RIGHT_WHEEL_ID, 0);
  Serial.println("Safety timeout! Emergency stop activated.");
}

// ========== Movement Control ==========
void updateMovement() {
  if (millis() - lastSpeedUpdate >= SPEED_RAMP_INTERVAL) {
    currentLeftSpeed = rampSpeed(currentLeftSpeed, targetLeftSpeed);
    currentRightSpeed = rampSpeed(currentRightSpeed, targetRightSpeed);
    
    sc.WritePWM(LEFT_WHEEL_ID, currentLeftSpeed);
    sc.WritePWM(RIGHT_WHEEL_ID, currentRightSpeed);
    
    lastSpeedUpdate = millis();
  }
}

int rampSpeed(int current, int target) {
  const int maxStep = 50;
  if (abs(target - current) < maxStep) return target;
  return current + (target > current ? maxStep : -maxStep);
}

// ========== Tilt Control ==========
void updateTilt() {
  if (tiltState == NONE) return;

  unsigned long elapsed = millis() - tiltStartTime;
  
  switch(tiltState) {
    case TILTING_LEFT:
      if (elapsed < TILT_STEP_DURATION) return;
      sc.WritePos(RIGHT_FOOT_ID, 650, 0, 400);
      tiltStartTime = millis();
      tiltState = TILTING_LEFT_STEP2;
      break;
      
    case TILTING_LEFT_STEP2:
      if (elapsed < TILT_STEP_DURATION) return;
      sc.WritePos(LEFT_FOOT_ID, 570, 0, 400);
      tiltState = NONE;
      break;
      
    case TILTING_RIGHT:
      if (elapsed < TILT_STEP_DURATION) return;
      sc.WritePos(LEFT_FOOT_ID, 390, 0, 400);
      tiltStartTime = millis();
      tiltState = TILTING_RIGHT_STEP2;
      break;
      
    case TILTING_RIGHT_STEP2:
      if (elapsed < TILT_STEP_DURATION) return;
      sc.WritePos(RIGHT_FOOT_ID, 470, 0, 400);
      tiltState = NONE;
      break;
  }
}

// ========== Command Handling ==========
void handleCommand(AsyncWebServerRequest *request) {
  String action = request->getParam("action")->value();
  
  if (action == "forward")    setTargetSpeeds(currentSpeed, -currentSpeed);
  else if (action == "backward") setTargetSpeeds(-currentSpeed, currentSpeed);
  else if (action == "left")  setTargetSpeeds(-currentSpeed, -currentSpeed);
  else if (action == "right") setTargetSpeeds(currentSpeed, currentSpeed);
  else if (action == "stop")  setTargetSpeeds(0, 0);
  else if (action == "tilt_left") {
    tiltState = TILTING_LEFT;
    tiltStartTime = millis();
  }
  else if (action == "tilt_right") {
    tiltState = TILTING_RIGHT;
    tiltStartTime = millis();
  }
  else if (action == "stand") setStandingMode();
  else if (action == "wheel") setWheelMode();
}

void setTargetSpeeds(int left, int right) {
  targetLeftSpeed = constrain(left, -1023, 1023);
  targetRightSpeed = constrain(right, -1023, 1023);
}

// ========== Mode Management ==========
void setStandingMode() {
  currentMode = MODE_STANDING;
  sc.WritePos(LEFT_FOOT_ID, 490, 0, 400);
  sc.WritePos(RIGHT_FOOT_ID, 530, 0, 400);
  setTargetSpeeds(0, 0);
}

void setWheelMode() {
  currentMode = MODE_WHEEL;
  sc.WritePos(LEFT_FOOT_ID, 764, 0, 400);
  sc.WritePos(RIGHT_FOOT_ID, 250, 0, 400);
}

// ========== Status Monitoring ==========
String getStatus() {
  String status = "Connection: ";
  status += WiFi.softAPgetStationNum() > 0 ? "AP (" + WiFi.softAPIP().toString() + ")\n" 
          : "STA (" + WiFi.localIP().toString() + ")\n";
  
  status += "Mode: ";
  switch(currentMode) {
    case MODE_STANDING: status += "Standing\n"; break;
    case MODE_WHEEL: status += "Wheel\n"; break;
    case MODE_TILTED_LEFT: status += "Tilt Left\n"; break;
    case MODE_TILTED_RIGHT: status += "Tilt Right\n"; break;
  }

  status += "Speed: ";
  switch(currentSpeed) {
    case SLOW: status += "Slow"; break;
    case MEDIUM: status += "Medium"; break;
    case FAST: status += "Fast"; break;
    case TURBO: status += "Turbo"; break;
  }
  
  status += "\nBattery: " + String(batteryVoltage, 1) + "V (";
  status += String(map(batteryVoltage*10, 60, 84, 0, 100)) + "%)\n";
  status += "Uptime: " + String(millis()/1000) + "s\n";
  return status;
}

void updateBattery() {
  if (millis() - lastUpdateTime >= UPDATE_INTERVAL) {
    int voltageRaw = sc.ReadVoltage(LEFT_WHEEL_ID);
    if(voltageRaw != -1) batteryVoltage = voltageRaw * 0.1;
    lastUpdateTime = millis();
  }
}

// ========== Speed Configuration ==========
void handleSpeedChange(AsyncWebServerRequest *request) {
  String preset = request->getParam("preset")->value();
  if(preset == "slow") currentSpeed = SLOW;
  else if(preset == "medium") currentSpeed = MEDIUM;
  else if(preset == "fast") currentSpeed = FAST;
  else if(preset == "turbo") currentSpeed = TURBO;
}
