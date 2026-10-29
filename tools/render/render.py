#!/usr/bin/env python3
"""Render deskpal's screens to PNG images for the README.

Compiles the real drawing code (deskpal/screens.cpp, deskpal/weather.cpp) and
Adafruit GFX for this computer, runs it with sample data, and turns the
128x64 frame buffers into OLED-looking PNGs in docs/. With --gif it also
writes docs/rotation.gif, an animated pass through the screens.

Needs: Python 3.10+ with Pillow, a C++17 compiler (g++, clang++, or the
`ziglang` pip package), and the Adafruit GFX and ArduinoJson libraries as
installed by the Arduino IDE or arduino-cli. See README.md next to this file.
"""

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont
except ImportError:
    sys.exit("Pillow is missing: pip install pillow")

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
SKETCH = REPO / "deskpal"
DOCS = REPO / "docs"

OLED_W, OLED_H = 128, 64
# (pbm name, label in the combined image or None to leave it out)
SCREENS = [
    ("clock", "Clock"),
    ("weather", "Weather"),
    ("room", "Room"),
    ("github", "GitHub"),
    ("github-no-token", None),
]

# ---------- Look ----------
SCALE = 6                     # screen pixels -> image pixels
GAP = 1                       # dark gap between neighbouring pixels
LIT = (196, 236, 255)         # the cyan-white of a typical SSD1306
UNLIT = (12, 14, 17)          # off pixels are barely visible, like on the real panel
GLASS = (5, 6, 8)
GLASS_MARGIN = 26
BEZEL = (30, 33, 38)
BEZEL_EDGE = (52, 57, 64)
BEZEL_WIDTH = 22
BEZEL_RADIUS = 30
GLOW_RADIUS = 5
GLOW_STRENGTH = 0.55

CARD = (15, 18, 22)
LABEL = (176, 184, 194)
LABEL_SIZE = 30
CARD_PAD = 44
TILE_GAP = 40
LABEL_GAP = 14


# ---------- Build ----------

def find_libraries(explicit):
    candidates = []
    if explicit:
        candidates.append(Path(explicit))
    if os.environ.get("ARDUINO_DIRECTORIES_USER"):
        candidates.append(Path(os.environ["ARDUINO_DIRECTORIES_USER"]) / "libraries")
    candidates += [
        Path(tempfile.gettempdir()) / "deskpal-acli" / "user" / "libraries",
        Path.home() / "Documents" / "Arduino" / "libraries",
        Path.home() / "Arduino" / "libraries",
    ]
    for lib in candidates:
        gfx = lib / "Adafruit_GFX_Library"
        json = lib / "ArduinoJson" / "src"
        if (gfx / "Adafruit_GFX.cpp").is_file() and (json / "ArduinoJson.h").is_file():
            return gfx, json
    sys.exit("Could not find Adafruit_GFX_Library and ArduinoJson. Looked in:\n  "
             + "\n  ".join(str(c) for c in candidates)
             + "\nPass --libraries <your Arduino libraries folder>.")


def find_compiler(explicit):
    if explicit:
        return shlex.split(explicit)
    if os.environ.get("CXX"):
        return shlex.split(os.environ["CXX"])
    for name in ("g++", "clang++"):
        if shutil.which(name):
            return [name]
    try:
        import ziglang  # noqa: F401  (pip install ziglang: a portable clang)
        return [sys.executable, "-m", "ziglang", "c++"]
    except ImportError:
        pass
    sys.exit("No C++ compiler found. Install g++ or clang++, or run: pip install ziglang")


def build_and_run(cxx, gfx, json, work):
    # Copy the sketch sources so a personal deskpal/secrets.h is never picked up:
    # the images always show the default settings from config.h.
    src = work / "src"
    src.mkdir()
    for name in ("config.h", "github.h", "room.h", "screens.h", "screens.cpp",
                 "weather.h", "weather.cpp"):
        shutil.copy2(SKETCH / name, src / name)

    exe = work / ("render.exe" if os.name == "nt" else "render")
    cmd = cxx + [
        # -w keeps library warnings quiet; zig's libc++ needs the second flag as well.
        "-std=gnu++17", "-O1", "-w", "-Wno-nullability-completeness",
        "-DARDUINO=10819",
        # ArduinoJson is only here so weather.cpp compiles; keep it away from
        # the Arduino String/Stream classes, which the host stubs do not have.
        "-DARDUINOJSON_ENABLE_ARDUINO_STRING=0",
        "-DARDUINOJSON_ENABLE_ARDUINO_STREAM=0",
        "-DARDUINOJSON_ENABLE_ARDUINO_PRINT=0",
        "-DARDUINOJSON_ENABLE_PROGMEM=0",
        "-I", str(HERE / "host"), "-I", str(src), "-I", str(gfx), "-I", str(json),
        str(HERE / "host" / "main.cpp"),
        str(src / "screens.cpp"),
        str(src / "weather.cpp"),
        str(gfx / "Adafruit_GFX.cpp"),
        "-o", str(exe),
    ]
    print("compiling:", " ".join(shlex.quote(c) for c in cmd))
    if subprocess.run(cmd).returncode != 0:
        sys.exit("compile failed")

    frames = work / "frames"
    frames.mkdir()
    if subprocess.run([str(exe), str(frames)]).returncode != 0:
        sys.exit("rendering failed")
    return frames


# ---------- Images ----------

