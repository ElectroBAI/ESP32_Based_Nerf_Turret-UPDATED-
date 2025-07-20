#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// === Servo Setup ===
Servo panServo;
Servo tiltServo;
const int PAN_SERVO_PIN = 19;
const int TILT_SERVO_PIN = 18;

// === Motor Pins ===
const int MOTOR1_A = 12;
const int MOTOR1_B = 13;
const int MOTOR2_A = 14;
const int MOTOR2_B = 27;

// === WiFi Credentials ===
const char* ssid = "sim";
const char* password = "simple12";

// === Web Server ===
WebServer server(80);

// === Firing Control ===
bool motorsRunning = false;
bool continuousMode = false;
bool isFiring = false;
bool triggerPressed = false;
unsigned long motorStartTime = 0;
unsigned long lastFireTime = 0;
unsigned long triggerPressStartTime = 0;
const unsigned long MOTOR_RUN_TIME = 2000;
const unsigned long FIRE_INTERVAL = 500;
const unsigned long TRIGGER_PRESS_TIME = 200;

// === Optimized Web Page (Stored in Flash) ===
const char MAIN_page[] PROGMEM = R"====(
<!DOCTYPE html><html>
<head><title>ESP32 Nerf Turret</title>
<style>
body { font-family: Arial; text-align: center; margin: 40px; }
button, input[type=range] { padding: 10px; margin: 10px; font-size: 16px; width: 300px; }
</style>
</head><body>
<h2>Dual Axis Nerf Turret</h2>
<p>Pan: <input type='range' min='0' max='180' onchange='send(this.value,"pan")'></p>
<p>Tilt: <input type='range' min='0' max='180' onchange='send(this.value,"tilt")'></p>
<button onclick='fire()'>SINGLE FIRE</button><br>
<button onclick='toggleContinuous()' id='continuousBtn'>START CONTINUOUS</button><br>
<button onclick='stopFiring()'>STOP</button>
<script>
let continuousMode = false;
let sendTimeout;
function send(val, axis) {
  clearTimeout(sendTimeout);
  sendTimeout = setTimeout(() => {
    fetch(`/aim?axis=${axis}&value=${val}`);
  }, 100);
}
function fire() { fetch('/fire'); }
function toggleContinuous() {
  if (!continuousMode) {
    continuousMode = true;
    fetch('/continuous?mode=start');
    document.getElementById('continuousBtn').textContent = 'STOP CONTINUOUS';
  } else {
    continuousMode = false;
    fetch('/continuous?mode=stop');
    document.getElementById('continuousBtn').textContent = 'START CONTINUOUS';
  }
}
function stopFiring() {
  continuousMode = false;
  fetch('/stop');
  document.getElementById('continuousBtn').textContent = 'START CONTINUOUS';
}
</script></body></html>
)====";

void setup() {
  Serial.begin(115200);
  panServo.attach(PAN_SERVO_PIN);
  tiltServo.attach(TILT_SERVO_PIN);
  panServo.write(90);
  tiltServo.write(90);

  pinMode(MOTOR1_A, OUTPUT);
  pinMode(MOTOR1_B, OUTPUT);
  pinMode(MOTOR2_A, OUTPUT);
  pinMode(MOTOR2_B, OUTPUT);
  stopMotors();

  WiFi.softAP(ssid, password);
  Serial.println("Access Point started");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send_P(200, "text/html", MAIN_page); });
  server.on("/fire", handleFire);
  server.on("/continuous", handleContinuous);
  server.on("/stop", handleStop);
  server.on("/aim", handleAim);
  server.begin();
}

void loop() {
  server.handleClient();
  updateTrigger();
  updateMotorTimeout();
  updateContinuousFire();
}

// === Web Handlers ===

void handleFire() {
  runMotors();
  motorsRunning = true;
  motorStartTime = millis();
  fireSingleShot();
  server.send(200, "text/plain", "Fired!");
}

void handleContinuous() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    continuousMode = (mode == "start");
    isFiring = continuousMode;

    if (continuousMode) {
      runMotors();
      motorsRunning = true;
      motorStartTime = millis();
      lastFireTime = millis();
    } else {
      stopMotors();
      motorsRunning = false;
    }
    server.send(200, "text/plain", continuousMode ? "Continuous ON" : "Continuous OFF");
  } else {
    server.send(400, "text/plain", "Missing mode");
  }
}

void handleStop() {
  continuousMode = false;
  isFiring = false;
  triggerPressed = false;
  stopMotors();
  motorsRunning = false;
  tiltServo.write(90);
  server.send(200, "text/plain", "STOPPED");
}

void handleAim() {
  if (server.hasArg("axis") && server.hasArg("value")) {
    int val = constrain(server.arg("value").toInt(), 0, 180);
    if (server.arg("axis") == "pan") panServo.write(val);
    else tiltServo.write(val);
    server.send(200, "text/plain", "Moved");
  } else {
    server.send(400, "text/plain", "Missing args");
  }
}

// === Utility Functions ===

void fireSingleShot() {
  tiltServo.write(0);
  triggerPressed = true;
  triggerPressStartTime = millis();
  Serial.println("Dart fired");
}

void updateTrigger() {
  if (triggerPressed && millis() - triggerPressStartTime > TRIGGER_PRESS_TIME) {
    tiltServo.write(90);
    triggerPressed = false;
  }
}

void updateMotorTimeout() {
  if (!continuousMode && motorsRunning && millis() - motorStartTime > MOTOR_RUN_TIME) {
    stopMotors();
    motorsRunning = false;
  }
}

void updateContinuousFire() {
  if (continuousMode && isFiring && millis() - lastFireTime > FIRE_INTERVAL) {
    fireSingleShot();
    lastFireTime = millis();
  }
}

void runMotors() {
  digitalWrite(MOTOR1_A, HIGH);
  digitalWrite(MOTOR1_B, LOW);
  digitalWrite(MOTOR2_A, HIGH);
  digitalWrite(MOTOR2_B, LOW);
  Serial.println("Motors spinning");
}

void stopMotors() {
  digitalWrite(MOTOR1_A, LOW);
  digitalWrite(MOTOR1_B, LOW);
  digitalWrite(MOTOR2_A, LOW);
  digitalWrite(MOTOR2_B, LOW);
  Serial.println("Motors stopped");
}
