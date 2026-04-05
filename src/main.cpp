#include <Arduino.h>
#include <SPI.h>            // For E-Ink Display
#include <ThreeWire.h>      // For DS1302 RTC
#include <RtcDS1302.h>      // For DS1302 RTC
#include <esp_sleep.h>      // For ESP32 Native Deep Sleep

// E-Ink includes
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// --- Pins Definition (ESP32-C3 DevKitM-1) ---
const int VIBRATION_PIN = 2; // HW Wakeup RTC GPIO
const int RTC_CLK = 1;
const int RTC_DAT = 0;
const int RTC_RST = 8;

const int EPD_CS = 11; // SPI CS pin for the E-Ink display
const int EPD_DC = 3;
const int EPD_RST = 4;
const int EPD_BUSY = 5;
// Note: ESP32-C3 Hardware SPI pins used by GxEPD2 by default:
// MOSI = 7, SCK = 6

// Configure RTC (Dat, Clk, Rst)
ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

// Configure E-Ink Driver for 1.54" 200x200 B/W
// ESP32 has plenty of RAM, so we allocate the full screen buffer!
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));

// --- Constants ---
const unsigned long DRIVE_TIMEOUT = 120000; // 120 seconds in milliseconds without vibration means "Parked"

// --- State Variables ---
volatile bool gotVibration = false; // Flag set in Interrupt
unsigned long lastVibrationMillis = 0;
bool isParked = false;

// --- Function Prototypes ---
void wakeUpISR();
void putToSleep();
void updateScreen(const RtcDateTime& dt);
RtcDateTime roundUpTime(const RtcDateTime& dt);

void setup() {
  Serial.begin(115200);
  
  // Vibration sensor using on-board comparator usually outputs clean High/Low
  pinMode(VIBRATION_PIN, INPUT); 
  
  // Attach interrupt to keep resetting the timer while awake
  attachInterrupt(digitalPinToInterrupt(VIBRATION_PIN), wakeUpISR, RISING); 

  Serial.println("System Initializing...");

  // --- Check Wakeup Reason ---
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke up from deep sleep due to vibration!");
  } else {
    Serial.println("Standard boot / reset.");
  }

  // --- Initialize RTC ---
  Rtc.Begin();
  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not running, starting now...");
    Rtc.SetIsRunning(true);
  }
  if (Rtc.GetIsWriteProtected()) {
    Serial.println("RTC was write protected, enabling writing...");
    Rtc.SetIsWriteProtected(false);
  }
  // Note: To set the time on first use, you can call the following line ONCE, then remove it:
  // Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__)); 

  // --- Initialize E-Ink ---
  // The first parameter sets the serial diagnostic bitrate. We use 0 generally in prod to disable.
  display.init(115200); 

  // --- Start System State ---
  // After a boot or a deep sleep wake-up, we are always "awake and driving"
  gotVibration = true; 
  isParked = false;
  lastVibrationMillis = millis();
  
  Serial.println("Setup Complete.");
}

void loop() {
  // If the sensor triggered the interrupt, acknowledge it
  if (gotVibration) {
    gotVibration = false;
    lastVibrationMillis = millis(); // Reset timeout
    
    if (isParked) { 
      // Theoretically won't happen here due to deep sleep reboot logic, but good for safety
      Serial.println("Vibration detected! Leaving parking space.");
      isParked = false;
    }
  }

  // Check if we have exceeded the drive timeout
  if (!isParked && (millis() - lastVibrationMillis) >= DRIVE_TIMEOUT) {
    Serial.println("Timeout reached. Parking vehicle.");
    isParked = true;

    // 1. Get current time
    RtcDateTime now = Rtc.GetDateTime();
    
    // 2. Round it UP based on Swedish rules
    now = roundUpTime(now);

    // 3. Update the display with our arrival time
    updateScreen(now);

    // 4. Tell the ESP32 to go into deep sleep to save the battery
    putToSleep();
  }
}

// ----------------------------------------------------
// ISR - Keep it extremely short. 
// Used purely to reset the timer flag while awake.
void IRAM_ATTR wakeUpISR() {
  gotVibration = true;
}

// ----------------------------------------------------
void putToSleep() {
  Serial.println("Entering Deep Sleep...");
  Serial.flush(); // Give serial time to finish printing

  // Ensure E-ink is totally powered down to prevent damage and use 0mA
  display.powerOff(); 

  // ESP32 Native Deep Sleep Wakeup Source
  // Configure the RTC controller to wake the processor up when the vibration pin goes HIGH (1)
  esp_deep_sleep_enable_gpio_wakeup(1 << VIBRATION_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
  
  // ** SLEEP MODE ACTIVATED **
  // The CPU completely powers off. RAM is cleared. 
  // When it wakes up, it will reboot starting all over from setup().
  esp_deep_sleep_start();
}

// ----------------------------------------------------
// Standard Swedish rounding rule: 
// "Avrundas till närmaste följande hel- eller halvtimme" (Round up to next half/full hour)
RtcDateTime roundUpTime(const RtcDateTime& dt) {
  uint16_t year = dt.Year();
  uint8_t month = dt.Month();
  uint8_t day = dt.Day();
  uint8_t hour = dt.Hour();
  uint8_t minute = dt.Minute();
  uint8_t second = 0; // Freeze seconds to 00
  
  if (minute > 0 && minute <= 30) {
    minute = 30;
  } else if (minute > 30) {
    minute = 0;
    hour++;
    if (hour >= 24) {
      hour = 0; // Wrap around safely. No need to update the date for a simple parking clock.
    }
  }

  return RtcDateTime(year, month, day, hour, minute, second);
}

// ----------------------------------------------------
void updateScreen(const RtcDateTime& dt) {
  Serial.println("Updating E-Ink Display...");

  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", dt.Hour(), dt.Minute());

  // Set rotation (depends on how you mount it in your case)
  display.setRotation(1); 
  
  // Send data to the display
  // Using full buffer, so no paged drawing loop is required anymore!
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);

  // Draw "Ankomsttid:"
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(25, 40); // Estimate center vertically/horizontally
  display.print("Ankomsttid:");

  // Draw the actual HH:MM
  display.setFont(&FreeSansBold24pt7b);
  display.setCursor(45, 120); 
  display.print(timeStr);
    
  // Push full buffer to display
  display.display(false); // false means full update

  Serial.println("Display Updated!");
}