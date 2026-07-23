//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: interface to steam managing game server/client match making
//
//=============================================================================

#ifndef ISERVERSINFO_H
#define ISERVERSINFO_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier0/threadtools.h"
#include "tier1/bitbuf.h"
#include "tier1/netadr.h"
#include "proto_oob.h"
#include "protocol.h"
#include "IServerRefreshResponse.h"
#include "Socket.h"
#include "ServerList.h"

#define LIST_REFRESH_TIMEOUT 3.5f // default timeout for all lists (excluding main list)

//
// class for each game server
// in server browser
// 
class serveritem_t
{
public:
	serveritem_t() = default;

	netadr_t m_NetAdr;						///< IP/Query Port/Connection Port for this server
	int  m_nPing;							///< current ping time in milliseconds
	int  m_nProtocolVersion;
	int	 m_nReceivedStatus;					///< internal
	bool m_bHadSuccessfulResponse;			///< server has responded successfully in the past
	bool m_bDoNotRefresh;					///< server is marked as not responding and should no longer be refreshed
	char m_szGameDir[64];					///< current game directory
	char m_szMap[64];						///< current map
	char m_szServerRules[256];				///< server rules
	char m_szGameDescription[128];			///< game description
	int  m_nAppID;
	int  m_nPlayers;
	int  m_nMaxPlayers;						///< Maximum players that can join this server
	int  m_nBotPlayers;						///< Number of bots (i.e simulated players) on this server
	bool m_bPassword;						///< true if this server needs a password to join
	bool m_bSecure;							///< server uses some kind of anticheat (e.g VAC)
	char m_szGameVersion[64];

	int  m_iFlags;

	/// Game server name
	char m_szServerName[256];

	char* ToString() noexcept {
		static char buffer[1024];
		memset(&buffer, 0, sizeof(buffer));

		sprintf(buffer, "%s, %i, %i, %d, %d, %s, %s, %s, %s, %i, %i, %i, %i, %d, %d, %s, %i, %s",
			m_NetAdr.ToString(),
			m_nPing,
			m_nProtocolVersion,
			m_bHadSuccessfulResponse,
			m_bDoNotRefresh,
			m_szGameDir,
			m_szMap,
			m_szServerRules,
			m_szGameDescription,
			m_nAppID,
			m_nPlayers,
			m_nMaxPlayers,
			m_nBotPlayers,
			m_bPassword,
			m_bSecure,
			m_szGameVersion,
			m_iFlags,
			m_szServerName);

		return buffer;
	}

};

enum EServerQuery
{
	k_eQuery_Any = -1,
	k_ePingServer = 1,
	k_ePlayerDetails,
//	k_eServerRules
};

//-----------------------------------------------------------------------------
// Purpose: Functions for match making services for clients to get to game lists and details
//-----------------------------------------------------------------------------
class CServersInfo : public CMsgHandler
{
public:
	CServersInfo(); // Constructor
	~CServersInfo(); // Destructor

	void Initialize(); // Do some things like parsing masterservers.vdf
	void Shutdown(); // Shutdown...

	void RunFrame(); // Runs every frame

public:
	// Request Server List from master server...
	void RequestInternetServerList(const char* gamedir, IServerRefreshResponse* response);
	void RequestLANServerList(const char* gamedir, IServerRefreshResponse* response);
	void RequestFavoritesServerList(const char* gamedir, IServerRefreshResponse* response);
	void RequestHistoryServerList(const char* gamedir, IServerRefreshResponse* response);

	// Stop refreshing current list
	void StopRefresh();

	// Add server to favorites/history list
	void AddFavoriteServer(uint32 unIP, uint16 usPort);
	void AddHistoryServer(uint32 unIP, uint16 usPort, int32 time32LastPlayed);

	// Remove server from favorites/history list
	void RemoveFavoriteServer(uint32 unIP, uint16 usPort);
	void RemoveHistoryServer(uint32 unIP, uint16 usPort);

	// Query info about single server (TODO!)
	void PingServer(uint32 unIP, uint16 usPort, IServerQueryResponse* response);
	void PlayerDetails(uint32 unIP, uint16 usPort, IServerQueryResponse* response);
	void ServerRules(uint32 unIP, uint16 usPort, IServerQueryResponse* response);
protected:
	// Internal functions //

	// Thread
	static void Thread(CServersInfo* pthis);

	// Add master server to m_vecMasterAddresses
	void AddMasterServer(const netadr_t& adr);

	// Add vector of netadr to m_vecMasterAddresses
	void AddMasterServers(const CUtlVector<netadr_t>& vec);

	// Use default master addresses
	void UseDefaultMasters();

	// Request server list from masterserver
	void RequestServerList(const netadr_t& adr);

	// Process server list
	void ProcessServerList(const netadr_t& from, bf_read& msg);

	// CMsgHandler
	bool Process(const netadr_t& from, bf_read& msg);

private:
	bool			m_bInitialized;
	bool			m_bWorking;
	ThreadHandle_t	m_hThread;

	// MasterServers.vdf
	CUtlVector<netadr_t> m_vecMasterAddresses;

	char			m_szGameDir[32];

	double			m_flStartRequestTime;
	bool			m_bRefreshing;

	CSocket*		m_pMasterSocket;		//<
	CSocket*		m_pQuerySocket;			//< used for server queries (TODO!)

	CServerList*	m_pCurrentList;			//< current server list (one of those)
	CServerList*	m_pMainList;			//< main internet list
	CServerList*	m_pFavoritesList;		//< favorites list
	CServerList*	m_pHistoryList;			//< history list
	CServerList*	m_pLanServerList;		//< lan server list
};	

extern CServersInfo* g_pServersInfo;

#endif // ISERVERSINFO_H
