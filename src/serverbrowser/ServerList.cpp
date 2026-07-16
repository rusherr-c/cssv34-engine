//========= Copyright   1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#define PROTECTED_THINGS_DISABLE

#include <stdlib.h>  // atoi
#include "ServersInfo.h"
#include "ServerList.h"
#include "Socket.h"
#include "proto_oob.h"

// for debugging
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>

#define min(a,b)    (((a) < (b)) ? (a) : (b))

typedef enum
{
	NONE = 0,
	INFO_REQUESTED,
	INFO_RECEIVED
} QUERYSTATUS;

//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CServerListMsgHandler : public CMsgHandler
{
public:
	CServerListMsgHandler(CServerList* list) { m_pList = list; }
	~CServerListMsgHandler() {}

	virtual bool Process(const netadr_t& from, bf_read& msg);

private:
	CServerList* m_pList;
};

//-----------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CServerListMsgHandler::Process(const netadr_t& from, bf_read& msg) {
	double recvTime = Plat_FloatTime();

	serveritem_t server{};

	// check connectionless header
	if (msg.ReadLong() != CONNECTIONLESS_HEADER)
		return false;

	char c = msg.ReadByte();

	// check if it's the info
	if (c != S2A_INFOREPLY)
		return false;

	if (from.GetIPHostByteOrder() == 0 || from.GetPort() == 0)
		return false;

	// set server address
	server.m_NetAdr = from;

	server.m_nProtocolVersion = msg.ReadByte();

	msg.ReadString(server.m_szServerName, sizeof(server.m_szServerName));
	msg.ReadString(server.m_szMap, sizeof(server.m_szMap));
	msg.ReadString(server.m_szGameDir, sizeof(server.m_szGameDir));

	msg.ReadString(server.m_szGameDescription, sizeof(server.m_szGameDescription));
	server.m_nAppID = msg.ReadShort();

	// player info
	server.m_nPlayers = msg.ReadByte();
	server.m_nMaxPlayers = msg.ReadByte();
	server.m_nBotPlayers = msg.ReadByte();

	// Password?
	msg.ReadByte(); // server type
	msg.ReadByte(); // env

	server.m_bPassword = msg.ReadByte();
	server.m_bSecure = msg.ReadByte();
	msg.ReadString(server.m_szGameVersion, sizeof(server.m_szGameVersion));

	server.m_iFlags = msg.ReadByte();

	if (server.m_iFlags & S2A_EDF_GAMEPORT)
	{
		server.m_NetAdr.SetPort(msg.ReadShort());
	}

	if (server.m_iFlags & S2A_EDF_STEAMID)
	{
		uint64 ulSteamID;
		msg.ReadBytes(&ulSteamID, 8);
	}

	if (server.m_iFlags & S2A_EDF_SOURCETV)
	{
		char str[64];
		msg.ReadShort(); // spectator port (unused)
		msg.ReadString(str, sizeof(str)); // spectator sv name
	}

	if (server.m_iFlags & S2A_EDF_GAMETAGS)
	{
		char str[64];
		msg.ReadString(str, sizeof(str));
	}

	if (server.m_iFlags & S2A_EDF_GAMEID)
	{
		uint64 ulGameID;
		msg.ReadBytes(&ulGameID, 8);
	}

	Msg("CServerListMsgHandler: server processed\n%s\n", server.ToString());
	
	// Update this server in the list
	m_pList->UpdateServer(&server.m_NetAdr, server, recvTime);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function used in query redblack tree
