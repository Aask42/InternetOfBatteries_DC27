#include "messages_internal.h"

void MessagesInternal::Flash_RemoveConnection(DirectConnectStruct *Connection)
{
    //go through and remove an entry from the flash
    uint8_t ConnectCount;
    uint8_t i;
    char MACStr[13];
    char MACBytes1[MAC_SIZE];
    char MACBytes2[MAC_SIZE];
    char key1[15];
    char key2[15];

    //get stored connections and their stored messages for private
    this->preferences.begin("messages");
    ConnectCount = this->preferences.getUChar("count", 0);
    for(i = 0; i < ConnectCount; i++)
    {
        //get mac of the key entry
        sprintf(key1, "%d-mac", i);
        this->preferences.getBytes(key1, MACBytes1, MAC_SIZE);

        //see if it matches our entry
        if(memcmp(MACBytes1, Connection->MAC, MAC_SIZE) == 0)
        {
            //found our entry, we need to replace it with the last entry if we aren't already last
            if(i != (ConnectCount - 1))
            {
                //get mac of the last key entry
                sprintf(key2, "%d-mac", ConnectCount - 1);
                this->preferences.getBytes(key2, MACBytes2, MAC_SIZE);

                //rewrite our current entry with the mac we just got
                this->preferences.putBytes(key1, MACBytes2, MAC_SIZE);
            }

            //remove the last entry
            sprintf(key1, "%d-mac", ConnectCount - 1);           
            this->preferences.remove(key1);

            //adjust our count and store it
            ConnectCount--;
            this->preferences.putUChar("count", ConnectCount);

            //now wipe out the section
            MACToStr(MACStr, Connection->MAC);
            this->preferences.begin(MACStr);
            this->preferences.clear();
            this->preferences.end();

            //exit the loop
            break;
        }
    }

    //close preferences
    this->preferences.end();
}

void MessagesInternal::Flash_StoreGraffitiFlag(DirectConnectStruct *Connection)
{
    char MACStr[13];

    MACToStr(MACStr, Connection->MAC);
    MACStr[12] = 0;

    //add an entry to flash
    this->preferences.begin(MACStr);
    this->preferences.putUChar("graffiti", Connection->GraffitiDone);
    this->preferences.end();
}

void MessagesInternal::Flash_StoreNewConnection(DirectConnectStruct *Connection)
{
    uint8_t ConnectCount;
    char key[15];
    char MACStr[13];

    MACToStr(MACStr, Connection->MAC);
    MACStr[12] = 0;

    //add an entry to flash
    this->preferences.begin(MACStr);
    this->preferences.putBytes("nick", Connection->Nickname, MAX_NICKNAME_LEN);
    this->preferences.putUChar("graffiti", Connection->GraffitiDone);
    this->preferences.putUChar("count", 0);
    this->preferences.putUChar("start", 0);
    this->preferences.end();

    //now add the entry to the master list
    this->preferences.begin("messages");
    ConnectCount = this->preferences.getUChar("count", 0);

    //set mac and nick
    sprintf(key, "%d-mac", ConnectCount);
    this->preferences.putBytes(key, Connection->MAC, MAC_SIZE);

    //now update the count and store it as everything else is stored
    ConnectCount++;
    this->preferences.putUChar("count", ConnectCount);
    this->preferences.end();
}

void MessagesInternal::Flash_StoreNewNickname(DirectConnectStruct *Connection)
{
    char MACStr[13];

    MACToStr(MACStr, Connection->MAC);
    MACStr[12] = 0;

    //add an entry to flash
    this->preferences.begin(MACStr);
    this->preferences.putBytes("nick", Connection->Nickname, MAX_NICKNAME_LEN);
    this->preferences.end();
}

void MessagesInternal::Flash_StoreNewMessage(DirectConnectStruct *Connection, char *Message, uint8_t MessageLen)
{
    char key[15];
    char MACStr[13];
    uint8_t MessageCount;
    uint8_t MessageStartPos;

    //if no connection then we are global, open up the global pref entry
    if(!Connection)
        this->preferences.begin("global-msgs");
    else
    {    
        MACToStr(MACStr, Connection->MAC);
        MACStr[12] = 0;

        //add an entry to flash
        this->preferences.begin(MACStr);
    }

    //first get our counts so we know where to put it
    MessageCount = this->preferences.getUChar("count", 0);
    MessageStartPos = this->preferences.getUChar("start", 0);

    //if we are already maxed then move start forward and rewrite the entry it is on
    if((Connection && (MessageCount == MAX_DIRECT_MESSAGES)) ||
        (!Connection && (MessageCount == MAX_GLOBAL_MESSAGES)))
    {
        //overwrite an entry in the preferences, wherever the current start is
        sprintf(key, "%d-len", MessageStartPos);
        this->preferences.putUChar(key, MessageLen);

        sprintf(key, "%d-msg", MessageStartPos);
        this->preferences.putBytes(key, Message, MessageLen);

        //advance our start position and store it
        MessageStartPos++;
        this->preferences.putUChar("start", MessageStartPos);
    }
    else
    {
        //just store a new entry
        sprintf(key, "%d-len", MessageCount);
        this->preferences.putUChar(key, MessageLen);

        sprintf(key, "%d-msg", MessageCount);
        this->preferences.putBytes(key, Message, MessageLen);

        //increment count and store it
        MessageCount++;
        this->preferences.putUChar("count", MessageCount);
    }
    this->preferences.end();
}

