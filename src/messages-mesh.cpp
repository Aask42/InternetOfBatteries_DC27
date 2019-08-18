#include "messages_internal.h"
#include "debug.h"

void MessagesInternal::Mesh_SendPrivateMessage(uint8_t *data, size_t len, bool GraffitiFlag)
{
    //find our connection based on the mac then send a private message to it
    uint8_t *Msg;
    int ConnID = this->FindConnectEntry(data);
    if(ConnID < 0)
        return;

    //if length is too long then limit it
    if(len > MAX_MESSAGE_LEN)
        len = MAX_MESSAGE_LEN;

    //found an entry, create a mesh message to send
    Msg = (uint8_t *)malloc(len - MAC_SIZE + 1);
    Msg[0] = 0;
    if(GraffitiFlag)
    {
        //they want to do graffiti, if the flag is already set then don't
        if(this->DirectConnections[ConnID]->GraffitiDone)
        {
            free(Msg);
            if(this->socket_server && this->connected && this->Options.Configured)
                this->socket_server->binaryAll("eGraffiti already sent", 22);
            return;
        }
        else
        {
            //indicate in process, we have to wait for confirmation to store it
            this->DirectConnections[ConnID]->GraffitiDone = 2;
        }
        Msg[0] = 2;
    }

    LEDHandler->StartScript(LED_PRIVATE_SEND, 1);

    memcpy(&Msg[1], &data[MAC_SIZE], len - MAC_SIZE);
    this->Mesh->Write(this->DirectConnections[ConnID]->MAC, Msg, len - MAC_SIZE + 1);
    free(Msg);

    //if not graffiti then add it to our internal list and update the flash
    if(!GraffitiFlag)
        this->AddPrivateMessageToArray(this->DirectConnections[ConnID]->MAC, &data[MAC_SIZE], len - MAC_SIZE, 1);
}

void MessagesInternal::ReportNewNickname(char *Nickname)
{
    //tell all connected entries of our new nickname
    int i;
    char Msg[MAX_NICKNAME_LEN + 1];
    int MsgLen;

    //set our nickname for the ping data response
    this->Mesh->SetPingData((uint8_t *)this->Options.Nickname, strlen(this->Options.Nickname));
    yield();

    //create the message
    Msg[0] = 1;
    strcpy(&Msg[1], Nickname);
    MsgLen = 1 + strlen(Nickname);

    //tell everyone
    for(i = 0; i < MAX_DIRECT_CONNECTS; i++)
    {
        if(this->DirectConnections[i])
            this->Mesh->Write(this->DirectConnections[i]->MAC, (uint8_t*)Msg, MsgLen);
    }
    yield();
}

void static_PingReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen)
{
    if(_globalMessages)
        _globalMessages->PingReceived(From_MAC, Data, DataLen);
}

void MessagesInternal::PingReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen)
{
    Serial.print("Ping received from ");
    SerialPrintMAC(From_MAC);
    Serial.print("\n");

    this->PingCount++;

    //if we are connected to the websocket then report
    if(this->socket_server && this->connected && this->Options.Configured)
    {
        char *Msg;
        Msg = (char *)malloc(1 + MAC_SIZE + DataLen);
        Msg[0] = 's';
        memcpy(&Msg[1], From_MAC, MAC_SIZE);
        memcpy(&Msg[1 + MAC_SIZE], Data, DataLen);
        this->socket_server->binaryAll(Msg, 1 + MAC_SIZE + DataLen);
        free(Msg);
    }
}

void static_MessageReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen)
{
    if(_globalMessages)
        _globalMessages->MessageReceived(From_MAC, Data, DataLen);
}

