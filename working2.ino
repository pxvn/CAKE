#include <SCServo.h>
#include <WiFi.h>
#include <WebServer.h>
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

// Servo Control Class
SCSCL sc;

// Servo IDs
#define LEFT_WHEEL_ID 1
#define RIGHT_WHEEL_ID 3
#define LEFT_FOOT_ID 4
#define RIGHT_FOOT_ID 2

// Positions and Modes
#define LEFT_WHEEL_STANDING_POS 1000
#define RIGHT_WHEEL_STANDING_POS 1000
#define LEFT_FOOT_STANDING_POS 490
#define RIGHT_FOOT_STANDING_POS 530
#define LEFT_FOOT_WHEEL_POS 764
#define RIGHT_FOOT_WHEEL_POS 250

#define MODE_STANDING 0
#define MODE_WHEEL 1
#define MODE_TILTED_LEFT 2
#define MODE_TILTED_RIGHT 3

int currentMode = MODE_STANDING;

// Speed Presets
#define SLOW_PWM 100
#define MEDIUM_PWM 300
#define FAST_PWM 500

int currentSpeed = MEDIUM_PWM;

AsyncWebServer server(80);

// Function Prototypes
void setup();
void loop();
void configureServos();
void moveWheels(int left_speed, int right_speed);
void tiltLeft();
void tiltRight();
void setStandingMode();
void setWheelMode();
void setTiltLeftMode();
void setTiltRightMode();
void rampPWM(int left_start, int right_start, int left_end, int right_end, int duration);
String getStatus();
void changeSpeed(String preset);

void setup() {
  Serial.begin(115200); // For debug messages
  Serial1.begin(1000000, SERIAL_8N1, 18, 19); // SC servo communication
  sc.pSerial = &Serial1;

  // Configure servos
  configureServos();

  // Initialize WiFi
  WiFi.softAP("CAKE_ROBOT", "12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Set up web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("action")) {
      String action = request->getParam("action")->value();
      if (action == "forward") {
        moveWheels(currentSpeed, -currentSpeed);
      } else if (action == "backward") {
        moveWheels(-currentSpeed, currentSpeed);
      } else if (action == "left") {
        moveWheels(-currentSpeed, -currentSpeed);
      } else if (action == "right") {
        moveWheels(currentSpeed, currentSpeed);
      } else if (action == "stop") {
        moveWheels(0, 0);
      } else if (action == "tilt_left") {
        tiltLeft();
      } else if (action == "tilt_right") {
        tiltRight();
      } else if (action == "stand") {
        setStandingMode();
      } else if (action == "wheel") {
        setWheelMode();
      }
    }
    request->send(200, "text/plain", "Command executed");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String status = getStatus();
    request->send(200, "text/plain", status);
  });

  server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("preset")) {
      String preset = request->getParam("preset")->value();
      changeSpeed(preset);
    }
    request->send(200, "text/plain", "Speed changed");
  });

  server.begin();
}

void loop() {
  // Main loop can be used for additional tasks if needed
}

void configureServos() {
  // Configure wheels (IDs 1 and 3)
  for (int id : {LEFT_WHEEL_ID, RIGHT_WHEEL_ID}) {
    sc.unLockEprom(id);
    sc.PWMMode(id); // Enable PWM mode for speed control
    sc.LockEprom(id);
    sc.EnableTorque(id, 1);
    sc.WritePos(id, 1000, 0, 200); // Initial position to 1000
  }

  // Configure feet (IDs 4 and 2)
  for (int id : {LEFT_FOOT_ID, RIGHT_FOOT_ID}) {
    sc.unLockEprom(id);
    sc.WritePos(id, (id == LEFT_FOOT_ID) ? LEFT_FOOT_STANDING_POS : RIGHT_FOOT_STANDING_POS, 0, 200);
    sc.LockEprom(id);
    sc.EnableTorque(id, 1);
  }

  setStandingMode();
}

void moveWheels(int left_speed, int right_speed) {
  rampPWM(0, 0, left_speed, right_speed, 200); // Gradual start/stop
}

void tiltLeft() {
  sc.WritePos(RIGHT_FOOT_ID, 650, 0, 200); // Move right leg to 650
  delay(100); // Brief delay
  sc.WritePos(LEFT_FOOT_ID, 570, 0, 200); // Move left leg to 570
  delay(100); // Brief delay
  sc.WritePos(RIGHT_FOOT_ID, 650, 0, 200); // Move right leg to 650 again
  currentMode = MODE_TILTED_LEFT;
}