//-----------------------------------------------------------------------------
bool QueryLessFunc( const query_t &item1, const query_t &item2 )
{
	// compare port then ip
	if (item1.addr.port < item2.addr.port)
		return true;
	else if (item1.addr.port > item2.addr.port)
		return false;

	int ip1 = *(int *)&item1.addr.ip;
	int ip2 = *(int *)&item2.addr.ip;

	return ip1 < ip2;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerList::CServerList(IServerListResponse *target) : m_Queries(0, MAX_QUERY_SOCKETS, QueryLessFunc)
{
	m_pResponseTarget = target;
	m_iUpdateSerialNumber = 1;

	m_nInvalidServers = 0;
	m_nRefreshedServers = 0;
	m_bRefreshing = false;

	// setup sockets
	m_pQuery = new CSocket(0);
	m_pQuery->AddHandler(new CServerListMsgHandler(this));
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerList::~CServerList()
{
	delete m_pQuery;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerList::RunFrame()
{
	if (m_pQuery)
	{
		m_pQuery->Frame();
	}

	QueryFrame();
}

//-----------------------------------------------------------------------------
// Purpose: gets a server from the list by id, range [0, ServerCount)
//-----------------------------------------------------------------------------
serveritem_t &CServerList::GetServer(unsigned int serverID)
{
	if (m_Servers.IsValidIndex(serverID))
	{
		return m_Servers[serverID];
	}

	// return a dummy
	static serveritem_t dummyServer;
	memset(&dummyServer, 0, sizeof(dummyServer));
	return dummyServer;
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of servers
//-----------------------------------------------------------------------------
int CServerList::ServerCount()
{
	return m_Servers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of servers not yet pinged
//-----------------------------------------------------------------------------
int CServerList::RefreshListRemaining()
{
	return m_RefreshList.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the server list is still in the process of talking to servers
//-----------------------------------------------------------------------------
bool CServerList::IsRefreshing()
{
	return m_bRefreshing;
}

//-----------------------------------------------------------------------------
// Purpose: adds a new server to the list
//-----------------------------------------------------------------------------
unsigned int CServerList::AddNewServer(serveritem_t &server)
{
	// make sure the server isn't already here
	// this is a bug in the master server, sending us servers we already have
	/*
	for (int i = 0; i < m_Servers.Count(); i++)
	{
		if (m_Servers[i].ip[0] == server.ip[0]
			&& m_Servers[i].ip[1] == server.ip[1]
			&& m_Servers[i].ip[2] == server.ip[2]
			&& m_Servers[i].ip[3] == server.ip[3]
			&& m_Servers[i].port == server.port)
		{
			// assert(!("ADDING DUPLICATE SERVER"));
			return i;
		}
	}
	*/

	unsigned int serverID = m_Servers.AddToTail(server);
	//m_Servers[serverID].serverID = serverID;
	return serverID;
}

//-----------------------------------------------------------------------------
// Purpose: Clears all servers from the list
//-----------------------------------------------------------------------------
void CServerList::Clear()
{
	StopRefresh();

	m_Servers.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: stops all refreshing
//-----------------------------------------------------------------------------
void CServerList::StopRefresh()
{
	// Reset query context
	m_Queries.RemoveAll();

	// reset server received states
	int count = ServerCount();
	for (int i = 0; i < count; i++)
	{
		m_Servers[i].m_bHadSuccessfulResponse = 0;
	}

	m_RefreshList.RemoveAll();

	// up the serial number so previous results don't interfere
	m_iUpdateSerialNumber++;

	m_nInvalidServers = 0;
	m_nRefreshedServers = 0;
	m_bRefreshing = false;
}

//-----------------------------------------------------------------------------
// Purpose: marks a server to be refreshed
//-----------------------------------------------------------------------------
void CServerList::AddServerToRefreshList(unsigned int serverID)
{
	if (!m_Servers.IsValidIndex(serverID))
		return;

	serveritem_t &server = m_Servers[serverID];
	server.m_bHadSuccessfulResponse = NONE;

	m_RefreshList.AddToTail(serverID);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerList::StartRefresh()
{
	if (m_RefreshList.Count() > 0)
	{
		m_bRefreshing = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles a refresh response from a server
//-----------------------------------------------------------------------------
void CServerList::UpdateServer(netadr_t* adr, serveritem_t& sv, double recvTime)
{
	if (!m_pResponseTarget)
		return;

	// find the reply in the query list
	query_t finder;
	finder.addr = *adr;
	int queryIndex = m_Queries.Find(finder);
	if (queryIndex == m_Queries.InvalidIndex())
		return;

	DevMsg("CServerList::UpdateServer: Updating \"%s\" server\n", sv.m_szServerName);

	query_t& query = m_Queries[queryIndex];
	int serverIndex = query.serverID;
	float sendTime = query.sendTime;

	// remove the query from the list
	m_Queries.RemoveAt(queryIndex);

	// update the server
	serveritem_t& server = m_Servers[serverIndex];
	if (server.m_nReceivedStatus != INFO_RECEIVED)
	{
		m_nRefreshedServers++;
		server = sv;
		server.m_nReceivedStatus = INFO_RECEIVED;
	}

	// dont copy received status!
	int recv_status = server.m_nReceivedStatus;
	server = sv;

	server.m_nReceivedStatus = recv_status;
	server.m_bHadSuccessfulResponse = true;

	int ping = (int)((recvTime - sendTime) * 1000);

	server.m_nReceivedStatus = INFO_RECEIVED;

	// notify the UI of the new server info
	m_pResponseTarget->ServerResponded(server);
}

//-----------------------------------------------------------------------------
// Purpose: recalculates a servers ping, from the last few ping times
//-----------------------------------------------------------------------------
int CServerList::CalculateAveragePing(serveritem_t &server)
{
	// you should not use that
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Called every frame to check queries
//-----------------------------------------------------------------------------
void CServerList::QueryFrame()
{
	if (!m_bRefreshing)
		return;

	if (!m_pResponseTarget)
		return;

	double curtime = Plat_FloatTime();

	// walk the query list, looking for any server timeouts
	unsigned short idx = m_Queries.FirstInorder();
	while (m_Queries.IsValidIndex(idx))
	{
		query_t &query = m_Queries[idx];
		if ((curtime - query.sendTime) > 1.2f)
		{
			// server has timed out
			serveritem_t &item = m_Servers[query.serverID];

			// mark the server
			if (!item.m_bHadSuccessfulResponse)
			{
				// remove the server if it has never responded before
				item.m_bDoNotRefresh = true;
				m_nInvalidServers++;
			}
			// respond to the game list notifying of the lack of response
			item.m_nReceivedStatus = false;

			// get the next server now, since we're about to delete it from query list
			unsigned short nextidx = m_Queries.NextInorder(idx);
			
			// delete the query
			m_Queries.RemoveAt(idx);

			// move to next item
			idx = nextidx;
		}
		else
		{
			// still waiting for server result
			idx = m_Queries.NextInorder(idx);
		}
	}

	// see if we should send more queries
	while (m_RefreshList.Count() > 0)
	{
		// get the first item from the list to refresh
		int currentServer = m_RefreshList[0];
		if (!m_Servers.IsValidIndex(currentServer))
			break;

		QueryServer(m_pQuery, currentServer);

		// remove the server from the refresh list
		m_RefreshList.Remove((int)0);
	}

	// Done querying?
	if (m_Queries.Count() < 1)
	{
		m_bRefreshing = false;
		m_pResponseTarget->RefreshComplete(nServerResponded);

		// up the serial number, so that we ignore any late results
		m_iUpdateSerialNumber++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: sends a status query packet to a single server
//-----------------------------------------------------------------------------
void CServerList::QueryServer(CSocket *socket, unsigned int serverID)
{
	serveritem_t &server = m_Servers[serverID];

	// add into query list
	query_t query;
	query.addr = server.m_NetAdr;
	query.serverID = serverID;

	int duplicateIndex = m_Queries.Find(query);
	if (m_Queries.IsValidIndex(duplicateIndex))
	{
		// we're already trying to ping the server... break out
		// kill the server
		server.m_bHadSuccessfulResponse = false;
		server.m_bDoNotRefresh = true;
		return;
	}

	// Set state
	server.m_nReceivedStatus = (int)INFO_REQUESTED;

	char buffer[64];
	bf_write msg(buffer,sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_INFOREQUEST);
	msg.WriteString(A2S_KEY_STRING);

	// Sendmessage
	socket->Send(server.m_NetAdr, msg);

	// insert the query into the list and set the time
	int idx = m_Queries.Insert(query);
	m_Queries[idx].sendTime = Plat_FloatTime();
}


