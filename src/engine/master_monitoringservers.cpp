#include <WinLite.h>
#include "quakedef.h"
#include "sys_dll.h"
#include "dbg.h"
#include "color.h"
#include "http.h"
#include "master.h"
#include "../thirdparty/nlohmann/json.hpp"

#define HTTP_REQUEST_TIMEOUT 5.0 // seconds

#undef WaitForSingleObject
#undef CreateThread
#undef EnterCriticalSection

using json = nlohmann::json;

const char* g_pszApiURLs[] = {
	"https://api.gamemonitoring.net/servers",
	"https://gamemonitoring.net/api/servers"
};

struct serverFilter
{
	serverFilter() { pchName[0] = pchValue[0] = 0; }
	serverFilter(const char* name, const char* value) {
		strcpy(pchName, name);
		strcpy(pchValue, value);
	}
	char pchName[64];
	char pchValue[32];
};

/*
* get server list from gamemonitoring api
* -----------
* @input filters, filtercount
* @output JSON
*/
char* get_servers(serverFilter* filters, int filtercount)
{
	Warning("get_servers\n");
	int iResSize = 0;
	int iRetCode = 0;
	void* pData = nullptr;

	char url[2048] = {};
	for (int i = 0; i < sizeof(g_pszApiURLs) / sizeof(*g_pszApiURLs); i++)
	{
		sprintf(url, "%s?", g_pszApiURLs[i]);
		for (int a = 0; a < filtercount; a++)
		{
			strcat(url, filters[a].pchName);
			strcat(url, "=");
			strcat(url, filters[a].pchValue);
			if (filtercount == 1 && a == 0)
				break;

			strcat(url, "&");
		}

		Msg("Request URL: %s\n", url);

		pData = HTTPGetRequest(url, &iResSize, &iRetCode);

		Msg(
			"HTTP result: code=%d size=%d ptr=%p\n",
			iRetCode,
			iResSize,
			pData);

		if (pData && iResSize && iRetCode == 200)
			break;
	}

	if (!pData || !iResSize || iRetCode != 200)
	{
		Warning("Request Error at get_servers.\nURL: % s\nReturn code : % i\n", url, iRetCode);
		return nullptr;
	}

	static std::string jsonBuffer;

	jsonBuffer.assign(
		(const char*)pData,
		iResSize
	);

	free(pData);

	Plat_DebugString(jsonBuffer.c_str());
	Plat_DebugString("\n");

	return (char*)jsonBuffer.c_str();
}

/*
* get information about server
* @input serverID
* @output JSON
*/
char* get_server(uint32 id)
{
	//Warning("get_server\n");
	int iResSize = 0;
	int iRetCode = 0;
	void* pData = nullptr;

	char url[2048] = {};
	for (int i = 0; i < sizeof(g_pszApiURLs) / sizeof(*g_pszApiURLs); i++)
	{
		sprintf(url, "%s/%u", g_pszApiURLs[i], id);

		//Msg("Request URL: %s\n", url);

		pData = HTTPGetRequest(url, &iResSize, &iRetCode);

		//Msg(
		//	"HTTP result: code=%d size=%d ptr=%p\n",
		//	iRetCode,
		//	iResSize,
		//	pData);

		if (pData && iResSize && iRetCode == 200)
			break;
	}

	if (!pData || !iResSize || iRetCode != 200)
	{
		Warning("Request Error at get_server.\nURL: % s\nReturn code : % i\n", url, iRetCode);
		return nullptr;
	}

	static std::string jsonBuffer;

	jsonBuffer.assign(
		(const char*)pData,
		iResSize
	);

	free(pData);

	extern ConVar developer;
	if (developer.GetInt() == 2) {
		Plat_DebugString(jsonBuffer.c_str());
		Plat_DebugString("\n");
	}

	return (char*)jsonBuffer.c_str();
}

//
// gameMonitoring.net server list
//
class CGameMonitoringServerList : public IServerList
{
public:
	CGameMonitoringServerList();
	~CGameMonitoringServerList();

	void RunFrame();
	void RequestServerList(const char* gamedir, IServerListResponse* response);
	void StopRefresh();

	void ProcessConnectionlessPacket(netpacket_t* packet) {}
	void AddServer(uint32 unIP, uint16 usPort, time_t timeLastPlayed = 0ull) {}
	void RemoveServer(uint32 unIP, uint16 usPort) {}

private:
	static DWORD WINAPI ThreadProc(LPVOID);
	void Worker();

	HANDLE m_hThread;
	HANDLE m_hWake;

	CRITICAL_SECTION m_CS;

	bool m_bShutdown;

	volatile LONG m_RequestID;
	volatile LONG m_WorkingRequest;

	bool m_bFinished;

	IServerListResponse* m_pResponse;

	CUtlVector<newgameserver_t> m_Queue;
};

static CGameMonitoringServerList s_monitoringservers;
IServerList* monitoringservers = (IServerList*)&s_monitoringservers;

