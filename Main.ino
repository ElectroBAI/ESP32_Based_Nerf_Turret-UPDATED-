#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// === Servo setup ===
Servo panServo;
Servo tiltServo;
const int PAN_SERVO_PIN = 19;    // Horizontal axis
const int TILT_SERVO_PIN = 18;   // Vertical axis

// === Motor pins ===
const int MOTOR1_A = 12;
const int MOTOR1_B = 13;
const int MOTOR2_A = 14;
const int MOTOR2_B = 27;

// === WiFi credentials ===
const char* ssid = "sim";
const char* password = "simple12";

// === Web server ===
WebServer server(80);

// === Motor & firing control ===
bool motorsRunning = false;
unsigned long motorStartTime = 0;
const unsigned long MOTOR_RUN_TIME = 2000;

bool continuousMode = false;
bool isFiring = false;
unsigned long lastFireTime = 0;
const unsigned long FIRE_INTERVAL = 500;
const unsigned long TRIGGER_PRESS_TIME = 200;
unsigned long triggerPressStartTime = 0;
bool triggerPressed = false;

void setup() {
  Serial.begin(115200);

  // === Servo setup ===
  panServo.attach(PAN_SERVO_PIN);
  tiltServo.attach(TILT_SERVO_PIN);
  panServo.write(90);
  tiltServo.write(90);

  // === Motor setup ===
  pinMode(MOTOR1_A, OUTPUT);
  pinMode(MOTOR1_B, OUTPUT);
  pinMode(MOTOR2_A, OUTPUT);
  pinMode(MOTOR2_B, OUTPUT);
  stopMotors();

  // === WiFi Access Point ===
  WiFi.softAP(ssid, password);
  Serial.println("Access Point started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // === Web routes ===
  server.on("/", handleRoot);
  server.on("/fire", handleFire);
  server.on("/continuous", handleContinuous);
  server.on("/stop", handleStop);
  server.on("/aim", handleAim);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  if (continuousMode && isFiring && millis() - lastFireTime > FIRE_INTERVAL) {
    fireSingleShot();
    lastFireTime = millis();
  }

  if (triggerPressed && millis() - triggerPressStartTime > TRIGGER_PRESS_TIME) {
    tiltServo.write(90);  // Reset trigger position
    triggerPressed = false;
  }

  if (!continuousMode && motorsRunning && millis() - motorStartTime > MOTOR_RUN_TIME) {
    stopMotors();
    motorsRunning = false;
    Serial.println("Motors stopped automatically");
  }
}

// === Web Handlers ===

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Nerf Turret</title><style>";
  html += "body { font-family: Arial; text-align: center; margin: 40px; }";
  html += "button, input[type=range] { padding: 10px; margin: 10px; font-size: 16px; width: 300px; }";
  html += "</style></head><body>";
  html += "<h1>ESP32 Nerf Turret</h1>";
  html += "<h3>Aim Controls</h3>";
  html += "Pan: <input type='range' min='0' max='180' onchange='send(this.value,\"pan\")'><br>";
  html += "Tilt: <input type='range' min='0' max='180' onchange='send(this.value,\"tilt\")'><br><br>";
  html += "<h3>Fire Controls</h3>";
  html += "<button onclick='fire()'>SINGLE FIRE</button><br>";
  html += "<button onclick='toggleContinuous()' id='continuousBtn'>START CONTINUOUS</button><br>";
  html += "<button onclick='stopFiring()'>STOP</button>";
  html += "<script>";
  html += "let continuousMode = false;";
  html += "function send(val, axis) { fetch(`/aim?axis=${axis}&value=${val}`); }";
  html += "function fire() { fetch('/fire'); }";
  html += "function toggleContinuous() { if (!continuousMode) { continuousMode = true; fetch('/continuous?mode=start'); document.getElementById('continuousBtn').textContent = 'STOP CONTINUOUS'; } else { continuousMode = false; fetch('/continuous?mode=stop'); document.getElementById('continuousBtn').textContent = 'START CONTINUOUS'; } }";
  html += "function stopFiring() { continuousMode = false; fetch('/stop'); document.getElementById('continuousBtn').textContent = 'START CONTINUOUS'; }";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleFire() {
  Serial.println("Single FIRE command received");
  runMotors();
  motorsRunning = true;
  motorStartTime = millis();
  fireSingleShot();
  server.send(200, "text/plain", "Fired!");
}

void handleContinuous() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    if (mode == "start") {
      Serial.println("Continuous mode started");
      continuousMode = true;
      isFiring = true;
      runMotors();
      motorsRunning = true;
      motorStartTime = millis();
      lastFireTime = millis();
      server.send(200, "text/plain", "Continuous firing started");
    } else {
      Serial.println("Continuous mode stopped");
      continuousMode = false;
      isFiring = false;
      stopMotors();
      motorsRunning = false;
      server.send(200, "text/plain", "Continuous firing stopped");
    }
  } else {
    server.send(400, "text/plain", "Missing mode parameter");
  }
}

void handleStop() {
  Serial.println("Emergency stop");
  continuousMode = false;
  isFiring = false;
  stopMotors();
  motorsRunning = false;
  tiltServo.write(90);
  triggerPressed = false;
  server.send(200, "text/plain", "STOPPED");
}

void handleAim() {
  if (server.hasArg("axis") && server.hasArg("value")) {
    String axis = server.arg("axis");
    int val = server.arg("value").toInt();
    val = constrain(val, 0, 180);
    if (axis == "pan") panServo.write(val);
    else if (axis == "tilt") tiltServo.write(val);
    server.send(200, "text/plain", "Moved");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

// === Utility Functions ===

void fireSingleShot() {
  tiltServo.write(0);  // Press trigger
  triggerPressed = true;
  triggerPressStartTime = millis();
  Serial.println("Dart fired");
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
