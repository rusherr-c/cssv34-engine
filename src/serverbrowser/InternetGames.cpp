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

#define NUM_COMMON_TAGS			20

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TagMenuButton::TagMenuButton(Panel *parent, const char *panelName, const char *text) : BaseClass(parent,panelName,text)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TagMenuButton::OnShowMenu( vgui::Menu *menu )
{
	PostActionSignal(new KeyValues("TagMenuButtonOpened"));
	BaseClass::OnShowMenu(menu);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCustomServerInfoURLQuery : public vgui::QueryBox
{
	DECLARE_CLASS_SIMPLE( CCustomServerInfoURLQuery, vgui::QueryBox );
public:
	CCustomServerInfoURLQuery(const char *title, const char *queryText,vgui::Panel *parent) : BaseClass( title, queryText, parent )
	{
		SetOKButtonText( "#ServerBrowser_CustomServerURLButton" );
	}
};

DECLARE_BUILD_FACTORY( TagInfoLabel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TagInfoLabel::TagInfoLabel(Panel *parent, const char *panelName) : BaseClass(parent,panelName, (const char *)NULL, NULL)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TagInfoLabel::TagInfoLabel(Panel *parent, const char *panelName, const char *text, const char *pszURL) : BaseClass(parent,panelName,text,pszURL)
{
}

//-----------------------------------------------------------------------------
// Purpose: If we were left clicked on, launch the URL
//-----------------------------------------------------------------------------
void TagInfoLabel::OnMousePressed(MouseCode code)
{
	if (code == MOUSE_LEFT)
	{
		if ( GetURL() )
		{
			// Pop up the dialog with the url in it
			CCustomServerInfoURLQuery *qb = new CCustomServerInfoURLQuery( "#ServerBrowser_CustomServerURLWarning", "#ServerBrowser_CustomServerURLOpen", this );
			if (qb != NULL)
			{
				qb->SetOKCommand( new KeyValues("DoOpenCustomServerInfoURL") );
				qb->AddActionSignalTarget(this);
				qb->MoveToFront();
				qb->DoModal();
			}
		}
	} 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TagInfoLabel::DoOpenCustomServerInfoURL( void )
{
	if ( GetURL() )
	{
		system()->ShellExecute("open", GetURL() );
	}
}


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

	m_szTagFilter[0] = 0;

	m_pTagFilter = new TextEntry(this, "TagFilter");
	m_pTagFilter->SetEnabled(false);
	m_pTagFilter->SetMaximumCharCount(MAX_TAG_CHARACTERS);

	m_pAddTagList = new TagMenuButton(this, "AddTagList", "#ServerBrowser_AddCommonTags");
	m_pTagListMenu = new Menu(m_pAddTagList, "TagList");
	m_pAddTagList->SetMenu(m_pTagListMenu);
	m_pAddTagList->SetOpenDirection(Menu::UP);
	m_pAddTagList->SetEnabled(false);

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
void CInternetGames::UpdateDerivedLayouts( void )
{
	const char *pPathID = "PLATFORM";

	KeyValues *pConditions = NULL;
	if ( ServerBrowser().IsWorkshopEnabled() )
	{
		pConditions = new KeyValues( "conditions" );
		if ( pConditions )
		{
			KeyValues *pNewKey = new KeyValues( "if_workshop_enabled" );
			if ( pNewKey )
			{
				pConditions->AddSubKey( pNewKey );
			}
		}
	}

	if ( m_pFilter->IsSelected() )
	{
		if ( g_pFullFileSystem->FileExists( "servers/CustomGamesPage_Filters.res", "MOD" ) )
		{
			pPathID = "MOD";
		}

		LoadControlSettings( "servers/CustomGamesPage_Filters.res", pPathID, NULL, pConditions );
	}
	else
	{
		if ( g_pFullFileSystem->FileExists( "servers/CustomGamesPage.res", "MOD" ) )
		{
			pPathID = "MOD";
		}

		LoadControlSettings( "servers/CustomGamesPage.res", pPathID, NULL, pConditions );
	}

	if ( pConditions )
	{
		pConditions->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::OnLoadFilter(KeyValues *filter)
{
	BaseClass::OnLoadFilter( filter );

	Q_strncpy(m_szTagFilter, filter->GetString("gametype"), sizeof(m_szTagFilter));

	if ( m_pTagFilter )
	{
		m_pTagFilter->SetText(m_szTagFilter);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInternetGames::CheckTagFilter( serveritem_t &server )
{
	bool bRetVal = true;

	// Custom games substring matches tags with the server's tags
	int count = Q_strlen( m_szTagFilter );
	if ( count )
	{
		CUtlVector<char*> TagList;
		V_SplitString( m_szTagFilter, ",", TagList );
		for ( int i = 0; i < TagList.Count(); i++ )
		{
			if ( ( Q_strnistr( server.m_szGameTags, TagList[i], MAX_TAG_CHARACTERS ) != 0 ) == TagsExclude() )
			{
				bRetVal = false;
				break;
			}
		}

		TagList.PurgeAndDeleteElements();
	}

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: gets filter settings from controls
//-----------------------------------------------------------------------------
void CInternetGames::OnSaveFilter(KeyValues *filter)
{
	BaseClass::OnSaveFilter( filter );

	if ( m_pTagFilter )
	{
		// tags
		m_pTagFilter->GetText(m_szTagFilter, sizeof(m_szTagFilter) - 1);
	}

	if ( m_szTagFilter[0] )
	{
		Q_strlower(m_szTagFilter);
	}

	if ( TagsExclude() )
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "gametype", "" ) );
	}
	else
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "gametype", m_szTagFilter ) );
	}

	filter->SetString("gametype", m_szTagFilter);
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

	// If we've found a server with some tags, enable the add tag button
	if (server.m_szGameTags[0])
	{
		m_pAddTagList->SetEnabled(true);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::ServerFailedToRespond( serveritem_t& server )
{
	m_bDirty = true;
	// TODO: implement this somehow
}

struct tagentry_t
{
	const char *pszTag;
	int iCount;
};
int __cdecl SortTagsInUse( const tagentry_t *pTag1, const tagentry_t *pTag2 )
{
	return (pTag1->iCount < pTag2->iCount);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::RecalculateCommonTags( void )
{
	// Regenerate our tag list
	m_pTagListMenu->DeleteAllItems();

	// Loop through our servers, and build a list of all the tags
	CUtlVector<tagentry_t> aTagsInUse;

	int iCount = m_pGameList->GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		int serverID = m_pGameList->GetItemUserData( i );
		serveritem_t *pServer = GetServer( serverID ); 
		if ( pServer && pServer->m_szGameTags && pServer->m_szGameTags[0] )
		{
			CUtlVector<char*> TagList;
			V_SplitString( pServer->m_szGameTags, ",", TagList );

			for ( int iTag = 0; iTag < TagList.Count(); iTag++ )
			{
				// First make sure it's not already in our list
				bool bFound = false;
				for ( int iCheck = 0; iCheck < aTagsInUse.Count(); iCheck++ )
				{
					if ( !Q_strnicmp(TagList[iTag], aTagsInUse[iCheck].pszTag, MAX_TAG_CHARACTERS ) )
					{
						aTagsInUse[iCheck].iCount++;
						bFound = true;
					}
				}

				if ( !bFound )
				{
					int iIdx = aTagsInUse.AddToTail();
					aTagsInUse[iIdx].pszTag = TagList[iTag];
					aTagsInUse[iIdx].iCount = 0;
				}
			}
		}
	}

	aTagsInUse.Sort( SortTagsInUse );

	int iTagsToAdd = min( aTagsInUse.Count(), NUM_COMMON_TAGS );
	for ( int i = 0; i < iTagsToAdd; i++ )
	{
		const char *pszTag = aTagsInUse[i].pszTag;
		m_pTagListMenu->AddMenuItem( pszTag, new KeyValues("AddTag", "tag", pszTag), this, new KeyValues( "data", "tag", pszTag ) );
	}

	m_pTagListMenu->SetFixedWidth( m_pAddTagList->GetWide() );
	m_pTagListMenu->InvalidateLayout( true, false );
	m_pTagListMenu->PositionRelativeToPanel( m_pAddTagList, Menu::UP );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::OnTagMenuButtonOpened( void )
{
	RecalculateCommonTags();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text from the message
//-----------------------------------------------------------------------------
void CInternetGames::OnAddTag(KeyValues *params)
{
	KeyValues *pkvText = params->FindKey("tag", false);
	if (!pkvText)
		return;

	AddTagToFilterList( pkvText->GetString() );
}


int SortServerTags( char* const *p1, char* const *p2 )
{
	return ( Q_strcmp( *p1, *p2 ) > 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInternetGames::AddTagToFilterList( const char *pszTag )
{
	char txt[ 128 ];
	m_pTagFilter->GetText( txt, sizeof( txt ) );

	CUtlVector<char*> TagList;
	V_SplitString( txt, ",", TagList );

	if ( txt[0] )
	{
		for ( int i = 0; i < TagList.Count(); i++ )
		{
			// Already in the tag list?
			if ( !Q_stricmp( TagList[i], pszTag ) )
			{
				TagList.PurgeAndDeleteElements();
				return;
			}
		}
	}

	char *pszNewTag = new char[64];
	Q_strncpy( pszNewTag, pszTag, 64 );
	TagList.AddToHead( pszNewTag );

	TagList.Sort( SortServerTags );

	// Append it
	char tmptags[MAX_TAG_CHARACTERS];
	tmptags[0] = '\0';

	for ( int i = 0; i < TagList.Count(); i++ )
	{
		if ( i > 0 )
		{
			Q_strncat( tmptags, ",", MAX_TAG_CHARACTERS );
		}

		Q_strncat( tmptags, TagList[i], MAX_TAG_CHARACTERS );
	}

	m_pTagFilter->SetText( tmptags );
	TagList.PurgeAndDeleteElements();

	// Update & apply filters now that the tag list has changed
	UpdateFilterSettings();
	ApplyGameFilters();
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
void CInternetGames::SetRefreshing(bool state)
{
	if ( state )
	{
		m_pAddTagList->SetEnabled( false );
	}

	BaseClass::SetRefreshing( state );
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

