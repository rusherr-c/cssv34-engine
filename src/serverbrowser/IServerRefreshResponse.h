//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef ISERVERREFRESHRESPONSE_H
#define ISERVERREFRESHRESPONSE_H
#ifdef _WIN32
#pragma once
#endif

class serveritem_t;

// Master Server response status
enum EMasterServerResponse
{
	k_eServerResponded = 0,
	k_eServerFailedToRespond,
	k_eNoServersListedOnMasterServer,
};

//-----------------------------------------------------------------------------
// Purpose: Callback interface for receiving updates of the servers
//
class IServerRefreshResponse
{
public:
	// Server has responded successfully and has updated data
	virtual void ServerResponded(serveritem_t& server) = 0;

	// Called when server response has timed out
	virtual void ServerFailedToRespond(serveritem_t& server) = 0;

	// Called when the current refresh list is complete
	virtual void RefreshComplete(EMasterServerResponse response) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Callback interface for receiving responses after pinging an
// individual server or requesting details on
// who is playing on a particular server.
// 
class IServerQueryResponse
{
public:
	// Server has responded successfully and has updated data
	virtual void ServerResponded(serveritem_t& server) {}

	// The server has responded successfully with updated game rules
	virtual void ServerRulesUpdated(serveritem_t& server) {}

	// Got data on a new player on the server -- you'll get this callback once per player
	// on the server which you have requested player data on.
	virtual void AddPlayerToList(const char* pchName, int nScore, float flTimePlayed) {}

	// The server failed to respond to the request for player details
	virtual void PlayersFailedToRespond() {}

	// The server has finished responding to the player details request
	virtual void PlayersRefreshComplete() {}

	// Virtual destructor
	virtual ~IServerQueryResponse() = default;
};

#endif // ISERVERREFRESHRESPONSE_H