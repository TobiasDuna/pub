// Biblioteker i bruk
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
// Initialisere I2C
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);

float starttid = 0;
float øltemp = 0;
float lufttemp = 0;

void setup() {
  Serial.begin(115200);
  pinMode(Knapp, INPUT_PULLUP);
  // Start DS18B20, DHT11 og OLED
  ds18b20.begin();
  dht.begin();

  // Start OLED og sjekk om den er tilkoblet
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED-feil"));
    while (true);
  }
  display.clearDisplay();
}

void loop() {
  if (digitalRead(Knapp) == LOW) {    //Vent på aktivering med knapp
    starttid = millis() / 60000;
    while (true) {
      // Les temperaturer
      ds18b20.requestTemperatures();
      øltemp = ds18b20.getTempCByIndex(0);
      lufttemp = dht.readTemperature() - 1.0;

      // Print til seriell
      Serial.print((millis() / 60000) - starttid);
      Serial.print(",");
      Serial.print(øltemp);
      Serial.print(",");
      Serial.println(lufttemp);

      // Print til OLED
      display.clearDisplay();
      display.setTextSize(1); 
      display.setTextColor(SSD1306_WHITE);

      
      display.setCursor(0, 5);
      display.print("Beer");
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.print(øltemp);
      display.print("C");

      display.setTextSize(1);
      display.setCursor(80, 5);
      display.print("Air");
      display.setTextSize(1);
      display.setCursor(80, 20);
      display.print(lufttemp);
      display.print("C");

      display.display();
      delay(60000);
    }
  }
}
  