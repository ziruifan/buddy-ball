#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "ble_bridge.h"
#include "stats.h"

// Minimal command handler — replaces xfer.h from claude-desktop-buddy.
// Handles the commands that Claude Desktop sends to the device.
// Returns true if the JSON was a command (caller should skip state parsing).

static void _cmdAck(const char* what, bool ok) {
  char b[64];
  int len = snprintf(b, sizeof(b), "{\"ack\":\"%s\",\"ok\":%s,\"n\":0}\n",
                     what, ok ? "true" : "false");
  bleWrite((const uint8_t*)b, len);
}

inline bool commandsHandle(JsonDocument& doc) {
  const char* cmd = doc["cmd"];
  if (!cmd) return false;

  if (strcmp(cmd, "status") == 0) {
    const Stats& s = stats();
    char b[320];
    extern uint8_t buddySpeciesIdx();
    int len = snprintf(b, sizeof(b),
      "{\"ack\":\"status\",\"ok\":true,\"n\":0,\"data\":{"
      "\"name\":\"%s\",\"owner\":\"%s\",\"sec\":%s,"
      "\"species\":%u,"
      "\"bat\":{\"pct\":100,\"mV\":3700,\"mA\":0,\"usb\":true},"
      "\"sys\":{\"up\":%lu,\"heap\":%u,\"fsFree\":0,\"fsTotal\":0},"
      "\"stats\":{\"appr\":%u,\"deny\":%u,\"lvl\":%u,\"nap\":%u}"
      "}}\n",
      petName(), ownerName(), bleSecure() ? "true" : "false",
      (unsigned)buddySpeciesIdx(),
      (unsigned long)(millis()/1000), (unsigned)ESP.getFreeHeap(),
      s.approvals, s.denials, s.level, (unsigned)s.napSeconds);
    bleWrite((const uint8_t*)b, len);
    return true;
  }

  if (strcmp(cmd, "name") == 0) {
    const char* n = doc["name"];
    if (n) petNameSet(n);
    _cmdAck("name", true);
    return true;
  }

  if (strcmp(cmd, "owner") == 0) {
    const char* n = doc["name"];
    if (n) ownerSet(n);
    _cmdAck("owner", true);
    return true;
  }

  if (strcmp(cmd, "unpair") == 0) {
    _cmdAck("unpair", true);
    bleClearBonds();
    return true;
  }

  if (strcmp(cmd, "permission") == 0) {
    const char* decision = doc["decision"];
    if (decision) {
      if (strcmp(decision, "once") == 0 || strcmp(decision, "always") == 0)
        statsOnApproval(0);
      else if (strcmp(decision, "deny") == 0)
        statsOnDenial();
    }
    return true;
  }

  if (strcmp(cmd, "species") == 0) {
    // Claude Desktop sends this immediately on connect — must ack or heartbeats stall.
    uint8_t idx = doc["idx"] | 0xFF;
    extern void buddySetSpeciesIdx(uint8_t);
    if (idx != 0xFF) {
      buddySetSpeciesIdx(idx);
      speciesIdxSave(idx);
      Serial.printf("[cmd] species idx=%u\n", idx);
    }
    _cmdAck("species", true);
    return true;
  }

  if (strcmp(cmd, "char_begin") == 0) {
    // GIF character install — not supported on this hardware.
    // Ack with ok:false so Desktop aborts the transfer and falls back to ASCII.
    _cmdAck("char_begin", false);
    return true;
  }

  // Consume any other command (char_end, file, chunk, file_end, …) so it
  // cannot fall through to state parsing and corrupt the ClaudeState fields.
  return true;
}
