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
#include "UtlVector.h"

#include <stdio.h>
#undef SendMessage

class CSocket;
class IGameList;

// Use this to pick apart the network stream, must be packed
#pragma pack(1)
typedef struct
{
	int		netID;
	int		sequenceNumber;
	char	packetID;
} SPLITPACKET;
#pragma pack()

#define MAX_PACKETS 16 // 4 bits for the packet count, so only 
#define MAX_RETRIES 2 // the number of fragments from other packets to drop before we declare the outstanding
					  // fragment lost :)

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

private:
    
    uintp m_hSocket;

    netadr_t m_Address;

    CUtlVector<CMsgHandler*> m_Handlers;
};

#endif // SOCKET_H