/*  Smart plant watering system

    This sketch is written to program the ESP32 in such a way that is checks the moisture content of soil and turns the water pump acvordingly.
    The whole system goes off when water in the tank runs out so as to save the power , once the water is filled back,the system starts back.
    Parameters like temperature,humidity,soil moisture,water level ,and states of pump all are displayed on LCD screen and can also be accessed from 
    mobile phone or PC 
    Behavior:
      -Soil moisture is measured in VWC(Volumetric Water Content) which is in layman's terms, volume of water in cubic centimeter.
      -When soil moisture sensor reads 300 VWC and less,the water pump turns on watering the soil, once the soil moisture reaches 
        600 and above, the relay turns off indicating the soil is moist enough.
      -Water level sensor reads the water level in tank and when it reads
        below 200,the system turns off to save power
      - Parameters like temperature,humidity,soil moisture,water level ,and states of pump all are displayed on LCD screen and can also be accessed from 
        mobile phone or PC via blynk cloud platform

*/
#define BLYNK_PRINT Serial

// authorization token to connect ESP32 to blynk cloud via WiFi
#define BLYNK_TEMPLATE_ID "TMPL35VZPkrUz"
#define BLYNK_TEMPLATE_NAME "Smart plant watering system"
#define BLYNK_AUTH_TOKEN "gGABXd_P12KLdMMHAVJ8_JTullNJo_eK"

//include all necessary libraries
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

//define WiFi credentials
char ssid[] = "Adil";
char pass[] = "123456";

// --- PIN MAP ---
const int PIN_DHT = 26;        // DHT11 data
const int PIN_WATER = 35;      // water level sensor (analog)
const int PIN_SOIL_A = 32;     // soil sensor 1 (analog)
const int PIN_SOIL_B = 33;     // soil sensor 2 (analog)
const int PIN_SOIL_C = 34;     // soil sensor 3 (analog)

// Relay outputs
const int RELAY_1 = 19;
const int RELAY_2 = 13;
const int RELAY_3 = 14;

//Push Button
const int PIN_BUTTON = 25;     // cycles the display, INPUT_PULLUP

// --- SENSORS / THRESHOLDS ---
#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- REVERSE LOGIC SENSOR THRESHOLDS (VWC style) ---
// Dry ≈ 300 VWC, Wet ≈ 600 VWC
const int SOIL_DRY_VWC = 300;  // Pump ON when below this (dry)
const int SOIL_WET_VWC = 600;  // Pump OFF when above this (wet)

// Water empty threshold
const int WATER_EMPTY_THRESHOLD_ADC = 200;

// Button debounce
unsigned long lastButtonMs = 0;
const unsigned long DEBOUNCE_MS = 15;
int lastButtonState = HIGH;
int page = 0;
const int TOTAL_PAGES = 3;

// Hysteresis states
bool pumpState1 = false;
bool pumpState2 = false;
bool pumpState3 = false;

String onOff(bool v) { return v ? "ON " : "OFF"; }

// --- Convert ADC → VWC (linear calibration between 300 and 600 for 12-bit ADC) ---
int adcToVWC(int adc) {
  // Reverse logic: low ADC = wet, high ADC = dry
  // Map 4095 → 0 VWC (dryest) and 0 → 1000 VWC (wettest) just for representation
  int vwc = map(adc, 4095, 0, 0, 1000);
  if (vwc < 0) vwc = 0;
  if (vwc > 1000) vwc = 1000;
  return vwc;
}

void setup() {
  Serial.begin(9600); 
  delay(1000);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
 

  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(RELAY_3, OUTPUT);
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);
  digitalWrite(RELAY_3, LOW);

  pinMode(25, INPUT_PULLUP);
  analogReadResolution(12); // 0-4095

  dht.begin();
  Wire.begin();

  lcd.init();
  lcd.backlight();

  Serial.println("System started (Reverse Logic + VWC Display).");
  Serial.print("SOIL_DRY_VWC (pump ON)  = "); Serial.println(SOIL_DRY_VWC);
  Serial.print("SOIL_WET_VWC (pump OFF) = "); Serial.println(SOIL_WET_VWC);
  Serial.print("WATER_EMPTY_THRESHOLD_ADC = "); Serial.println(WATER_EMPTY_THRESHOLD_ADC);

}

