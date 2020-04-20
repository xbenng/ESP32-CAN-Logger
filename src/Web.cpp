#include <WiFi.h>
#include <WiFiAP.h>
// #include <WiFiClient.h>
// #include <WebServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// #include <ESPmDNS.h>

#include "FS.h"
#include "SPIFFS.h"
#include "SD_MMC.h"


namespace Web {

const char *ssid = "logger";
const char *password = "tangerine";

static AsyncWebServer server(80);

void setup()
{
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, password);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Hello, world");
    });

    server.serveStatic("/web", SPIFFS, "/web");
    server.serveStatic("/sd", SD_MMC, "/");

    server.begin();

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

}



}  // namespace Web