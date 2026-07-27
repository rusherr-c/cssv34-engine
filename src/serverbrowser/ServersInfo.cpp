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
};

// This is set and used by RequestServerList and ProcessServerList
static netadr_t gLastAdr;

ServersInfoQueryResponse::ServersInfoQueryResponse() {
	m_unIP = 0;
	m_usPort = 0;
	m_nChallengeNr = -1;
	m_bResponseSet = 0;
	m_pResponseTarget = 0;
	m_currentQuery = k_eQuery_Any;
}
ServersInfoQueryResponse::~ServersInfoQueryResponse() {

}

void ServersInfoQueryResponse::SetResponseTarget(IServerQueryResponse* response)
{
	if (response)
	{
		m_bResponseSet = true;
		m_pResponseTarget = response;
	}
}

// Set current query
void ServersInfoQueryResponse::SetCurrentQuery(EServerQuery query, uint32 unIP, uint16 usPort) {
	m_currentQuery = query;
	m_unIP = unIP;
	m_usPort = usPort;
}

// Get challenge number received in ChallengeReceived callback
int ServersInfoQueryResponse::GetChallengeNr()
{
	return m_nChallengeNr;
}

// Got challenge number from the server
void ServersInfoQueryResponse::ChallengeReceived(int challenge) {
	m_nChallengeNr = challenge;

	if (m_currentQuery == k_ePlayerDetails)
		return g_pServersInfo->PlayerDetails(m_unIP, m_usPort, this);
	else if (m_currentQuery == k_eServerRules)
		return g_pServersInfo->ServerRules(m_unIP, m_usPort, this);

	m_pResponseTarget->ChallengeReceived(challenge);
}

// Server has responded successfully and has updated data
void ServersInfoQueryResponse::ServerResponded(serveritem_t& server) {
	if (m_bResponseSet)
		m_pResponseTarget->ServerResponded(server);
}

// Got data on a server rule -- you'll get this callback once per FCVAR_NOTIFY
// cvar on the server which you have requested rules data on.
void ServersInfoQueryResponse::RulesResponded(const char* pchRule, const char* pchValue) {
	if (m_bResponseSet)
		m_pResponseTarget->RulesResponded(pchRule, pchValue);
}

// The server failed to respond to the request for server rules
void ServersInfoQueryResponse::RulesFailedToRespond() {
	if (m_bResponseSet)
		m_pResponseTarget->RulesFailedToRespond();
}

// The server has finished responding to the server rules request
void ServersInfoQueryResponse::RulesRefreshComplete() {
	if (m_bResponseSet)
		m_pResponseTarget->RulesRefreshComplete();
}

// Got data on a new player on the server -- you'll get this callback once per player
// on the server which you have requested player data on.
void ServersInfoQueryResponse::AddPlayerToList(const char* pchName, int nScore, float flTimePlayed) {
	if (m_bResponseSet)
		m_pResponseTarget->AddPlayerToList(pchName, nScore, flTimePlayed);
}

// The server failed to respond to the request for player details
void ServersInfoQueryResponse::PlayersFailedToRespond() {
	if (m_bResponseSet)
		m_pResponseTarget->PlayersFailedToRespond();
}

// The server has finished responding to the player details request
void ServersInfoQueryResponse::PlayersRefreshComplete() {
	if (m_bResponseSet)
		m_pResponseTarget->PlayersRefreshComplete();
}


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
	m_pMasterSocket = new CSocket();
	m_pQueryResponse = new ServersInfoQueryResponse();
	m_pQuerySocket = new CSocket();
	m_pQueryHandler = new CServerDetailsMsgHandler(m_pQueryResponse);

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
	delete m_pQuerySocket;
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
	m_pQuerySocket->AddHandler(m_pQueryHandler);

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
		Msg("Could not load file MasterServers.vdf, server browser will not function.\n");
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

	Msg("ServersInfo shutting down.\n");
}

// Runs every frame
void CServersInfo::RunFrame()
{
	if (!m_bRefreshing)
		return;

	m_pMasterSocket->Frame();
	m_pQuerySocket->Frame();

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
	Warning("PingServer %s\n", netadr_t(unIP, usPort).ToString());
	m_pQueryResponse->SetResponseTarget(response);
	m_pQueryResponse->SetCurrentQuery(k_ePingServer, unIP, usPort);

	char buf[64];
	bf_write msg(buf, sizeof(buf));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_INFO_REQUEST);
	msg.WriteString(A2S_KEY_STRING);

	m_pQuerySocket->Send(netadr_t(unIP, usPort), msg);
}

void CServersInfo::PlayerDetails(uint32 unIP, uint16 usPort, IServerQueryResponse* response) {
	m_pQueryResponse->SetResponseTarget(response);
	m_pQueryResponse->SetCurrentQuery(k_ePlayerDetails, unIP, usPort);

	char buf[16];
	bf_write msg(buf, sizeof(buf));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_PLAYER_REQUEST);
	msg.WriteLong(m_pQueryResponse->GetChallengeNr());

	m_pQuerySocket->Send(netadr_t(unIP, usPort), msg);
}

void CServersInfo::ServerRules(uint32 unIP, uint16 usPort, IServerQueryResponse* response) {
	m_pQueryResponse->SetResponseTarget(response);
	m_pQueryResponse->SetCurrentQuery(k_eServerRules, unIP, usPort);

	char buf[16];
	bf_write msg(buf, sizeof(buf));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_RULES_REQUEST);
	msg.WriteLong(m_pQueryResponse->GetChallengeNr());

	m_pQuerySocket->Send(netadr_t(unIP, usPort), msg);
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

	// check connectionless header
	if (msg.ReadLong() != CONNECTIONLESS_HEADER)
		return false;

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