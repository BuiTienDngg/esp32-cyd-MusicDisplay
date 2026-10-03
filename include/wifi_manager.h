#pragma once

#include <Arduino.h>

void beginWifi();
void handleWifi();
bool wifiConnected();
bool wifiStackStarted();
String wifiIpAddress();