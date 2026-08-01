/*
 *
 * Copyright (c) 2026 RuSHeRR
 *
 * Purpose: implementation of class that works
 *	like SteamClient's SteamMatchmakingServers on Master Server Query Protocol
 * 
 * VDC: https://developer.valvesoftware.com/wiki/Master_Server_Query_Protocol
 * 
*/
#include "pch_serverbrowser.h"

// maximum masterservers that can be parsed from masterservers.vdf
#define MAX_MASTERSERVERS 16

// Thread sleep interval (ms)
#define THREAD_SLEEP_INTERVAL 50

#define LANBROADCAST_MIN_PORT 27000
#define LANBROADCAST_MAX_PORT 27100

static char masterServers[][37] =
{	
	"78.154.103.37:10232", // nttnmDev (https://github.com/nttnmDev/cssv34masterserver)
	"91.218.230.217:27011", // reserved
};

//
// Purpose: used internally in engine,
// returns how many master servers we have in the list
//
DLL_EXPORT int GetNumMasterServers()
{
	return _ARRAYSIZE(masterServers);
}

//
// Purpose: used internally in engine,
// get master server address at nServer
//
DLL_EXPORT int GetMasterServer(int nServer, char* szIpAddrPort, int nLen)
{
	if (!masterServers[nServer])
		return -1;

	V_strncpy(szIpAddrPort, masterServers[nServer], nLen);

	return 0;
}

// This is set and used by RequestServerList and ProcessServerList
static netadr_t gLastAdr;

void CServersInfo::Thread(CServersInfo* pthis)
{
	if (!pthis)
		return;

	DevMsg("ServersInfo receive thread started.\n");

	while (pthis->m_bWorking)
	{
		if (!pthis->m_bInitialized)
			break;

		Sleep(THREAD_SLEEP_INTERVAL);
		pthis->RunFrame();
	}

	Msg("ServersInfo receive thread shutting down.\n");

	return;
}
 
CServersInfo::CServersInfo()
{
	m_bInitialized = false;
	m_pMasterSocket = new CSocket(24010);
	m_pServerCommunication = new CServerCommunication();

	m_szGameDir[0] = 0;
	m_bRefreshing = false;

	// null for some time...
	m_pCurrentList = nullptr;
	m_pMainList = new CServerList(nullptr);
	m_pFavoritesList = new CServerList(nullptr);
	m_pHistoryList = new CServerList(nullptr);
	m_pLanServerList = new CServerList(nullptr);
}

CServersInfo::~CServersInfo()
{
	Shutdown();

	m_szGameDir[0] = 0;

	delete m_pMasterSocket;
	delete m_pServerCommunication;
	delete m_pMainList;
	delete m_pHistoryList;
	delete m_pLanServerList;
}

// Do some things like parsing masterservers.vdf (do not call in constructor!)
void CServersInfo::Initialize() {
	if (m_bInitialized)
		return;

	m_bInitialized = true;
	m_bWorking = true;

	// create our thread
	m_hThread = (ThreadHandle_t)
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Thread, this, 0, 0);

	m_pMasterSocket->AddHandler(this);
	//m_pQuerySocket->AddHandler(m_pQueryHandler);

	// load masters from config file
	KeyValues* kv = new KeyValues("MasterServers");

	CUtlStringList masterServerNames;

	if (kv->LoadFromFile(g_pFullFileSystem, "masterservers.vdf", "CONFIG"))
	{
		// iterate the list loading all the servers
		for (KeyValues* srv = kv->GetFirstSubKey(); srv != NULL; srv = srv->GetNextKey())
		{
			// default?
			if (!strcmp(srv->GetString("addr"), "default"))
				UseDefaultMasters();

			masterServerNames.AddToTail((char*)srv->GetString("addr"));
		}
	}
	else
	{
		Msg("Could not load file MasterServers.vdf.\n");

		// Save defaults
		KeyValues* entry = kv->FindKey("0", true);
		entry->SetString("addr", "default");

		kv->SaveToFile(g_pFullFileSystem, "masterservers.vdf", "CONFIG");
		kv->deleteThis();
	}

	// make sure we have at least one master listed
	if (masterServerNames.Count() < 1)
	{
		// add the default master
		UseDefaultMasters();	
		return;
	}

	// add masters
	FOR_EACH_VEC(masterServerNames, i)
	{
		if (!masterServerNames.IsValidIndex(i))
			continue;

		AddMasterServer(masterServerNames[i]);
	}
}

// Shutdown...
void CServersInfo::Shutdown() {
	if (!m_bInitialized)
		return;

	m_bInitialized = false;
	m_bWorking = false;

	WaitForSingleObject(m_hThread, INFINITE);

	CloseHandle(m_hThread);
	m_hThread = nullptr;
}

