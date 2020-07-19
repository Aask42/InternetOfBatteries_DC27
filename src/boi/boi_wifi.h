#ifndef __boi_wifi_h__
#define __boi_wifi_h__

#include <Arduino.h>
#include "DNSServer.h"                  // Patched lib
#include "ESPAsyncWebServer.h"
#include "boi.h"
#include <pthread.h>
#include "messages.h"
#include "app.h"

class boi;
class Messages;
struct OptionsStruct;

class RequestHandler : public AsyncWebHandler {
    public:
        RequestHandler();
        bool canHandle(AsyncWebServerRequest *request);
        void handleRequest(AsyncWebServerRequest *request);
        void GuestCounter();
        int CurrentGuestCount;
    private:
        Preferences preferences;

};

class boi_wifi
{
public:
    typedef enum WifiModeEnum
    {
        NormalMode = 1,
        BusinessCardMode = 2,
        RickMode = 3,
        PartyMode = 4
    } WifiModeEnum;

    boi_wifi(boi *boiData, Messages *message_handler, WifiModeEnum NewMode);
    ~boi_wifi();

    void handleRoot();
    void monitor_captive_portal();
    void RefreshSettings();

    private:
        boi *_boi;
        char *HandleName;
        char *CaptivePortalName;
        DNSServer *dnsServer;
        AsyncWebServer *server;
        Messages *message_handler;
        RequestHandler *request_handler;

        pthread_t ServerCheckThread;
        Preferences preferences;

        void setup_captive_portal(const OptionsStruct *Options);
        void Reconfigure(const OptionsStruct *Options);
        void DisableWiFi();

        void ActivateBusinessCard();
        void ActivateRick();
        void ActivateParty();
        void ActivateNormal();
};

#endif