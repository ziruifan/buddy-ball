#pragma once
#include <stdint.h>
#include <stddef.h>

// Nordic UART Service-compatible BLE bridge.
// Service UUID  6e400001-b5a3-f393-e0a9-e50e24dcca9e
// RX char       6e400002-...  (desktop → device, WRITE)
// TX char       6e400003-...  (device → desktop, NOTIFY)

void bleInit(const char* deviceName);
bool bleConnected();
bool bleSecure();
// Non-zero while a 6-digit pairing passkey should be on screen.
uint32_t blePasskey();
void bleClearBonds();
size_t bleAvailable();
int bleRead();
size_t bleWrite(const uint8_t* data, size_t len);
