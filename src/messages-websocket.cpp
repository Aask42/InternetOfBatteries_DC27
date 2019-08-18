#include "messages_internal.h"

void MessagesInternal::RegisterWebsocket()
{
    Serial.println("Registering websocket");

    this->LastAccessTime = esp_timer_get_time();
    this->server = new AsyncWebServer(81);
    this->socket_server = new AsyncWebSocket("/ws");
    this->socket_server->onEvent(static_handleWebSocket);
    this->server->addHandler(this->socket_server);
    this->server->begin();
}

void MessagesInternal::DeregisterWebSocket()
{
    //remove the socket_server, this will force data to start queuing
    this->server->removeHandler(this->socket_server);
    this->server->end();

    delete this->server;

    this->server = 0;
    this->socket_server = 0;
    this->connected = 0;
}

void static_handleWebSocket(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
    if(_globalMessages)
        _globalMessages->handleWebSocket(server, client, type, arg, data, len);
}

void MessagesInternal::handleWebSocket(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
    MessageEntryStruct *NewMessage;
    if(type == WS_EVT_CONNECT)
    {
        Serial.println("websocket got connection");
        this->connected = 1;
        this->SendConnectedData();
    }
    else if(type == WS_EVT_DISCONNECT)
    {
        Serial.println("websocket disconnected");
        this->connected = 0;
    }
    else if(type == WS_EVT_ERROR)
    {
        Serial.println("websocket had an error");
    }
    else if(type == WS_EVT_DATA)
    {
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        if(info->final && info->index == 0 && info->len == len)
        {
            //Serial.printf("Received %s\n", data);
            if(info->opcode == WS_BINARY)
            {
                //not configured don't do anything unless light test, factory reset, or brightness change
                if(!this->Options.Configured && ((data[0] != 'o') && (data[0] != 'L') && (data[0] != 'R') && (data[0] != 'l')))
                    return;

                //allocate a new entry and add it to the list to be parsed
                NewMessage = (MessageEntryStruct *)malloc(sizeof(MessageEntryStruct) + len);
                NewMessage->next = 0;
                NewMessage->Len = len;
                memcpy(NewMessage->Message, data, len);

                //add to the list
                pthread_mutex_lock(&message_lock);
                if(this->MessagesTail)
                    this->MessagesTail->next = NewMessage;
                else
                    this->MessagesBegin = NewMessage;       //no tail so no beginning, set it

                //set the tail
                this->MessagesTail = NewMessage;
                pthread_mutex_unlock(&message_lock);
            }
        }
    }    
}

