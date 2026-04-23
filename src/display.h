#pragma once
#include <Arduino.h>

enum BuddyState {
  BS_SLEEP,
  BS_IDLE,
  BS_BUSY,
  BS_ATTENTION,
  BS_CELEBRATE
};

void displayInit();
// Draw session (red, top) and week (blue, base) overlapping at the same ring position.
void displayDrawRings(uint32_t sessionTok, uint32_t weekTok);
// Animate the buddy. Call at ~10fps.
void displayDrawBuddy(BuddyState state, uint32_t ms);
// Replace the display with a large passkey number during BLE pairing.
void displayShowPasskey(uint32_t pk);
// Redraw background + rings after passkey screen clears.
void displayForceRedraw();
