#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "ble_bridge.h"
#include "commands.h"

// ClaudeState holds the latest decoded heartbeat from Claude Desktop.
struct ClaudeState {
  uint8_t  sessionsTotal;
  uint8_t  sessionsRunning;
  uint8_t  sessionsWaiting;
  bool     recentlyCompleted;
  uint32_t tokensToday;    // since local midnight (bridge-reported)
  uint32_t tokensSession;  // cumulative since bridge start
  uint32_t lastUpdated;
  char     msg[24];
  bool     connected;
  bool     tokensValid;  // true after at least one heartbeat with token data
  char     promptId[40];
  char     promptTool[20];
  char     promptHint[44];
};

static uint32_t _lastLiveMs  = 0;
static uint32_t _lastBtByteMs = 0;

// Epoch received from desktop time-sync packet. Used by week_tracker.
static uint32_t _syncedEpoch = 0;
static uint32_t _syncedAt    = 0;   // millis() when sync was received

inline uint32_t dataCurrentEpoch() {
  if (_syncedEpoch == 0) return 0;
  return _syncedEpoch + (millis() - _syncedAt) / 1000;
}

inline bool dataConnected() {
  return _lastLiveMs != 0 && (millis() - _lastLiveMs) <= 30000;
}

static void _applyJson(const char* line, ClaudeState* out) {
  Serial.printf("[rx] %.120s\n", line);
  JsonDocument doc;
  if (deserializeJson(doc, line)) { Serial.println("[rx] parse fail"); return; }

  // Command from desktop (status, name, owner, unpair, permission)
  if (commandsHandle(doc)) { _lastLiveMs = millis(); return; }

  // Time sync: {"time":[epoch_sec, tz_offset_sec]}
  JsonArray t = doc["time"];
  if (!t.isNull() && t.size() == 2) {
    uint32_t epoch  = t[0].as<uint32_t>();
    int32_t  tzOff  = t[1].as<int32_t>();
    _syncedEpoch = epoch + tzOff;   // local midnight-aligned epoch
    _syncedAt    = millis();
    _lastLiveMs  = millis();
    return;
  }

  out->sessionsTotal     = doc["total"]     | out->sessionsTotal;
  out->sessionsRunning   = doc["running"]   | out->sessionsRunning;
  out->sessionsWaiting   = doc["waiting"]   | out->sessionsWaiting;
  out->recentlyCompleted = doc["completed"] | false;

  JsonVariant tokVar = doc["tokens"];
  if (!tokVar.isNull()) {
    uint32_t bt = tokVar.as<uint32_t>();
    out->tokensSession = bt;
    statsOnBridgeTokens(bt);
    Serial.printf("[tok-rx] session=%lu\n", (unsigned long)bt);
  }
  JsonVariant todayVar = doc["tokens_today"];
  if (!todayVar.isNull()) {
    out->tokensToday = todayVar.as<uint32_t>();
    out->tokensValid = true;
    Serial.printf("[tok-rx] today=%lu\n", (unsigned long)out->tokensToday);
  }

  const char* m = doc["msg"];
  if (m) { strncpy(out->msg, m, sizeof(out->msg)-1); out->msg[sizeof(out->msg)-1]=0; }

  JsonObject pr = doc["prompt"];
  if (!pr.isNull()) {
    const char* pid = pr["id"]; const char* pt = pr["tool"]; const char* ph = pr["hint"];
    strncpy(out->promptId,   pid ? pid : "", sizeof(out->promptId)-1);   out->promptId[sizeof(out->promptId)-1]=0;
    strncpy(out->promptTool, pt  ? pt  : "", sizeof(out->promptTool)-1); out->promptTool[sizeof(out->promptTool)-1]=0;
    strncpy(out->promptHint, ph  ? ph  : "", sizeof(out->promptHint)-1); out->promptHint[sizeof(out->promptHint)-1]=0;
  } else {
    out->promptId[0] = 0; out->promptTool[0] = 0; out->promptHint[0] = 0;
  }
  out->lastUpdated = millis();
  _lastLiveMs = millis();
}

template<size_t N>
struct _LineBuf {
  char    buf[N];
  uint16_t len = 0;
  void feed(ClaudeState* out) {
    while (bleAvailable()) {
      int c = bleRead();
      if (c < 0) break;
      _lastBtByteMs = millis();
      if (c == '\n' || c == '\r') {
        if (len > 0) { buf[len]=0; if (buf[0]=='{') _applyJson(buf, out); len=0; }
      } else if (len < N-1) {
        buf[len++] = (char)c;
      }
    }
  }
};

static _LineBuf<1024> _btLine;

inline void dataPoll(ClaudeState* out) {
  _btLine.feed(out);
  out->connected = dataConnected();
  if (!out->connected) {
    out->sessionsTotal=0; out->sessionsRunning=0; out->sessionsWaiting=0;
    out->recentlyCompleted=false; out->lastUpdated=millis();
    out->tokensValid = false;
    strncpy(out->msg, "No Claude connected", sizeof(out->msg)-1);
    out->msg[sizeof(out->msg)-1]=0;
  }
}