void MessagesInternal::MessageReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen)
{
    int EntryID;

    //go find our entry assuming it isn't a ping
    EntryID = this->FindConnectEntry(From_MAC);
    if(EntryID < 0)
        return;

    if(DataLen == 0) {
        Serial.print("Messaged received by ");
        SerialPrintMAC(From_MAC);
        Serial.print("\n");

        //if our graffiti flag is 2 then make it 1 and store the flag
        if(this->DirectConnections[EntryID]->GraffitiDone == 2)
        {
            this->DirectConnections[EntryID]->GraffitiDone = 1;
            this->Flash_StoreGraffitiFlag(this->DirectConnections[EntryID]);
        }
    } else {
        Serial.print("Saw message from ");
        SerialPrintMAC(From_MAC);
        Serial.printf(": data len %d, data:", DataLen);
        DEBUG_DUMPHEX(0, Data, DataLen);
        Serial.print("\n");

        LEDHandler->StartScript(LED_PRIVATE_RECV, 1);

        //indicate they need to pay attention to the wifi if not connected
        if(!this->connected)
            this->NewMeshMessages = 1;

        //if data is too long then cap it
        if(DataLen > 128)
            DataLen = 128;

        //if the first byte is 0x01 then it is a nickname
        if(Data[0] == 1)
        {
            //copy in the nickname
            DataLen--;
            Data++;
            if(DataLen > MAX_NICKNAME_LEN)
                DataLen = MAX_NICKNAME_LEN;
            memcpy(this->DirectConnections[EntryID]->Nickname, Data, DataLen);
            this->DirectConnections[EntryID]->Nickname[DataLen] = 0;

            //update flash
            this->Flash_StoreNewNickname(this->DirectConnections[EntryID]);

            //if we have a websocket connection then report it
            if(this->socket_server && this->connected && this->Options.Configured)
            {
                //add the nickname indicator
                char *NewMessage = (char *)malloc(DataLen + MAC_SIZE + 1);
                NewMessage[0] = 'n';
                memcpy(&NewMessage[1], From_MAC, MAC_SIZE);
                memcpy(&NewMessage[1 + MAC_SIZE], Data, DataLen);
                this->socket_server->binaryAll(NewMessage, DataLen + MAC_SIZE + 1);
                free(NewMessage);
            }
        }
        else if(Data[0] == 2)       //2 = graffiti
        {
            //got a graffiti entry from this person, see if we can add it to the list
            GraffitiMessageStruct *Graffiti = this->Graffiti;
            while(Graffiti)
            {
                if(memcmp(Graffiti->MAC, From_MAC, MAC_SIZE) == 0)
                    break;

                Graffiti = Graffiti->next;
            }

            //if we have no entry then create a new one and store it
            if(!Graffiti)
            {
                //adjust for the first byte of data as it is a flag for the type of data
                DataLen--;
                Data++;

                //data len is 1 too long due to the first byte, we need a null terminator so they cancel each other out
                Graffiti = (GraffitiMessageStruct *)malloc(sizeof(GraffitiMessageStruct) + DataLen);

                memcpy(Graffiti->MAC, From_MAC, MAC_SIZE);
                memcpy(Graffiti->Nickname, this->DirectConnections[EntryID]->Nickname, MAX_NICKNAME_LEN);
                memcpy(Graffiti->Message, Data, DataLen);
                Graffiti->MessageLen = DataLen;
                Graffiti->next = this->Graffiti;
                this->Graffiti = Graffiti;

                Flash_StoreGraffitiMessage(Graffiti);

                //send to the webserver if available
                if(this->socket_server && this->connected && this->Options.Configured)
                {
                    char *NewMessage = (char *)malloc(DataLen + 1 + MAC_SIZE + MAX_NICKNAME_LEN);

                    //add the indicator
                    NewMessage[0] = 'G';

                    //copy the message, skip the first byte
                    memcpy(&NewMessage[1], From_MAC, MAC_SIZE);
                    memcpy(&NewMessage[1 + MAC_SIZE], Graffiti->Nickname, MAX_NICKNAME_LEN);
                    memcpy(&NewMessage[1 + MAC_SIZE + MAX_NICKNAME_LEN], Data, DataLen);
                    Serial.println(NewMessage);

                    //send
                    this->socket_server->binaryAll(NewMessage, DataLen + 1 + MAC_SIZE + MAX_NICKNAME_LEN);
                    free(NewMessage);
                }
            }
        }
        else if(Data[0] == 3)
        {
            //other side is indicating we already set graffiti on them so set our flag
            this->DirectConnections[EntryID]->GraffitiDone = 1;
            this->Flash_StoreGraffitiFlag(this->DirectConnections[EntryID]);

            //if the browser is connected then alert it
            if(this->socket_server && this->connected && this->Options.Configured)
            {
                //DataLen has an extra character at the beginning
                char *NewMessage = (char *)malloc(MAC_SIZE + 3);
                NewMessage[0] = 'c';
                memcpy(&NewMessage[1], From_MAC, MAC_SIZE);
                NewMessage[MAC_SIZE + 1] = 1;
                NewMessage[MAC_SIZE + 2] = 0;
                this->socket_server->binaryAll(NewMessage, MAC_SIZE + 3);
                free(NewMessage);
            }
        }
        else
        {
            //shift for the special byte
            Data++;
            DataLen--;
            this->AddPrivateMessageToArray(From_MAC, Data, DataLen, 0);

            //if we have a web socket then send the message we got
            if(this->socket_server && this->connected && this->Options.Configured)
            {
                //DataLen has an extra character at the beginning
                char *NewMessage = (char *)malloc(DataLen + MAC_SIZE + 2);

                //add the mac and indicator
                NewMessage[0] = 'p';
                memcpy(&NewMessage[1], From_MAC, MAC_SIZE);
                NewMessage[MAC_SIZE + 1] = 0;   //indicate it is a remote message

                //copy the message, skip the first byte
                memcpy(&NewMessage[2 + MAC_SIZE], Data, DataLen);
                Serial.println("Received a private message");

                //send
                this->socket_server->binaryAll(NewMessage, DataLen + MAC_SIZE + 2);
                free(NewMessage);
            }
        }    
    }
}

