/*
 *
 * Copyright (c) 2026 RuSHeRR
 *
 * Purpose: processes network packets from 
 *	the Source Engine server
 *
 * VDC: https://developer.valvesoftware.com/wiki/Server_queries
 * 
*/
#ifndef SERVERMSGHANDLERDETAILS_H
#define SERVERMSGHANDLERDETAILS_H
#ifdef _WIN32
#pragma once
#endif

#include "ServerList.h"
#include "Socket.h"

class serveritem_t;
class CServerList;

//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CServerDetailsMsgHandler : public CMsgHandler
{
public:
	CServerDetailsMsgHandler(CServerList* list);
	~CServerDetailsMsgHandler();

	// CMsgHandler
	virtual bool Process(const netadr_t& from, bf_read& msg);

protected:

	// process info
	bool ProcessInfo(bf_read& msg, serveritem_t &server);

private:
	CServerList* m_pServerList;

	int m_nServersResponded;
};


#endif // SERVERMSGHANDLERDETAILS_H