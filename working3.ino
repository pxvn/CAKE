#include <SCServo.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>



SCSCL sc;

// Operation Modes
enum RobotMode { 
  MODE_STANDING, 
  MODE_WHEEL, 
  MODE_TILTED_LEFT, 
  MODE_TILTED_RIGHT 
};

// Speed Presets
enum SpeedPreset { SLOW = 200, MEDIUM = 400, FAST = 700, TURBO = 1000 };

// Servo Configuration
#define LEFT_WHEEL_ID 1
#define RIGHT_WHEEL_ID 3
#define LEFT_FOOT_ID 4
#define RIGHT_FOOT_ID 2

// System State
struct SystemState {
  RobotMode mode = MODE_STANDING;
  float voltage = 0.0;
  unsigned long uptime = 0;
  unsigned long lastCommand = 0;
  bool connected = false;
};

SystemState systemState;
AsyncWebServer server(80);
SpeedPreset currentSpeed = MEDIUM;

// Keep the HTML content from your UI at the end
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>CAKE Companion</title>
  <style>
    :root {
      --primary: #2196f3;
      --secondary: #4caf50;
      --background: #121212;
      --surface: #1e1e1e;
      --text: #f1f1f1;
      --muted: #888;
      --danger: #e53935;
      --accent: #ffc107;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    html, body {
      font-family: 'Segoe UI', Roboto, sans-serif;
      background: var(--background);
      color: var(--text);
      padding: 24px;
      margin: 0;
      min-height: 100vh;
    }

    h2, h3 {
      margin-bottom: 12px;
      font-weight: 600;
    }

    .dashboard {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
      gap: 24px;
      width: 100%;
      max-width: 1200px;
      margin: 0 auto;
    }

    .panel {
      background: var(--surface);
      padding: 24px;
      border-radius: 12px;
      box-shadow: 0 0 12px rgba(0,0,0,0.2);
    }

    .button-group {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 12px;
      margin-top: 12px;
    }

    button {
      padding: 14px;
      background: var(--primary);
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 500;
      cursor: pointer;
      transition: all 0.2s ease;
    }

    button:hover {
      background: #42a5f5;
    }

    button:active {
      transform: scale(0.98);
    }

    button.danger {
      background: var(--danger);
    }

    select {
      width: 100%;
      padding: 12px;
      border-radius: 8px;
      background: #2c2c2c;
      color: var(--text);
      border: none;
      margin-top: 12px;
      font-size: 16px;
    }

    .indicator {
      margin: 16px 0;
    }

    .indicator-label {
      margin-bottom: 6px;
      font-size: 14px;
      color: var(--muted);
    }

    .progress-bar {
      height: 20px;
      background: #2f2f2f;
      border-radius: 10px;
      overflow: hidden;
    }

    .progress-fill {
      height: 100%;
      background: var(--secondary);
      transition: width 0.4s ease;
    }

    .position-meter {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 16px;
      margin-top: 16px;
    }

    .meter-item {
      background: #292929;
      padding: 16px;
      border-radius: 8px;
      text-align: center;
    }

    .meter-title {
      font-size: 14px;
      color: var(--muted);
      margin-bottom: 4px;
    }

    .meter-value {
      font-size: 20px;
      font-weight: 600;
      color: var(--accent);
    }

    .connection-status {
      position: fixed;
      top: 20px;
      right: 20px;
      padding: 8px 16px;
      border-radius: 20px;
      background: #444;
      font-size: 14px;
      font-weight: 500;
      transition: background 0.3s ease;
    }

    .connected {
      background: var(--secondary);
      color: black;
    }

    .disconnected {
      background: var(--danger);
    }

    .system-info {
      margin-top: 24px;
      font-size: 14px;
      color: var(--muted);
    }

    .system-info span {
      color: var(--text);
      font-weight: 500;
    }

    .info-item {
      margin-top: 8px;
    }
  </style>
