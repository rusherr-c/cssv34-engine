//========= Copyright   1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERLIST_H
#define SERVERLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "IServerRefreshResponse.h"
#include "ServerDetailsMsgHandler.h"
#include "netadr.h"

#include <UtlRBTree.h>
#include <UtlVector.h>

class CSocket;
class IServerRefreshResponse;
class serveritem_t;

// holds a single query - needs to public unfortunately
struct query_t
{
	netadr_t addr;
	int serverID;
	double sendTime;
};

//-----------------------------------------------------------------------------
// Purpose: Handles a list of servers, and can refresh them
//-----------------------------------------------------------------------------
class CServerList
{
	friend class CServersInfo;
public:
	////////////////////
	CServerList(IServerRefreshResponse *gameList);
	~CServerList();

	// Handles a frame of networking
	void RunFrame();
	
	// gets a server from the list by id, range [0, ServerCount)
	serveritem_t &GetServer(unsigned int serverID);

	// returns the number of servers
	int ServerCount();

	// adds a new server to the list, returning a handle to the server
	unsigned int AddNewServer(serveritem_t &server);

	// removes server from server list and refresh list
	void RemoveServer(unsigned int serverID);

	// starts a refresh
	void StartRefresh();

	// stops all refreshing
	void StopRefresh();

	// clears all servers from the list
	void Clear();

	// marks a server to be refreshed
	void AddServerToRefreshList(unsigned int serverID);

	// marks all server to be refreshed
	void AddAllServersToRefreshList();

	// responses
	void UpdateServer(netadr_t& adr, serveritem_t& server, double recvTime);

	// find server by address
	int  FindServer(netadr_t& adr);

	// returns true if servers are currently being refreshed
	bool IsRefreshing();

	// returns the number of servers not yet pinged
	int RefreshListRemaining();

private:
	// Run query logic for this frame
	void QueryFrame();

	// Send query to specified server on specified socket
	void QueryServer(unsigned int serverID);

	// recalculates a servers ping, from the last few ping times
	int CalculateAveragePing(serveritem_t &server);

	IServerRefreshResponse *m_pResponseTarget;

	enum
	{
		MAX_QUERY_SOCKETS = 255,
	};

	CSocket	*m_pQuery;	// Query socket for game server info

	// holds the list of all the currently active queries
	CUtlRBTree<query_t, unsigned short> m_Queries;

	// list of all known servers
	CUtlVector<serveritem_t> m_Servers;

	// list of servers to be refreshed, in order... this would be more optimal as a queue
	CUtlVector<int> m_RefreshList;

	int	m_iUpdateSerialNumber; // serial number of current update so we don't get results overlapping between different server list updates
	bool m_bRefreshing;	// Is refreshing taking place?
	int	m_nInvalidServers;	// number of servers marked as 'no not refresh'
	int	m_nRefreshedServers; // count of servers refreshed
};

#endif // SERVERLIST_H