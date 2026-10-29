# Screen renderer

Makes the screen images in `docs/` from the real drawing code, without an ESP32 or Wokwi.

`render.py` compiles `deskpal/screens.cpp` and `deskpal/weather.cpp` together with the Adafruit GFX library for your computer. A small fake `Adafruit_SSD1306` (in `host/`) draws into an in-memory 128x64 buffer instead of sending it to the screen, and `host/main.cpp` calls each `draw...Screen()` function with sample data and saves the result. The script then scales each frame up with an OLED look and writes:

- `docs/screen-clock.png`, `screen-weather.png`, `screen-room.png`, `screen-github.png`, `screen-github-no-token.png`
- `docs/screens.png`, the four main screens in a 2x2 grid (used at the top of the main README)

The sample data lives in `host/main.cpp`: 14:32 on Thursday 29 October 2026, 18 °C and partly cloudy in Tehran, 23.4 °C and 41 % in the room, and a made-up 13-week contribution grid.

## Running it

You need:

- Python 3.10 or newer with Pillow
- A C++ compiler: `g++` or `clang++` on your PATH, or the `ziglang` pip package (a self-contained clang, handy on Windows)
- The Adafruit GFX Library and ArduinoJson libraries, installed the usual way (Arduino IDE library manager or `arduino-cli lib install`)

```sh
python -m venv ../deskpal-render-venv                  # outside the repo
../deskpal-render-venv/bin/pip install pillow ziglang  # Windows: ..\deskpal-render-venv\Scripts\pip
../deskpal-render-venv/bin/python tools/render/render.py
```

The script looks for the libraries in `$ARDUINO_DIRECTORIES_USER/libraries`, then `~/Documents/Arduino/libraries` and `~/Arduino/libraries`. If yours are elsewhere, pass `--libraries <folder>`. Use `--cxx "<command>"` to pick a compiler.

The build happens in a temporary folder that is deleted afterwards. Only the PNGs in `docs/` change.

## Notes

- The sketch files are copied before compiling, so your own `secrets.h` is never used. The images always show the defaults from `config.h` (Tehran, `@A2M33`).
- `githubHasToken()` comes from `host/main.cpp` instead of `github.cpp`, so one run can draw the GitHub screen with and without a token. `github.cpp` itself is not compiled; it needs the real HTTP and Wi-Fi libraries.
- If you change how a screen looks, run the script again and commit the new images with the code.