void loop() {
  
  checkButton();

  int waterADC = analogRead(PIN_WATER);
  bool waterEmpty = (waterADC < WATER_EMPTY_THRESHOLD_ADC);

  int soil1ADC = analogRead(PIN_SOIL_A);
  int soil2ADC = analogRead(PIN_SOIL_B);
  int soil3ADC = analogRead(PIN_SOIL_C);

  // Convert to VWC
  int soil1VWC = adcToVWC(soil1ADC);
  int soil2VWC = adcToVWC(soil2ADC);
  int soil3VWC = adcToVWC(soil3ADC);

  static unsigned long lastDHT = 0;
  static float lastTemp = NAN, lastHum = NAN;
  if (millis() - lastDHT > 2000) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h)) lastHum = h;
    if (!isnan(t)) lastTemp = t;
    lastDHT = millis();
  }

  // --- Pump Logic (using VWC thresholds) ---
  if (waterEmpty) {
    pumpState1 = pumpState2 = pumpState3 = false;
  } else {
    if (!pumpState1 && soil1VWC <= SOIL_DRY_VWC) pumpState1 = true;     // Dry -> ON
    else if (pumpState1 && soil1VWC >= SOIL_WET_VWC) pumpState1 = false; // Wet -> OFF

    if (!pumpState2 && soil2VWC <= SOIL_DRY_VWC) pumpState2 = true;
    else if (pumpState2 && soil2VWC >= SOIL_WET_VWC) pumpState2 = false;

    if (!pumpState3 && soil3VWC <= SOIL_DRY_VWC) pumpState3 = true;
    else if (pumpState3 && soil3VWC >= SOIL_WET_VWC) pumpState3 = false;

    Blynk.run();
    checkButton();
  }

  // Relay Active LOW
  digitalWrite(RELAY_1, pumpState1 ? LOW : HIGH);
  digitalWrite(RELAY_2, pumpState2 ? LOW : HIGH);
  digitalWrite(RELAY_3, pumpState3 ? LOW : HIGH);

  displayPage(page, lastTemp, lastHum, waterADC, waterEmpty,
              soil1VWC, soil2VWC, soil3VWC);

  // --- Serial Debug ---
  Serial.print("W:"); Serial.print(waterADC);
  Serial.print(" | S1:"); Serial.print(soil1VWC);
  Serial.print(" R1:"); Serial.print(onOff(pumpState1));
  Serial.print(" | S2:"); Serial.print(soil2VWC);
  Serial.print(" R2:"); Serial.print(onOff(pumpState2));
  Serial.print(" | S3:"); Serial.print(soil3VWC);
  Serial.print(" R3:"); Serial.print(onOff(pumpState3));
  Serial.print(" | DHT T:"); Serial.print(isnan(lastTemp) ? 0 : lastTemp);
  Serial.print(" H:"); Serial.println(isnan(lastHum) ? 0 : lastHum);

  Blynk.virtualWrite(V0,isnan(lastTemp) ? 0 : lastTemp);
  Blynk.virtualWrite(V1,isnan(lastHum) ? 0 : lastHum);
  Blynk.virtualWrite(V2,waterADC);
  Blynk.virtualWrite(V3,soil1VWC);
  Blynk.virtualWrite(V4,soil2VWC);
  Blynk.virtualWrite(V5,soil3VWC);
  Blynk.virtualWrite(V6,pumpState1);
  Blynk.virtualWrite(V7,pumpState2);
  Blynk.virtualWrite(V8,pumpState3);


  delay(350);
}

// --- Button handler with debounce ---
  void checkButton() {
int reading = digitalRead(25);   // your button pin

  // Detect falling edge (HIGH → LOW)
  if (reading != lastButtonState) {
    lastButtonMs = millis();  // reset debounce timer
  }

  if ((millis() - lastButtonMs) > DEBOUNCE_MS) {
    
    // If stable LOW → button pressed
    if (reading == LOW) {
      page++;
      if (page >= TOTAL_PAGES) page = 0;

      Serial.print("Page changed to: ");
      Serial.println(page);

      delay(150);  // small delay to avoid double scroll
    }
  }

  lastButtonState=reading;
}

 
// --- Display Pages ---
void displayPage(int p, float temp, float hum, int waterADC, bool waterEmpty,
                 int s1VWC, int s2VWC, int s3VWC) {
  lcd.clear();

  switch (p) {
    case 0:
      lcd.setCursor(0,0);
      if (isnan(temp)) lcd.print("Temperature: --C ");
      else { lcd.print("Temperature:"); lcd.print(temp, 1); lcd.print("C "); }
      lcd.setCursor(0,2);
      if (isnan(hum)) lcd.print("Humidity: --%");
      else { lcd.print("Humidity:"); lcd.print(hum, 0); lcd.print("%"); }
      break;


    case 1:
      lcd.setCursor(0,0);
      lcd.print("Water Tank:");
      lcd.print(waterEmpty ? "EMPTY" : "FULL   ");
      lcd.setCursor(0,2);
      lcd.print("Water Level:"); lcd.print(waterADC);
      break;


    case 2:
      lcd.setCursor(0,0);
      lcd.print("Field 1:"); lcd.print(s1VWC); lcd.print(" vwc "); lcd.print(onOff(pumpState1));
      lcd.setCursor(0,1);
      lcd.print("Field 2:"); lcd.print(s2VWC); lcd.print(" vwc "); lcd.print(onOff(pumpState2));
      lcd.setCursor(0,2);
      lcd.print("Field 3:"); lcd.print(s3VWC); lcd.print(" vwc "); lcd.print(onOff(pumpState3));
      break;

    default:
      lcd.setCursor(0,0);
      lcd.print("Page error");
      break;


  }
}