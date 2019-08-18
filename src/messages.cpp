#include "messages_internal.h"
#include "app.h"

MessagesInternal *_globalMessages;
pthread_mutex_t message_lock;

Messages *NewMessagesHandler()
{
    MessagesInternal *Msg = new MessagesInternal();
    return Msg;
}

void *static_run_messages(void *)
{
    if(_globalMessages)
        _globalMessages->Run();

    return 0;
}

void MessagesInternal::Run()
{
    MessageEntryStruct *CurMessage;
    unsigned long LastPingTime = 0;
    unsigned long LastScanTime = 0;

    Serial.println("Message thread running\n");
    while(1)
    {
        //if our flag is set about new mesh messages then flash the stat led otherwise
        //turn it off
        if(this->NewMeshMessages && !this->StatLEDActive)
        {
            this->StatLEDActive = 1;
            LEDHandler->StartScript(LED_STAT_FLASH, 0);
        }
        else if(!this->NewMeshMessages && this->StatLEDActive)
        {
            this->StatLEDActive = 0;
            LEDHandler->StopScript(LED_STAT_FLASH);
            LEDHandler->StartScript(LED_STAT_OFF, 0);
        }

        if(!this->MessagesBegin)
        {
            //if the socket server is registered and too much time has passed
            //then shut down the web side
            if((this->Options.WifiTimeout > 0) && this->socket_server && ((esp_timer_get_time() - this->LastAccessTime) / 1000000ULL) > this->Options.WifiTimeout)
            {
                //delete the wifi so everything shuts down for the web server
                delete BatteryWifi;
                BatteryWifi = 0;

                //yield and delay a couple seconds then continue so that everything should be updated
                yield();
                delay(2000);
                continue;
            }

            //if we have a websocket and it's been 45 seconds then ping
            if(this->socket_server && this->connected && ((esp_timer_get_time() - LastPingTime) > 45000000ULL))
            {
                LastPingTime = esp_timer_get_time();
                this->socket_server->binaryAll("P", 1);
            }

            //if we have a websocket and it's been 30 seconds then scan
            if(this->socket_server && this->connected && ((esp_timer_get_time() - LastScanTime) > 30000000ULL))
            {
                LastScanTime = esp_timer_get_time();
                this->DoScan();
            }

            yield();

            //if not connected then delay a full 2 seconds
            if(!this->connected)
                delay(2000);
            else
                delay(100);
            continue;
        }

        //cycle until we get through all messages
        while(this->MessagesBegin)
        {
            //lock the mutex when we remove an entry from the list
            pthread_mutex_lock(&message_lock);
            CurMessage = this->MessagesBegin;
            this->MessagesBegin = CurMessage->next;

            //if the last entry then wipe out the tail
            if(this->MessagesTail == CurMessage)
                this->MessagesTail = 0;
            pthread_mutex_unlock(&message_lock);

            //now process the message itself and then release it's memory
            //we don't check this earlier as we don't want to flood stale messages to a new connection
            //this will just cause the messages to be ignored and released upon websocket close
            if(this->connected)
            {
                //if handle indicates we should update time then update
                if(this->handleWebSocketData(CurMessage->Message, CurMessage->Len))
                {
                    //ping is kept seperate so we know when we last pinged vs last actual command
                    this->LastAccessTime = esp_timer_get_time();
                    LastPingTime = this->LastAccessTime;
                }
            }

            free(CurMessage);

            //yield incase anything needs to process
            yield();
        }
    }
}

void MessagesInternal::ForceDisconnect(uint8_t *data)
{
    //find our connection
    int ConnID = this->FindConnectEntry(data);
    if(ConnID < 0)
        return;

    //tell the mesh to delete it
    this->Mesh->ForceDisconnect(this->DirectConnections[ConnID]->MAC);

    //tell flash to wipe it out
    this->Flash_RemoveConnection(this->DirectConnections[ConnID]);
}

void MessagesInternal::DoFactoryReset()
{
    uint8_t ConnectCount;
    uint8_t i;
    char MACStr[13];
    uint8_t MAC[MAC_SIZE];
    char key[10];

    //wipe out all NVS stored data

    //wipe out mesh
    this->preferences.begin("mesh");
    this->preferences.clear();
    this->preferences.end();

    //wipe out each of our connections
    this->preferences.begin("config");
    this->preferences.clear();
    this->preferences.end();

    //wipe out each of our connections
    this->preferences.begin("messages");
    ConnectCount = this->preferences.getUChar("count", 0);
    this->preferences.end();

    for(i = 0; i < ConnectCount; i++)
    {
        //get the mac entry
        sprintf(key, "%d-mac", i);
        this->preferences.begin("messages");
        this->preferences.getBytes(key, MAC, MAC_SIZE);
        this->preferences.end();

        //generate the string of the mac
        MACToStr(MACStr, MAC);
        MACStr[12] = 0;

        //remove the preference entry
        this->preferences.begin(MACStr);
        this->preferences.clear();
        this->preferences.end();
    }

    //wipe out the main entry for messages
    this->preferences.begin("messages");
    this->preferences.clear();
    this->preferences.end();

    //wipe out the global entry
    this->preferences.begin("global-msgs");
    this->preferences.clear();
    this->preferences.end();

    //wipe out options
    this->preferences.begin("options");
    this->preferences.clear();
    this->preferences.end();

    //wipe out boi
    this->preferences.begin("boi");
    this->preferences.clear();
    this->preferences.end();

    /*
    //wipe out graffiti
    this->preferences.begin("graffiti");
    this->preferences.clear();
    this->preferences.end();
    */
}

