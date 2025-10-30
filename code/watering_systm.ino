#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>

// === OLED Display Setup ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === BME280 Sensor ===
Adafruit_BME280 bme;

// === Pins ===
const int Trig = 8;
const int Echo = 9;
const int sensorPin = A0;  // Soil moisture
const int relay = 12;      // Relay controlling pump

// === Variables ===
float temperature = 0, humidity = 0, pressure = 0, Distance = 0;
int moisture = 0;
const int DRY_THRESHOLD = 440;   // BELOW this → soil is dry → water (was 445)
const int WET_THRESHOLD = 400;   // BELOW this → soil is wet → stop watering (was 420)

bool bmeFound = false;
bool oledFound = false;
bool isWatering = false;

unsigned long lastWaterTime = 0;
const unsigned long WATERING_INTERVAL = 60000; // 1 minute between watering cycles
const unsigned long WATERING_DURATION = 2000;  // 2 seconds watering time
unsigned long wateringStartTime = 0;

// ======== Setup ========
void setup() {
  Serial.begin(9600);
  Serial.println(F("System Booting..."));

  pinMode(Trig, OUTPUT);
  pinMode(Echo, INPUT);
  pinMode(relay, OUTPUT);
  pinMode(sensorPin, INPUT);
  
  // Start with pump OFF
  digitalWrite(relay, HIGH); 
  Serial.println(F("Pump starting in OFF state"));
  
  Wire.begin();
  Wire.setWireTimeout(250);
  delay(100);

  // Initialize OLED
  Serial.println(F("Initializing OLED..."));
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oledFound = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Plant Monitor"));
    display.println(F("Initializing..."));
    display.display();
    Serial.println(F("OLED initialized!"));
  } else {
    Serial.println(F("OLED FAILED"));
  }

  delay(500);

  // Initialize BME280
  Serial.println(F("Initializing BME280..."));
  if (bme.begin(0x76)) {
    bmeFound = true;
    Serial.println(F("BME280 initialized!"));
  } else {
    Serial.println(F("BME280 not found!"));
  }

  delay(1500);
  
  // Show ready message
  if (oledFound) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("System Ready"));
    display.println(F("Pump: OFF"));
    display.display();
  }
  
  Serial.println(F("Setup Complete"));
  Serial.println(F("=== MOISTURE CALIBRATION ==="));
  Serial.println(F("Air (dry): ~470"));
  Serial.println(F("Moist soil: ~435")); 
  Serial.println(F("Water (wet): ~420"));
  Serial.println(F("Very wet: ~350"));
  Serial.println(F("WATER when ABOVE: 440"));
  Serial.println(F("STOP when BELOW: 400"));
  Serial.println(F("============================"));
}

// ======== Loop ========
void loop() {
  readSensors();
  if (oledFound) updateDisplay();
  checkWatering();

  // === FIXED: CORRECTED WATERING LOGIC ===
  // Water when moisture is HIGH (dry soil) and conditions are met
  if (!isWatering) {
    if (moisture > DRY_THRESHOLD) {
      // Soil is DRY (high reading) - check if we can water
      if (millis() - lastWaterTime > WATERING_INTERVAL) {
        Serial.println(F("🚰 Soil is DRY - Starting watering"));
        waterPlant();
      } else {
        Serial.print(F("Soil dry but waiting. Time left: "));
        Serial.print((WATERING_INTERVAL - (millis() - lastWaterTime)) / 1000);
        Serial.println(F("s"));
      }
    } else if (moisture < WET_THRESHOLD) {
      // Soil is WET (low reading) - no watering needed
      Serial.println(F("💧 Soil is WET - No watering needed"));
    } else {
      // Soil moisture is in good range
      Serial.println(F("✅ Soil moisture is GOOD"));
    }
  }

  delay(1000);
}

// ======== Sensor Reading ========
void readSensors() {
  // BME280
  if (bmeFound) {
    temperature = (bme.readTemperature() * 9.0 / 5.0) + 32; // °F
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F;
  }

  // Soil moisture (averaged for stability)
  moisture = readSoil();

  // Ultrasonic distance
  digitalWrite(Trig, LOW);
  delayMicroseconds(2);
  digitalWrite(Trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(Trig, LOW);
  long duration = pulseIn(Echo, HIGH, 30000);
  Distance = (duration > 0) ? duration * 0.034 / 2 : 0;

  // Serial monitor output
  Serial.print(F("Moisture: ")); Serial.print(moisture);
  if (moisture > DRY_THRESHOLD) Serial.print(F(" (DRY)"));
  else if (moisture < WET_THRESHOLD) Serial.print(F(" (WET)"));
  else Serial.print(F(" (GOOD)"));
  
  Serial.print(F(" | Temp: ")); Serial.print(temperature, 1);
  Serial.print(F("°F | Pump: "));
  Serial.println(digitalRead(relay) ? "OFF" : "ON");
}

// ======== Soil sensor averaging ========
int readSoil() {
  long sum = 0;
  const int samples = 5;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(sensorPin);
    delay(10);
  }
  return sum / samples;
}

// ======== Display Update ========
void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);

  // Display BME280 data
  if (bmeFound) {
    display.print(F("Temp: ")); display.print(temperature, 1); display.println(F("F"));
    display.print(F("Humidity: ")); display.print(humidity, 0); display.println(F("%"));
  }

  display.print(F("Moisture: ")); display.println(moisture);
  
  // Show moisture status clearly
  if (moisture > DRY_THRESHOLD) display.println(F("Status: TOO DRY"));
  else if (moisture < WET_THRESHOLD) display.println(F("Status: TOO WET")); 
  else display.println(F("Status: GOOD"));

  display.print(F("Distance: ")); display.print(Distance, 0); display.println(F("cm"));

  // Display watering status
  if (isWatering) {
    display.println(F(""));
    display.println(F(">> WATERING <<"));
    display.print(F("Time left: "));
    display.print((WATERING_DURATION - (millis() - wateringStartTime)) / 1000);
    display.println(F("s"));
  }
  
  display.print(F("Pump: "));
  display.println(digitalRead(relay) ? "OFF" : "ON");

  display.display();
}

// ======== Water Plant ========
void waterPlant() {
  if (!isWatering) {
    isWatering = true;
    wateringStartTime = millis();
    digitalWrite(relay, LOW);  // Turn pump ON
    Serial.println(F("💧 PUMP ON - Starting 2 second watering"));
    
    if (oledFound) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("WATERING ACTIVE"));
      display.println(F(""));
      display.println(F("Soil too dry!"));
      display.println(F("Watering..."));
      display.display();
    }
  }
}

// ======== Stop Watering ========
void checkWatering() {
  if (isWatering) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - wateringStartTime;
    
    if (elapsedTime >= WATERING_DURATION) {
      digitalWrite(relay, HIGH); // Turn pump OFF
      isWatering = false;
      lastWaterTime = millis();
      Serial.println(F("✅ PUMP OFF - Watering complete"));
      
      if (oledFound) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(F("WATERING DONE"));
        display.println(F(""));
        display.println(F("Pump: OFF"));
        display.display();
        delay(1000);
      }
    }
  }
}