CGameMonitoringServerList::CGameMonitoringServerList()
{
	InitializeCriticalSection(&m_CS);

	m_bShutdown = false;
	m_bFinished = false;

	m_hWake = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	//m_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	m_hThread = CreateThread(
		nullptr,
		0,
		ThreadProc,
		this,
		0,
		nullptr);
}

CGameMonitoringServerList::~CGameMonitoringServerList()
{
	m_bShutdown = true;

	SetEvent(m_hWake);

	WaitForSingleObject(m_hThread, INFINITE);

	CloseHandle(m_hThread);
	CloseHandle(m_hWake);
	//CloseHandle(m_hStopEvent);

	DeleteCriticalSection(&m_CS);
}

void CGameMonitoringServerList::RunFrame()
{
	if (!m_pResponse)
		return;

	EnterCriticalSection(&m_CS);

	while (m_Queue.Count())
	{
		m_pResponse->ServerResponded(m_Queue[0]);
		m_Queue.Remove(0);
	}

	bool finished = m_bFinished;

	if (finished)
		m_bFinished = false;

	LeaveCriticalSection(&m_CS);

	if (finished)
		m_pResponse->RefreshComplete(nServerResponded);
}

void CGameMonitoringServerList::StopRefresh()
{
	InterlockedIncrement(&m_RequestID);
}

DWORD WINAPI CGameMonitoringServerList::ThreadProc(LPVOID p)
{
	((CGameMonitoringServerList*)p)->Worker();
	
	return 0;
}

void CGameMonitoringServerList::Worker()
{
	while (!m_bShutdown)
	{
		WaitForSingleObject(m_hWake, INFINITE);

		if (m_bShutdown)
			break;

		//
		// We remember the current request ID.
		// If RequestServerList() is called again,
		// m_RequestID will change and this Worker will terminate itself.
		//
		LONG requestID = m_RequestID;

		char game[32];
		itoa(GetSteamInfIDVersionInfo().AppID, game, 10);

		serverFilter filters[] =
		{
			{ "game", game },
			{ "version", GetSteamInfIDVersionInfo().szVersionString },
			{ "limit", "256" }
		};

		char* raw = get_servers(filters, ARRAYSIZE(filters));

		if (!raw)
			continue;

		json root;

		try
		{
			root = json::parse(raw);
		}
		catch (const std::exception& e)
		{
			Warning("GameMonitoring: %s\n", e.what());
			continue;
		}

		if (!root.contains("response"))
			continue;

		if (!root["response"].contains("items"))
			continue;

		for (const auto& item : root["response"]["items"])
		{
			//
			// User pressed Refresh?
			//

			if ((item & 15) == 0)
				Sleep(1);

			if (requestID != m_RequestID)
				break;

			char* rawServer = get_server(item["id"]);

			if (!rawServer)
				continue;

			json serverRoot;

			try
			{
				serverRoot = json::parse(rawServer);
			}
			catch (...)
			{
				continue;
			}

			if (!serverRoot.contains("response"))
				continue;

			auto& sv = serverRoot["response"];

			newgameserver_t server{};

			try
			{
				char connectAddr[32];

				sprintf(connectAddr, "%s:%u", sv["ip"].get<std::string>().c_str(), sv["port"].get<short>());

				server.m_NetAdr.SetFromString(connectAddr);
				//Msg("Added %s\n", connectAddr);
			}
			catch (...)
			{
				continue;
			}

			strcpy(server.m_szGameDir,
				GetSteamInfIDVersionInfo().szProductString);

			strcpy(server.m_szMap,
				sv["map"].get<std::string>().c_str());

			strcpy(server.m_szGameDescription,
				sv["gamemode"].get<std::string>().c_str());

			strcpy(server.m_szServerName,
				sv["name"].get<std::string>().c_str());

			strcpy(server.m_szGameVersion,
				sv["version"].get<std::string>().c_str());

			server.m_nProtocolVersion = 7;
			server.m_nAppID = sv["game"];
			server.m_bSecure = sv["secured"];
			server.m_bPassword = sv["private"];
			server.m_nPlayers = sv["numplayers"];
			server.m_nBotPlayers = sv["bots"];
			server.m_nMaxPlayers = sv["maxplayers"];

			//
			// During the HTTP request, 
			// the user may have already canceled the update.
			//
			if (requestID != m_RequestID)
				break;

			EnterCriticalSection(&m_CS);
			m_Queue.AddToTail(server);
			LeaveCriticalSection(&m_CS);
		}

		//
		// Marking update as finished only if,
		// this is still relevant request
		//
		if (requestID == m_RequestID)
		{
			EnterCriticalSection(&m_CS);
			m_bFinished = true;
			LeaveCriticalSection(&m_CS);
		}
	}
}

void CGameMonitoringServerList::RequestServerList(
	const char* gamedir,
	IServerListResponse* response)
{
	EnterCriticalSection(&m_CS);

	m_pResponse = response;

	m_Queue.RemoveAll();

	m_bFinished = false;

	InterlockedIncrement(&m_RequestID);

	LeaveCriticalSection(&m_CS);

	SetEvent(m_hWake);
}
