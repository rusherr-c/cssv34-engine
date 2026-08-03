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

/*
 * Purpose: used internally in engine,
 * returns how many master servers we have in the list
*/
DLL_EXPORT int GetNumMasterServers()
{
	return _ARRAYSIZE(masterServers);
}

/*
 * Purpose: used internally in engine,
 * get master server address at nServer
*/
DLL_EXPORT int GetMasterServer(int nServer, char* szIpAddrPort, int nLen)
{
	if (!masterServers[nServer])
		return -1;

	V_strncpy(szIpAddrPort, masterServers[nServer], nLen);

	return 0;
}

static ConVar sb_serversinfo_multithreading("sb_serversinfo_multithreading", "1", FCVAR_ARCHIVE,
	"Enable multithreading (fixes socket bugs & memory leaks)", CServersInfo::MultiThreadingChangeCallback);

static ConVar sb_serversinfo_threads("sb_serversinfo_threads", "3", FCVAR_ARCHIVE,
	"Maximum serversinfo threads (2 or higher only if multithreading is enabled)", 1, 1, 3, 3,
	CServersInfo::ThreadCountChangeCallback);

// This is set and used by RequestServerList and ProcessServerList
static netadr_t gLastAdr;

void CServersInfo::Thread(int* pThreadNum)
{
	if (!g_pServersInfo)
		return;

	int threadNum = (int)pThreadNum;

	DevMsg("ServersInfo thread %i started.\n", threadNum + 1);

	while (g_pServersInfo->m_bWorking)
	{
		if (!g_pServersInfo->m_bInitialized)
			break;

		Sleep(THREAD_SLEEP_INTERVAL);
		g_pServersInfo->RunFrame(threadNum);
	}

	Msg("ServersInfo thread %i shutting down.\n", threadNum + 1);

	return;
}

/* CONSTRUCTOR */
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

/* DESTRUCTOR */
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