bool MessagesInternal::handleWebSocketData(uint8_t *data, size_t len)
{
    //process message where we aren't tying up the tcp stack
    switch(data[0])
    {
        case 'b':
        {
            LEDHandler->StartScript(LED_BROADCAST_SEND, 1);

            //broadcast message
            uint16_t nicklen = strlen(this->Options.Nickname);
            uint8_t *Message = (uint8_t *)malloc(len + nicklen + 1);      //remove b, add \x01 separator and add our message flag

            //add nickname, seperator and message, flag \x01 at the beginning that it is a message
            Message[0] = 0x01;
            memcpy(&Message[1], this->Options.Nickname, nicklen);
            Message[nicklen + 1] = 0x01;
            memcpy(&Message[nicklen + 2], &data[1], len - 1);
            this->Mesh->Write(this->Mesh->BroadcastMAC, Message, len + nicklen + 1);

            AddGlobalMessageToArray(&Message[1], len - 1 + nicklen + 1);    //remove b add \x01
            free(Message);

            break;
        }

        case 'o':
        {
            OptionsStruct NewOptions;
            const char *ErrorMsg = 0;
            bool WasConfigured = this->Options.Configured;

            //new options being set
            memset(&NewOptions, 0, sizeof(NewOptions));
            NewOptions.DisplaySplash = data[1] & 1;
            NewOptions.BrightnessButtons = data[2] & 1;

            char *DividingChar;
            char *CurPos;
            uint8_t CharCount;
            DividingChar = strchr((const char *)&data[3], '\x01');
            if(!DividingChar)
                break;

            //set a null terminator for atoi
            *DividingChar = 0;
            NewOptions.WifiTimeout = atoi((const char *)&data[3]);

            //step ahead
            CurPos = DividingChar + 1;
            DividingChar = strchr(CurPos, '\x01');
            if(!DividingChar)
                break;

            //set a null terminator for atoi
            *DividingChar = 0;
            NewOptions.BrightnessValue = atoi((const char *)CurPos);

            //step ahead
            CurPos = DividingChar + 1;
            DividingChar = strchr(CurPos, '\x01');
            if(!DividingChar)
                break;

            //copy the string
            CharCount = DividingChar - CurPos;
            if(!CharCount || (CharCount > sizeof(this->Options.Nickname) - 1))
            {
                ErrorMsg = "eNickname must be between 1 and 20 characters";
                if(CharCount)
                    CharCount = sizeof(this->Options.Nickname) - 1;
            }
            memcpy(NewOptions.Nickname, CurPos, CharCount);
            NewOptions.Nickname[sizeof(OptionsStruct::Nickname) - 1] = 0;

            //step ahead
            CurPos = DividingChar + 1;
            DividingChar = strchr(CurPos, '\x01');
            if(!DividingChar)
                break;

            //copy the string
            CharCount = DividingChar - CurPos;
            if(!CharCount || (CharCount > sizeof(this->Options.WifiName) - 1))
            {
                ErrorMsg = "eWifi name must be between 1 and 20 characters";
                if(CharCount)
                    CharCount = sizeof(this->Options.WifiName) - 1;
            }

            memcpy(NewOptions.WifiName, CurPos, CharCount);
            NewOptions.WifiName[sizeof(OptionsStruct::WifiName) - 1] = 0;

            //step ahead
            CurPos = DividingChar + 1;

            //copy the string at the end of the buffer
            CharCount = (char *)&data[len] - CurPos;
            if((CharCount < 8) || (CharCount > sizeof(this->Options.WifiPassword) - 1))
            {
                ErrorMsg = "eWifi password must be between 8 and 20 characters";
                if(CharCount)
                    CharCount = sizeof(this->Options.WifiPassword) - 1;
            }

            memcpy(NewOptions.WifiPassword, CurPos, CharCount);
            NewOptions.WifiPassword[sizeof(OptionsStruct::WifiPassword) - 1] = 0;

            //if any errors then report them and don't save
            if(ErrorMsg)
            {
                this->socket_server->binaryAll(ErrorMsg, strlen(ErrorMsg));
                break;
            }

            //indicate configured and store it
            NewOptions.Configured = 1;
            this->preferences.begin("config");
            this->preferences.putBytes("options", &NewOptions, sizeof(NewOptions));
            this->preferences.end();

            //determine if we need to alert to a nickname change
            if(strcmp(NewOptions.Nickname, this->Options.Nickname))
                this->ReportNewNickname(NewOptions.Nickname);

            //determine if the wifi needs to be updated
            int Reload = 0;
            if(!this->Options.Configured || strcmp(this->Options.WifiName, NewOptions.WifiName) || strcmp(this->Options.WifiPassword, NewOptions.WifiPassword))
                Reload = 1;

            //update brightness if required
            if(NewOptions.BrightnessValue != this->Options.BrightnessValue)
                this->SetBrightness(NewOptions.BrightnessValue);

            //set new values
            memcpy(&this->Options, &NewOptions, sizeof(NewOptions));

            //report no error
            if(Reload)
            {
                this->socket_server->binaryAll("eSaved - Rebooting", 18);

                //give the websocket 3 seconds to transmit and nvs to flush out
                delay(3000);
                esp_restart();
            }
            else
            {
                this->socket_server->binaryAll("eSaved", 6);

                //if not previously configured then configure
                if(!WasConfigured)
                    this->SendConnectedData();
            }
            break;
        }

        case 'p':
            //private message
            this->Mesh_SendPrivateMessage(&data[1], len - 1, 0);
            break;

        case 'G':
            //graffiti message
            this->Mesh_SendPrivateMessage(&data[1], len - 1, 1);
            break;

        case 'L':
            //light show test
            LEDHandler->StartScript(LED_TEST, 1);
            break;

        case 'l':
            this->SetBrightness(data[1]);
            break;

        case 's':
            //kick off a scan of the area
            //this->DoScan();
            break;

        case 'c':
            //establish a connection
            this->ConnectToDevice(&data[1]);
            break;

        case 'd':
            Serial.printf("Sending disconnect for %02x:%02x:%02x:%02x:%02x:%02x\n", data[1], data[2], data[3], data[4], data[5], data[6]);
            if(this->Mesh->Disconnect(&data[1]) == MeshNetwork::MeshWriteErrors::ResettingConnection)
                this->socket_server->binaryAll("eConnection Resetting, try again");
            break;

        case 'P':
            //ping-pong response, don't do anything for time
            return false;

        case 'F':
            //force disconnect a client
            this->ForceDisconnect(&data[1]);
            break;

        default:
            //unknown
            return false;
    }

    //indicate that time should be updated
    return true;
}

