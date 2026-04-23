#include <Servo.h>

// =========================
// PINNER
// =========================
const int ESC_PIN  = 9;
const int HALL_PIN = A0;

// =========================
// ESC
// =========================
Servo esc;

const int ESC_STOP_US = 1050;
const int ESC_MIN_US  = 1060;
const int ESC_MAX_US  = 1200;

int escCommandUs = ESC_STOP_US;

// =========================
// HALL / RPM
// =========================
const int PULSES_PER_REV = 1;

// Juster disse etter rå A0-plot
const int HALL_THRESHOLD_LOW  = 300;
const int HALL_THRESHOLD_HIGH = 320;

// Pulsdeteksjon
bool pulseLatched = false;
unsigned long lastAcceptedPulseMicros = 0;
unsigned long pulsePeriodMicros = 0;
bool newPulseAvailable = false;

// Minste tillatte tid mellom to ekte pulser
// Velg ut fra høyeste realistiske RPM.
// 4000 RPM => 60e6 / 4000 = 15000 us ved 1 puls/rev
const unsigned long MIN_PULSE_GAP_US = 8000;

float rpmMeasured = 0.0;
float rpmFiltered = 0.0;
unsigned long lastRpmUpdateMs = 0;
const unsigned long RPM_TIMEOUT_MS = 1000;

const float RPM_FILTER_ALPHA = 0.20f;

// Debug
int hallAnalogValue = 0;

// =========================
// REGULATOR
// =========================
float rpmSetpoint = 0.0f;

// Start mye snillere
float Kp = 0.005f;
float Ki = 0.01f;

float integralTerm = 0.0f;

const unsigned long CONTROL_INTERVAL_MS = 50;
unsigned long lastControlMs = 0;

const float INTEGRAL_MIN = -2000.0f;
const float INTEGRAL_MAX =  2000.0f;

// =========================
// SERIAL
// =========================
String inputBuffer;

// =========================
// HJELPEFUNKSJONER
// =========================
void writeEsc(int us) {
  if (us < ESC_STOP_US) us = ESC_STOP_US;
  if (us > ESC_MAX_US)  us = ESC_MAX_US;

  escCommandUs = us;
  esc.writeMicroseconds(us);
}

void stopMotor() {
  rpmSetpoint = 0.0f;
  integralTerm = 0.0f;
  writeEsc(ESC_STOP_US);
}

void updateHallSensor() {
  hallAnalogValue = analogRead(HALL_PIN);
  unsigned long now = micros();

  // Trigger når signalet går under lav terskel
  if (!pulseLatched && hallAnalogValue < HALL_THRESHOLD_LOW) {
    // Aksepter bare hvis det er lenge nok siden forrige godkjente puls
    if (lastAcceptedPulseMicros != 0) {
      unsigned long dt = now - lastAcceptedPulseMicros;

      if (dt >= MIN_PULSE_GAP_US) {
        pulsePeriodMicros = dt;
        newPulseAvailable = true;
        lastAcceptedPulseMicros = now;
      }
    } else {
      lastAcceptedPulseMicros = now;
    }

    pulseLatched = true;
  }

  // Rearm først når signalet har gått godt tilbake
  if (pulseLatched && hallAnalogValue > HALL_THRESHOLD_HIGH) {
    pulseLatched = false;
  }
}

void updateRPM() {
  updateHallSensor();

  if (newPulseAvailable && pulsePeriodMicros > 0) {
    unsigned long localPeriod = pulsePeriodMicros;
    newPulseAvailable = false;

    float pulseFreq = 1000000.0f / (float)localPeriod;
    rpmMeasured = (pulseFreq * 60.0f) / (float)PULSES_PER_REV;
    lastRpmUpdateMs = millis();

    if (rpmFiltered <= 0.1f) {
      rpmFiltered = rpmMeasured;
    } else {
      rpmFiltered = RPM_FILTER_ALPHA * rpmMeasured +
                    (1.0f - RPM_FILTER_ALPHA) * rpmFiltered;
    }
  }

  if (millis() - lastRpmUpdateMs > RPM_TIMEOUT_MS) {
    rpmMeasured = 0.0f;
    rpmFiltered = 0.0f;
  }
}

void updateController() {
  unsigned long now = millis();
  if (now - lastControlMs < CONTROL_INTERVAL_MS) return;

  float dt = (now - lastControlMs) / 1000.0f;
  lastControlMs = now;

  if (rpmSetpoint <= 0.0f) {
    integralTerm = 0.0f;
    writeEsc(ESC_STOP_US);
    return;
  }

  float error = rpmSetpoint - rpmFiltered;

  // PI uten å la integratoren løpe helt ukritisk
  float proposedIntegral = integralTerm + error * dt;
  if (proposedIntegral > INTEGRAL_MAX) proposedIntegral = INTEGRAL_MAX;
  if (proposedIntegral < INTEGRAL_MIN) proposedIntegral = INTEGRAL_MIN;

  float controlP = Kp * error;
  float controlI = Ki * proposedIntegral;

  int baseUs = ESC_MIN_US;
  int unsatCommand = (int)(baseUs + controlP + controlI + 0.5f);

  // Anti-windup: bare godta integratoroppdatering hvis vi ikke presser lenger inn i metning
  bool saturatingHigh = (unsatCommand > ESC_MAX_US);
  bool saturatingLow  = (unsatCommand < ESC_MIN_US);

  if (!(saturatingHigh && error > 0) && !(saturatingLow && error < 0)) {
    integralTerm = proposedIntegral;
  }

  int command = (int)(baseUs + Kp * error + Ki * integralTerm + 0.5f);

  if (command < ESC_MIN_US) command = ESC_MIN_US;
  if (command > ESC_MAX_US) command = ESC_MAX_US;

  writeEsc(command);
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        float value = inputBuffer.toFloat();
        rpmSetpoint = value;

        if (rpmSetpoint <= 0.0f) {
          stopMotor();
        } else {
          if (escCommandUs < ESC_MIN_US) {
            writeEsc(ESC_MIN_US);
          }
        }

        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}

void printPlot() {
  static unsigned long lastPrintMs = 0;
  unsigned long now = millis();
  if (now - lastPrintMs < 50) return;
  lastPrintMs = now;

  float escScaled = (escCommandUs - ESC_STOP_US) * 40.0f;

  // Plot:
  // 1: setpoint
  // 2: rpmFiltered
  // 3: escScaled
  // 4: raw A0
  Serial.print(rpmSetpoint);
  Serial.print(" ");
  Serial.print(rpmFiltered);
  Serial.print(" ");
  Serial.print(escScaled);
  Serial.print(" ");
  Serial.println(hallAnalogValue);
}

// =========================
// SETUP / LOOP
// =========================
void setup() {
  Serial.begin(115200);
  pinMode(HALL_PIN, INPUT);

  esc.attach(ESC_PIN);
  writeEsc(ESC_STOP_US);

  delay(3000);

  lastControlMs = millis();
}

void loop() {
  handleSerial();
  updateRPM();
  updateController();
  printPlot();
}