#include "pch_serverbrowser.h"

// maximum masterservers that can be parsed from masterservers.vdf
#define MAX_MASTERSERVERS 16

static char masterServers[][32] =
{
	"80.78.244.170:27011", // css v34 default
	"78.154.103.37:10232", // nttnmDev (https://github.com/nttnmDev/cssv34masterserver)
};

// Thread sleep interval (ms)
#define THREAD_SLEEP_INTERVAL 40

#define LANBROADCAST_MIN_PORT 27000
#define LANBROADCAST_MAX_PORT 27100

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
	m_pSocket = new CSocket();

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

	delete m_pSocket;
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

	m_pSocket->Frame();
	
	if (m_pCurrentList)
		m_pCurrentList->RunFrame();

	if (m_flStartRequestTime < Plat_FloatTime() - LIST_REFRESH_TIMEOUT) {
		StopRefresh();
	}
}

// Request Server List from master server
void CServersInfo::RequestInternetServerList(const char* gamedir, IServerListResponse* response) {
	if (!response || !gamedir)
		return;

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

	V_strcpy_safe(m_szGameDir, gamedir);
	m_pLanServerList->m_pResponseTarget = response;
	m_pCurrentList = m_pLanServerList;
	m_flStartRequestTime = Plat_FloatTime();

	char buffer[64];
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

	if (m_pCurrentList)
		m_pCurrentList->StopRefresh();
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
	
}

// Process server list
void CServersInfo::ProcessServerList(bf_read* msg) {

}

// CMsgHandler
bool CServersInfo::Process(netadr_t* from, bf_read* msg) {

}