void MessagesInternal::Flash_GetStoredGlobalMessages()
{
    uint8_t MessageCount;
    uint8_t MessageLen;
    uint8_t MessageStartPos;
    uint8_t j;
    char key[15];

    this->preferences.begin("global-msgs");

    //get messages
    MessageCount = this->preferences.getUChar("count", 0);
    MessageStartPos = this->preferences.getUChar("start", 0);

    //we need to fill the bottom part of the array due to our rotating buffer design
    for(j = 0; j < MessageCount; j++)
    {
        sprintf(key, "%d-len", (MessageStartPos + j) % MAX_GLOBAL_MESSAGES);
        MessageLen = this->preferences.getUChar(key, 0);
        this->BroadcastMessages[MAX_GLOBAL_MESSAGES - MessageCount + j - 1] = (char *)malloc(MessageLen);

        sprintf(key, "%d-msg", (MessageStartPos + j) % MAX_GLOBAL_MESSAGES);
        this->preferences.getBytes(key, this->BroadcastMessages[MAX_GLOBAL_MESSAGES - MessageCount + j - 1], MessageLen);
    }
    Serial.printf("Loaded %d stored messages from global\n", MessageCount);
    this->preferences.end();
}

void MessagesInternal::Flash_GetStoredConnections()
{
    uint8_t ConnectCount;
    uint8_t MessageCount;
    uint8_t MessageLen;
    uint8_t MessageStartPos;
    uint8_t i, j;
    char MACStr[13];
    DirectConnectStruct *NewConnection;
    char key[15];

    //get stored connections and their stored messages for private
    this->preferences.begin("messages");
    ConnectCount = this->preferences.getUChar("count", 0);
    for(i = 0; i < ConnectCount; i++)
    {
        //allocate memory
        NewConnection = (DirectConnectStruct *)malloc(sizeof(DirectConnectStruct));
        memset(NewConnection, 0, sizeof(DirectConnectStruct));

        //get mac
        sprintf(key, "%d-mac", i);
        this->preferences.getBytes(key, NewConnection->MAC, MAC_SIZE);
        this->DirectConnections[i] = NewConnection;
    }

    //done with global info, get each entry's info
    this->preferences.end();
    yield();

    for(i = 0; i < ConnectCount; i++)
    {
        NewConnection = this->DirectConnections[i];
        MACToStr(MACStr, NewConnection->MAC);
        MACStr[12] = 0;

        this->preferences.begin(MACStr);
        this->preferences.getBytes("nick", NewConnection->Nickname, MAX_NICKNAME_LEN);

        //get messages
        MessageCount = this->preferences.getUChar("count", 0);
        MessageStartPos = this->preferences.getUChar("start", 0);
        NewConnection->GraffitiDone = this->preferences.getUChar("graffiti", 0);

        //we need to fill the bottom part of the array due to our rotating buffer design
        for(j = 0; j < MessageCount; j++)
        {
            sprintf(key, "%d-len", (MessageStartPos + j) % MAX_DIRECT_MESSAGES);
            MessageLen = this->preferences.getUChar(key, 0);
            NewConnection->Messages[MAX_DIRECT_MESSAGES - MessageCount + j - 1] = (char *)malloc(MessageLen + 1);

            sprintf(key, "%d-msg", (MessageStartPos + j) % MAX_DIRECT_MESSAGES);
            this->preferences.getBytes(key, NewConnection->Messages[MAX_DIRECT_MESSAGES - MessageCount + j - 1], MessageLen);
        }
        Serial.printf("Loaded %d stored messages from mac %s\n", MessageCount, MACStr);
        this->preferences.end();
        yield();
    }
}

void MessagesInternal::Flash_StoreGraffitiMessage(GraffitiMessageStruct *Graffiti)
{
    uint8_t GraffitiCount;
    char key[15];

    //store a graffiti message, this is only allowing up to 255 entries but we only have 80 badges so we are good
    this->preferences.begin("graffiti");
    GraffitiCount = this->preferences.getUChar("count", 0);
    sprintf(key, "%d-len", GraffitiCount);
    this->preferences.putUShort(key, Graffiti->MessageLen);
    sprintf(key, "%d-msg", GraffitiCount);

    //store the mac and message itself
    this->preferences.putBytes(key, Graffiti->MAC, Graffiti->MessageLen + MAC_SIZE + MAX_NICKNAME_LEN);
    this->preferences.putUChar("count", GraffitiCount + 1);
    this->preferences.end();
}

void MessagesInternal::Flash_GetStoredGraffitiMessages()
{
    uint8_t GraffitiCount;
    uint8_t i;
    GraffitiMessageStruct *NewGraffiti;
    unsigned short MessageLen;
    char key[15];

    //read all of the entries from flash
    this->preferences.begin("graffiti");
    GraffitiCount = this->preferences.getUChar("count", 0);
    this->Graffiti = 0;

    for(i = 0; i < GraffitiCount; i++)
    {
        sprintf(key, "%d-len", i);
        MessageLen = this->preferences.getUShort(key, 0);

        //allocate a new entry, take out the mac size
        NewGraffiti = (GraffitiMessageStruct *)malloc(sizeof(GraffitiMessageStruct) + MessageLen);
        NewGraffiti->MessageLen = MessageLen;

        //read into the mac and message
        sprintf(key, "%d-msg", i);
        this->preferences.getBytes(key, NewGraffiti->MAC, MessageLen + MAC_SIZE + MAX_NICKNAME_LEN);
        
        //add it to the list
        NewGraffiti->next = this->Graffiti;
        this->Graffiti = NewGraffiti;
        yield();
    }

    this->preferences.end();
    Serial.printf("Found %d graffiti entries\n", GraffitiCount);
}