void static_BroadcastMessageReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen)
{
    if(_globalMessages)
        _globalMessages->BroadcastMessageReceived(From_MAC, Data, DataLen);
}

void MessagesInternal::BroadcastMessageReceived(const uint8_t *From_MAC, const uint8_t *Data, unsigned int DataLen)
{
    Serial.print("Saw broadcast from ");
    SerialPrintMAC(From_MAC);
    Serial.printf(": data len %d, type %02x, data: %s\n", DataLen, Data[0], Data);

    //if our master MAC 06 35 47 24 56 84 (nekisaloth spelled out on a phone) then do the specified light show
    if(memcmp(From_MAC, "\x06\x35\x47\x24\x56\x84", 6) == 0)
    {
        LEDHandler->StartScript(Data[0], 1);
        return;
    }

    LEDHandler->StartScript(LED_BROADCAST_RECV, 1);

    //indicate they need to pay attention to the wifi if not connected
    if(!this->connected)
        this->NewMeshMessages = 1;

    //if the data length is too long then cap it
    if(DataLen > 128)
        DataLen = 128;

    //check what type of message this is from broadcast
    if(Data[0] == 0x02)
    {
        //handle a status update on the scoreboard
    }
    else if(Data[0] == 0x01)
    {
        //store broadcast message for later displaying
        Serial.println("Storing broadcast for later");

        Data++;
        DataLen--;
        AddGlobalMessageToArray(Data, DataLen);

        if(this->socket_server && this->connected && this->Options.Configured)
        {
            //if connected and configured then pass through a message that we got
            char *NewMessage = (char *)malloc(DataLen + 1);

            //add in the header and data, first character is changed to the flag for websocket
            NewMessage[0] = 'b';
            memcpy(&NewMessage[1], Data, DataLen);
            this->socket_server->binaryAll(NewMessage, DataLen + 1);
            free(NewMessage);
        }
    }
}

void static_DeviceConnected(const uint8_t *MAC, const char *Name, int Succeeded)
{
    if(_globalMessages)
        _globalMessages->DeviceConnected(MAC, Name, Succeeded);
}

