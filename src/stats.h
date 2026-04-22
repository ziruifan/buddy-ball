#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Include from exactly one translation unit (main.cpp).

static const uint32_t TOKENS_PER_LEVEL = 50000;

struct Stats {
  uint32_t napSeconds;
  uint16_t approvals;
  uint16_t denials;
  uint16_t velocity[8];
  uint8_t  velIdx;
  uint8_t  velCount;
  uint8_t  level;
  uint32_t tokens;
};

static Stats _stats;
static Preferences _prefs;
static bool _dirty = false;

inline void statsLoad() {
  _prefs.begin("buddy", false);  // false = create namespace on first boot
  _stats.napSeconds = _prefs.getUInt("nap", 0);
  _stats.approvals  = _prefs.getUShort("appr", 0);
  _stats.denials    = _prefs.getUShort("deny", 0);
  _stats.velIdx     = _prefs.getUChar("vidx", 0);
  _stats.velCount   = _prefs.getUChar("vcnt", 0);
  _stats.level      = _prefs.getUChar("lvl", 0);
  _stats.tokens     = _prefs.getUInt("tok", 0);
  if (_prefs.isKey("vel"))
    _prefs.getBytes("vel", _stats.velocity, sizeof(_stats.velocity));
  else
    memset(_stats.velocity, 0, sizeof(_stats.velocity));
  _prefs.end();
  if (_stats.tokens == 0 && _stats.level > 0)
    _stats.tokens = (uint32_t)_stats.level * TOKENS_PER_LEVEL;
}

inline void statsSave() {
  if (!_dirty) return;
  _prefs.begin("buddy", false);
  _prefs.putUInt("nap", _stats.napSeconds);
  _prefs.putUShort("appr", _stats.approvals);
  _prefs.putUShort("deny", _stats.denials);
  _prefs.putUChar("vidx", _stats.velIdx);
  _prefs.putUChar("vcnt", _stats.velCount);
  _prefs.putUChar("lvl", _stats.level);
  _prefs.putUInt("tok", _stats.tokens);
  _prefs.putBytes("vel", _stats.velocity, sizeof(_stats.velocity));
  _prefs.end();
  _dirty = false;
}

inline void statsOnApproval(uint32_t secondsToRespond) {
  _stats.approvals++;
  _stats.velocity[_stats.velIdx] = (uint16_t)min(secondsToRespond, 65535u);
  _stats.velIdx = (_stats.velIdx + 1) % 8;
  if (_stats.velCount < 8) _stats.velCount++;
  _dirty = true; statsSave();
}

static uint32_t _lastBridgeTokens = 0;
static bool _tokensSynced = false;
static bool _levelUpPending = false;

inline void statsOnBridgeTokens(uint32_t bridgeTotal) {
  if (!_tokensSynced) {
    _lastBridgeTokens = bridgeTotal;
    _tokensSynced = true;
    return;
  }
  if (bridgeTotal < _lastBridgeTokens) {
    _lastBridgeTokens = bridgeTotal;
    return;
  }
  uint32_t delta = bridgeTotal - _lastBridgeTokens;
  _lastBridgeTokens = bridgeTotal;
  if (delta == 0) return;

  uint8_t lvlBefore = (uint8_t)(_stats.tokens / TOKENS_PER_LEVEL);
  _stats.tokens += delta;
  uint8_t lvlAfter = (uint8_t)(_stats.tokens / TOKENS_PER_LEVEL);

  if (lvlAfter > lvlBefore) {
    _stats.level = lvlAfter;
    _levelUpPending = true;
    _dirty = true; statsSave();
  }
}

inline bool statsPollLevelUp() {
  bool r = _levelUpPending;
  _levelUpPending = false;
  return r;
}

inline void statsOnDenial() { _stats.denials++; _dirty = true; statsSave(); }
inline void statsMarkDirty() { _dirty = true; }

inline uint16_t statsMedianVelocity() {
  if (_stats.velCount == 0) return 0;
  uint16_t tmp[8];
  memcpy(tmp, _stats.velocity, sizeof(tmp));
  uint8_t n = _stats.velCount;
  for (uint8_t i = 1; i < n; i++) {
    uint16_t k = tmp[i]; int8_t j = i - 1;
    while (j >= 0 && tmp[j] > k) { tmp[j+1] = tmp[j]; j--; }
    tmp[j+1] = k;
  }
  return tmp[n/2];
}

inline uint8_t statsMoodTier() {
  uint16_t vel = statsMedianVelocity();
  int8_t tier;
  if (vel == 0) tier = 2;
  else if (vel < 15) tier = 4;
  else if (vel < 30) tier = 3;
  else if (vel < 60) tier = 2;
  else if (vel < 120) tier = 1;
  else tier = 0;
  uint16_t a = _stats.approvals, d = _stats.denials;
  if (a + d >= 3) {
    if (d > a) tier -= 2;
    else if (d * 2 > a) tier -= 1;
  }
  if (tier < 0) tier = 0;
  return (uint8_t)tier;
}

inline uint8_t statsFedProgress() {
  return (uint8_t)((_stats.tokens % TOKENS_PER_LEVEL) / (TOKENS_PER_LEVEL / 10));
}

inline const Stats& stats() { return _stats; }

// --- Pet name / owner -------------------------------------------------------

static char _petName[24] = "Buddy";
static char _ownerName[32] = "";

static void _safeCopy(char* dst, size_t dstLen, const char* src) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j < dstLen - 1; i++) {
    char c = src[i];
    if (c != '"' && c != '\\' && c >= 0x20) dst[j++] = c;
  }
  dst[j] = 0;
}

inline void petNameLoad() {
  _prefs.begin("buddy", false);
  if (_prefs.isKey("petname")) _prefs.getString("petname", _petName, sizeof(_petName));
  if (_prefs.isKey("owner"))   _prefs.getString("owner", _ownerName, sizeof(_ownerName));
  _prefs.end();
}

inline void petNameSet(const char* name) {
  _safeCopy(_petName, sizeof(_petName), name);
  _prefs.begin("buddy", false);
  _prefs.putString("petname", _petName);
  _prefs.end();
}

inline const char* petName() { return _petName; }

inline void ownerSet(const char* name) {
  _safeCopy(_ownerName, sizeof(_ownerName), name);
  _prefs.begin("buddy", false);
  _prefs.putString("owner", _ownerName);
  _prefs.end();
}

inline const char* ownerName() { return _ownerName; }

// --- Species index persistence -----------------------------------------------
// Uses a local Preferences handle so this is safe to call from any TU.

inline uint8_t speciesIdxLoad() {
  Preferences p;
  p.begin("buddy", false);
  uint8_t idx = p.getUChar("species", 3);  // default: blob
  p.end();
  return idx;
}

inline void speciesIdxSave(uint8_t idx) {
  Preferences p;
  p.begin("buddy", false);
  p.putUChar("species", idx);
  p.end();
}
