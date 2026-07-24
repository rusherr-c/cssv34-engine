//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFavoriteGames::CFavoriteGames(vgui::Panel *parent) : 
	CBaseGamesPage(parent, "FavoriteGames", eFavoritesServer )
{
	m_bRefreshOnListReload = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFavoriteGames::~CFavoriteGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: loads favorites list from disk
//-----------------------------------------------------------------------------
void CFavoriteGames::LoadFavoritesList(KeyValues* favoritesData)
{
	// load in favorites
	for (KeyValues* dat = favoritesData->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey())
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

		// add to list
		BaseClass::ServerResponded(server);

		// next, add to serversinfo (this is neccessary because serversinfo does refresh thing)
		g_pServersInfo->AddFavoriteServer(server.m_NetAdr.GetIPHostByteOrder(), server.m_NetAdr.GetPort());
	}

	// set empty message
	m_pGameList->SetEmptyListText("#ServerBrowser_NoFavoriteServers");

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
void CFavoriteGames::SaveFavoritesList(KeyValues* favoritesData)
{
	favoritesData->Clear();

	// loop through all the servers writing them into the doc
	for (int i = 0; i < m_vecServers.Count(); i++)
	{
		serveritem_t& server = *GetServer(i);
		if (server.m_bDoNotRefresh)
			continue;

		KeyValues* dat = favoritesData->CreateNewKey();

		dat->SetString("name", server.m_szServerName);
		dat->SetString("gamedir", server.m_szGameDir);
		dat->SetInt("players", server.m_nPlayers);
		dat->SetInt("maxplayers", server.m_nMaxPlayers);
		dat->SetString("map", server.m_szMap);
		dat->SetString("address", server.m_NetAdr.ToString());
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CFavoriteGames::SupportsItem(InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
	case ADDSERVER:
		return true;

	case ADDCURRENTSERVER:
		return !IsSteam() && BFiltersVisible();
	
	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CFavoriteGames::RefreshComplete(EMasterServerResponse response)
{
	SetRefreshing(false);
	if (response == k_eNoServersListedOnMasterServer)
	{
		// set empty message
		m_pGameList->SetEmptyListText("#ServerBrowser_NoFavoriteServers");
	}
	else
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

	}
	m_pGameList->SortList();

	BaseClass::RefreshComplete(response);
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CFavoriteGames::OnOpenContextMenu(int itemID)
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());

	// get the server
	int serverID = GetSelectedServerID();

	if ( serverID != -1 )
	{
		// Activate context menu
		menu->ShowMenu(this, serverID, true, true, true, false);
		menu->AddMenuItem("RemoveServer", "#ServerBrowser_RemoveServerFromFavorites", new KeyValues("RemoveFromFavorites"), this);
	}
	else
	{
		// no selected rows, so don't display default stuff in menu
		menu->ShowMenu( this,(uint32)-1, false, false, false, false );
	}
	
	menu->AddMenuItem("AddServerByName", "#ServerBrowser_AddServerByIP", new KeyValues("AddServerByName"), this);
}


//-----------------------------------------------------------------------------
// Purpose: removes a server from the favorites
//-----------------------------------------------------------------------------
void CFavoriteGames::OnRemoveFromFavorites()
{
	if (!g_pServersInfo)
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
			g_pServersInfo->RemoveFavoriteServer(pServer->m_NetAdr.GetIPHostByteOrder(), pServer->m_NetAdr.GetPort());
		}
	}

	UpdateStatus();
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a server by IP address
//-----------------------------------------------------------------------------
void CFavoriteGames::OnAddServerByName()
{
	// open the add server dialog
	CDialogAddServer *dlg = new CDialogAddServer( &ServerBrowserDialog(), this );
	dlg->MoveToCenterOfScreen();
	dlg->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: Adds the currently connected server to the list
//-----------------------------------------------------------------------------
void CFavoriteGames::OnAddCurrentServer()
{
	serveritem_t* pConnected = ServerBrowserDialog().GetCurrentConnectedServer();

	if (pConnected)
	{
		g_pServersInfo->AddFavoriteServer(pConnected->m_NetAdr.GetIPHostByteOrder(), pConnected->m_NetAdr.GetPort());
		m_bRefreshOnListReload = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse posted messages
//			 
//-----------------------------------------------------------------------------
void CFavoriteGames::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "AddServerByName"))
	{
		OnAddServerByName();
	}
	else if (!Q_stricmp(command, "AddCurrentServer" ))
	{
		OnAddCurrentServer();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: enables adding server
//-----------------------------------------------------------------------------
void CFavoriteGames::OnConnectToGame()
{
	m_pAddCurrentServer->SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: disables adding current server
//-----------------------------------------------------------------------------
void CFavoriteGames::OnDisconnectFromGame( void )
{
	m_pAddCurrentServer->SetEnabled( false );
}