</head>
<body>
  <div class="connection-status" id="connectionStatus">Connecting...</div>

  <div class="dashboard">
    <div class="panel">
      <h2>Motion Control</h2>
      <div class="button-group">
        <button onclick="sendCommand('forward')">Forward</button>
        <button onclick="sendCommand('backward')">Backward</button>
        <button onclick="sendCommand('left')">Turn Left</button>
        <button onclick="sendCommand('right')">Turn Right</button>
      </div>
      <button class="danger" onclick="sendCommand('stop')" style="margin-top: 16px;">Emergency Stop</button>

      <h3 style="margin-top: 24px;">Posture & Modes</h3>
      <div class="button-group">
        <button onclick="sendCommand('stand')">Stand Mode</button>
        <button onclick="sendCommand('wheel')">Wheel Mode</button>
        <button onclick="sendCommand('tilt_left')">Tilt Left</button>
        <button onclick="sendCommand('tilt_right')">Tilt Right</button>
      </div>

      <h3 style="margin-top: 24px;">Speed Preset</h3>
      <select id="speedSelect" onchange="changeSpeed(this.value)">
        <option value="200">Slow (200)</option>
        <option value="400" selected>Medium (400)</option>
        <option value="700">Fast (700)</option>
        <option value="1000">Turbo (1000)</option>
      </select>
    </div>

    <div class="panel">
      <h2>System Status</h2>

      <div class="indicator">
        <div class="indicator-label">Current Mode</div>
        <div class="progress-bar">
          <div class="progress-fill" id="modeIndicator" style="width: 100%"></div>
        </div>
      </div>

      <div class="indicator">
        <div class="indicator-label">Battery Voltage</div>
        <div class="progress-bar">
          <div class="progress-fill" id="voltageIndicator" style="width: 0%"></div>
        </div>
      </div>

      <div class="position-meter">
        <div class="meter-item">
          <div class="meter-title">Left Wheel</div>
          <div class="meter-value" id="posLw">--</div>
        </div>
        <div class="meter-item">
          <div class="meter-title">Right Wheel</div>
          <div class="meter-value" id="posRw">--</div>
        </div>
        <div class="meter-item">
          <div class="meter-title">Left Foot</div>
          <div class="meter-value" id="posLf">--</div>
        </div>
        <div class="meter-item">
          <div class="meter-title">Right Foot</div>
          <div class="meter-value" id="posRf">--</div>
        </div>
      </div>

      <div class="system-info">
        <div class="info-item">IP Address: <span id="ipAddress">--</span></div>
        <div class="info-item">Uptime: <span id="uptime">--</span></div>
        <div class="info-item">Last Sync: <span id="connectionTime">--</span></div>
      </div>
    </div>
  </div>

  <script>
    let lastUpdate = Date.now();
    const connectionStatus = document.getElementById('connectionStatus');

    async function fetchStatus() {
      try {
        const response = await fetch('/status');
        if (!response.ok) throw new Error();

        const data = await response.json();
        lastUpdate = Date.now();
        updateUI(data);

        connectionStatus.classList.add('connected');
        connectionStatus.classList.remove('disconnected');
        connectionStatus.textContent = `Connected · ${formatTime(data.uptime)}`;
      } catch (error) {
        connectionStatus.classList.remove('connected');
        connectionStatus.classList.add('disconnected');
        connectionStatus.textContent = 'Disconnected';
      }
    }

    function updateUI(data) {
      document.getElementById('modeIndicator').style.width = `${Math.min(data.percentage, 100)}%`;
      document.getElementById('modeIndicator').parentElement.previousElementSibling.textContent = `Mode: ${data.mode}`;

      const voltageFill = document.getElementById('voltageIndicator');
      voltageFill.style.width = `${data.percentage}%`;
      voltageFill.parentElement.previousElementSibling.textContent = `Battery: ${data.voltage}V (${data.percentage}%)`;

      document.getElementById('posLw').textContent = data.positions.lw;
      document.getElementById('posRw').textContent = data.positions.rw;
      document.getElementById('posLf').textContent = data.positions.lf;
      document.getElementById('posRf').textContent = data.positions.rf;

      document.getElementById('ipAddress').textContent = data.ip;
      document.getElementById('uptime').textContent = formatTime(data.uptime);
    }

    function formatTime(seconds) {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      return `${h}h ${m}m ${s}s`;
    }

    async function sendCommand(action) {
      try {
        await fetch(`/cmd?action=${action}`);
      } catch (error) {
        console.error('Command failed:', error);
      }
    }

    async function changeSpeed(preset) {
      try {
        await fetch(`/speed?preset=${preset}`);
      } catch (error) {
        console.error('Speed change failed:', error);
      }
    }

    // Polling
    setInterval(fetchStatus, 1000);
    fetchStatus();

    setInterval(() => {
      const elapsed = Date.now() - lastUpdate;
      if (elapsed > 2000) {
        connectionStatus.classList.remove('connected');
        connectionStatus.classList.add('disconnected');
        connectionStatus.textContent = `Disconnected · ${Math.floor(elapsed / 1000)}s`;
      }
    }, 500);
  </script>
