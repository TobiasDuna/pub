#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <DHT.h>

// Sensorer
#define DS18B20_PIN 26
#define DHT_PIN 25
#define DHT_TYPE DHT11

// Knapp
#define Knapp 14

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// LED
#define LED_GRONN 16
#define LED_GUL 17
#define LED_BLA 18

// Initialisere I2C
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);

// Variabler for temperaturer og tid
float starttid = 0;
float øltemp = 0;
float lufttemp = 0;
float targetTemp = 14.0;  // Grense for temperatur for drikken
float alpha = 0;
float måleintervall = 30;  // sekunder
float første_øltemp = 0;  // Første målte temperatur

void setup() {
  Serial.begin(115200);
  pinMode(Knapp, INPUT_PULLUP);

  // Start DS18B20, DHT11 og OLED
  ds18b20.begin();
  dht.begin();

  // Start OLED og sjekk om den er tilkoblet
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED-feil"));
    while (true)
      ;
  }
  display.clearDisplay();

  // Sett opp LED
  pinMode(LED_GRONN, OUTPUT);
  pinMode(LED_GUL, OUTPUT);
  pinMode(LED_BLA, OUTPUT);
}

// Funksjon for å beregne tiden til øltemperaturen når targetTemp
float beregnTidTilMålTemp(float T_0, float T_k, float T_mål, float alpha) {
  return -log((T_mål - T_k) / (T_0 - T_k)) / alpha;
}

void loop() {
  if (digitalRead(Knapp) == LOW) {  // Vent på aktivering med knapp
    starttid = millis() / 60000.0;  // Starttid i minutter

    // Ta første temperaturmåling for å sette initial øltemp
    ds18b20.requestTemperatures();
    første_øltemp = ds18b20.getTempCByIndex(0);  // Lagre første måling som T_0

    while (true) {
      // Les temperaturer
      ds18b20.requestTemperatures();
      øltemp = ds18b20.getTempCByIndex(0);      // Nyeste måling av øltemperatur
      lufttemp = dht.readTemperature() - 0.75;  // Oppdater lufttemperaturen kontinuerlig

      // Sjekk om temperaturen vil stige eller synke
      if ((første_øltemp - lufttemp) != 0) {
        // Beregn alpha kontinuerlig basert på første og nyeste måling
        alpha = -log(fabs((øltemp - lufttemp) / (første_øltemp - lufttemp))) / ((millis() / 60000.0) - starttid);
      }

      // Beregn forventet tid til måltemperatur med kontinuerlig alpha
      float tidTilMålTemp = beregnTidTilMålTemp(øltemp, lufttemp, targetTemp, alpha);  // Tid i minutter

      // Print til seriell for logging
      Serial.print((millis() / 60000.0) - starttid);
      Serial.print(',');
      Serial.print(øltemp);
      Serial.print(',');
      Serial.println(lufttemp);

      // Print til OLED
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      display.setCursor(0, 5);
      display.print("Beer");
      display.setCursor(0, 20);
      display.print(øltemp);
      display.print("C");

      display.setCursor(80, 5);
      display.print("Air");
      display.setCursor(80, 20);
      display.print(lufttemp);
      display.print("C");

      // Vis estimert tid til måltemperatur
      display.setCursor(0, 40);
      display.print("Tid til ");
      display.print(targetTemp);
      display.print("C: ");
      display.setCursor(0, 50);
      display.print(tidTilMålTemp);
      display.print(" min");

      display.display();

      // Kontroller LED basert på tid til måltemperatur
      if (tidTilMålTemp > 10) {
        digitalWrite(LED_GRONN, HIGH);
        digitalWrite(LED_GUL, LOW);
        digitalWrite(LED_BLA, LOW);
      } else if (tidTilMålTemp > 5) {
        digitalWrite(LED_GRONN, LOW);
        digitalWrite(LED_GUL, HIGH);
        digitalWrite(LED_BLA, LOW);
      } else {
        digitalWrite(LED_GRONN, LOW);
        digitalWrite(LED_GUL, LOW);
        digitalWrite(LED_BLA, HIGH);
      }

      delay(måleintervall * 1000);  // Vent mellom målingene
    }
  }
}
