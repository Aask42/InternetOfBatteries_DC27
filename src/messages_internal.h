#ifndef _messages_internal_h
#define _messages_internal_h

#include "messages.h"
#include "ESPAsyncWebServer.h"
#include "boi/boi.h"

class MessagesInternal;
class boi_wifi;
extern MessagesInternal *_globalMessages;
extern pthread_mutex_t message_lock;
extern boi_wifi *BatteryWifi;       //pull in from app.cpp

void static_SendToDeviceFailed(const uint8_t *MAC);
void static_PingReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen);
void static_MessageReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen);
void static_BroadcastMessageReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen);
void static_DeviceConnected(const uint8_t *MAC, const char *Name, int Succeeded);
void SerialPrintMAC(const uint8_t *mac);
void static_handleWebSocket(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

class MessagesInternal : public Messages
{
    public:
        MessagesInternal();

        //register/deregister a web socket, all queued messages will be sent accordingly
        void RegisterWebsocket();
        void DeregisterWebSocket();
        void HandleSensorData(struct SensorDataStruct *SensorData);
        const OptionsStruct *GetOptions();
        void DoScan();
        void BrightnessModified();
        uint8_t GetPingCount();
        void DoFactoryReset();
        
        void PingReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen);
        void MessageReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen);
        void BroadcastMessageReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen);
        void DeviceConnected(const uint8_t *MAC, const char *Name, int Succeeded);
        void SendToDeviceFailed(const uint8_t *MAC);

        void handleWebSocket(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
        void Run();

    private:
        typedef struct DirectConnectStruct
        {
            uint8_t MAC[MAC_SIZE];
            uint8_t GraffitiDone;
            char Nickname[MAX_NICKNAME_LEN + 1];
            char *Messages[MAX_DIRECT_MESSAGES];
        } DirectConnectStruct;

        typedef struct __attribute__((packed)) GraffitiMessageStruct
        {
            GraffitiMessageStruct *next;
            uint8_t MessageLen;
            uint8_t MAC[MAC_SIZE];
            char Nickname[MAX_NICKNAME_LEN];
            char Message[0];
        } GraffitiMessageStruct;

        typedef struct __attribute__((packed)) MessageEntryStruct
        {
            MessageEntryStruct *next;
            size_t Len;
            uint8_t Message[0];
        } MessageEntryStruct;

        void ForceDisconnect(uint8_t *data);
        void Mesh_SendPrivateMessage(uint8_t *data, size_t len, bool GraffitiFlag);
        void AddGlobalMessageToArray(const uint8_t *Data, unsigned int DataLen);
        void AddPrivateMessageToArray(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen, uint8_t Type);
        void SendKnownConnections();
        void SendStoredBroadcastMessages();
        void SendConnectedData();
        void SendGraffiti();
        void ConnectToDevice(uint8_t *data);
        void Flash_GetStoredConnections();
        void Flash_GetStoredGlobalMessages();
        void Flash_StoreNewConnection(DirectConnectStruct *Connection);
        void Flash_RemoveConnection(DirectConnectStruct *Connection);
        void Flash_StoreNewMessage(DirectConnectStruct *Connection, char *Message, uint8_t MessageLen);
        void Flash_StoreNewNickname(DirectConnectStruct *Connection);
        void Flash_StoreGraffitiMessage(GraffitiMessageStruct *Graffiti);
        void Flash_GetStoredGraffitiMessages();
        void Flash_StoreGraffitiFlag(DirectConnectStruct *Connection);
        bool handleWebSocketData(uint8_t *data, size_t len);

        int FindConnectEntry(const uint8_t *MAC);
        void ReportNewNickname(char *Nickname);
        void MACToStr(char *OutBuf, const uint8_t MAC[MAC_SIZE]);
        void FactoryReset();

        void SetBrightness(uint8_t BrightnessValue);

        //data storage when the websocket isn't connected
        DirectConnectStruct *DirectConnections[MAX_DIRECT_CONNECTS];
        char *BroadcastMessages[MAX_GLOBAL_MESSAGES];
        GraffitiMessageStruct *Graffiti;
        bool NewMeshMessages;
        bool StatLEDActive;
        pthread_t MessageThread;
        MessageEntryStruct *MessagesBegin;
        MessageEntryStruct *MessagesTail;

        //internal classes we need
        MeshNetwork *Mesh;
        AsyncWebSocket *socket_server;
        AsyncWebServer *server;
        uint8_t WebSocketID;
        bool connected;
        OptionsStruct Options;
        Preferences preferences;
        unsigned long LastAccessTime;
        uint8_t PingCount;
};
#endif