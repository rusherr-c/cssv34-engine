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
// Purpose: 
//-----------------------------------------------------------------------------
void CFavoriteGames::LoadFavoritesList()
{
	/*
	if ()
	{
		// set empty message
		m_pGameList->SetEmptyListText("#ServerBrowser_NoFavoriteServers");
	}
	else
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

	}
	*/
	if (m_bRefreshOnListReload)
	{
		m_bRefreshOnListReload = false;
		StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CFavoriteGames::SupportsItem(IGameList::InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
	case ADDSERVER:
		return true;

	case ADDCURRENTSERVER:
		return true;
	
	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CFavoriteGames::RefreshComplete( NServerResponse response )
{
	SetRefreshing(false);
	if ( response == nNoServersListedOnMasterServer )
	{
		// set empty message
		m_pGameList->SetEmptyListText("#ServerBrowser_NoFavoriteServers");
	}
	else
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

	}
	m_pGameList->SortList();

	BaseClass::RefreshComplete( response );
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
	serveritem_t *pConnected = ServerBrowserDialog().GetCurrentConnectedServer();

	if ( pConnected )
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
