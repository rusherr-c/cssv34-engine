//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef INTERNETGAMES_H
#define INTERNETGAMES_H
#ifdef _WIN32
#pragma once
#endif

#include "BaseGamesPage.h"

#define MAX_TAG_CHARACTERS			128

//-----------------------------------------------------------------------------
// Purpose: Tag info label
//-----------------------------------------------------------------------------
class TagInfoLabel : public vgui::URLLabel
{
	DECLARE_CLASS_SIMPLE(TagInfoLabel, vgui::URLLabel);
public:
	TagInfoLabel(Panel* parent, const char* panelName);
	TagInfoLabel(Panel* parent, const char* panelName, const char* text, const char* pszURL);

	virtual void	OnMousePressed(vgui::MouseCode code);

	MESSAGE_FUNC(DoOpenCustomServerInfoURL, "DoOpenCustomServerInfoURL");
};

//-----------------------------------------------------------------------------
// Purpose: Tag menu button
//-----------------------------------------------------------------------------
class TagMenuButton : public vgui::MenuButton
{
	DECLARE_CLASS_SIMPLE(TagMenuButton, vgui::MenuButton);
public:
	TagMenuButton(Panel* parent, const char* panelName, const char* text);

	virtual void OnShowMenu(vgui::Menu* menu);
};

//-----------------------------------------------------------------------------
// Purpose: Internet games list
//-----------------------------------------------------------------------------
class CInternetGames : public CBaseGamesPage
{

	DECLARE_CLASS_SIMPLE( CInternetGames, CBaseGamesPage );

public:
	CInternetGames( vgui::Panel *parent, const char *panelName = "InternetGames", EPageType eType = eInternetServer );
	~CInternetGames();

	// property page handlers
	virtual void OnPageShow();

	virtual void UpdateDerivedLayouts( void ) OVERRIDE;

	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem(IGameList::InterfaceItem_e item);

	// gets a new server list
	MESSAGE_FUNC( GetNewServerList, "GetNewServerList" );

	// serverlist refresh responses
	virtual void ServerResponded( serveritem_t& server );
	virtual void ServerFailedToRespond( serveritem_t& server );

	virtual void SetRefreshing(bool state);
	virtual void RefreshComplete( EMasterServerResponse response );

	MESSAGE_FUNC_INT( OnRefreshServer, "RefreshServer", serverID );

	virtual int		GetRegionCodeToFilter();
	virtual bool	CheckTagFilter( serveritem_t &server );

	void			RecalculateCommonTags( void );
	void			AddTagToFilterList( const char *pszTag );

protected:
	virtual void OnLoadFilter(KeyValues* filter) OVERRIDE;
	virtual void OnSaveFilter(KeyValues* filter) OVERRIDE;

	MESSAGE_FUNC_PARAMS(OnAddTag, "AddTag", params);
	MESSAGE_FUNC(OnTagMenuButtonOpened, "TagMenuButtonOpened");

	// vgui overrides
	virtual void PerformLayout();
	virtual void OnTick();

	virtual const char *GetStringNoUnfilteredServers() { return "#ServerBrowser_NoInternetGames"; }
	virtual const char *GetStringNoUnfilteredServersOnMaster() { return "#ServerBrowser_MasterServerHasNoServersListed"; }
	virtual const char *GetStringNoServersResponded() { return "#ServerBrowser_NoInternetGamesResponded"; }

private:
	// Called once per frame to see if sorting needs to occur again
	void CheckRedoSort();
	// Called once per frame to check re-send request to master server
	void CheckRetryRequest( ESteamServerType serverType );
	// opens context menu (user right clicked on a server)
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );

	struct regions_s
	{
		CUtlSymbol name;
		unsigned char code;
	};

	CUtlVector<struct regions_s> m_Regions;	// list of the different regions you can query for

	float				m_fLastSort;	// Time of last re-sort
	bool				m_bDirty;	// Has the list been modified, thereby needing re-sort
	bool				m_bRequireUpdate;	// checks whether we need an update upon opening
	
	// error cases for if no servers are listed
	bool				m_bAnyServersRetrievedFromMaster;
	bool				m_bAnyServersRespondedToQuery;
	bool				m_bNoServersListedOnMaster;

	bool m_bOfflineMode;

private:
	TagInfoLabel	*m_pTagInfoURL;
	TagMenuButton	*m_pAddTagList;
	vgui::Menu		*m_pTagListMenu;
	vgui::TextEntry	*m_pTagFilter;
	char			m_szTagFilter[MAX_TAG_CHARACTERS];
};

#endif // INTERNETGAMES_H
