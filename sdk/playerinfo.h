#pragma once

typedef struct PlayerInfo_s
{
    __int64         unknown;            //0x0000
    union
    {
        __int64       steamID64;          //0x0008 - SteamID64
        struct
        {
            __int32     xuidLow;
            __int32     xuidHigh;
        };
    };
    char            szName[128];        //0x0010 - Player Name
    int             userId;             //0x0090 - Unique Server Identifier
    char            szSteamID[20];      //0x0094 - STEAM_X:Y:Z
    char            pad_0x00A8[0x10];   //0x00A8
    unsigned long   iSteamID;           //0x00B8 - SteamID
    char            szFriendsName[128];
    bool            fakeplayer;
    bool            ishltv;
    unsigned int    customFiles[4];
    unsigned char   filesDownloaded;
} PlayerInfo_t;

class IVEngineClient
{
public:
    virtual void unused1() = 0;
    virtual void unused2() = 0;
    virtual void unused3() = 0;
    virtual void unused4() = 0;
    virtual void unused5() = 0;
    virtual void unused6() = 0;
    virtual void unused7() = 0;
    virtual void unused8() = 0;
    virtual bool GetPlayerInfo(int index, PlayerInfo_t* pInfo) = 0;
};