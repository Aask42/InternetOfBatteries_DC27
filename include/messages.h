#ifndef _messages_h
#define _messages_h

#include <Arduino.h>
#include "ESPAsyncWebServer.h"
#include "mesh.h"
#include "boi/boi.h"
#include "boi/boi_wifi.h"

#define MAX_DIRECT_CONNECTS 25
#define MAX_DIRECT_MESSAGES 50
#define MAX_GLOBAL_MESSAGES 100
#define MAX_NICKNAME_LEN 20
#define MAX_WIFINAME_LEN 20
#define MAX_MESSAGE_LEN 128

class boi;
class boi_wifi;
struct SensorDataStruct;

typedef struct OptionsStruct
{
    bool DisplaySplash;
    char Nickname[20 + 1];
    char WifiName[20 + 1];
    char OriginalWifiName[20 + 1];
    char WifiPassword[20 + 1];
    short WifiTimeout;
    uint8_t BrightnessValue;
    bool Configured;
    bool BrightnessButtons;
} OptionsStruct;

class Messages
{
    public:
        //register/deregister a web socket, all queued messages will be sent accordingly
        virtual void RegisterWebsocket();
        virtual void DeregisterWebSocket();
        virtual const OptionsStruct *GetOptions();
        virtual void DoScan();
        virtual void BrightnessModified();
        
        virtual void HandleSensorData(struct SensorDataStruct *SensorData);
        virtual uint8_t GetPingCount();
        virtual void DoFactoryReset();
};

//message network initialization
Messages *NewMessagesHandler();

#endif