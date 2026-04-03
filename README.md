# Smart Parking Disk 

This is the firmware for a smart parking disk based on the Arduino Nano. 

## Features Implemented
1. **Deep Sleep State Machine:** Uses the Rocket Scream `LowPower` library. The Arduino spends 99% of its life in an ultra-low power state, consuming almost 0mA.
2. **Interrupt Wake-ups:** The `SW-420` vibration module is tied to `D2`, pulling it HIGH when the car is shaking (driving), which guarantees instant wake-up.
3. **Swedish Rounding Rules:** The parking time calculates to the exact minute, and seamlessly rounds UP to the next full half-hour mark (e.g., `14:18` rounds to `14:30`, and `14:32` rounds to `15:00`). 
4. **E-Ink Display System:** The code has been specifically targeted at the `1.54" 200x200 B/W SPI E-ink` display, using `GxEPD2` along with `Adafruit_GFX` to render large customized fonts perfectly in the center inside an elegant layout.

## Next Steps for Setup

1. Start a new project in your Arduino IDE or PlatformIO.
2. Find and install the following libraries inside the Library Manager:
   - `LowPower` by Rocket Scream
   - `RTC` by Makuna (for DS1302)
   - `GxEPD2` by Jean-Marc Zingg
   - `Adafruit GFX Library` by Adafruit
3. Paste the code into your `main.cpp` / `.ino` file and compile/upload to the Arduino Nano.

> [!TIP]
> The first time you power up the RTC module, it will likely not know the time! You can uncomment `Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));` inside the `setup()` function, flash the code once to burn the current time to the RTC chip, and then comment it out and flash again for regular use.