void MessagesInternal::SendConnectedData()
{
    const char *msg;
    if(!this->Options.Configured)
        msg = "Boptions";
    else if(this->Options.DisplaySplash)
        msg = "Bsplash";
    else
        msg = "Bbroadcast";

    this->socket_server->binaryAll(msg, strlen(msg));

    //generate a string of options to send
    String OptionString;
    OptionString = "o";
    if(this->Options.DisplaySplash)
        OptionString += "1";
    else
        OptionString += "0";

    if(this->Options.BrightnessButtons)
        OptionString += "1";
    else
        OptionString += "0";

    OptionString += this->Options.WifiTimeout;
    OptionString += '\x01';
    OptionString += this->Options.BrightnessValue;
    OptionString += '\x01';
    OptionString += this->Options.Nickname;
    OptionString += '\x01';
    OptionString += this->Options.WifiName;
    OptionString += '\x01';
    OptionString += this->Options.WifiPassword;
    this->socket_server->binaryAll(OptionString.c_str(), OptionString.length());

    if(this->Options.Configured)
    {
        this->SendKnownConnections();
        this->SendStoredBroadcastMessages();
        this->SendGraffiti();
        this->DoScan();

        //we sent everything, reset our flag
        this->NewMeshMessages = 0;
    }
}

void MessagesInternal::SendGraffiti()
{
    //cycle through the graffiti and send it all over the web socket
    GraffitiMessageStruct *CurEntry;
    char *Msg;

    CurEntry = this->Graffiti;
    while(CurEntry)
    {
        //create a message to send
        Msg = (char *)malloc(CurEntry->MessageLen + MAC_SIZE + 1 + MAX_NICKNAME_LEN);
        Msg[0] = 'G';
        memcpy(&Msg[1], CurEntry->MAC, CurEntry->MessageLen + MAC_SIZE + MAX_NICKNAME_LEN);
        
        //send it
        this->socket_server->binaryAll(Msg, CurEntry->MessageLen + MAC_SIZE + 1 + MAX_NICKNAME_LEN);
        free(Msg);
        CurEntry = CurEntry->next;
        yield();
    };
}