// Runs every frame
void CServersInfo::RunFrame()
{
	// This needs to be runned before refresh check!
	m_pServerCommunication->RunFrame();

	if (!m_bRefreshing)
		return;

	m_pMasterSocket->Frame();

	if (m_pCurrentList)
		m_pCurrentList->RunFrame();

	if (m_flStartRequestTime < Plat_FloatTime() - LIST_REFRESH_TIMEOUT) {
		if (m_pCurrentList)
		{
			if (m_pCurrentList->ServerCount() < 1)
				if (m_pCurrentList->m_pResponseTarget)
					m_pCurrentList->m_pResponseTarget->RefreshComplete(k_eNoServersListedOnMasterServer);
				else;
			else
				m_pCurrentList->m_pResponseTarget->RefreshComplete(k_eServerResponded);
		}
		StopRefresh();
	}
}

// Request Server List from master server
void CServersInfo::RequestInternetServerList(const char* gamedir, IServerRefreshResponse* response) {
	if (!response || !gamedir)
		return;

	StopRefresh();
	m_bRefreshing = true;

	V_strcpy_safe(m_szGameDir, gamedir);
	m_pMainList->m_pResponseTarget = response;
	m_pCurrentList = m_pMainList;
	m_flStartRequestTime = Plat_FloatTime();
	gLastAdr = netadr_t();

	FOR_EACH_VEC(m_vecMasterAddresses, i)
	{
		RequestServerList(m_vecMasterAddresses[i]);
	}
}

// Request LAN Server List
void CServersInfo::RequestLANServerList(const char* gamedir, IServerRefreshResponse* response) {
	if (!response || !gamedir)
		return;

	StopRefresh();
	m_bRefreshing = true;

	V_strcpy_safe(m_szGameDir, gamedir);
	m_pLanServerList->m_pResponseTarget = response;
	m_pCurrentList = m_pLanServerList;
	m_flStartRequestTime = Plat_FloatTime();

	char buffer[32];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_INFO_REQUEST);
	msg.WriteString(A2S_KEY_STRING);

	for (int i = LANBROADCAST_MIN_PORT; i < LANBROADCAST_MAX_PORT + 1; i++)
	{
		m_pLanServerList->m_pQuery->Broadcast(i, msg);
	}
}

// Request Favorites List
void CServersInfo::RequestFavoritesServerList(const char* gamedir, IServerRefreshResponse* response) {
	if (!response || !gamedir)
		return;

	StopRefresh();
	m_bRefreshing = true;

	V_strcpy_safe(m_szGameDir, gamedir);
	m_pFavoritesList->m_pResponseTarget = response;
	m_pCurrentList = m_pFavoritesList;
	m_flStartRequestTime = Plat_FloatTime();

	m_pFavoritesList->AddAllServersToRefreshList();
	m_pFavoritesList->StartRefresh();
}

// Request History List
void CServersInfo::RequestHistoryServerList(const char* gamedir, IServerRefreshResponse* response) {
	if (!response || !gamedir)
		return;

	StopRefresh();
	m_bRefreshing = true;

	V_strcpy_safe(m_szGameDir, gamedir);
	m_pHistoryList->m_pResponseTarget = response;
	m_pCurrentList = m_pHistoryList;
	m_flStartRequestTime = Plat_FloatTime();

	m_pHistoryList->AddAllServersToRefreshList();
	m_pHistoryList->StartRefresh();
}

// Stop refreshing current list
void CServersInfo::StopRefresh() {
	if (!m_bRefreshing)
		return;

	m_bRefreshing = false;
	m_flStartRequestTime = 0.0f;

	m_pMainList->Clear(); // Clearing instead of StopRefresh
	m_pFavoritesList->StopRefresh();
	m_pHistoryList->StopRefresh();
	m_pLanServerList->StopRefresh();
}

// Add server to favorites/history list
void CServersInfo::AddFavoriteServer(uint32 unIP, uint16 usPort) {
	serveritem_t server{};
	server.m_NetAdr.SetIPAndPort(unIP, usPort);

	// Add this server to server list
	unsigned id = m_pFavoritesList->AddNewServer(server);
	// Add to refresh list
	m_pFavoritesList->AddServerToRefreshList(id);
}

void CServersInfo::AddHistoryServer(uint32 unIP, uint16 usPort, int32 time32LastPlayed) {
	serveritem_t server{};
	server.m_NetAdr.SetIPAndPort(unIP, usPort);

	// Add this server to server list
	unsigned id = m_pHistoryList->AddNewServer(server);
	// Add to refresh list
	m_pHistoryList->AddServerToRefreshList(id);
}

