#pragma once

// Opcional: crear config.local.h para reemplazar estas opciones.
#if __has_include("config.local.h")
#include "config.local.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef AP_SSID
#define AP_SSID "ESP32CAM-Gatos"
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "gatos2026"
#endif

constexpr unsigned long CAPTURE_INTERVAL_MS = 500;
constexpr unsigned CALIBRATION_FRAMES = 8;
constexpr unsigned STABLE_FRAMES = 3;
constexpr size_t MAX_JPEG_BYTES = 64 * 1024;