void tiltRight() {
  sc.WritePos(LEFT_FOOT_ID, 390, 0, 200); // Move left leg to 390
  delay(100); // Brief delay
  sc.WritePos(RIGHT_FOOT_ID, 470, 0, 200); // Move right leg to 470
  delay(100); // Brief delay
  sc.WritePos(RIGHT_FOOT_ID, 350, 0, 200); // Move right leg to 350
  currentMode = MODE_TILTED_RIGHT;
}

void setStandingMode() {
  currentMode = MODE_STANDING;
  sc.WritePos(LEFT_WHEEL_ID, LEFT_WHEEL_STANDING_POS, 0, 200);
  sc.WritePos(RIGHT_WHEEL_ID, RIGHT_WHEEL_STANDING_POS, 0, 200);
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_STANDING_POS, 0, 200);
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_STANDING_POS, 0, 200);
  sc.EnableTorque(LEFT_WHEEL_ID, 0); // Disable torque after standing
  sc.EnableTorque(RIGHT_WHEEL_ID, 0);
}

void setWheelMode() {
  currentMode = MODE_WHEEL;
  sc.WritePos(LEFT_WHEEL_ID, LEFT_WHEEL_STANDING_POS, 0, 200);
  sc.WritePos(RIGHT_WHEEL_ID, RIGHT_WHEEL_STANDING_POS, 0, 200);
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_WHEEL_POS, 0, 200);
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_WHEEL_POS, 0, 200);
  sc.EnableTorque(LEFT_WHEEL_ID, 1); // Re-enable torque for wheels
  sc.EnableTorque(RIGHT_WHEEL_ID, 1);
}

void setTiltLeftMode() {
  currentMode = MODE_TILTED_LEFT;
}

void setTiltRightMode() {
  currentMode = MODE_TILTED_RIGHT;
}

void rampPWM(int left_start, int right_start, int left_end, int right_end, int duration) {
  int steps = 10;
  int step_duration = duration / steps;
  for (int i = 0; i <= steps; i++) {
    int left_pwm = left_start + (left_end - left_start) * i / steps;
    int right_pwm = right_start + (right_end - right_start) * i / steps;
    sc.WritePWM(LEFT_WHEEL_ID, left_pwm);
    sc.WritePWM(RIGHT_WHEEL_ID, right_pwm);
    delay(step_duration);
  }
}

String getStatus() {
  String status = "Mode: ";
  switch (currentMode) {
    case MODE_STANDING:
      status += "Standing\n";
      break;
    case MODE_WHEEL:
      status += "Wheel\n";
      break;
    case MODE_TILTED_LEFT:
      status += "Tilted Left\n";
      break;
    case MODE_TILTED_RIGHT:
      status += "Tilted Right\n";
      break;
  }

  status += "Left Wheel Pos: " + String(sc.ReadPos(LEFT_WHEEL_ID)) + "\n";
  status += "Right Wheel Pos: " + String(sc.ReadPos(RIGHT_WHEEL_ID)) + "\n";
  status += "Left Foot Pos: " + String(sc.ReadPos(LEFT_FOOT_ID)) + "\n";
  status += "Right Foot Pos: " + String(sc.ReadPos(RIGHT_FOOT_ID)) + "\n";
  status += "Battery: " + String(getBatteryPercentage()) + "%\n";

  return status;
}

void changeSpeed(String preset) {
  if (preset == "slow") {
    currentSpeed = SLOW_PWM;
  } else if (preset == "medium") {
    currentSpeed = MEDIUM_PWM;
  } else if (preset == "fast") {
    currentSpeed = FAST_PWM;
  }
}

float getBatteryPercentage() {
  // Placeholder for battery percentage reading
  // Replace with actual battery monitoring logic
  // Example: Read from an analog pin or use a dedicated battery sensor
  float batteryVoltage = analogRead(A0) * (3.3 / 4095.0) * 2; // Assuming a voltage divider
  float batteryPercentage = (batteryVoltage - 3.7) / (4.2 - 3.7) * 100; // Assuming 3.7V is 0% and 4.2V is 100%
  batteryPercentage = constrain(batteryPercentage, 0, 100);
  return batteryPercentage;
}