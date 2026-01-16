//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ServerBrowser.h"
#include "ServerBrowserDialog.h"
#include "IRunGameEngine.h"
#include "Friends/IFriendsUser.h"
#include "DialogGameInfo.h"

#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

// expose the server browser interfaces
CServerBrowser g_ServerBrowserSingleton;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerBrowser, IServerBrowser, SERVERBROWSER_INTERFACE_VERSION, g_ServerBrowserSingleton);
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerBrowser, IVGuiModule, "VGuiModuleServerBrowser001", g_ServerBrowserSingleton);

IRunGameEngine *g_pRunGameEngine = NULL;
IFriendsUser *g_pFriendsUser = NULL;

CServerBrowser::CServerBrowser() {}
CServerBrowser::~CServerBrowser() {}

// IVGui module implementation
bool CServerBrowser::Initialize(CreateInterfaceFn* factorylist, int numFactories) { return true; }
bool CServerBrowser::PostInitialize(CreateInterfaceFn* modules, int factoryCount) { return true; }
vgui::VPANEL CServerBrowser::GetPanel() { return 0; }
bool CServerBrowser::Activate() { return true; }
bool CServerBrowser::IsValid() { return true; }
void CServerBrowser::Shutdown() {}
void CServerBrowser::Deactivate() {}
void CServerBrowser::Reactivate() {}
void CServerBrowser::SetParent(vgui::VPANEL parent) {}

// IServerBrowser implementation
// joins a specified game - game info dialog will only be opened if the server is fully or passworded
bool CServerBrowser::JoinGame(uint32 unGameIP, uint16 usGamePort) { return true; }
bool CServerBrowser::JoinGame(uint64 ulSteamIDFriend) { return true; }

// opens a game info dialog to watch the specified server; associated with the friend 'userName'
bool CServerBrowser::OpenGameInfoDialog(uint64 ulSteamIDFriend) { return true; }

// forces the game info dialog closed
void CServerBrowser::CloseGameInfoDialog(uint64 ulSteamIDFriend) {}

// closes all the game info dialogs
void CServerBrowser::CloseAllGameInfoDialogs() {}

// methods
void CServerBrowser::CreateDialog() {}
void CServerBrowser::Open(){}