#include <Arduino.h>
#include "ble_bridge.h"
#include "stats.h"
#include "week_tracker.h"
#include "data.h"
#include "display.h"

static char     btName[20]       = "Claude";
static ClaudeState claude;
static BuddyState buddyState     = BS_SLEEP;
static uint32_t celebrateUntil   = 0;
static bool     passkeyWasShown  = false;
static uint32_t lastShownPasskey = 0;

static BuddyState stateFromClaude(const ClaudeState& t) {
  if (!t.connected)           return BS_SLEEP;
  if (t.sessionsWaiting > 0)  return BS_ATTENTION;
  if (t.sessionsRunning > 0)  return BS_BUSY;
  return BS_IDLE;
}

void setup() {
  Serial.begin(115200);

  // Display + sprite allocation FIRST — sprite needs 41KB contiguous heap
  // that BLE would otherwise fragment. TFT init is ~20ms, not a real delay.
  displayInit();

  // BLE second — still well before the user opens Hardware Buddy
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_BT);
  snprintf(btName, sizeof(btName), "Claude-%02X%02X", mac[4], mac[5]);
  bleInit(btName);

  // Load persistent state
  statsLoad();
  petNameLoad();
  weekTrackerLoad();

  memset(&claude, 0, sizeof(claude));

  // Initial display: sleeping blob + empty rings
  displayDrawRings(0, 0);
  displayDrawBlob(BS_SLEEP, 0);

  Serial.printf("[boot] %s ready\n", btName);
}

void loop() {
  uint32_t now = millis();

  dataPoll(&claude);

  // Determine display state
  BuddyState newState = stateFromClaude(claude);

  // Level-up one-shot celebrate
  if (statsPollLevelUp()) {
    celebrateUntil = now + 3000;
  }
  if (now < celebrateUntil) newState = BS_CELEBRATE;

  buddyState = newState;

  // Passkey screen takes over while pairing — only redraw on change
  uint32_t pk = blePasskey();
  if (pk) {
    if (pk != lastShownPasskey) {
      displayShowPasskey(pk);
      lastShownPasskey = pk;
    }
    passkeyWasShown = true;
    return;
  }
  if (passkeyWasShown) {
    lastShownPasskey = 0;
    displayForceRedraw();
    passkeyWasShown = false;
  }

  // Rings: update at most 1Hz
  static uint32_t lastRingMs = 0;
  if (now - lastRingMs >= 1000) {
    uint32_t epochNow = dataCurrentEpoch();
    uint32_t weekTok  = weekTrackerUpdate(claude.tokensToday, epochNow, claude.connected && claude.tokensValid);
    displayDrawRings(claude.tokensSession, weekTok);
    lastRingMs = now;

    Serial.printf("[tok] session=%lu today=%lu week=%lu\n",
                  (unsigned long)claude.tokensSession,
                  (unsigned long)claude.tokensToday,
                  (unsigned long)weekTok);
  }

  // Blob: ~10fps
  static uint32_t lastBlobMs = 0;
  if (now - lastBlobMs >= 100) {
    displayDrawBlob(buddyState, now);
    lastBlobMs = now;
  }

  // Flush NVS if dirty (level-up already saves; this is a safety valve)
  statsSave();
}