</body>
</html>

)rawliteral";
void setup() {
  Serial.begin(115200);
  Serial1.begin(1000000, SERIAL_8N1, 18, 19);
  sc.pSerial = &Serial1;

  // Servo Initialization
  configureServos();

  // WiFi Setup
  WiFi.softAP("CAKE_ROBOT", "12345678");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Web Server Endpoints
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    systemState.lastCommand = millis();
    if(request->hasParam("action")) handleCommand(request);
    request->send(200, "text/plain", "OK");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(512);
    doc["mode"] = String(systemState.mode);
    doc["voltage"] = systemState.voltage;
    doc["percentage"] = map(systemState.voltage*10, 60, 84, 0, 100);
    doc["ip"] = WiFi.softAPIP().toString();
    doc["uptime"] = millis() / 1000;
    
    JsonObject positions = doc.createNestedObject("positions");
    positions["lw"] = sc.ReadPos(LEFT_WHEEL_ID);
    positions["rw"] = sc.ReadPos(RIGHT_WHEEL_ID);
    positions["lf"] = sc.ReadPos(LEFT_FOOT_ID);
    positions["rf"] = sc.ReadPos(RIGHT_FOOT_ID);
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(request->hasParam("preset")) {
      int preset = request->getParam("preset")->value().toInt();
      currentSpeed = (SpeedPreset)constrain(preset, 200, 1000);
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Update system state every 2 seconds
  static unsigned long lastUpdate;
  if(currentMillis - lastUpdate >= 2000) {
    updateBattery();
    systemState.uptime = currentMillis / 1000;
    lastUpdate = currentMillis;
  }

  // Emergency stop on connection loss
  if(currentMillis - systemState.lastCommand > 2000 && systemState.connected) {
    emergencyStop();
    systemState.connected = false;
  }
}

// Servo Configuration
void configureServos() {
  // Configure wheels in PWM mode
  for(int id : {LEFT_WHEEL_ID, RIGHT_WHEEL_ID}) {
    sc.unLockEprom(id);
    sc.PWMMode(id);
    sc.LockEprom(id);
    sc.EnableTorque(id, 1);
  }

  // Configure feet in position mode
  for(int id : {LEFT_FOOT_ID, RIGHT_FOOT_ID}) {
    sc.unLockEprom(id);
    sc.WritePos(id, (id == LEFT_FOOT_ID) ? 490 : 530, 0, 400);
    sc.LockEprom(id);
    sc.EnableTorque(id, 1);
  }
}

// Movement Functions
void moveWheels(int left, int right) {
  sc.WritePWM(LEFT_WHEEL_ID, constrain(left, -1000, 1000));
  sc.WritePWM(RIGHT_WHEEL_ID, constrain(right, -1000, 1000));
}

void tiltLeft() {
  unsigned long start = millis();
  while(millis() - start < 400) {
    sc.WritePos(RIGHT_FOOT_ID, 650, 0, 0);
    sc.WritePos(LEFT_FOOT_ID, 570, 0, 0);
  }
  systemState.mode = MODE_TILTED_LEFT;
}

void tiltRight() {
  unsigned long start = millis();
  while(millis() - start < 400) {
    sc.WritePos(LEFT_FOOT_ID, 390, 0, 0);
    sc.WritePos(RIGHT_FOOT_ID, 470, 0, 0);
  }
  systemState.mode = MODE_TILTED_RIGHT;
}

// System Functions
void updateBattery() {
  int raw = sc.ReadVoltage(LEFT_WHEEL_ID);
  systemState.voltage = raw != -1 ? raw * 0.1 : 0.0;
}

void emergencyStop() {
  moveWheels(0, 0);
  setStandingMode();
}

void handleCommand(AsyncWebServerRequest *request) {
  String action = request->getParam("action")->value();
  
  if(action == "forward")       moveWheels(currentSpeed, -currentSpeed);
  else if(action == "backward") moveWheels(-currentSpeed, currentSpeed);
  else if(action == "left")     moveWheels(-currentSpeed, -currentSpeed);
  else if(action == "right")    moveWheels(currentSpeed, currentSpeed);
  else if(action == "stop")     moveWheels(0, 0);
  else if(action == "tilt_left") tiltLeft();
  else if(action == "tilt_right") tiltRight();
  else if(action == "stand")    setStandingMode();
  else if(action == "wheel")    setWheelMode();
}

// Mode Functions
void setStandingMode() {
  systemState.mode = MODE_STANDING;
  sc.WritePos(LEFT_FOOT_ID, 490, 0, 400);
  sc.WritePos(RIGHT_FOOT_ID, 530, 0, 400);
}

void setWheelMode() {
  systemState.mode = MODE_WHEEL;
  sc.WritePos(LEFT_FOOT_ID, 764, 0, 400);
  sc.WritePos(RIGHT_FOOT_ID, 250, 0, 400);
}