// Remove server from favorites/history list
void CServersInfo::RemoveFavoriteServer(uint32 unIP, uint16 usPort) {
	netadr_t adr(unIP, usPort);

	// Find server by id
	uint id = m_pFavoritesList->FindServer(adr);

	m_pFavoritesList->RemoveServer(id);
}

void CServersInfo::RemoveHistoryServer(uint32 unIP, uint16 usPort) {
	netadr_t adr(unIP, usPort);

	// Find server by id
	uint id = m_pHistoryList->FindServer(adr);

	m_pHistoryList->RemoveServer(id);
}

// Query info about single server (TODO!)
void CServersInfo::PingServer(uint32 unIP, uint16 usPort, IServerQueryResponse* response) {
	int handle = m_pServerCommunication->QueryServerInfo(netadr_t(unIP, usPort), response);
}

void CServersInfo::PlayerDetails(uint32 unIP, uint16 usPort, IServerQueryResponse* response) {
	int handle = m_pServerCommunication->QueryPlayerDetails(netadr_t(unIP, usPort), response);
}

void CServersInfo::ServerRules(uint32 unIP, uint16 usPort, IServerQueryResponse* response) {
	int handle = m_pServerCommunication->QueryServerRules(netadr_t(unIP, usPort), response);
}
// Internal functions //

void CServersInfo::AddMasterServer(const netadr_t& adr) {
	if (!adr.IsValid() || !adr.IsBaseAdrValid())
		return;

	for (auto& s : m_vecMasterAddresses)
	{
		if (s.CompareAdr(adr))
			return;
	}

	ConColorMsg(Color(150, 255, 150, 255), "Added master server %s\n", adr.ToString());
	
	m_vecMasterAddresses.AddToTail(adr);
}

void CServersInfo::AddMasterServers(const CUtlVector<netadr_t>& vec) {

	FOR_EACH_VEC(vec, i)
	{
		if (!vec.IsValidIndex(i))
			continue;

		AddMasterServer(vec[i]);
	}
}

// Use default master addresses
void CServersInfo::UseDefaultMasters() 
{
	netadr_t adr;

	Msg("Using default master addresses\n");

	for (int i = 0; i < ARRAYSIZE(masterServers); i++)
	{
		if (i > MAX_MASTERSERVERS)
			break;

		adr.SetFromString(masterServers[i], true);
		
		AddMasterServer(adr);
	}
}

// Request server list from masterserver
void CServersInfo::RequestServerList(const netadr_t& adr) {
	if (!m_bRefreshing)
		return;

	// reset request time
	m_flStartRequestTime = Plat_FloatTime();

	char gamedir[256];
	strcpy(gamedir, "\\gamedir\\");
	strcat(gamedir, m_szGameDir);

	char buf[256];
	bf_write msg(buf, sizeof(buf));

	msg.WriteByte(C2M_CLIENTQUERY);
	msg.WriteByte(0xFF);
	msg.WriteString(gLastAdr.ToString());
	msg.WriteString(gamedir);

	m_pMasterSocket->Send(adr, msg);
}

// Process server list
void CServersInfo::ProcessServerList(const netadr_t& from, bf_read& msg) {
	if (!m_bRefreshing)
		return;

	uint32 unIP = ntohl(msg.ReadLong());
	uint16 usPort = ntohs(msg.ReadWord());

	int i = 0;

	while (i < msg.m_nDataBytes)
	{
		serveritem_t server{};
		server.m_NetAdr = netadr_t(unIP, usPort);

		// Add this server to server list
		unsigned id = m_pMainList->AddNewServer(server);
		// Add to refresh list
		m_pMainList->AddServerToRefreshList(id);

		// Next ip & port
		unIP = ntohl(msg.ReadLong());
		usPort = ntohs(msg.ReadWord());

		if (!msg.IsOverflowed()) {
			gLastAdr.SetIPAndPort(unIP, usPort);
		}

		i += 6;
	}

	if (gLastAdr.IsValid())
	{
		RequestServerList(from);
	}

	// Start Refreshing the list
	m_pMainList->StartRefresh();

}

// CMsgHandler
bool CServersInfo::Process(const netadr_t& from, bf_read& msg) {

	char c = msg.ReadByte();

	switch (c)
	{

	case M2C_QUERY:
	{
		// Next after 'f' is 0A
		if (msg.ReadByte() != 0x0A)
			return false;

		FOR_EACH_VEC(m_vecMasterAddresses, i)
		{
			if (!from.CompareAdr(m_vecMasterAddresses[i]))
				continue;
			else
				break;

			return false;
		}

		ProcessServerList(from, msg);

		break;
	}
	default:
		break;

	}

	return true;
}