/*
 * Purpose: Do some things like parsing masterservers.vdf
*/
void CServersInfo::Initialize() {
	if (m_bInitialized)
		return;

	m_bInitialized = true;
	m_bWorking = true;

	// create our threads
	CreateAllThreads();

	m_pMasterSocket->AddHandler(this);

	// load masters from config file
	KeyValues* kv = new KeyValues("MasterServers");

	CUtlStringList masterServerNames;
	m_vecMasterAddresses.RemoveAll();

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

/*
 * Purpose: Shutdown
*/
void CServersInfo::Shutdown() {
	if (!m_bInitialized)
		return;

	m_pMasterSocket->RemoveHandler(this);
	m_bInitialized = false;
	m_bWorking = false;

	// Wait until all threads have finished
	WaitForMultipleObjects(m_nThreadCount, (HANDLE*)m_hThreads, TRUE, INFINITE);

	// Clean up handles
	for (int i = 0; i < m_nThreadCount; ++i) {
		CloseHandle(m_hThreads[i]);
	}
}

/*
 * Purpose: RunFrame
*/
void CServersInfo::RunFrame(int threadNum)
{
	if (threadNum == 0 && m_nThreadCount > 1) {
		m_pServerCommunication->RunFrame();
		return;
	}

	if (threadNum == 1 && m_nThreadCount > 1 && m_nThreadCount > 2) {
		m_pMasterSocket->Frame();
		return;
	}
	else if (threadNum == 2 || m_nThreadCount == 2)
	{
		if (!m_bRefreshing)
			return;

		if (m_pCurrentList)
			m_pCurrentList->RunFrame();

		float timeout = ((m_pCurrentList == m_pMainList) ?
			MAIN_LIST_REFRESH_TIMEOUT : LIST_REFRESH_TIMEOUT);

		if (m_flStartRequestTime < Plat_FloatTime() - timeout) {
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

	if (m_nThreadCount == 1)
	{
		m_pServerCommunication->RunFrame();

		m_pMasterSocket->Frame();

		if (!m_bRefreshing)
			return;

		if (m_pCurrentList)
			m_pCurrentList->RunFrame();

		float timeout = ((m_pCurrentList == m_pMainList) ?
			MAIN_LIST_REFRESH_TIMEOUT : LIST_REFRESH_TIMEOUT);

		if (m_flStartRequestTime < Plat_FloatTime() - timeout) {
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
}

/*
 * Purpose: Request server list from the master server
*/
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

/*
 * Purpose: Request LAN Server List
*/
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

/*
 * Purpose: Request favorites list
*/
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

/*
 * Purpose: Request history list
*/
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

/*
 * Purpose: Stop refreshing current list
*/
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

/*
 * Purpose: Query info about a single server
*/
void CServersInfo::PingServer(uint32 unIP, uint16 usPort, IServerQueryResponse* response) {
	int handle = m_pServerCommunication->QueryServerInfo(netadr_t(unIP, usPort), response);
}

/*
 * Purpose: Query information about players on this server
*/
void CServersInfo::PlayerDetails(uint32 unIP, uint16 usPort, IServerQueryResponse* response) {
	int handle = m_pServerCommunication->QueryPlayerDetails(netadr_t(unIP, usPort), response);
}

/*
 * Purpose: Query server rules
*/
void CServersInfo::ServerRules(uint32 unIP, uint16 usPort, IServerQueryResponse* response) {
	int handle = m_pServerCommunication->QueryServerRules(netadr_t(unIP, usPort), response);
}

/*
 * MultiThreading change callback
*/
void CServersInfo::MultiThreadingChangeCallback(
	IConVar* pConVar, const char* pOldValue, float flOldValue) {

	if (!g_pServersInfo)
		return;

	if (sb_serversinfo_threads.GetInt() < 2)
		return;

	ConVarRef var(pConVar);

	if (atoi(pOldValue) == var.GetInt())
		return;

	g_pServersInfo->Shutdown();
	g_pServersInfo->Initialize();
}

/*
 * ThreadCount change callback
*/
void CServersInfo::ThreadCountChangeCallback(
	IConVar* pConVar, const char* pOldValue, float flOldValue) {

	if (!g_pServersInfo)
		return;

	ConVarRef var(pConVar);

	if (!sb_serversinfo_multithreading.GetBool())
	{
		var.SetValue(1);
		return;
	}

	if (atoi(pOldValue) == var.GetInt())
		return;

	g_pServersInfo->Shutdown();
	g_pServersInfo->Initialize();
}

/*
 * Helper function that creates all threads
*/
void CServersInfo::CreateAllThreads()
{
	bool enableMultiThreading = sb_serversinfo_multithreading.GetBool();
	m_nThreadCount = sb_serversinfo_threads.GetInt();

	if (enableMultiThreading && m_nThreadCount > 1)
	{
		for (int i = 0; i < m_nThreadCount; i++)
		{
			m_hThreads[i] = (ThreadHandle_t)
				CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Thread, (void*)i, 0, 0);
		}

		return;
	}

	m_nThreadCount = 1;
	int id = 0;

	m_hThreads[0] = (ThreadHandle_t)
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Thread, (void*)0, 0, 0);
}

/*
 * Add master server to the master list
*/
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

/*
 * Add master server to the master list
*/
void CServersInfo::AddMasterServers(const CUtlVector<netadr_t>& vec) {

	FOR_EACH_VEC(vec, i)
	{
		if (!vec.IsValidIndex(i))
			continue;

		AddMasterServer(vec[i]);
	}
}

/*
 * Use default master servers
*/
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

/*
 * Request server list from master server
*/
void CServersInfo::RequestServerList(const netadr_t& adr) {
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

/*
 * Process server list
*/
void CServersInfo::ProcessServerList(const netadr_t& from, bf_read& msg)
{
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

		if (!msg.IsOverflowed())
		{
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

/*
 * CMsgHandler
*/
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
