// Diagnostics: a log kept in memory, the cause of the last restart and the
// wall clock (NTP).
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Call first in setup(): allocates the log and reads the restart cause.
void diagBegin();
// Prints to the serial port and keeps the line for the web interface.
void logMsg(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// Oldest first: [{"t": uptime s, "at": epoch or 0, "msg": "..."}].
void logToJson(JsonArray out);

// Restart cause as an id (poweron, software, panic, task_wdt, brownout...)
// and as a label in the configured language for Home Assistant.
const char* resetReasonId();
const char* resetReasonLabel();
// Why this firmware restarted itself last time ("" when it did not).
const String& previousRestartNote();
// Restarts with a note shown after the boot.
[[noreturn]] void diagRestart(const String& why);
// Restarts caused by the camera since power on, survives a restart.
uint8_t cameraRestartCount();
void setCameraRestartCount(uint8_t n);

// Starts or changes the NTP server (empty: no clock).
void clockBegin(const String& server);
// Seconds since 1970 (UTC), 0 while the clock is not set.
uint32_t epochNow();
// "2026-11-14T08:30:00+00:00", "" for 0.
String isoTime(uint32_t epoch);
