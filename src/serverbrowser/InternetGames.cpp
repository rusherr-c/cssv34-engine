//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

// How often to re-sort the server list
const float MINIMUM_SORT_TIME = 1.5f;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//			NOTE:	m_Servers can not use more than 96 sockets, else it will
//					cause internet explorer to Stop working under win98 SE!
//-----------------------------------------------------------------------------
CInternetGames::CInternetGames(vgui::Panel *parent, const char *panelName, EPageType eType ) : 
	CBaseGamesPage(parent, panelName, eType )
{
	m_fLastSort = 0.0f;
	m_bDirty = false;
	m_bRequireUpdate = true;
	m_bOfflineMode = !IsSteamGameServerBrowsingEnabled();

	m_bAnyServersRetrievedFromMaster = false;
	m_bNoServersListedOnMaster = false;
	m_bAnyServersRespondedToQuery = false;

	m_pLocationFilter->DeleteAllItems();
	KeyValues *kv = new KeyValues("Regions");
	if (kv->LoadFromFile( g_pFullFileSystem, "servers/Regions.vdf", NULL))
	{
		// iterate the list loading all the servers
		for (KeyValues *srv = kv->GetFirstSubKey(); srv != NULL; srv = srv->GetNextKey())
		{
			struct regions_s region;

			region.name = srv->GetString("text");
			region.code = srv->GetInt("code");
			KeyValues *regionKV = new KeyValues("region", "code", region.code);
			m_pLocationFilter->AddItem( region.name.String(), regionKV );
			regionKV->deleteThis();
			m_Regions.AddToTail(region);
		}
	}
	else
	{
		Assert(!("Could not load file servers/Regions.vdf; server browser will not function."));
	}
	kv->deleteThis();

	LoadFilterSettings();

	ivgui()->AddTickSignal( GetVPanel(), 250 );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CInternetGames::~CInternetGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::PerformLayout()
{
	if ( !m_bOfflineMode && m_bRequireUpdate && ServerBrowserDialog().IsVisible() )
	{
		PostMessage( this, new KeyValues( "GetNewServerList" ), 0.1f );
		m_bRequireUpdate = false;
	}

	if ( m_bOfflineMode )
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_OfflineMode");
		m_pConnect->SetEnabled( false );
		m_pRefreshAll->SetEnabled( false );
		m_pRefreshQuick->SetEnabled( false );
		m_pAddServer->SetEnabled( false );
		m_pFilter->SetEnabled( false );
	}

	BaseClass::PerformLayout();
	m_pLocationFilter->SetEnabled(true);
}

//-----------------------------------------------------------------------------
// Purpose: Activates the page, starts refresh if needed
//-----------------------------------------------------------------------------
void CInternetGames::OnPageShow()
{
	if ( m_pGameList->GetItemCount() == 0 && ServerBrowserDialog().IsVisible() )
		BaseClass::OnPageShow();
	// the "internet games" tab (unlike the other browser tabs)
	// does not automatically start a query when the user
	// navigates to this tab unless they have no servers listed.
}


//-----------------------------------------------------------------------------
// Purpose: Called every frame, maintains sockets and runs refreshes
//-----------------------------------------------------------------------------
void CInternetGames::OnTick()
{
	if ( m_bOfflineMode )
	{
		BaseClass::OnTick();
		return;
	}

	BaseClass::OnTick();

	CheckRedoSort();
}


//-----------------------------------------------------------------------------
// Purpose: Handles incoming server refresh data
//			updates the server browser with the refreshed information from the server itself
//-----------------------------------------------------------------------------
void CInternetGames::ServerResponded( serveritem_t& server )
{
	m_bDirty = true;

	BaseClass::ServerResponded(server);
	m_bAnyServersRespondedToQuery = true;
	m_bAnyServersRetrievedFromMaster = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::ServerFailedToRespond( serveritem_t& server )
{
	m_bDirty = true;
	// TODO: implement this somehow
}


//-----------------------------------------------------------------------------
// Purpose: Called when server refresh has been completed
//-----------------------------------------------------------------------------
void CInternetGames::RefreshComplete( EMasterServerResponse response )
{
	SetRefreshing(false);
	UpdateFilterSettings();

	if ( response != k_eServerFailedToRespond )
	{
		if ( m_bAnyServersRespondedToQuery )
		{
			m_pGameList->SetEmptyListText( GetStringNoUnfilteredServers() );
		}
		else if ( response == k_eNoServersListedOnMasterServer )
		{
			m_pGameList->SetEmptyListText( GetStringNoUnfilteredServersOnMaster() );
		}
		else
		{
			m_pGameList->SetEmptyListText( GetStringNoServersResponded() );
		}
	}
	else
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_MasterServerNotResponsive");
	}

	// perform last sort
	m_bDirty = false;
	m_fLastSort = Plat_FloatTime();
	if (IsVisible())
	{
		m_pGameList->SortList();
	}

	UpdateStatus();

	BaseClass::RefreshComplete( response );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::GetNewServerList()
{
	BaseClass::GetNewServerList();
	UpdateStatus();

	m_bRequireUpdate = false;
	m_bAnyServersRetrievedFromMaster = false;
	m_bAnyServersRespondedToQuery = false;

	m_pGameList->DeleteAllItems();
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CInternetGames::SupportsItem(IGameList::InterfaceItem_e item)
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
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::CheckRedoSort( void )
{
	float fCurTime;

	// No changes detected
	if ( !m_bDirty )
		return;

	fCurTime = Plat_FloatTime();
	// Not time yet
	if ( fCurTime - m_fLastSort < MINIMUM_SORT_TIME)
		return;

	// postpone sort if mouse button is down
	if ( input()->IsMouseDown(MOUSE_LEFT) || input()->IsMouseDown(MOUSE_RIGHT) )
	{
		// don't sort for at least another second
		m_fLastSort = fCurTime - MINIMUM_SORT_TIME + 1.0f;
		return;
	}

	// Reset timer
	m_bDirty	= false;
	m_fLastSort = fCurTime;

	// Force sort to occur now!
	m_pGameList->SortList();
}


//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CInternetGames::OnOpenContextMenu(int itemID)
{
	// get the server
	int serverID = GetSelectedServerID();

	if ( serverID == -1 )
		return;

	// Activate context menu
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());
	menu->ShowMenu(this, serverID, true, true, true, true);
}

//-----------------------------------------------------------------------------
// Purpose: refreshes a single server
//-----------------------------------------------------------------------------
void CInternetGames::OnRefreshServer(int serverID)
{
	BaseClass::OnRefreshServer( serverID );

	ServerBrowserDialog().UpdateStatusText("#ServerBrowser_GettingNewServerList");
}


//-----------------------------------------------------------------------------
// Purpose: get the region code selected in the ui
// Output: returns the region code the user wants to filter by
//-----------------------------------------------------------------------------
int CInternetGames::GetRegionCodeToFilter()
{
	KeyValues *kv = m_pLocationFilter->GetActiveItemUserData();
	if ( kv )
		return kv->GetInt( "code" );
	else
		return 255;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInternetGames::CheckTagFilter( gameserveritem_t &server )
{
	// Servers without tags go in the official games, servers with tags go in custom games
	bool bOfficialServer = !( server.m_szGameTags && server.m_szGameTags[0] );
	if ( !bOfficialServer )
		return false;

	return true;
}
