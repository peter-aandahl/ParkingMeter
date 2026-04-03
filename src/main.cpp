#include <Arduino.h>
#include <LowPower.h>       // For deep sleep
#include <SPI.h>            // For E-Ink Display
#include <ThreeWire.h>      // For DS1302 RTC
#include <RtcDS1302.h>      // For DS1302 RTC

// E-Ink includes
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>

// --- Pins Definition ---
const int VIBRATION_PIN = 2; // SW-420 DO -> D2 (MUST be D2 or D3 for interrupts on Nano)
const int RTC_CLK = 6;
const int RTC_DAT = 7;
const int RTC_RST = 8;

const int EPD_CS = 10;
const int EPD_DC = 9;
const int EPD_RST = 4;
const int EPD_BUSY = 3;

// Configure RTC with minimal memory usage
ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);  // Keep simple
RtcDS1302<ThreeWire> Rtc(myWire);

// Configure E-Ink Driver for 1.54" 200x200 B/W
// Arduino Nano only has 2KB of RAM, so we must limit the page height (e.g. to 32)
// to use paged drawing rather than trying to allocate a 5KB full-screen buffer.
GxEPD2_BW<GxEPD2_154_D67, 32> display(GxEPD2_154_D67(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));

// #define DEBUG_MODE  // Uncomment to enable serial debugging
#ifdef DEBUG_MODE
#define DEBUG_PRINT(x) Serial.println(x)
#define DEBUG_PRINT_P(x) printFromPROGMEM(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINT_P(x)
#endif

// PROGMEM strings to save RAM (only used in debug mode)
#ifdef DEBUG_MODE
const char MSG_INIT[] PROGMEM = "System Initializing...";
const char MSG_RTC_NOT_RUNNING[] PROGMEM = "RTC was not running, starting now...";
const char MSG_RTC_WRITE_PROTECTED[] PROGMEM = "RTC was write protected, enabling writing...";
const char MSG_SETUP_COMPLETE[] PROGMEM = "Setup Complete.";
const char MSG_VIBRATION_DETECTED[] PROGMEM = "Vibration detected! Leaving parking space.";
const char MSG_TIMEOUT[] PROGMEM = "Timeout reached. Parking vehicle.";
const char MSG_DEEP_SLEEP[] PROGMEM = "Entering Deep Sleep...";
const char MSG_WAKE_UP[] PROGMEM = "Woke up from sleep!";
const char MSG_UPDATING_DISPLAY[] PROGMEM = "Updating E-Ink Display...";
const char MSG_DISPLAY_UPDATED[] PROGMEM = "Display Updated!";
#endif

// This string is always needed for display
const char MSG_ARRIVAL_TIME[] PROGMEM = "Ankomsttid:";

// Helper function to print PROGMEM strings
void printFromPROGMEM(const char* str) {
  char buffer[30];  // Reduced from 50
  strcpy_P(buffer, str);
  Serial.println(buffer);
}
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
#ifdef DEBUG_MODE
  Serial.begin(9600);
#endif
  
  // Vibration sensor using on-board comparator usually outputs clean High/Low
  pinMode(VIBRATION_PIN, INPUT); 

  DEBUG_PRINT_P(MSG_INIT);

  // --- Initialize RTC ---
  Rtc.Begin();
  if (!Rtc.GetIsRunning()) {
    DEBUG_PRINT_P(MSG_RTC_NOT_RUNNING);
    Rtc.SetIsRunning(true);
  }
  if (Rtc.GetIsWriteProtected()) {
    DEBUG_PRINT_P(MSG_RTC_WRITE_PROTECTED);
    Rtc.SetIsWriteProtected(false);
  }
  // Note: To set the time on first use, you can call the following line ONCE, then remove it:
  // Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__)); 

  // --- Initialize E-Ink ---
  // Use minimal initialization to save memory
  display.init(0);  // Simple init

  // --- Start System State ---
  gotVibration = true; // Assume awake and driving on first boot
  isParked = false;
  
  DEBUG_PRINT_P(MSG_SETUP_COMPLETE);
}

void loop() {
  // If the sensor triggered the interrupt, acknowledge it
  if (gotVibration) {
    gotVibration = false;
    lastVibrationMillis = millis(); // Reset timeout
    
    if (isParked) { 
      // We were parked. The interrupt woke us up and we are now driving.
      DEBUG_PRINT_P(MSG_VIBRATION_DETECTED);
      isParked = false;
    }
  }

  // Check if we have exceeded the drive timeout
  if (!isParked && (millis() - lastVibrationMillis) >= DRIVE_TIMEOUT) {
    DEBUG_PRINT_P(MSG_TIMEOUT);
    isParked = true;

    // 1. Get current time
    RtcDateTime now = Rtc.GetDateTime();
    
    // 2. Round it UP based on Swedish rules
    now = roundUpTime(now);

    // 3. Update the display with our arrival time
    updateScreen(now);

    // 4. Tell the Arduino to go into deep sleep to save the battery
    putToSleep();
  }
}

// ----------------------------------------------------
// ISR - Keep it extremely short. 
// Used purely to wake up the system and set the flag.
void wakeUpISR() {
  gotVibration = true;
}

// ----------------------------------------------------
void putToSleep() {
  DEBUG_PRINT_P(MSG_DEEP_SLEEP);
#ifdef DEBUG_MODE
  Serial.flush(); // Give serial time to finish printing
#endif

  // Ensure E-ink is totally powered down to prevent damage and use 0mA
  display.powerOff();

  // We attach the interrupt right before sleeping.
  // The SW-420 goes HIGH upon vibration.
  attachInterrupt(digitalPinToInterrupt(VIBRATION_PIN), wakeUpISR, RISING); 
  
  // ** SLEEP MODE ACTIVATED **
  // The CPU stops here completely until D2 sees a rising edge.
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  
  // CPU RESUMES HERE WHEN WOKEN UP
  detachInterrupt(digitalPinToInterrupt(VIBRATION_PIN));
  
  DEBUG_PRINT_P(MSG_WAKE_UP);
  
  // NOTE: The millis() timer is frozen while asleep! 
  // But that's okay, `gotVibration` will be true, and the main loop will instantly 
  // reset `lastVibrationMillis` = millis(), starting the timer fresh.
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
  DEBUG_PRINT_P(MSG_UPDATING_DISPLAY);

  char timeStr[6];  // Reduced from default sprintf buffer
  sprintf(timeStr, "%02d:%02d", dt.Hour(), dt.Minute());

  // Set rotation (depends on how you mount it in your case)
  display.setRotation(1); 
  
  // Send data to the display
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Draw "Ankomsttid:" with built-in font
    display.setFont(NULL);  // Use default system font
    display.setTextSize(2); // Make it larger
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(20, 30);
    display.print(F("Ankomsttid:"));  // Use F() macro to save RAM

    // Draw the actual HH:MM with built-in font
    display.setFont(NULL);  // Use default system font
    display.setTextSize(3); // Even larger for time
    display.setCursor(35, 80);
    display.print(timeStr);
    
  } while (display.nextPage());

  DEBUG_PRINT_P(MSG_DISPLAY_UPDATED);
}