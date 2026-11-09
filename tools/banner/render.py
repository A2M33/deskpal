"""Render the README images with Playwright and Microsoft Edge.

    banner.html  -> docs/banner.png  (1280x640)
    screens.html -> docs/screens.png (the four main screens, at 2x)

Both pages use the screen images that tools/render/render.py writes to docs/,
so run that first if the screens changed.

    pip install playwright
    python tools/banner/render.py
"""
from pathlib import Path

from playwright.sync_api import sync_playwright

HERE = Path(__file__).resolve().parent
DOCS = HERE.parent.parent / "docs"


def main() -> None:
    with sync_playwright() as p:
        browser = p.chromium.launch(channel="msedge")

        page = browser.new_page(viewport={"width": 1280, "height": 640}, color_scheme="dark")
        page.goto((HERE / "banner.html").as_uri())
        page.evaluate("document.fonts.ready")
        page.screenshot(path=str(DOCS / "banner.png"))
        print(DOCS / "banner.png")

        page = browser.new_page(viewport={"width": 800, "height": 400}, color_scheme="dark",
                                device_scale_factor=2)
        page.goto((HERE / "screens.html").as_uri())
        page.evaluate("document.fonts.ready")
        page.locator("body").screenshot(path=str(DOCS / "screens.png"))
        print(DOCS / "screens.png")

        browser.close()


if __name__ == "__main__":
    main()
