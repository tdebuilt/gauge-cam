// Firmware updates from the GitHub releases (HTTPS, checked certificates)
// and the confirmation of a new firmware: a firmware that crashes or cannot
// reach Wi-Fi before it is confirmed is replaced by the previous one.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

void otaBegin();
// Daily check, and runs a requested installation (blocks for about a minute).
void otaLoop();
void otaRequestCheck();
void otaRequestInstall();
bool otaBusy();
// Marks the running firmware as good (no rollback from now on).
void otaConfirm(const char* why);
// True while a freshly installed firmware has not been confirmed yet.
bool otaPendingVerify();
// installed, latest, available, checked_s, error, progress, release_url...
void otaToJson(JsonObject out);
// The Home Assistant "update" entity state.
void otaToHomeAssistant(JsonObject out);
