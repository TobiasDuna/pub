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

const int ESC_STOP_US    = 1000; // stopp
const int POST_BOOST_US  = 1065; // lavt nivå rett etter boost
const int ESC_TARGET_US  = 1135; // ønsket kjørehastighet

const int START_BOOST_US = 1250;                // startboost
const unsigned long START_BOOST_TIME_MS = 300;  // hvor lenge boost varer

const unsigned long ESC_ARM_TIME_MS          = 2000;
const unsigned long DROP_AFTER_BOOST_TIME_MS = 150;   // rask ned etter boost
const unsigned long RAMP_TO_TARGET_TIME_MS   = 5000;  // rolig opp til target
const unsigned long RAMP_DOWN_NORMAL_MS      = 6000;
const unsigned long RAMP_DOWN_FAST_MS        = 1000;

// =========================
// TID / ENCODER
// =========================
volatile int encoderDelta = 0;

unsigned long setSeconds = 60;
unsigned long remainingSeconds = 60;

const unsigned long MIN_SECONDS  = 5;
const unsigned long MAX_SECONDS  = 3600;
const unsigned long STEP_SECONDS = 5;

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
// TILSTANDSMASKIN
// =========================
enum State {
  IDLE,
  START_BOOST,
  DROP_AFTER_BOOST,
  RAMP_TO_TARGET,
  RUNNING,
  RAMP_DOWN_NORMAL,
  RAMP_DOWN_FAST
};

State state = IDLE;

int currentEscUs = ESC_STOP_US;
unsigned long rampStartTime = 0;
int rampFromUs = ESC_STOP_US;
int rampToUs = ESC_STOP_US;

// =========================
// TIDTAKING / KNAPP / DISPLAY
// =========================
unsigned long lastSecondTick = 0;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 30;

unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 250;

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
  currentEscUs = us;
  esc.writeMicroseconds(us);
}

void startRamp(int fromUs, int toUs) {
  rampFromUs = fromUs;
  rampToUs = toUs;
  rampStartTime = millis();
}

unsigned long getRampDuration(State s) {
  if (s == DROP_AFTER_BOOST) return DROP_AFTER_BOOST_TIME_MS;
  if (s == RAMP_TO_TARGET)   return RAMP_TO_TARGET_TIME_MS;
  if (s == RAMP_DOWN_NORMAL) return RAMP_DOWN_NORMAL_MS;
  if (s == RAMP_DOWN_FAST)   return RAMP_DOWN_FAST_MS;
  return 0;
}

void updateRamp() {
  unsigned long duration = getRampDuration(state);
  if (duration == 0) return;

  unsigned long elapsed = millis() - rampStartTime;

  if (elapsed >= duration) {
    writeEsc(rampToUs);

    if (state == DROP_AFTER_BOOST) {
      state = RAMP_TO_TARGET;
      startRamp(POST_BOOST_US, ESC_TARGET_US);
    } else if (state == RAMP_TO_TARGET) {
      state = RUNNING;
      lastSecondTick = millis();
    } else if (state == RAMP_DOWN_NORMAL || state == RAMP_DOWN_FAST) {
      state = IDLE;
      remainingSeconds = setSeconds;
    }
    return;
  }

  float frac = (float)elapsed / (float)duration;
  int value = rampFromUs + (int)((rampToUs - rampFromUs) * frac);
  writeEsc(value);
}

void beginRun() {
  remainingSeconds = setSeconds;
  state = START_BOOST;
  writeEsc(START_BOOST_US);
  rampStartTime = millis();
}

void beginNormalStop() {
  state = RAMP_DOWN_NORMAL;
  startRamp(currentEscUs, ESC_STOP_US);
}

void beginFastStop() {
  state = RAMP_DOWN_FAST;
  startRamp(currentEscUs, ESC_STOP_US);
}

void applyEncoderChange() {
  static int encoderAccum = 0;

  noInterrupts();
  int delta = encoderDelta;
  encoderDelta = 0;
  interrupts();

  encoderAccum += delta;

  while (encoderAccum >= 2) {
    if (state == IDLE) {
      if (setSeconds + STEP_SECONDS <= MAX_SECONDS) {
        setSeconds += STEP_SECONDS;
      } else {
        setSeconds = MAX_SECONDS;
      }
      remainingSeconds = setSeconds;
    }
    encoderAccum -= 2;
  }

  while (encoderAccum <= -2) {
    if (state == IDLE) {
      if (setSeconds >= MIN_SECONDS + STEP_SECONDS) {
        setSeconds -= STEP_SECONDS;
      } else {
        setSeconds = MIN_SECONDS;
      }
      remainingSeconds = setSeconds;
    }
    encoderAccum += 2;
  }
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

void updateCountdown() {
  if (state != RUNNING) return;

  unsigned long now = millis();

  if (now - lastSecondTick >= 1000) {
    lastSecondTick += 1000;

    if (remainingSeconds > 0) {
      remainingSeconds--;
    }

    if (remainingSeconds == 0) {
      beginNormalStop();
    }
  }
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

void formatTime(unsigned long totalSeconds, char *buffer, size_t len) {
  unsigned int minutes = totalSeconds / 60;
  unsigned int seconds = totalSeconds % 60;
  snprintf(buffer, len, "%02u:%02u", minutes, seconds);
}

const char* stateText() {
  switch (state) {
    case IDLE:              return "STOPPET";
    case START_BOOST:       return "STARTER";
    case DROP_AFTER_BOOST:  return "SENKER";
    case RAMP_TO_TARGET:    return "RAMPER OPP";
    case RUNNING:           return "KJORER";
    case RAMP_DOWN_NORMAL:  return "RAMPER NED";
    case RAMP_DOWN_FAST:    return "HURTIGSTOPP";
    default:                return "";
  }
}

void drawDisplay() {
  char timeBuf[10];
  unsigned long shownSeconds = (state == IDLE) ? setSeconds : remainingSeconds;
  formatTime(shownSeconds, timeBuf, sizeof(timeBuf));

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Sentrifuge");

  display.setCursor(0, 10);
  display.print("Status: ");
  display.print(stateText());

  display.setCursor(0, 20);
  display.print("RPM: ");
  display.print((int)(rpm + 0.5f));

  display.setTextSize(2);
  display.setCursor(0, 34);
  display.print(timeBuf);

  display.setTextSize(1);
  display.setCursor(0, 56);
  if (state == IDLE) {
    display.print("Roter=tid Trykk=start");
  } else {
    display.print("Trykk for rask stopp");
  }

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

  Serial.begin(115200);

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
  display.setCursor(0, 16);
  display.println("Hold rotor fri");
  display.display();

  delay(ESC_ARM_TIME_MS);

  state = IDLE;
  remainingSeconds = setSeconds;
  drawDisplay();
}

// =========================
// LOOP
// =========================
void loop() {
  applyEncoderChange();

  if (buttonPressed()) {
    if (state == IDLE) {
      beginRun();
    } else if (state == START_BOOST || state == DROP_AFTER_BOOST || state == RAMP_TO_TARGET || state == RUNNING || state == RAMP_DOWN_NORMAL) {
      beginFastStop();
    }
  }

  if (state == START_BOOST) {
    if (millis() - rampStartTime >= START_BOOST_TIME_MS) {
      state = DROP_AFTER_BOOST;
      startRamp(START_BOOST_US, POST_BOOST_US);
    }
  }

  updateRamp();
  updateCountdown();
  updateRPM();

  if (millis() - lastDisplayUpdate >= displayInterval) {
    lastDisplayUpdate = millis();
    drawDisplay();
  }
}