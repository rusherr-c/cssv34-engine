#include "convar.h"
#include "interface.h"
#include "ServerBrowser\IServerBrowser.h"
#include "filesystem.h"
#include "sys_dll.h"
#include "color.h"

CSysModule* g_pServerBrowserDLL;
CreateInterfaceFn g_ServerBrowserFactory;
IServerBrowser* g_pServerBrowser;



CON_COMMAND_F(open_serverbrowser, "Opens a server browser window", FCVAR_NONE) {
	if (!g_pServerBrowserDLL) {
		g_pServerBrowserDLL = g_pFullFileSystem->LoadModule("serverbrowser.dll", "BASEBIN");
		ConColorMsg(Color(55, 55, 200, 255), "Loaded serverbrowser.dll\n");
		g_ServerBrowserFactory = Sys_GetFactory(g_pServerBrowserDLL);
		g_pServerBrowser = (IServerBrowser*)g_ServerBrowserFactory(SERVERBROWSER_INTERFACE_VERSION, nullptr);
		g_pServerBrowser->Activate();
		ConColorMsg(Color(55, 200, 55, 255), "Activated ServerBrowser\n");
		
	}
	else {
		g_pServerBrowser->Activate();
		ConColorMsg(Color(55, 200, 55, 255), "Activated ServerBrowser\n");
	}
}