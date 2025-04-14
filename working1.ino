#include <SCServo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

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
    </script>
</body>
</html>
)rawliteral";

SCSCL sc; // SC series servo control class

// Servo IDs
#define LEFT_WHEEL_ID 1
#define RIGHT_WHEEL_ID 3
#define LEFT_FOOT_ID 4
#define RIGHT_FOOT_ID 2

// Positions and modes
#define LEFT_WHEEL_STANDING_POS 1000
#define RIGHT_WHEEL_STANDING_POS 1000
#define LEFT_FOOT_STANDING_POS 490
#define RIGHT_FOOT_STANDING_POS 530
#define LEFT_FOOT_WHEEL_POS 764
#define RIGHT_FOOT_WHEEL_POS 250

#define MODE_STANDING 0
#define MODE_WHEEL 1

int currentMode = MODE_STANDING;

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200); // For debug messages
  Serial1.begin(1000000, SERIAL_8N1, 18, 19); // SC servo communication
  sc.pSerial = &Serial1;

  // Configure servos
  configureServos();

  // Initialize WiFi
  WiFi.softAP("0000", "12121212");
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
        moveWheels(300, 300);
      } else if (action == "backward") {
        moveWheels(-300, -300);
      } else if (action == "left") {
        moveWheels(-300, 300);
      } else if (action == "right") {
        moveWheels(300, -300);
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

  server.begin();
}

void loop() {
  // Main loop can be used for additional tasks if needed
}

void configureServos() {
  // Configure left wheel (ID 1)
  sc.unLockEprom(LEFT_WHEEL_ID);
  sc.PWMMode(LEFT_WHEEL_ID);
  sc.LockEprom(LEFT_WHEEL_ID);
  sc.EnableTorque(LEFT_WHEEL_ID, 1);

  // Configure right wheel (ID 3)
  sc.unLockEprom(RIGHT_WHEEL_ID);
  sc.PWMMode(RIGHT_WHEEL_ID);
  sc.LockEprom(RIGHT_WHEEL_ID);
  sc.EnableTorque(RIGHT_WHEEL_ID, 1);

  // Configure left foot (ID 4)
  sc.unLockEprom(LEFT_FOOT_ID);
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_STANDING_POS, 0, 300);
  sc.LockEprom(LEFT_FOOT_ID);
  sc.EnableTorque(LEFT_FOOT_ID, 1);

  // Configure right foot (ID 2)
  sc.unLockEprom(RIGHT_FOOT_ID);
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_STANDING_POS, 0, 300);
  sc.LockEprom(RIGHT_FOOT_ID);
  sc.EnableTorque(RIGHT_FOOT_ID, 1);

  // Set to standing mode initially
  setStandingMode();
}

void moveWheels(int left_speed, int right_speed) {
  sc.WritePWM(LEFT_WHEEL_ID, left_speed);
  sc.WritePWM(RIGHT_WHEEL_ID, right_speed);
  Serial.print("Left: "); Serial.print(left_speed);
  Serial.print(" | Right: "); Serial.println(right_speed);
}

void tiltLeft() {
  sc.WritePos(LEFT_FOOT_ID, 390, 0, 300);
  delay(1000);
  sc.WritePos(RIGHT_FOOT_ID, 350, 0, 300);
  delay(1000);
  setStandingMode();
}

void tiltRight() {
  sc.WritePos(RIGHT_FOOT_ID, 350, 0, 300);
  delay(1000);
  sc.WritePos(LEFT_FOOT_ID, 390, 0, 300);
  delay(1000);
  setStandingMode();
}

void setStandingMode() {
  currentMode = MODE_STANDING;
  sc.WritePos(LEFT_WHEEL_ID, LEFT_WHEEL_STANDING_POS, 0, 300);
  sc.WritePos(RIGHT_WHEEL_ID, RIGHT_WHEEL_STANDING_POS, 0, 300);
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_STANDING_POS, 0, 300);
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_STANDING_POS, 0, 300);
}

void setWheelMode() {
  currentMode = MODE_WHEEL;
  sc.tus += "Left Foot Pos: "; status += sc.ReadPos(LEFT_FOOT_ID); status += "\n";
  status += "Right Foot Pos: "; status += sc.ReadPos(RIGHT_FOOT_ID); status += "\n";

  return status;
}WritePos(LEFT_WHEEL_ID, LEFT_WHEEL_STANDING_POS, 0, 300);
  sc.WritePos(RIGHT_WHEEL_ID, RIGHT_WHEEL_STANDING_POS, 0, 300);
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_WHEEL_POS, 0, 300);
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_WHEEL_POS, 0, 300);
}

String getStatus() {
  String status = "Mode: ";
  if (currentMode == MODE_STANDING) {
    status += "Standing\n";
  } else {
    status += "Wheel\n";
  }

  status += "Left Wheel Pos: "; status += sc.ReadPos(LEFT_WHEEL_ID); status += "\n";
  status += "Right Wheel Pos: "; status += sc.ReadPos(RIGHT_WHEEL_ID); status += "\n";
  status += "Left Foot Pos: "; status += sc.ReadPos(LEFT_FOOT_ID); status += "\n";
  status += "Right Foot Pos: "; status += sc.ReadPos(RIGHT_FOOT_ID); status += "\n";

  return status;
}