void MessagesInternal::ConnectToDevice(uint8_t *data)
{
    int i;

    //if we have too many connections then fail
    for(i = 0; i < MAX_DIRECT_CONNECTS; i++)
    {
        //if an entry is empty then we have a slot to use
        if(!this->DirectConnections[i])
            break;
    }
    yield();

    Serial.println("Connecting to device");

    //if we didn't find an empty entry then fail
    if(i == MAX_DIRECT_CONNECTS)
        this->socket_server->binaryAll("eToo many direct connections");
    else
        Mesh->Connect(data);
}

uint8_t MessagesInternal::GetPingCount()
{
    return this->PingCount;
}

const OptionsStruct *MessagesInternal::GetOptions()
{
    return &this->Options;
}

void SerialPrintMAC(const uint8_t *mac)
{
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int MessagesInternal::FindConnectEntry(const uint8_t *MAC)
{
    int i;

    //find the right connection
    yield();
    for(i = 0; i < MAX_DIRECT_CONNECTS; i++)
    {
        if(this->DirectConnections[i] && (memcmp(this->DirectConnections[i]->MAC, MAC, MAC_SIZE) == 0))
            return i; //found the entry
    }

    return -1;
}

void MessagesInternal::AddGlobalMessageToArray(const uint8_t *Data, unsigned int DataLen)
{
    //add in a the message
    if(this->BroadcastMessages[0])
        free(this->BroadcastMessages[0]);

    char *NewMessage = (char *)malloc(DataLen + sizeof(uint16_t));
    *(uint16_t *)&NewMessage[0] = DataLen;
    memcpy(&NewMessage[sizeof(uint16_t)], Data, DataLen);

    //shuffle all memory over to make room for this entry
    memmove(&this->BroadcastMessages[0], &this->BroadcastMessages[1], sizeof(this->BroadcastMessages) - sizeof(this->BroadcastMessages[0]));
    this->BroadcastMessages[MAX_GLOBAL_MESSAGES - 1] = NewMessage;

    //update flash
    this->Flash_StoreNewMessage(0, NewMessage, DataLen + sizeof(uint16_t));
}

void MessagesInternal::AddPrivateMessageToArray(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen, uint8_t Type)
{
    int EntryID;
    DirectConnectStruct *CurEntry;

    EntryID = this->FindConnectEntry(From_MAC);
    if(EntryID < 0)
        return;

    //add in a the message
    CurEntry = this->DirectConnections[EntryID];
    if(CurEntry->Messages[0])
        free(CurEntry->Messages[0]);

    char *NewMessage = (char *)malloc(DataLen + sizeof(uint16_t) + 1);
    *(uint16_t *)&NewMessage[0] = DataLen + 1;
    NewMessage[2] = Type;
    memcpy(&NewMessage[sizeof(uint16_t) + 1], Data, DataLen);

    //shuffle all memory over to make room for this entry
    memmove(&CurEntry->Messages[0], &CurEntry->Messages[1], sizeof(CurEntry->Messages) - sizeof(CurEntry->Messages[0]));
    CurEntry->Messages[MAX_DIRECT_MESSAGES - 1] = NewMessage;

    //update flash
    this->Flash_StoreNewMessage(this->DirectConnections[EntryID], NewMessage, DataLen + 1 + sizeof(uint16_t));
}

MessagesInternal::MessagesInternal()
{
    //initialize mesh
    MeshNetwork::MeshNetworkData MeshInitData;
    MeshNetwork::MeshInitErrors MeshInitialized;

    //wipe out all of our data
    memset(this->DirectConnections, 0, sizeof(this->DirectConnections));
    memset(this->BroadcastMessages, 0, sizeof(this->BroadcastMessages));
    this->Mesh = 0;
    this->socket_server = 0;
    this->connected = 0;
    this->Graffiti = 0;
    this->MessagesBegin = 0;
    this->MessagesTail = 0;
    this->LastAccessTime = 0;
    this->NewMeshMessages = 0;
    this->StatLEDActive = 0;
    memset(&this->Options, 0, sizeof(this->Options));

    this->preferences.begin("config");
    this->preferences.getBytes("options", &this->Options, sizeof(this->Options));
    this->preferences.end();

    if(!strlen(this->Options.WifiName))
    {
        //generate a new random name, we only have 80 badges so highly unlikely we will collide
        memcpy(this->Options.WifiName, "I'm A Battery! ", 15);
        this->Options.WifiName[15] = (esp_random() % 25) + 'A';
        this->Options.WifiName[16] = (esp_random() % 10) + '0';
        this->Options.WifiName[17] = (esp_random() % 10) + '0';
        this->Options.WifiName[18] = (esp_random() % 10) + '0';
        this->Options.WifiName[19] = 0;

        //store it
        this->preferences.begin("config");
        this->preferences.putBytes("options", &this->Options, sizeof(this->Options));
        this->preferences.end();
    }

    //if options are not configured then configure them, we handle the wifi name seperately so that it is stored
    //and reloaded with the same value
    if(!this->Options.Configured)
    {
        Serial.println("Options not configured yet, setting defaults");
        this->Options.DisplaySplash = 1;
        memcpy(this->Options.Nickname, this->Options.WifiName, MAX_NICKNAME_LEN);
        this->Options.WifiTimeout = -1;
        if(!this->Options.BrightnessValue)
            this->Options.BrightnessValue = LEDHandler->GetLEDBrightness() * 200;   //get whatever the default is
        else
            LEDHandler->SetLEDBrightness((float)this->Options.BrightnessValue / 200.0);

        //indicate they need to pay attention to the wifi
        this->NewMeshMessages = 1;
    }
    else
        this->SetBrightness(this->Options.BrightnessValue);

    //load up our stored information
    //we really should pull this from mesh but I don't have time to sync them so cross our fingers we don't desync
    this->Flash_GetStoredConnections();
    this->Flash_GetStoredGlobalMessages();
    this->Flash_GetStoredGraffitiMessages();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&cfg) != ESP_OK)
    {
        Serial.println("Error initializing wifi");
    }

    if(esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        Serial.println("Error setting wifi to STA");

    if(esp_wifi_start() != ESP_OK)
        Serial.println("Error starting wifi in STA mode");

    _globalMessages = this;

    //initialize the mesh network configuration
    MeshInitData.BroadcastMask[0] = 3;
    MeshInitData.BroadcastMask[1] = 7;
    MeshInitData.BroadcastMask[2] = 24;
    MeshInitData.BroadcastLFSR = 0xace2b13a;
    MeshInitData.DiffieHellman_P = 4169116887;
    MeshInitData.DiffieHellman_G = 3889611491;
    MeshInitData.SendFailedCallback = static_SendToDeviceFailed;
    MeshInitData.ConnectedCallback = static_DeviceConnected;
    MeshInitData.PingCallback = static_PingReceived;
    MeshInitData.ReceiveMessageCallback = static_MessageReceived;
    MeshInitData.BroadcastMessageCallback = static_BroadcastMessageReceived;

    this->Mesh = NewMeshNetwork(&MeshInitData, &MeshInitialized);
    if(!this->Mesh || (MeshInitialized != MeshNetwork::MeshInitErrors::MeshInitialized))
    {
        Serial.printf("Error initializing mesh: %d\n", (int)MeshInitialized);
        this->Mesh = 0;   //guarantee not setup
    }
    else
    {
        Serial.print("MAC used by mesh: ");
        SerialPrintMAC(Mesh->GetMAC());
        Serial.print("\n");
    }

    //set our nickname for the ping data response
    this->Mesh->SetPingData((uint8_t *)this->Options.Nickname, strlen(this->Options.Nickname));

    //setup our thread that will parse any websocket data
    pthread_mutex_init(&message_lock, NULL);
    if(pthread_create(&this->MessageThread, NULL, static_run_messages, 0))
    {
        //failed
        Serial.println("Failed to setup message thread");
        _globalMessages = 0;
        return;
    }
}

void MessagesInternal::MACToStr(char *OutBuf, const uint8_t MAC[MAC_SIZE])
{
    int i;
    for(i = 0; i < MAC_SIZE; i++)
    {
        OutBuf[i*2] = ((MAC[i] & 0xf0) >> 4) + 0x30;
        if(OutBuf[i*2] > 0x39)
            OutBuf[i*2] += 7;
        OutBuf[(i*2)+1] = (MAC[i] & 0x0f) + 0x30;
        if(OutBuf[(i*2)+1] > 0x39)
            OutBuf[(i*2)+1] += 7;        
    }
    yield();
}

void MessagesInternal::SetBrightness(uint8_t BrightnessValue)
{
    float NewBrightness;
    if(BrightnessValue > 200)
        BrightnessValue = 200;
    NewBrightness = (float)BrightnessValue / 200.0;
    LEDHandler->SetLEDBrightness(NewBrightness);
}