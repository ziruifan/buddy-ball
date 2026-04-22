#include "display.h"
#include "buddy.h"
#include <TFT_eSPI.h>

static TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft);

// ── Ring geometry ─────────────────────────────────────────────────────────────
// Week ring drawn first (base), session ring drawn on top at the same position.
static const uint16_t RING_ROUT = 115, RING_RIN = 105;
static const uint32_t SESSION_CAP = 50000, WEEK_CAP = 1000000;
static const uint16_t COL_SESSION_BG = 0x3166;
static const uint16_t COL_SESSION    = 0xffff;
static const uint16_t COL_WEEK       = 0xdbaa;

static bool     _needFullRedraw  = true;
static uint32_t _drawnSessionDeg = UINT32_MAX;
static uint32_t _drawnWeekDeg    = UINT32_MAX;

static void _drawRings(uint32_t sessionTok, uint32_t weekTok) {
  uint32_t sDeg = (uint32_t)((float)min(sessionTok, SESSION_CAP) / SESSION_CAP * 360.0f);
  uint32_t wDeg = (uint32_t)((float)min(weekTok,   WEEK_CAP)   / WEEK_CAP   * 360.0f);

  if (!_needFullRedraw && sDeg == _drawnSessionDeg && wDeg == _drawnWeekDeg) return;
  _drawnSessionDeg = sDeg;
  _drawnWeekDeg    = wDeg;

  // Dark background — full 360° (shows as empty session ring)
  tft.drawSmoothArc(120, 120, RING_ROUT, RING_RIN, 0, 360, COL_SESSION_BG, TFT_BLACK, false);
  // Session arc (red base layer)
  if (sDeg > 0)
    tft.drawSmoothArc(120, 120, RING_ROUT, RING_RIN, 0, sDeg, COL_SESSION, TFT_BLACK, true);
  // Week arc (blue top layer)
  if (wDeg > 0)
    tft.drawSmoothArc(120, 120, RING_ROUT, RING_RIN, 0, wDeg, COL_WEEK, TFT_BLACK, true);
}

// ── 565 color helper ──────────────────────────────────────────────────────────
static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3);
}

// ── Public API ────────────────────────────────────────────────────────────────

void displayInit() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  pinMode(40, OUTPUT);
  digitalWrite(40, HIGH);

  spr.setColorDepth(16);
  void* buf = spr.createSprite(144, 144);
  if (!buf) Serial.printf("[disp] WARN: sprite alloc failed (144x144 = %d bytes)\n", 144*144*2);
  else      Serial.printf("[disp] sprite OK, free heap %u\n", ESP.getFreeHeap());

  buddyInit();
  _needFullRedraw = true;
}

void displayDrawRings(uint32_t sessionTok, uint32_t weekTok) {
  _drawRings(sessionTok, weekTok);
  _needFullRedraw = false;
}

void displayDrawBlob(BuddyState state, uint32_t /*ms*/) {
  if (!spr.created()) return;
  buddyTick((uint8_t)state);

  tft.fillCircle(120, 120, 96, TFT_BLACK);
  spr.pushSprite(48, 48, TFT_BLACK);
}

void displayShowPasskey(uint32_t pk) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("PAIRING", 120, 80);

  char buf[8];
  snprintf(buf, sizeof(buf), "%06lu", (unsigned long)pk);
  tft.setTextSize(4);
  tft.setTextColor(rgb(100, 220, 255), TFT_BLACK);
  tft.drawString(buf, 120, 120);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Enter in Claude Desktop", 120, 165);
}

void displayForceRedraw() {
  tft.fillScreen(TFT_BLACK);
  _needFullRedraw = true;
  _drawnSessionDeg = UINT32_MAX;
  _drawnWeekDeg    = UINT32_MAX;
  buddyInvalidate();
}