void MessagesInternal::DeviceConnected(const uint8_t *MAC, const char *Name, int Succeeded)
{
    if(Succeeded == -1)
    {
        Serial.print("Disconnection from ");
        SerialPrintMAC(MAC);
        Serial.print(" completed\n");

        if(this->socket_server && this->connected && this->Options.Configured)
        {
            char *NewMessage = (char *)malloc(MAC_SIZE + 1);
            NewMessage[0] = 'd';
            memcpy(&NewMessage[1], MAC, MAC_SIZE);

            //add the mac and indicator
            this->socket_server->binaryAll(NewMessage, MAC_SIZE + 1);
            free(NewMessage);
        }

        int EntryID;
        DirectConnectStruct *CurEntry;

        EntryID = this->FindConnectEntry(MAC);
        if(EntryID < 0)
            return;

        //free all memory
        CurEntry = this->DirectConnections[EntryID];
        int i;
        for(i = 0; i < MAX_DIRECT_MESSAGES; i++)
        {
            if(CurEntry->Messages[i])
                free(CurEntry->Messages[i]);
        }
        yield();

        this->Flash_RemoveConnection(CurEntry);
        free(this->DirectConnections[EntryID]);
        this->DirectConnections[EntryID] = 0;
    }
    else
    {
        Serial.print("Connection to ");
        SerialPrintMAC(MAC);
        if(Succeeded == 1)
        {
            Serial.print(" succeeded\n");

            //allocate a new entry if there is room
            DirectConnectStruct *NewEntry = (DirectConnectStruct *)malloc(sizeof(DirectConnectStruct));
            int i;
            memset(NewEntry, 0, sizeof(DirectConnectStruct));
            memcpy(NewEntry->MAC, MAC, MAC_SIZE);
            memcpy(NewEntry->Nickname, Name, MAX_NICKNAME_LEN);

            for(i = 0; i < MAX_DIRECT_CONNECTS; i++)
            {
                if(!this->DirectConnections[i])
                {
                    //found an entry to use
                    this->DirectConnections[i] = NewEntry;
                    this->Flash_StoreNewConnection(NewEntry);
                    break;
                }
            }
            yield();

            //if we hit the end then fail
            if(i >= MAX_DIRECT_CONNECTS)
            {
                Serial.println("Out of room for direct connections, terminating");
                free(NewEntry);
                this->Mesh->Disconnect(MAC);
                return;
            }
            if(this->socket_server && this->connected && this->Options.Configured)
            {
                //we are fully connected and got a websocket going that is configured, alert the user
                uint16_t MsgLen = 2 + MAC_SIZE + strlen(this->DirectConnections[i]->Nickname);
                char *Msg = (char *)malloc(MsgLen);
                Msg[0] = 'c';
                memcpy(&Msg[1], this->DirectConnections[i]->MAC, MAC_SIZE);
                Msg[7] = this->DirectConnections[i]->GraffitiDone;
                memcpy(&Msg[2 + MAC_SIZE], this->DirectConnections[i]->Nickname, strlen(this->DirectConnections[i]->Nickname));

                Serial.println("Sending connect to websocket");
                this->socket_server->binaryAll(Msg, MsgLen);
                free(Msg);
            }

            //skim our graffiti, if we have a matching mac then alert the other side to it
            GraffitiMessageStruct *CurGraffiti;
            CurGraffiti = this->Graffiti;
            while(CurGraffiti)
            {
                if(memcmp(CurGraffiti->MAC, MAC, MAC_SIZE) == 0)
                {
                    //found a match, report it
                    uint8_t Msg[1] = {3};
                    this->Mesh->Write(MAC, Msg, sizeof(Msg));
                    Serial.print("Graffiti found, alerting other side\n");
                    break;
                }
                CurGraffiti = CurGraffiti->next;
            };
        }
        else
        {
            Serial.print(" failed\n");

            //we tried to connect to someone, report the error
            const char *Msg = "eFailed to connect";
            this->socket_server->binaryAll(Msg, strlen(Msg));
        }
    }
}

void static_SendToDeviceFailed(const uint8_t *MAC)
{
    if(_globalMessages)
        _globalMessages->SendToDeviceFailed(MAC);
}

void MessagesInternal::SendToDeviceFailed(const uint8_t *MAC)
{
    const char *Msg;
    char *Msg2;
    int MsgLen;
    int EntryID = this->FindConnectEntry(MAC);
    char OutMsg[7];

    if(EntryID < 0)
        return;

    Serial.print("Failed to send to device ");
    SerialPrintMAC(MAC);
    Serial.print("\n");

    //if our graffiti flag is 2 then reset it
    if(this->DirectConnections[EntryID]->GraffitiDone == 2)
        this->DirectConnections[EntryID]->GraffitiDone = 0;

    if(this->socket_server && this->connected && this->Options.Configured)
    {
        //build up a custom error response with the nickname we failed to send to
        Msg = "eError sending message to ";
        MsgLen = strlen(Msg);
        Msg2 = (char *)malloc(MsgLen + MAX_NICKNAME_LEN + 1);
        memcpy(Msg2, Msg, MsgLen);
        memcpy(&Msg2[MsgLen], this->DirectConnections[EntryID]->Nickname, MAX_NICKNAME_LEN);
        Msg2[MsgLen + MAX_NICKNAME_LEN] = 0;
        this->socket_server->binaryAll(Msg2, strlen(Msg2));
        free(Msg2);

        //reset the graffiti flag on the entry
        OutMsg[0] = 'g';
        memcpy(&OutMsg[1], MAC, MAC_SIZE);
        this->socket_server->binaryAll(OutMsg, 7);
    }
}