void MessagesInternal::SendKnownConnections()
{
    int i, j;
    char *Msg;
    int MsgLen;
    for(i = 0; i < MAX_DIRECT_CONNECTS; i++)
    {
        //if we have a connection then send it
        if(this->DirectConnections[i])
        {
            //craft our message
            MsgLen = 2 + MAC_SIZE + strlen(this->DirectConnections[i]->Nickname);
            Msg = (char *)malloc(MsgLen);
            Msg[0] = 'c';
            memcpy(&Msg[1], this->DirectConnections[i]->MAC, MAC_SIZE);
            Msg[7] = this->DirectConnections[i]->GraffitiDone;
            memcpy(&Msg[2 + MAC_SIZE], this->DirectConnections[i]->Nickname, strlen(this->DirectConnections[i]->Nickname));
            this->socket_server->binaryAll(Msg, MsgLen);
            free(Msg);

            //if graffiti is not done then don't show any messages for this connection
            if(!this->DirectConnections[i]->GraffitiDone)
                continue;

            //send all messages we have
            for(j = 0; j < MAX_DIRECT_MESSAGES; j++)
            {
                //if we have a message then send it
                if(this->DirectConnections[i]->Messages[j])
                {
                    MsgLen = 1 + MAC_SIZE + *(uint16_t *)this->DirectConnections[i]->Messages[j];
                    Msg = (char *)malloc(MsgLen);
                    Msg[0] = 'p';
                    memcpy(&Msg[1], this->DirectConnections[i]->MAC, MAC_SIZE);
                    memcpy(&Msg[1 + MAC_SIZE], &this->DirectConnections[i]->Messages[j][sizeof(uint16_t)], *(uint16_t *)this->DirectConnections[i]->Messages[j]);
                    this->socket_server->binaryAll(Msg, MsgLen);
                    free(Msg);
                }
                yield();
            }

            yield();
        }
    }
}

void MessagesInternal::SendStoredBroadcastMessages()
{
    int i;
    unsigned short DataLen;
    for(i = 0; i < MAX_GLOBAL_MESSAGES; i++)
    {
        //if empty move on to the next one, we store at the end due to a rolling window
        //of messages so the first few could be empty
        if(!this->BroadcastMessages[i])
            continue;

        Serial.println("Found stored broadcast");
        DataLen = *(unsigned short *)&this->BroadcastMessages[i][0];
        char *NewMessage = (char *)malloc(DataLen + 1);
        NewMessage[0] = 'b';

        //copy the original message
        memcpy(&NewMessage[1], &this->BroadcastMessages[i][sizeof(unsigned short)], DataLen);

        //send it
        Serial.println(NewMessage);
        this->socket_server->binaryAll(NewMessage, DataLen + 1);
        free(NewMessage);
        free(this->BroadcastMessages[i]);
        yield();
    }

    //wipe them all out
    memset(this->BroadcastMessages, 0, sizeof(this->BroadcastMessages));
}

void MessagesInternal::BrightnessModified()
{
    //get the new value then figure out what scale we need
    this->Options.BrightnessValue = LEDHandler->GetLEDBrightness() * 200;

    Serial.printf("Brightness modified to %d\n", this->Options.BrightnessValue);

    //save it
    this->preferences.begin("config");
    this->preferences.putBytes("options", &this->Options, sizeof(OptionsStruct));
    this->preferences.end();

    //if we have a connection then tell the web browser of the new value
    if(this->socket_server)
    {
        char OutBuffer[2];
        OutBuffer[0] = 'l';
        OutBuffer[1] = this->Options.BrightnessValue;
        this->socket_server->binaryAll(OutBuffer, 2);
    }
}

void MessagesInternal::HandleSensorData(struct SensorDataStruct *SensorData)
{
    //if we have a connected socket then send a blob of data about the sensors
    if(!this->connected)
        return;

    //generate the data
    char *NewMessage = (char *)malloc(sizeof(SensorDataStruct) + 1);
    NewMessage[0] = 'S';
    memcpy(&NewMessage[1], SensorData, sizeof(SensorDataStruct));

    this->socket_server->binaryAll(NewMessage, sizeof(SensorDataStruct) + 1);
    free(NewMessage);
}

void MessagesInternal::DoScan()
{
    //kick off a scan, if the websocket is connected then tell it to reset it's display
    if(this->socket_server && this->connected && this->Options.Configured)
        this->socket_server->binaryAll("s");
    this->PingCount = 0;
    this->Mesh->Ping();
}