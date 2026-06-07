/*
 * AmbientSync-ESP32.ino
 * 
 * Developed by: SepSR (Sepehr)
 * Description: An optimized, asynchronous multi-sensor monitoring & closed-loop 
 *              actuation system using ESP32, ST7789 TFT, and physical sensors.
 * Features: Non-blocking cooperative scheduling, adaptive PWM dimming, hysteresis 
 *           humidity loop, and acoustic peak-to-peak analysis.
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> 
#include <DHT.h>

// ST7789 SPI Display Pin Definitions
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4

// Sensor, Actuator, and Peripheral GPIO Assignments
#define DHTPIN       27  // DHT11 Data Pin moved to safe GPIO27 to prevent boot constraints
#define DHTTYPE      DHT11
#define LDR_PIN      34  // Analog Input Pin for LDR voltage divider
#define MIC_PIN      35  // Analog Input Pin for MAX4466 microphone preamplifier
#define MIST_PIN     12  // Output pin connected to 2N2222 transistor base to trigger mist maker pulses
#define BTN_PIN      13  // Input pin for manual toggle pushbutton
#define RED_LED_PIN  14  // High-temperature alert indicator LED
#define GRN_LED_PIN  25  // PWM controlled adaptive green LED

// Object Initializations
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHTPIN, DHTTYPE);

// Environmental Regulation Threshold Constants
const float HUMIDITY_LOW_THRESHOLD  = 40.0; // Trigger threshold to activate mist maker
const float HUMIDITY_HIGH_THRESHOLD = 50.0; // Cut-off threshold to deactivate mist maker
const float TEMP_ALERT_THRESHOLD    = 32.0; // Limit for triggering hazardous temp alarm

// Global State Variables
bool isMistMakerOn = false; 
bool manualModeActive = false;

// Pushbutton Debouncing State Variables
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50; // 50ms interval to eliminate mechanical contact noise

// Non-blocking Timing Variables (Asynchronous Scheduling)
unsigned long lastRedBlinkTime = 0;
bool redLedState = false;
unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_READ_INTERVAL = 2000; // Poll sensors every 2000ms

// Processed Sensor Variables
float currentTemperature = 0.0;
float currentHumidity = 0.0;
int currentLightPercent = 0;
String soundStatusText = "Quiet";

void setup() {
  Serial.begin(115200);
  delay(500); 
  Serial.println("--- System Initializing ---");
  
  // Set Pin Directions
  pinMode(MIST_PIN, OUTPUT);
  digitalWrite(MIST_PIN, LOW); 
  
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  
  pinMode(GRN_LED_PIN, OUTPUT);
  analogWrite(GRN_LED_PIN, 0); // Start with adaptive LED disabled
  
  pinMode(LDR_PIN, INPUT);
  pinMode(MIC_PIN, INPUT);
  
  // Configure input button with internal pull-up to eliminate external resistors
  pinMode(BTN_PIN, INPUT_PULLUP);

  // Initialize ST7789 TFT screen with 240x320 landscape configuration
  tft.init(240, 320); 
  tft.setRotation(1); 
  tft.fillScreen(ST77XX_BLACK);
  
  dht.begin();
  drawStaticUI(); // Draw static labels once to prevent flickering during refresh
  
  Serial.println("System Ready.");
}

void loop() {
  unsigned long currentTime = millis();
  
  // 1. Asynchronously poll manual control button to handle user overrides
  checkManualButton();
  
  // 2. Async blinking loop for Red LED if hazard threshold is reached (>32 C)
  if (currentTemperature > TEMP_ALERT_THRESHOLD) {
    if (currentTime - lastRedBlinkTime >= 500) { 
      lastRedBlinkTime = currentTime;
      redLedState = !redLedState;
      digitalWrite(RED_LED_PIN, redLedState ? HIGH : LOW);
    }
  } else {
    digitalWrite(RED_LED_PIN, LOW); // Enforce LED off state under nominal temperatures
  }
  
  // 3. Sensor Polling and System Actuation scheduler
  if (currentTime - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
    lastSensorReadTime = currentTime;
    
    readSensors();
    if (!manualModeActive) {
      controlHumidifierAuto();
    }
    controlAdaptiveGreenLED();
    updateDisplayValues();
  }
}

// Function to simulate physical button presses on the mist maker PCB via 2N2222 transistor
void pulseMistButton(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(MIST_PIN, HIGH);
    delay(150); // Simulate press down state for 150ms
    digitalWrite(MIST_PIN, LOW);
    delay(250); // Release state delay before sending consecutive pulse
  }
}

// Debounced button polling state-machine
void checkManualButton() {
  int reading = digitalRead(BTN_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != currentButtonState) {
      currentButtonState = reading;
      
      if (currentButtonState == LOW) { // Button active low (pressed)
        manualModeActive = !manualModeActive; 
        Serial.print("Manual Button Pressed. Manual Mode: ");
        Serial.println(manualModeActive ? "ACTIVE" : "INACTIVE");
        
        if (manualModeActive) {
          if (!isMistMakerOn) {
            pulseMistButton(1); // Turn on mist maker immediately on manual override
            isMistMakerOn = true;
          }
        } else {
          controlHumidifierAuto(); // Return to closed-loop automatic state
        }
        updateDisplayValues();
      }
    }
  }
  lastButtonState = reading;
}

// Time-domain signal analyzer for MAX4466 output voltage peak-to-peak tracking
unsigned int getSoundPeakToPeak() {
  unsigned long startMillis = millis();
  unsigned int peakToPeak = 0;
  unsigned int signalMax = 0;
  unsigned int signalMin = 4095;

  // Windowed scan for 50 milliseconds to capture full wave frequency cycles
  while (millis() - startMillis < 50) {
    int sample = analogRead(MIC_PIN);
    if (sample < 4096) { 
      if (sample > signalMax) signalMax = sample;
      if (sample < signalMin) signalMin = sample;
    }
  }
  peakToPeak = signalMax - signalMin; // Extract signal amplitude envelopes
  return peakToPeak;
}

// Core sensor ingestion routine
void readSensors() {
  // Read Temperature and Humidity from DHT11
  float tempH = dht.readHumidity();
  float tempT = dht.readTemperature();
  
  if (!isnan(tempH) && !isnan(tempT)) {
    currentHumidity = tempH;
    currentTemperature = tempT;
  } else {
    currentHumidity = 0.0;
    currentTemperature = 0.0;
  }

  // Poll Light Sensor and map range to percentage (0% to 100%)
  int rawLDR = analogRead(LDR_PIN);
  currentLightPercent = map(rawLDR, 0, 4095, 0, 100); 
  if (currentLightPercent < 0) currentLightPercent = 0;
  if (currentLightPercent > 100) currentLightPercent = 100;

  // Read sound level and categorize noise levels based on peak amplitude thresholds
  unsigned int soundPeak = getSoundPeakToPeak();
  Serial.print("Sound Amplitude: ");
  Serial.println(soundPeak);

  if (soundPeak < 150) {
    soundStatusText = "Quiet";
  } else if (soundPeak >= 150 && soundPeak < 500) {
    soundStatusText = "Moderate";
  } else if (soundPeak >= 500 && soundPeak < 1200) {
    soundStatusText = "Normal";
  } else if (soundPeak >= 1200 && soundPeak < 2500) {
    soundStatusText = "Loud";
  } else {
    soundStatusText = "Very Loud";
  }
}

// Dynamically adjusts Green LED intensity via duty-cycle modulation
void controlAdaptiveGreenLED() {
  if (currentLightPercent > 40) {
    analogWrite(GRN_LED_PIN, 0); // Disable LED under nominal ambient lighting
  } else {
    // Map brightness inversely proportional to lighting levels (Darker environment -> Brighter LED)
    int pwmValue = map(currentLightPercent, 0, 40, 255, 0);
    analogWrite(GRN_LED_PIN, pwmValue);
  }
}

// Automatic closed-loop actuation based on environmental humidity variables
void controlHumidifierAuto() {
  if (currentHumidity <= 0.1) return; // Prevent action in the event of sensor error

  if (currentHumidity < HUMIDITY_LOW_THRESHOLD) {
    if (!isMistMakerOn) {
      pulseMistButton(1); // Trigger 1 pulse to transition state to ACTIVE mode
      isMistMakerOn = true;
    }
  } 
  else if (currentHumidity > HUMIDITY_HIGH_THRESHOLD) {
    if (isMistMakerOn) {
      pulseMistButton(2); // Trigger 2 pulses to bypass intermittent mode and turn off device
      isMistMakerOn = false;
    }
  }
}

// Initial draw of static labels on TFT to optimize performance
void drawStaticUI() {
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  
  tft.setCursor(10, 10);
  tft.println("ENVIRONMENT MONITOR");
  tft.drawFastHLine(10, 30, 300, ST77XX_BLUE);
  
  tft.setCursor(15, 50);
  tft.print("Temp: ");
  
  tft.setCursor(15, 85);
  tft.print("Humidity: ");
  
  tft.setCursor(15, 120);
  tft.print("Light: ");
  
  tft.setCursor(15, 155);
  tft.print("Sound: ");
  
  tft.setCursor(15, 195);
  tft.print("Actuator:");
}

// Localized pixel redraw routine to prevent flickering artifacts
void updateDisplayValues() {
  tft.setTextSize(2);
  
  // 1. Temperature Display & High Heat warning indicators
  tft.fillRect(110, 50, 200, 20, ST77XX_BLACK); 
  tft.setCursor(110, 50);
  if (currentTemperature > 0.1) {
    if (currentTemperature > TEMP_ALERT_THRESHOLD) {
      tft.setTextColor(ST77XX_RED);
      tft.print(currentTemperature, 1);
      tft.print(" C  [ ! ]"); 
    } else {
      tft.setTextColor(ST77XX_YELLOW);
      tft.print(currentTemperature, 1);
      tft.print(" C");
    }
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.print("ERR");
  }
  
  // 2. Humidity Display
  tft.fillRect(130, 85, 180, 20, ST77XX_BLACK);
  tft.setCursor(130, 85);
  tft.setTextColor(ST77XX_CYAN);
  if (currentHumidity > 0.1) {
    tft.print(currentHumidity, 1);
    tft.print(" %");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.print("ERR");
  }
  
  // 3. Ambient Light Display
  tft.fillRect(110, 120, 200, 20, ST77XX_BLACK);
  tft.setCursor(110, 120);
  tft.setTextColor(ST77XX_ORANGE);
  tft.print(currentLightPercent);
  tft.print(" %");
  
  // 4. Acoustic Environment Categorization Output
  tft.fillRect(110, 155, 200, 20, ST77XX_BLACK);
  tft.setCursor(110, 155);
  if (soundStatusText == "Quiet" || soundStatusText == "Moderate") {
    tft.setTextColor(ST77XX_GREEN);
  } else if (soundStatusText == "Normal") {
    tft.setTextColor(ST77XX_CYAN);
  } else {
    tft.setTextColor(ST77XX_RED); 
  }
  tft.print(soundStatusText);
  
  // 5. System Actuator Status Block
  tft.fillRect(130, 195, 180, 25, ST77XX_BLACK);
  tft.setCursor(130, 195);
  
  if (manualModeActive) {
    tft.setTextColor(ST77XX_ORANGE);
    tft.print("MANUAL ON");
  } else {
    if (isMistMakerOn) {
      tft.setTextColor(ST77XX_GREEN);
      tft.print("AUTO ON");
    } else {
      tft.setTextColor(ST77XX_RED);
      tft.print("AUTO OFF");
    }
  }
}