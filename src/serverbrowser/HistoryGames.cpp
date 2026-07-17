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
void CHistoryGames::LoadHistoryList()
{
	if ( m_bRefreshOnListReload )
	{
		m_bRefreshOnListReload = false;
		StartRefresh();
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CHistoryGames::SupportsItem(IGameList::InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
	case GETNEWLIST:
		return true;

	default:
		return false;
	}
}



//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CHistoryGames::RefreshComplete( NServerResponse response )
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
	if ( !SteamMatchmakingServers() || !SteamMatchmaking() )
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

