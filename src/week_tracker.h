#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

// NVS-backed weekly token accumulation.
// Include from exactly one translation unit (main.cpp).
//
// The BLE API only gives tokens_today (resets at midnight). We accumulate
// daily deltas into a weekly total that survives reboots.

static Preferences _wkPrefs;
static uint32_t _wkStart   = 0;   // epoch of Monday 00:00:00 UTC this week
static uint32_t _wkTokens  = 0;   // tokens accumulated this week
static uint32_t _wkLastDay = 0;   // last seen tokens_today value

// Epoch of the Monday at or before the given UTC epoch.
static uint32_t _mondayOf(uint32_t epoch) {
  // day_of_week: 0=Thu 1970-01-01 is Thursday
  // (epoch/86400 + 4) % 7 gives 0=Mon,1=Tue,...,6=Sun
  uint32_t daysSinceEpoch = epoch / 86400;
  uint32_t dow = (daysSinceEpoch + 4) % 7; // 0=Mon
  return (epoch - dow * 86400) / 86400 * 86400;
}

inline void weekTrackerLoad() {
  _wkPrefs.begin("wklcd", false);  // create namespace on first boot
  _wkStart   = _wkPrefs.getUInt("wk_start", 0);
  _wkTokens  = _wkPrefs.getUInt("wk_tok", 0);
  _wkLastDay = _wkPrefs.getUInt("wk_last", 0);
  _wkPrefs.end();
}

static void _weekTrackerSave() {
  _wkPrefs.begin("wklcd", false);
  _wkPrefs.putUInt("wk_start", _wkStart);
  _wkPrefs.putUInt("wk_tok", _wkTokens);
  _wkPrefs.putUInt("wk_last", _wkLastDay);
  _wkPrefs.end();
}

// Call each second. epochNow=0 means no time sync yet — skip rollover check.
// connected=false skips delta accumulation (prevents phantom tokens when
// claude.tokensToday is 0 before BLE connects, which would reset _wkLastDay).
inline uint32_t weekTrackerUpdate(uint32_t tokensToday, uint32_t epochNow, bool connected) {
  if (epochNow > 0) {
    uint32_t monday = _mondayOf(epochNow);
    if (monday > _wkStart) {
      _wkStart   = monday;
      _wkTokens  = connected ? tokensToday : 0;
      _wkLastDay = connected ? tokensToday : 0;
      _weekTrackerSave();
      return _wkTokens;
    }
  }

  if (!connected) return _wkTokens;

  uint32_t delta;
  if (tokensToday >= _wkLastDay) {
    delta = tokensToday - _wkLastDay;
  } else {
    // Midnight rollover: tokens_today reset to 0 (or small value)
    delta = tokensToday;
  }

  _wkLastDay = tokensToday;

  if (delta == 0) return _wkTokens;

  _wkTokens += delta;
  if (delta >= 100) _weekTrackerSave();

  return _wkTokens;
}

inline uint32_t weekTrackerGet() { return _wkTokens; }
