#pragma once
#include <stddef.h>
#include <stdint.h>

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
#define AP_SSID "XIAO-S3-Gatos"
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "gatos2026"
#endif

constexpr const char *FIRMWARE_VERSION = "3.1-video";
constexpr unsigned long CAPTURE_INTERVAL_MS = 125;  // Objetivo: 8 análisis/s.
constexpr uint16_t STREAM_PORT = 81;
constexpr unsigned long LEARNING_INTERVAL_MS = 250;
constexpr unsigned CALIBRATION_FRAMES = 8;
constexpr unsigned MAX_CALIBRATION_ATTEMPTS = 80;
constexpr unsigned long CALIBRATION_TIMEOUT_MS = 25000;
constexpr unsigned STABLE_FRAMES = 3;
constexpr size_t MAX_JPEG_BYTES = 96 * 1024;