def read_pbm(path):
    """Reads the plain (P1) PBM files main.cpp writes."""
    tokens = path.read_text().split()
    assert tokens[0] == "P1", path
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    assert (w, h) == (OLED_W, OLED_H) and len(bits) == w * h, path
    return [[bits[y * w + x] == "1" for x in range(w)] for y in range(h)]


def oled_tile(pixels):
    """One screen: lit pixels with gaps and a soft glow, behind glass, in a rounded bezel."""
    sw, sh = OLED_W * SCALE, OLED_H * SCALE
    lit = Image.new("RGB", (sw, sh), (0, 0, 0))
    base = Image.new("RGB", (sw, sh), GLASS)
    dl, db = ImageDraw.Draw(lit), ImageDraw.Draw(base)
    for y, row in enumerate(pixels):
        for x, on in enumerate(row):
            box = (x * SCALE, y * SCALE, x * SCALE + SCALE - GAP - 1, y * SCALE + SCALE - GAP - 1)
            if on:
                dl.rectangle(box, fill=LIT)
            else:
                db.rectangle(box, fill=UNLIT)

    glow = lit.filter(ImageFilter.GaussianBlur(GLOW_RADIUS))
    glow = Image.eval(glow, lambda v: int(v * GLOW_STRENGTH))
    screen = ImageChops.lighter(ImageChops.add(base, glow), lit)

    tw = sw + 2 * (GLASS_MARGIN + BEZEL_WIDTH)
    th = sh + 2 * (GLASS_MARGIN + BEZEL_WIDTH)
    tile = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
    d = ImageDraw.Draw(tile)
    d.rounded_rectangle((0, 0, tw - 1, th - 1), BEZEL_RADIUS, fill=BEZEL, outline=BEZEL_EDGE, width=2)
    g = BEZEL_WIDTH
    d.rounded_rectangle((g, g, tw - 1 - g, th - 1 - g), 10, fill=GLASS)
    tile.paste(screen, (g + GLASS_MARGIN, g + GLASS_MARGIN))
    return tile


def load_font(size):
    try:
        return ImageFont.load_default(size=size)  # Pillow 10.1+
    except TypeError:
        pass
    for name in ("DejaVuSans.ttf", "segoeui.ttf", "arial.ttf", "Helvetica.ttc"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def combined(tiles):
    """2x2 grid of labelled tiles on a dark rounded card."""
    font = load_font(LABEL_SIZE)
    tw, th = tiles[0][1].size
    label_h = LABEL_GAP + LABEL_SIZE + 6
    w = 2 * CARD_PAD + 2 * tw + TILE_GAP
    h = 2 * CARD_PAD + 2 * (th + label_h) + TILE_GAP
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, w - 1, h - 1), 36, fill=CARD)
    for i, (label, tile) in enumerate(tiles):
        x = CARD_PAD + (i % 2) * (tw + TILE_GAP)
        y = CARD_PAD + (i // 2) * (th + label_h + TILE_GAP)
        img.alpha_composite(tile, (x, y))
        d.text((x + tw / 2, y + th + LABEL_GAP), label, fill=LABEL, font=font, anchor="mt")
    return img


def rotation_gif(frames, path):
    """One pass through the screen rotation as an animated GIF, at half size.

    GIF has no partial transparency, so the tiles sit on the card colour, and
    all frames share one palette so the colours don't flicker between screens.
    """
    entries = [line.split() for line in (frames / "rotation.txt").read_text().splitlines() if line]
    images, durations = [], []
    for name, ms in entries:
        tile = oled_tile(read_pbm(frames / f"{name}.pbm"))
        flat = Image.new("RGB", tile.size, CARD)
        flat.paste(tile, (0, 0), tile)
        images.append(flat.resize((flat.width // 2, flat.height // 2), Image.LANCZOS))
        durations.append(int(ms))
    palette = images[0].quantize(colors=64, method=Image.Quantize.MEDIANCUT)
    frames_p = [img.quantize(palette=palette, dither=Image.Dither.NONE) for img in images]
    frames_p[0].save(path, save_all=True, append_images=frames_p[1:], duration=durations,
                     loop=0, optimize=True, disposal=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--libraries", help="Arduino libraries folder with Adafruit GFX and ArduinoJson")
    parser.add_argument("--cxx", help='C++ compiler command, e.g. "g++" or "zig c++"')
    parser.add_argument("--out", default=str(DOCS), help="output folder (default: docs/)")
    parser.add_argument("--gif", action="store_true",
                        help="also write rotation.gif, an animated pass through the screens")
    args = parser.parse_args()

    gfx, json = find_libraries(args.libraries)
    cxx = find_compiler(args.cxx)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    # Everything built goes to a temporary folder, so nothing lands in the repo.
    with tempfile.TemporaryDirectory(prefix="deskpal-render-", ignore_cleanup_errors=True) as tmp:
        frames = build_and_run(cxx, gfx, json, Path(tmp))
        tiles = []
        for name, label in SCREENS:
            tile = oled_tile(read_pbm(frames / f"{name}.pbm"))
            path = out / f"screen-{name}.png"
            tile.save(path, optimize=True)
            print("wrote", path)
            if label:
                tiles.append((label, tile))
        if args.gif:
            path = out / "rotation.gif"
            rotation_gif(frames, path)
            print("wrote", path)

    path = out / "screens.png"
    combined(tiles).save(path, optimize=True)
    print("wrote", path)


if __name__ == "__main__":
    main()
