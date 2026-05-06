#pragma once

#include <ESPAsyncWebServer.h>

void initWebServer();
void webServerLoop();
void sendState(AsyncWebSocketClient *client=nullptr);
