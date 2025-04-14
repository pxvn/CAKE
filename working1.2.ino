#include <SCServo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

SCSCL sc;
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>CAKE Robot Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { background: #000; color: #fff; text-align: center; }
    button {
      padding: 15px;
      margin: 5px;
      font-size: 20px;
      background: #4CAF50;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    }
    button:hover { background: #45a049; }
    #status { color: #fff; margin: 20px; }
  </style>
</head>
<body>
  <h2>CAKE Robot Controller</h2>
  <div id="status"></div>

  <div style="margin:20px">
    <button onclick="sendCmd('forward')">↑</button>
    <button onclick="sendCmd('left')">&larr;</button>
    <button onclick="sendCmd('right')">&rarr;</button>
    <button onclick="sendCmd('backward')">↓</button>
    <button onclick="sendCmd('stop')">Stop</button>
  </div>

  <div>
    <button onclick="sendCmd('tilt_left')">Lean Left</button>
    <button onclick="sendCmd('tilt_right')">Lean Right</button>
    <button onclick="sendCmd('neutral')">Neutral</button>
    <button onclick="sendCmd('fall_left')">Fall Left</button>
    <button onclick="sendCmd('fall_right')">Fall Right</button>
    <button onclick="sendCmd('spin_tilted')">Spin Tilted</button>
  </div>

  <div>
    <button onclick="setSpeed(200)">Slow</button>
    <button onclick="setSpeed(300)">Medium</button>
    <button onclick="setSpeed(400)">Fast</button>
  </div>

  <script>
    let ws = new WebSocket('ws://' + location.host + '/ws');

    function sendCmd(cmd) {
      fetch('/cmd?action=' + cmd);
    }

    function setSpeed(speed) {
      ws.send(JSON.stringify({command: 'speed', value: speed}));
    }

    ws.onmessage = (event) => {
      document.getElementById('status').innerHTML = event.data;
    };

    setInterval(() => {
      fetch('/status').then(res => res.text()).then(data => {
        document.getElementById('status').innerHTML = data;
      });
    }, 1000);
  </script>
</body>
</html>
)rawliteral";

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
int currentSpeed = 300;
bool isTilted = false;

AsyncWebServer server(80);

void moveWheels(int left_speed, int right_speed) {
  sc.WritePWM(LEFT_WHEEL_ID, left_speed);
  sc.WritePWM(RIGHT_WHEEL_ID, right_speed);
  Serial.printf("L: %d R: %d\n", left_speed, right_speed);
}

void moveForward() { moveWheels(currentSpeed, currentSpeed); }
void moveBackward() { moveWheels(-currentSpeed, -currentSpeed); }
void moveLeft() { moveWheels(-currentSpeed, currentSpeed); }
void moveRight() { moveWheels(currentSpeed, -currentSpeed); }
void moveStop() { moveWheels(0, 0); }

void smoothServoMove(int id, int target) {
  int current = sc.ReadPos(id);
  for (int i = current; i != target; i += (target > current ? 1 : -1)) {
    sc.WritePos(id, i, 0, 150);
    delay(20);
  }
}

void tiltLeft() {
  smoothServoMove(RIGHT_FOOT_ID, 650);
  smoothServoMove(LEFT_FOOT_ID, 570);
  delay(500);
  isTilted = true;
}

void tiltRight() {
  smoothServoMove(LEFT_FOOT_ID, 390);
  smoothServoMove(RIGHT_FOOT_ID, 470);
  delay(500);
  smoothServoMove(LEFT_FOOT_ID, 390);
  smoothServoMove(RIGHT_FOOT_ID, 350);
  isTilted = true;
}

void setStandingMode() {
  currentMode = MODE_STANDING;
  isTilted = false;
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_STANDING_POS, 0, 300);
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_STANDING_POS, 0, 300);
}

void fallLeft() {
  tiltLeft();
  sc.WritePos(LEFT_FOOT_ID, LEFT_FOOT_WHEEL_POS, 0, 300);
}

void fallRight() {
  tiltRight();
  sc.WritePos(RIGHT_FOOT_ID, RIGHT_FOOT_WHEEL_POS, 0, 300);
}

void spinWhileTilted() {
  if (isTilted) {
    moveWheels(currentSpeed, 0);
  }
}

void emergencyStop() {
  moveStop();
  setStandingMode();
}

String getStatus() {
  String status = "Battery: ";
  status += sc.ReadVoltage(RIGHT_FOOT_ID);
  status += "V | Mode: ";
  status += (currentMode == MODE_STANDING) ? "Standing" : "Wheel";
  return status;
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      DynamicJsonDocument doc(200);
      deserializeJson(doc, (char*)data);
      String command = doc["command"];
      int value = doc["value"];
      if (command == "speed") {
        currentSpeed = value;
      } else if (command == "move") {
        moveWheels(value, -value);
      }
    }
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("action")) {
      String action = request->getParam("action")->value();
      if (action == "forward") moveForward();
      else if (action == "backward") moveBackward();
      else if (action == "left") moveLeft();
      else if (action == "right") moveRight();
      else if (action == "stop") moveStop();
      else if (action == "tilt_left") tiltLeft();
      else if (action == "tilt_right") tiltRight();
      else if (action == "neutral") setStandingMode();
      else if (action == "fall_left") fallLeft();
      else if (action == "fall_right") fallRight();
      else if (action == "spin_tilted") spinWhileTilted();
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getStatus());
  });

  server.addHandler(&ws);
  ws.onEvent(onWsEvent);
  server.begin();
}

void configureServos() {
  for (int id : {LEFT_WHEEL_ID, RIGHT_WHEEL_ID}) {
    sc.unLockEprom(id);
    sc.PWMMode(id);
    sc.LockEprom(id);
    sc.EnableTorque(id, 1);
  }

  for (int id : {LEFT_FOOT_ID, RIGHT_FOOT_ID}) {
    sc.unLockEprom(id);
    sc.WritePos(id, sc.ReadPos(id), 0, 300);
    sc.LockEprom(id);
    sc.EnableTorque(id, 1);
  }
}

void pingAllServos() {
  for (int id = 1; id <= 4; id++) {
    if (sc.Ping(id) != -1) {
      Serial.printf("Servo %d online\n", id);
    }
  }
}

void initWiFi() {
  WiFi.softAP("CAKE_ROBOT", "12345678");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(1000000, SERIAL_8N1, 18, 19);
  sc.pSerial = &Serial1;

  configureServos();
  initWiFi();
  setupWebServer();
  pingAllServos();

  setStandingMode();
}

void loop() {
  ws.cleanupClients();

  // Optional auto-stabilization
  static unsigned long lastCheck = 0;
  if (isTilted && millis() - lastCheck > 1000) {
    int leftPos = sc.ReadPos(LEFT_FOOT_ID);
    int rightPos = sc.ReadPos(RIGHT_FOOT_ID);
    if (abs(leftPos - LEFT_FOOT_STANDING_POS) > 200 || abs(rightPos - RIGHT_FOOT_STANDING_POS) > 200) {
      emergencyStop();
    }
    lastCheck = millis();
  }

  delay(10);
}
