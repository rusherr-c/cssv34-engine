#include "pch_serverbrowser.h"

// maximum masterservers that can be parsed from masterservers.vdf
#define MAX_MASTERSERVERS 16

static char masterServers[][32] =
{
	"78.154.103.37:10232", // nttnmDev (https://github.com/nttnmDev/cssv34masterserver)
};

// Thread sleep interval (ms)
#define THREAD_SLEEP_INTERVAL 40

#define LANBROADCAST_MIN_PORT 27000
#define LANBROADCAST_MAX_PORT 27100

static CServersInfo s_serversinfo;
CServersInfo* g_pServersInfo = &s_serversinfo;

void CServersInfo::Thread(CServersInfo* pthis)
{
	if (!pthis)
		return;

	DevMsg("ServersInfo receive thread started.\n");

	while (pthis->m_bWorking)
	{
		ThreadSleep(THREAD_SLEEP_INTERVAL);
		pthis->RunFrame();
	}

	DevMsg("ServersInfo receive thread shutting down.\n");

	return;
}
 
CServersInfo::CServersInfo()
{
	m_bInitialized = false;
	m_pMasterSocket = new CSocket();
	m_pQuerySocket = new CSocket();

	m_szGameDir[0] = 0;
	m_bRefreshing = false;

	// null for some time...
	m_pCurrentList = nullptr;
	m_pMainList = new CServerList(nullptr);
	m_pFavoritesList = new CServerList(nullptr);
	m_pHistoryList = new CServerList(nullptr);
	m_pLanServerList = new CServerList(nullptr);

	Initialize();
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

// Do some things like parsing masterservers.vdf
void CServersInfo::Initialize() {
	if (m_bInitialized)
		return;

	m_bInitialized = true;
	m_bWorking = true;

	// create our thread
	m_hThread = CreateSimpleThread((ThreadFunc_t)Thread, this);

	m_pMasterSocket->AddHandler(this);
	UseDefaultMasters();
}

// Shutdown...
void CServersInfo::Shutdown() {
	if (!m_bInitialized)
		return;

	m_bInitialized = false;
	m_bWorking = false;

	CloseHandle(m_hThread);
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
					m_pCurrentList->m_pResponseTarget->RefreshComplete(nNoServersListedOnMasterServer);
				else;
			else
				m_pCurrentList->m_pResponseTarget->RefreshComplete(nServerResponded);
		}
		StopRefresh();
	}
}

// Request Server List from master server
void CServersInfo::RequestInternetServerList(const char* gamedir, IServerListResponse* response) {
	if (!response || !gamedir)
		return;

	StopRefresh();
	m_bRefreshing = true;

	V_strcpy_safe(m_szGameDir, gamedir);
	m_pMainList->m_pResponseTarget = response;
	m_pCurrentList = m_pMainList;
	m_flStartRequestTime = Plat_FloatTime();

	FOR_EACH_VEC(m_vecMasterAddresses, i)
	{
		RequestServerList(m_vecMasterAddresses[i]);
	}
}

// Request LAN Server List
void CServersInfo::RequestLANServerList(const char* gamedir, IServerListResponse* response) {
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
	msg.WriteByte(A2S_INFOREQUEST);
	msg.WriteString(A2S_KEY_STRING);

	for (int i = LANBROADCAST_MIN_PORT; i < LANBROADCAST_MAX_PORT + 1; i++)
	{
		m_pLanServerList->m_pQuery->Broadcast(i, msg);
	}
}

// Request Favorites List
void CServersInfo::RequestFavoritesServerList(const char* gamedir, IServerListResponse* response) {
	if (!response || !gamedir)
		return;

	StopRefresh();
	m_bRefreshing = true;

	V_strcpy_safe(m_szGameDir, gamedir);
	m_pFavoritesList->m_pResponseTarget = response;
	m_pCurrentList = m_pFavoritesList;
	m_flStartRequestTime = Plat_FloatTime();

	m_pFavoritesList->StartRefresh();
}

// Request History List
void CServersInfo::RequestHistoryServerList(const char* gamedir, IServerListResponse* response) {
	if (!response || !gamedir)
		return;

	StopRefresh();
	m_bRefreshing = true;

	V_strcpy_safe(m_szGameDir, gamedir);
	m_pHistoryList->m_pResponseTarget = response;
	m_pCurrentList = m_pHistoryList;
	m_flStartRequestTime = Plat_FloatTime();

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
	// todo
}

void CServersInfo::AddHistoryServer(uint32 unIP, uint16 usPort, time_t timeLastPlayed) {
	// todo
}

// Remove server from favorites/history list
void CServersInfo::RemoveFavoriteServer(uint32 unIP, uint16 usPort) {
	// todo
}

void CServersInfo::RemoveHistoryServer(uint32 unIP, uint16 usPort) {
	// todo
}

// Query info about single server (TODO!)
void CServersInfo::PingServer(uint32 unIP, uint16 usPort, IServerPingResponse* response) {
	// todo
}

void CServersInfo::PlayerDetails(uint32 unIP, uint16 usPort, IServerPlayersResponse* response) {
	// todo
}

bool CServersInfo::CancelServerQuery(EServerQuery type, uint32 unIP, uint16 usPort) {
	// todo
	return true;
}

// Internal functions //

void CServersInfo::AddMasterServer(const netadr_t& adr) {
	if (adr.GetType() != NA_IP)
		return;
	
	if (adr.GetIPHostByteOrder() == 0 || adr.GetPort() == 0)
		return;
	
	m_vecMasterAddresses.AddToTail(adr);
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

		adr.SetFromString(masterServers[i]);
		
		AddMasterServer(adr);
	}
}

// This is set and used by RequestServerList and ProcessServerList
static netadr_t lastServerAddress;

// Request server list from masterserver
void CServersInfo::RequestServerList(const netadr_t& adr) {
	if (!m_bRefreshing)
		return;

	char gamedir[256];
	strcpy(gamedir, "\\gamedir\\");
	strcat(gamedir, m_szGameDir);

	char buf[256];
	bf_write msg(buf, sizeof(buf));

	msg.WriteByte(C2M_CLIENTQUERY);
	msg.WriteByte(0xFF);
	msg.WriteString(lastServerAddress.ToString());
	msg.WriteString(gamedir);

	m_pMasterSocket->Send(adr, msg);
}

// Process server list
void CServersInfo::ProcessServerList(const netadr_t& from, bf_read& msg) {
	if (!m_bRefreshing)
		return;

	uint32 unIP = ntohl(msg.ReadLong());
	uint16 usPort = ntohs(msg.ReadShort());
	uint32 id = 0;

	while (unIP != 0 && usPort != 0)
	{
		serveritem_t server{};
		server.m_NetAdr = netadr_t(unIP, usPort);

		// Add this server to server list
		id = m_pMainList->AddNewServer(server);
		// Add to refresh list
		m_pMainList->AddServerToRefreshList(id);

		// Next ip & port
		unIP = ntohl(msg.ReadLong());
		usPort = ntohs(msg.ReadShort());

		lastServerAddress.SetIPAndPort(unIP, usPort);
		//RequestServerList(from);
	}

	if (lastServerAddress.IsValid())
	{
		RequestServerList(from);
	}

	// Start Refreshing
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
	case S2A_INFOREPLY:
	{
		// todo
		break;
	}
	default:
		break;

	}

	return true;
}