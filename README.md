# Smart Parking Disk 

This is the firmware for a smart parking disk based on the ultra-low power **ESP32-C3** architecture. 

## Features Implemented
1. **Deep Sleep State Machine:** Leverages the ESP32's native deep sleep coprocessor. The ESP32 spends 99% of its life in an ultra-low power state, consuming almost 0mA, waking up via the RTC hardware.
2. **Interrupt Wake-ups:** The `SW-420` vibration module is tied to `GPIO 2`. It pulls the pin HIGH when the car is shaking (driving), triggering an `EXT0` hardware wake-up.
3. **Swedish Rounding Rules:** The parking time calculates to the exact minute, and seamlessly rounds UP to the next full half-hour mark (e.g., `14:18` rounds to `14:30`, and `14:32` rounds to `15:00`). 
4. **E-Ink Display System:** The code uses the ESP32's substantial RAM to hold a full, unpaged display buffer for the `1.54" 200x200 B/W SPI E-ink` display, enabling high-speed rendering using `GxEPD2` and `Adafruit_GFX`.

## Hardware Pinout (ESP32-C3 Zero)
- **Vibration Sensor (SW-420):** `GPIO 2`
- **E-Ink Display:** 
  - CS: `10`, DC: `3`, RST: `4`, BUSY: `5`
  - DI/MOSI: `7`, CLK/SCK: `6`
- **RTC (DS1302):** 
  - DAT: `0`, CLK: `1`, RST: `8`

## Next Steps for Setup

1. Start a new project in your Arduino IDE or PlatformIO. Select an **ESP32-C3** development board (like the ESP32C3 Dev Module).
2. Find and install the following libraries inside the Library Manager:
   - `RTC` by Makuna (for DS1302)
   - `GxEPD2` by Jean-Marc Zingg
   - `Adafruit GFX Library` by Adafruit
3. Paste the code into your `main.cpp` / `.ino` file and compile/upload.

> [!TIP]
> The first time you power up the RTC module, it will likely not know the time! You can uncomment `Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));` inside the `setup()` function, flash the code once to burn the current time to the RTC chip, and then comment it out and flash again for regular use.