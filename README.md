<p align="center"><img src="docs/header.svg" alt="deskpal" width="100%"></p>

<p align="center">
  <a href="https://github.com/A2M33/deskpal/actions/workflows/build.yml"><img alt="build" src="https://github.com/A2M33/deskpal/actions/workflows/build.yml/badge.svg"></a>
  <img alt="ESP32" src="https://img.shields.io/badge/board-ESP32-e7352c">
  <img alt="Runs in Wokwi" src="https://img.shields.io/badge/simulator-Wokwi-6e2bc2">
  <a href="https://github.com/A2M33/deskpal/blob/main/LICENSE"><img alt="MIT license" src="https://img.shields.io/badge/license-MIT-3fb950"></a>
</p>

A small desk display built on an ESP32. It shows the time, the weather outside, the temperature and humidity in your room, and your recent GitHub activity on a 0.96" OLED screen.

An ESP32 is a cheap microcontroller board (around $5) with Wi-Fi built in. You program it from your computer over USB, and it runs your code on its own as soon as it gets power.

![The four deskpal screens: clock, weather, room and GitHub](docs/screens.png)

I rendered these on my PC from the drawing code in `deskpal/screens.cpp`, so they match the OLED pixel for pixel. `tools/render` has the script.

## What it shows

The display rotates through four screens, eight seconds each.

