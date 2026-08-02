/*
 *
 * Copyright (c) 2026 RuSHeRR
 *
 * Purpose: easy socket
 *	class implementation
 *
*/
#if !defined( SOCKET_H )
#define SOCKET_H
#ifdef _WIN32
#pragma once
#endif

#include "netadr.h"
#include "bitbuf.h"
#include "utlvector.h"
#include "utlmap.h"

#include <stdio.h>
#undef SendMessage

class CSocket;
class IGameList;

enum { 
    MAX_ROUTABLE_PACKET = 1400,
    MAX_RECEIVE_PACKET = 8192,
    SPLIT_SIZE = (MAX_ROUTABLE_PACKET - 10),
    NET_MAX_MESSAGE = 96016
};

// Split long packets.  Anything over 1460 is failing on some routers
typedef struct
{
    int		currentSequence;
    int		splitCount;
    int		totalSize;
    char	buffer[NET_MAX_MESSAGE];
} LONGPACKET;

// Use this to pick apart the network stream, must be packed
#pragma pack(1)
typedef struct
{
	int		netID;
	int		sequenceNumber;
	short	packetID;
} SPLITPACKET;

// This one only exists in first split and when it's compressed
typedef struct
{
    int decompressedSize;
    int crc;
} SPLITPACKET_COMPRESSED;
#pragma pack()

struct splitpacket_t
{
    LONGPACKET packet;
    int flags[69];
    double lastReceiveTime;

    splitpacket_t()
    {
        Q_memset(&packet, 0, sizeof(packet));
        Q_memset(flags, 0, sizeof(flags));
        lastReceiveTime = 0.0;
    }
};

//-----------------------------------------------------------------------------
// Purpose: Instances a message handler for incoming messages.
//-----------------------------------------------------------------------------
class CMsgHandler
{
public:
    virtual ~CMsgHandler() {}

    virtual bool Process(const netadr_t& from, bf_read& msg) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Creates a non-blocking, broadcast capable, UDP socket.  If port is
//  specified, binds it to listen on that port, otherwise, chooses a random port.
//-----------------------------------------------------------------------------
class CSocket
{
public:
    CSocket(uint16 port = 0);
    ~CSocket();

    bool Open(uint16 port = 0);
    void Close();

    bool IsValid() const;

    int Send(const netadr_t& to, const void* data, int length);
    int Send(const netadr_t& to, bf_write& msg);

    int Broadcast(uint16 port, const void* data, int length);
    int Broadcast(uint16 port, bf_write& msg);

    void Frame();

    void AddHandler(CMsgHandler* handler);
    void RemoveHandler(CMsgHandler* handler);

    uintp GetSocket() const;
    const netadr_t& GetAddress() const;

protected:

    bool ReceiveData();

    splitpacket_t* FindOrCreateSplitPacket(const netadr_t& adr);
    void RemoveSplitPacket(const netadr_t& adr);
    void CleanupSplitPackets();

    CUtlMap< netadr_t, splitpacket_t > m_SplitPackets;
    
private:
    
    uintp m_hSocket;

    netadr_t m_Address;

    CUtlVector<CMsgHandler*> m_Handlers;
};

#endif // SOCKET_H