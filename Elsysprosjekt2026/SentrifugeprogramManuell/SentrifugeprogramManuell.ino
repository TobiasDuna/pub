#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

// =========================
// PINNER
// =========================
const int ENC_S2_PIN    = 2;
const int ENC_S1_PIN    = 3;
const int ENC_KEY_PIN   = 5;
const int HALL_PIN      = A0;
const int ESC_PIN       = 9;

// =========================
// OLED
// =========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =========================
// ESC
// =========================
Servo esc;

const int ESC_STOP_US = 1050;
const int ESC_MIN_US  = 1060;
const int ESC_MAX_US  = 1200;
const int ESC_STEP_US = 2;

int currentEscUs = ESC_STOP_US;

// =========================
// ENCODER
// =========================
volatile int encoderDelta = 0;

// =========================
// HALL / RPM (ANALOG A0)
// =========================
const int PULSES_PER_REV = 1;

const int HALL_THRESHOLD_LOW  = 300;
const int HALL_THRESHOLD_HIGH = 450;

bool pulseLatched = false;
unsigned long lastAcceptedPulseMicros = 0;
unsigned long pulsePeriodMicros = 0;
bool newPulseAvailable = false;

float rpmMeasured = 0.0f;
float rpm = 0.0f;
unsigned long lastRpmUpdateMs = 0;

const unsigned long MIN_PULSE_GAP_US = 8000;
const unsigned long RPM_TIMEOUT_MS   = 1500;

const float RPM_FILTER_ALPHA = 0.20f;

int hallAnalogValue = 0;

// =========================
// KNAPP
// =========================
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 30;

// =========================
// DISPLAY
// =========================
unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 200;

// =========================
// ENCODER INTERRUPT
// =========================
void handleEncoder() {
  if (digitalRead(ENC_S1_PIN) == digitalRead(ENC_S2_PIN)) {
    encoderDelta--;
  } else {
    encoderDelta++;
  }
}

// =========================
// HJELPEFUNKSJONER
// =========================
void writeEsc(int us) {
  if (us < ESC_STOP_US) us = ESC_STOP_US;
  if (us > ESC_MAX_US)  us = ESC_MAX_US;

  currentEscUs = us;
  esc.writeMicroseconds(us);
}

bool buttonPressed() {
  bool reading = digitalRead(ENC_KEY_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  bool pressedEvent = false;

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableButtonState) {
      stableButtonState = reading;
      if (stableButtonState == LOW) {
        pressedEvent = true;
      }
    }
  }

  lastButtonReading = reading;
  return pressedEvent;
}

void applyEncoderChange() {
  static int encoderAccum = 0;

  noInterrupts();
  int delta = encoderDelta;
  encoderDelta = 0;
  interrupts();

  encoderAccum += delta;

  while (encoderAccum >= 2) {
    if (currentEscUs <= ESC_STOP_US) {
      currentEscUs = ESC_MIN_US;
    } else {
      currentEscUs += ESC_STEP_US;
    }

    if (currentEscUs > ESC_MAX_US) currentEscUs = ESC_MAX_US;
    encoderAccum -= 2;
  }

  while (encoderAccum <= -2) {
    if (currentEscUs > ESC_MIN_US) {
      currentEscUs -= ESC_STEP_US;
      if (currentEscUs < ESC_MIN_US) currentEscUs = ESC_MIN_US;
    } else {
      currentEscUs = ESC_STOP_US;
    }

    encoderAccum += 2;
  }

  writeEsc(currentEscUs);
}

void updateHallSensor() {
  hallAnalogValue = analogRead(HALL_PIN);
  unsigned long now = micros();

  if (!pulseLatched && hallAnalogValue < HALL_THRESHOLD_LOW) {
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

    if (rpm <= 0.1f) {
      rpm = rpmMeasured;
    } else {
      rpm = RPM_FILTER_ALPHA * rpmMeasured + (1.0f - RPM_FILTER_ALPHA) * rpm;
    }
  }

  if (millis() - lastRpmUpdateMs > RPM_TIMEOUT_MS) {
    rpmMeasured = 0.0f;
    rpm = 0.0f;
  }
}

void drawDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Manuel modus");

  display.setCursor(0, 14);
  display.print("RPM: ");
  display.print((int)(rpm + 0.5f));

  display.setCursor(0, 26);
  display.print("us: ");
  display.print(currentEscUs);

  display.setCursor(0, 38);
  display.print("Status: ");
  if (currentEscUs <= ESC_STOP_US) {
    display.print("STOPP");
  } else {
    display.print("KJORER");
  }

  display.setCursor(0, 56);
  display.print("Trykk=stopp");

  display.display();
}

// =========================
// SETUP
// =========================
void setup() {
  pinMode(ENC_S1_PIN, INPUT_PULLUP);
  pinMode(ENC_S2_PIN, INPUT_PULLUP);
  pinMode(ENC_KEY_PIN, INPUT_PULLUP);
  pinMode(HALL_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(ENC_S1_PIN), handleEncoder, CHANGE);

  esc.attach(ESC_PIN);
  writeEsc(ESC_STOP_US);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    while (true) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Armerer ESC...");
  display.display();

  delay(2000);
  drawDisplay();
}

// =========================
// LOOP
// =========================
void loop() {
  applyEncoderChange();

  if (buttonPressed()) {
    writeEsc(ESC_STOP_US);
  }

  updateRPM();

  if (millis() - lastDisplayUpdate >= displayInterval) {
    lastDisplayUpdate = millis();
    drawDisplay();
  }
}