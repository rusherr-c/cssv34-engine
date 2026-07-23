//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// normally pragma warning is disabled in vgui.h
#pragma warning( disable: 4800 )	// disables 'performance warning converting int to bool'

#include <stdlib.h>		// atoi
#include "ServerDetailsMsgHandler.h"

#include "ServersInfo.h"
#include "ServerList.h"

#define SPLITPACKET_HEADER 0xFFFFFFFE
#define S2A_EDF_GAMEPORT 0x80
#define S2A_EDF_STEAMID 0x10
#define S2A_EDF_SOURCETV 0x40
#define S2A_EDF_GAMETAGS 0x20
#define S2A_EDF_GAMEID 0x01

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerDetailsMsgHandler::CServerDetailsMsgHandler(CServerList* list)
	: CMsgHandler()
{
	m_pServerList = list;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerDetailsMsgHandler::~CServerDetailsMsgHandler()
{
}

//-----------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CServerDetailsMsgHandler::Process(const netadr_t& from, bf_read& msg) {
	double recvTime = Plat_FloatTime();

	serveritem_t server{};

	// check connectionless header
	if (msg.ReadLong() != CONNECTIONLESS_HEADER)
		return false;

	char c = msg.ReadByte();

	// check if it's the info
	if (c != S2A_INFOREPLY)
		return false;

	if (from.GetIPHostByteOrder() == 0 || from.GetPort() == 0)
		return false;

	// set server address
	server.m_NetAdr = from;

	server.m_nProtocolVersion = msg.ReadByte();

	msg.ReadString(server.m_szServerName, sizeof(server.m_szServerName));
	msg.ReadString(server.m_szMap, sizeof(server.m_szMap));
	msg.ReadString(server.m_szGameDir, sizeof(server.m_szGameDir));

	msg.ReadString(server.m_szGameDescription, sizeof(server.m_szGameDescription));
	server.m_nAppID = msg.ReadShort();

	// player info
	server.m_nPlayers = msg.ReadByte();
	server.m_nMaxPlayers = msg.ReadByte();
	server.m_nBotPlayers = msg.ReadByte();

	// Password?
	msg.ReadByte(); // server type
	msg.ReadByte(); // env

	server.m_bPassword = msg.ReadByte();
	server.m_bSecure = msg.ReadByte();
	msg.ReadString(server.m_szGameVersion, sizeof(server.m_szGameVersion));

	server.m_iFlags = msg.ReadByte();

	if (server.m_iFlags & S2A_EDF_GAMEPORT)
	{
		server.m_NetAdr.SetPort(msg.ReadShort());
	}

	if (server.m_iFlags & S2A_EDF_STEAMID)
	{
		uint64 ulSteamID;
		msg.ReadBytes(&ulSteamID, 8);
	}

	if (server.m_iFlags & S2A_EDF_SOURCETV)
	{
		char str[64];
		msg.ReadShort(); // spectator port (unused)
		msg.ReadString(str, sizeof(str)); // spectator sv name
	}

	if (server.m_iFlags & S2A_EDF_GAMETAGS)
	{
		char str[64];
		msg.ReadString(str, sizeof(str));
	}

	if (server.m_iFlags & S2A_EDF_GAMEID)
	{
		uint64 ulGameID;
		msg.ReadBytes(&ulGameID, 8);
	}

	DevMsg("CServerListMsgHandler: server processed\n%s\n", server.ToString());

	// Update it
	m_pServerList->UpdateServer(server.m_NetAdr, server, recvTime);

	return true;
}