1. **Clock**: time and date, synced from the internet (NTP). Set your time zone in `secrets.h`; the default is Tehran.
2. **Weather**: current temperature, a short description, a small icon and wind speed from [Open-Meteo](https://open-meteo.com). Open-Meteo needs no account or API key. The data refreshes every 15 minutes.
3. **Room**: temperature and humidity from a DHT22 sensor, plus a comfort label like "Comfortable" or "Dry".
4. **GitHub**: your contribution grid for the last 12 weeks, plus the total. Busier days get bigger squares, since the screen only has one color. Refreshes every 30 minutes and needs a token (see below).

Press the button to jump to the next screen. Hold it for about a second to pause the rotation, and hold it again to resume. A pause icon appears in the top bar while rotation is off.

If Wi-Fi drops or a website doesn't answer, deskpal keeps showing the last data it got and tells you how old it is. It reconnects by itself.

## Try it in Wokwi

[Wokwi](https://wokwi.com) simulates the ESP32, the screen, the sensor and the button in your browser, with internet access. You don't need any hardware.

1. Go to [wokwi.com](https://wokwi.com), sign in, and start a new project with the **ESP32** board (Arduino template).
2. Open the `sketch.ino` tab and replace its contents with `deskpal/deskpal.ino` from this repo.
3. Open the `diagram.json` tab and replace its contents with `diagram.json` from this repo. The parts and wires appear on the right.
4. Add the libraries. Click the small arrow next to the file tabs, choose **New file**, name it `libraries.txt`, and paste in `libraries.txt` from this repo. (You can also add each one from the **Library Manager** tab.)
5. Add the remaining code files the same way, using the exact same names: `config.h`, `button.h`, `room.h`, `weather.h`, `weather.cpp`, `github.h`, `github.cpp`, `screens.h`, `screens.cpp`.
6. Press the green play button. The first build takes a minute.

Wokwi has a virtual Wi-Fi network called `Wokwi-GUEST`, and deskpal uses it by default, so the clock and weather work right away. To change the room readings, click the DHT22 while the simulation runs and drag the sliders.

The GitHub screen will ask for a token. You can add a `secrets.h` file in Wokwi too, but if you save the project, anyone with the link can read it. Revoke that token once you're done testing.

## Parts

Prices are rough, from the usual online electronics shops.

| Part | Notes | Price |
|---|---|---|
| ESP32 DevKit board | Any "ESP32 DevKit V1" or "DevKitC" with 30 or 38 pins | $5-8 |
| 0.96" OLED, 128x64, SSD1306, I2C | The 4-pin version (GND, VCC, SCL, SDA) | $3-5 |
| DHT22 (AM2302) sensor | A module on a small board saves you a resistor | $3-6 |
| Push button | Any small tactile switch | $0.10 |
| Breadboard and jumper wires | | $5 |
| USB cable | One that carries data; some cables only charge | |

Total: about $20.

## Wiring

| From | To ESP32 pin |
|---|---|
| OLED GND | GND |
| OLED VCC | 3V3 |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| DHT22 VCC (+) | 3V3 |
| DHT22 data (out) | GPIO 15 |
| DHT22 GND (-) | GND |
| Button, one leg | GPIO 4 |
| Button, other leg | GND |

The button needs no resistor, because the code turns on the ESP32's internal pull-up. If you use a bare DHT22 (4 pins, no board), put a 10 kΩ resistor between its data pin and 3V3.

The OLED's I2C address is set to `0x3C` in `config.h`. A few screens use `0x3D`; the troubleshooting section covers that.

## Setup in Arduino IDE

1. Install [Arduino IDE 2](https://www.arduino.cc/en/software).
2. Add ESP32 support: open **Tools > Board > Boards Manager**, search for `esp32`, and install **esp32 by Espressif Systems**. If it doesn't show up, go to **File > Preferences** and add this to "Additional boards manager URLs", then try again:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Install the libraries from **Tools > Manage Libraries**. Click "Install all" when it offers dependencies.
   - Adafruit SSD1306
   - Adafruit GFX Library
   - DHT sensor library (by Adafruit)
   - ArduinoJson (by Benoit Blanchon, version 7)
4. Open `deskpal/deskpal.ino`. The IDE shows the other files as tabs.
5. Make your settings file: copy `deskpal/secrets.example.h` to `deskpal/secrets.h` and fill in your Wi-Fi name and password, plus your GitHub username and token. Every line is optional; delete a line to keep the default. Git ignores `secrets.h`, so your password stays on your computer.
6. Pick **Tools > Board > esp32 > ESP32 Dev Module** and the right port under **Tools > Port**, then click Upload.
7. Open **Tools > Serial Monitor** at 115200 baud to watch the log.

If you prefer the command line:

```sh
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library" "DHT sensor library" "ArduinoJson"
arduino-cli compile --fqbn esp32:esp32:esp32 deskpal
arduino-cli upload --fqbn esp32:esp32:esp32 -p COM3 deskpal
```

Replace `COM3` with your port (`/dev/ttyUSB0` or similar on Linux and macOS).

## Getting a GitHub token

GitHub only serves contribution data to signed-in requests, so deskpal needs a personal access token. A read-only token for public data is enough.

1. On GitHub, open **Settings > Developer settings > Personal access tokens > Fine-grained tokens**.
2. Click **Generate new token**.
3. Give it a name like `deskpal` and pick an expiration date.
4. Under **Repository access**, choose **Public repositories**. Leave every permission at "No access".
5. Click **Generate token** and copy it right away. GitHub shows it only once.
6. Put it in `secrets.h`:
   ```c
   #define GITHUB_TOKEN "github_pat_..."
   ```

Set `GITHUB_USER` to the account whose grid you want. It doesn't have to be your own.

## A note on HTTPS

deskpal fetches the weather and GitHub data over HTTPS, but it skips checking the servers' certificates (`setInsecure()` in the code). The connection is still encrypted. Skipping the check means someone controlling your network could pretend to be GitHub and capture your token. With a read-only token for public data, that risk is small. If you want full checking, look up `WiFiClientSecure::setCACert()` and add the root certificates for `api.github.com` and `api.open-meteo.com`.

## Troubleshooting

**The screen stays black.** Check that SDA goes to GPIO 21 and SCL to GPIO 22; swapping them is the most common mistake. Then open the Serial Monitor: "OLED not found" means the ESP32 can't see the screen. Some screens use address `0x3D`, so try changing `OLED_ADDRESS` in `config.h`.

**It says "Connecting to Wi-Fi" forever.** The ESP32 only works with 2.4 GHz Wi-Fi, not 5 GHz. Check the network name and password in `secrets.h`; both are case-sensitive.

**The Room screen says "No sensor reading".** Check the DHT22's data wire on GPIO 15 and its power. A bare sensor needs the 10 kΩ pull-up resistor described above.

**The clock is off by a few hours.** Check your `TIMEZONE` string. POSIX time zones flip the sign, so UTC+1 is written `-1`. `secrets.example.h` lists a few examples.

**The GitHub screen says "Token rejected".** The token expired or has a typo. Make a new one.

**Upload fails with "Failed to connect to ESP32".** Hold the **BOOT** button on the board while the upload starts. If your computer doesn't see the board at all, try another USB cable (some only carry power) or install the USB driver for your board's chip (CP210x or CH340, printed on the chip near the USB port).

## License

MIT. See [LICENSE](LICENSE).
