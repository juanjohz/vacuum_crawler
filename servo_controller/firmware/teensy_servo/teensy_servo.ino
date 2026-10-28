#include <Arduino.h>
#include <PWMServo.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

constexpr int SERVO_PIN = 4;
constexpr float MIN_ANGLE = 60.0f;
constexpr float MAX_ANGLE = 150.0f;
constexpr unsigned long WATCHDOG_MS = 500;
PWMServo cameraServo;
float angle = 149.0f; // Original known startup position; applied on first SPEED command.
float speed = 0.0f;
int lastAngle = -1;
bool enabled = false;
char line[40];
size_t used = 0;
bool overflow = false;
unsigned long lastCommand = 0;
unsigned long lastUpdate = 0;

void handleLine() {
  if (strcmp(line, "HELLO") == 0) {
    speed = 0;
    Serial1.println("VACUUM_SERVO_UART_V2");
  } else if (strcmp(line, "STOP") == 0) {
    speed = 0; // Keep holding the current camera angle.
  } else if (strncmp(line, "SPEED ", 6) == 0) {
    char *end;
    float requested = strtof(line + 6, &end);
    if (end == line + 6 || *end != '\0' || !isfinite(requested) || fabsf(requested) > 90) {
      speed = 0;
      return;
    }
    if (!enabled) {
      cameraServo.attach(SERVO_PIN);
      cameraServo.write(lroundf(angle));
      enabled = true;
    }
    speed = requested;
    lastCommand = millis();
  } else {
    speed = 0;
  }
}

void setup() {
  Serial.begin(115200); // USB debug/flashing connection, independent of Serial1.
  Serial1.begin(115200); // Teensy pin 0 RX1, pin 1 TX1.
  lastUpdate = millis();
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastUpdate;
  if (now - lastCommand >= WATCHDOG_MS) speed = 0;
  if (elapsed >= 20) {
    // Cap elapsed time so a stalled loop cannot produce a large jump.
    float dt = (elapsed > 50 ? 50 : elapsed) / 1000.0f;
    lastUpdate = now;
    angle = constrain(angle + speed * dt, MIN_ANGLE, MAX_ANGLE);
    int output = lroundf(angle);
    if (enabled && output != lastAngle) {
      cameraServo.write(output);
      lastAngle = output;
    }
  }
  for (int budget = 0; budget < 64 && Serial1.available(); ++budget) {
    char c = Serial1.read();
    if (c == '\n') {
      line[used] = '\0';
      if (!overflow) handleLine();
      else speed = 0;
      used = 0;
      overflow = false;
    } else if (c != '\r') {
      if (used < sizeof(line) - 1) line[used++] = c;
      else overflow = true;
    }
  }
}
