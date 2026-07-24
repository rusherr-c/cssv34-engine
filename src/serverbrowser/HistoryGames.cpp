//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHistoryGames::CHistoryGames(vgui::Panel *parent) : 
	CBaseGamesPage(parent, "HistoryGames", eHistoryServer )
{
	m_bRefreshOnListReload = false;
	m_pGameList->AddColumnHeader(9, "LastPlayed", "#ServerBrowser_LastPlayed", 100);
	m_pGameList->SetSortFunc(9, LastPlayedCompare);
	m_pGameList->SetSortColumn(9);

	if ( !IsSteamGameServerBrowsingEnabled() )
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_OfflineMode");
		m_pConnect->SetEnabled( false );
		m_pRefreshAll->SetEnabled( false );
		m_pRefreshQuick->SetEnabled( false );
		m_pAddServer->SetEnabled( false );
		m_pFilter->SetEnabled( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHistoryGames::~CHistoryGames()
{
}		

//-----------------------------------------------------------------------------
// Purpose: loads favorites list from disk
//-----------------------------------------------------------------------------
void CHistoryGames::LoadHistoryList(KeyValues* historyData)
{
	// load in favorites
	for (KeyValues* dat = historyData->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey())
	{
		serveritem_t server;
		memset(&server, 0, sizeof(server));

		const char* addr = dat->GetString("address");
		server.m_NetAdr.SetFromString(addr, true); // do a dns lookup
		server.m_nPlayers = 0;
		V_strncpy(server.m_szServerName, dat->GetString("name"), sizeof(server.m_szServerName));
		V_strncpy(server.m_szMap, dat->GetString("map"), sizeof(server.m_szMap));
		V_strncpy(server.m_szGameDir, dat->GetString("gamedir"), sizeof(server.m_szGameDir));
		server.m_nPlayers = dat->GetInt("players");
		server.m_nMaxPlayers = dat->GetInt("maxplayers");
		server.m_ulTimeLastPlayed = dat->GetInt("lastplayed");

		// add to list
		BaseClass::ServerResponded(server);

		// next, add to serversinfo (this is neccessary because serversinfo does refresh thing)
		g_pServersInfo->AddHistoryServer(server.m_NetAdr.GetIPHostByteOrder(), server.m_NetAdr.GetPort(), server.m_ulTimeLastPlayed);
	}

	// set empty message
	m_pGameList->SetEmptyListText("#ServerBrowser_NoServersPlayed");

	// refresh this list
	if (m_bRefreshOnListReload)
	{
		m_bRefreshOnListReload = false;
		StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: saves the current list of servers to the favorites section of the data file
//-----------------------------------------------------------------------------
void CHistoryGames::SaveHistoryList(KeyValues* historyData)
{
	historyData->Clear();

	// loop through all the servers writing them into the doc
	for (int i = 0; i < m_vecServers.Count(); i++)
	{
		serveritem_t& server = *GetServer(i);
		if (server.m_bDoNotRefresh)
			continue;

		KeyValues* dat = historyData->CreateNewKey();

		dat->SetString("name", server.m_szServerName);
		dat->SetString("gamedir", server.m_szGameDir);
		dat->SetInt("players", server.m_nPlayers);
		dat->SetInt("maxplayers", server.m_nMaxPlayers);
		dat->SetString("map", server.m_szMap);
		dat->SetString("address", server.m_NetAdr.ToString());
		dat->SetInt("lastplayed", server.m_ulTimeLastPlayed);
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CHistoryGames::SupportsItem(InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
		return true;
	
	case ADDSERVER:
	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CHistoryGames::RefreshComplete( EMasterServerResponse response )
{
	SetRefreshing(false);
	m_pGameList->SetEmptyListText("#ServerBrowser_NoServersPlayed");
	m_pGameList->SortList();

	BaseClass::RefreshComplete( response );
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CHistoryGames::OnOpenContextMenu(int itemID)
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());

	// get the server
	int serverID = GetSelectedServerID();

	if(  serverID != -1 )
	{
		// Activate context menu
		menu->ShowMenu(this, serverID, true, true, true, true);
		menu->AddMenuItem("RemoveServer", "#ServerBrowser_RemoveServerFromHistory", new KeyValues("RemoveFromHistory"), this);
	}
	else
	{
		// no selected rows, so don't display default stuff in menu
		menu->ShowMenu(this, (uint32)-1, false, false, false, false);
	}
}


//-----------------------------------------------------------------------------
// Purpose: removes a server from the favorites
//-----------------------------------------------------------------------------
void CHistoryGames::OnRemoveFromHistory()
{
	if (!SteamMatchmakingServers() || !SteamMatchmaking())
		return;

	// iterate the selection
	for (int iGame = 0; iGame < m_pGameList->GetSelectedItemsCount(); iGame++)
	{
		int itemID = m_pGameList->GetSelectedItem(iGame);
		int serverID = m_pGameList->GetItemData(itemID)->userData;

		serveritem_t* pServer = GetServer(serverID);

		if (pServer)
		{
			m_pGameList->RemoveItem(itemID);
			g_pServersInfo->RemoveHistoryServer(pServer->m_NetAdr.GetIPHostByteOrder(), pServer->m_NetAdr.GetPort());
		}
	}

	UpdateStatus();
	InvalidateLayout();
	Repaint();
}

