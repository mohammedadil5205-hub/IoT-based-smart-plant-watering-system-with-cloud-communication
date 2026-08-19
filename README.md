# IoT-based-smart-plant-watering-system-with-cloud-communication
This project involves development of ESP32 bsed smart and safe plant watering system and data communication through BlynkIoT app.

-------------------------------------------------------------------------------------
PROJECT TITLE: Development of an ESP32 Smart and Safe Outdoor Plant Watering system
-------------------------------------------------------------------------------------

NOTE: This project was done under the guidance of faculty in our college in cooperation with 3 team members, thank you everyone for their support.

------------------------------------------------------------
PROJECT DESCRIPTION
------------------------------------------------------------
This is an IoT-based Smart Agriculture system designed to automate irrigation 
for three separate plant fields. It uses an ESP32 microcontroller to monitor 
real-time data.

Key Features:
1. Multi-Field Monitoring: Monitors soil moisture for 3 distinct zones.
2. Environmental Sensing: Tracks Temperature and Humidity using DHT11.
3. Water Tank Safety: Checks water tank level; prevents pump operation if the tank is empty.
4. Interface: Displays data locally on a 20x4 LCD (switchable pages via button) 
   and remotely via the Blynk IoT Cloud App.

------------------------------------------------------------
CONTENTS (File Structure)
-----------------------------------------------------------

1. \Datasheets
   - push button.pdf
   - water sensor.pdf
   - dc-mini-submersible-water-pump.pdf
   - DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf
   - ESP-WROOM-32.pdf
   - I2C_1602_LCD.pdf
   - 204lcd.pdf
   - Relay-Module-Datasheet.pdf
   - Soil Moisture Sensor.pdf

2. \Code
   - Smart_Watering_System.ino (Main Arduino Sketch)
   - Smart_Watering_System.txt (txt file)

3. \Image gallery
    -circuit diagram.png
    -block diagram.png
    -flow chart.png
    -final model.png
    -blynkIoT setup.png
   
4. \report
    -project report.txt

------------------------------------------------------------
HARDWARE CONNECTIONS (Pin Map)
------------------------------------------------------------
* DHT11 Sensor:         GPIO 26
* Water Level Sensor:   GPIO 35 (Analog)
* Soil Sensor 1:        GPIO 32
* Soil Sensor 2:        GPIO 33
* Soil Sensor 3:        GPIO 34
* Relay 1 (Pump 1):     GPIO 19
* Relay 2 (Pump 2):     GPIO 13
* Relay 3 (Pump 3):     GPIO 14
* Push Button:          GPIO 25 (Internal Pull-up)
* LCD Display:          I2C (SDA, SCL)

------------------------------------------------------------
SOFTWARE REQUIREMENTS & LIBRARIES
------------------------------------------------------------
To run this project, you need the Arduino IDE (Version 1.8.x or 2.x).
You must install the following libraries via the Library Manager:

1. Blynk (by Volodymyr Shymanskyy)
2. DHT sensor library (by Adafruit)
3. LiquidCrystal_I2C (by Frank de Brabander)
4. Adafruit Unified Sensor

------------------------------------------------------------
HOW TO RUN THE PROJECT
------------------------------------------------------------
1. Install the required libraries listed above.
2. Connect the ESP32 to your PC.
3. Open the file `Smart_Watering_System.ino` from the \Code folder.
4. **IMPORTANT:** Update the WiFi credentials in lines 23-24:
      char ssid[] = Adil
      char pass[] = 123456;
5. Select the Board: "DOIT ESP32 DEVKIT V1".
6. Compile and Upload the code.
7. Press the Push Button (Pin 25) to toggle through the LCD menu pages.

------------------------------------------------------------
BLYNK DASHBOARD SETUP (Virtual Pins)
------------------------------------------------------------
If configuring the mobile app, map these Virtual Pins:

V0: Temperature
V1: Humidity
V2: Water Tank Level (ADC)
V3: Field 1 Moisture
V4: Field 2 Moisture
V5: Field 3 Moisture
V6: Pump 1 Status
V7: Pump 2 Status
V8: Pump 3 Status

------------------------------------------------------------
Smart_Watering_System.ino
------------------------------------------------------------
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
