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

#include "Socket.h"

class CServerList;

//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CServerDetailsMsgHandler : public CMsgHandler
{
public:
	CServerDetailsMsgHandler(CServerList* list);
	~CServerDetailsMsgHandler();

	virtual bool Process(const netadr_t& from, bf_read& msg);

private:
	CServerList* m_pServerList;
};


#endif // SERVERMSGHANDLERDETAILS_H