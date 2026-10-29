#pragma once

// Setup portal: deskpal opens its own Wi-Fi network (PORTAL_AP_NAME). Join it
// with a phone and a setup page pops up, where you pick your Wi-Fi and enter
// the GitHub user, token and location. Saving stores everything (settings.h)
// and restarts the device.
//
// The portal runs alongside loop(), so the screen keeps working while it is open.

// Opens the portal. Does nothing if it is already open.
void startPortal();

bool portalActive();

// Call on every loop(). Handles the web page, saving and the timeout.
void updatePortal();
