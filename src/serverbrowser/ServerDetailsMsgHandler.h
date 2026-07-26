//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

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
	CServerDetailsMsgHandler(IServerQueryResponse* response);
	~CServerDetailsMsgHandler();

	// CMsgHandler
	virtual bool Process(const netadr_t& from, bf_read& msg);

protected:
	// Internal function //

	// process multi packet response
	bool ProcessLong(const netadr_t& from, bf_read& msg);
	// process info
	bool ProcessInfo(bf_read& msg, serveritem_t &server);
	// process rules
	bool ProcessRules(bf_read& msg);
	// something
	bool ProcessPlayers(bf_read& msg);
	// returns processed challenge
	int ProcessChallenge(bf_read& msg);

private:
	CServerList* m_pServerList;
	IServerQueryResponse* m_pResponseTarget;

	int m_nServersResponded;
};


#endif // SERVERMSGHANDLERDETAILS_H