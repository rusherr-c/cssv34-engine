/*
 *
 * Copyright (c) 2026 RuSHeRR
 *
 * Purpose: processes network packets from
 *	the Source Engine server
 *
 * VDC: https://developer.valvesoftware.com/wiki/Server_queries
 *
*/

// normally pragma warning is disabled in vgui.h
#pragma warning( disable: 4800 )	// disables 'performance warning converting int to bool'

#include "pch_serverbrowser.h"

#define S2A_EDF_GAMEPORT 0x80
#define S2A_EDF_STEAMID 0x10
#define S2A_EDF_SOURCETV 0x40
#define S2A_EDF_GAMETAGS 0x20
#define S2A_EDF_GAMEID 0x01

//-----------------------------------------------------------------------------
// Purpose: Constructors
//-----------------------------------------------------------------------------
CServerDetailsMsgHandler::CServerDetailsMsgHandler(CServerList* list)
	: CMsgHandler()
{
	m_pServerList = list;
	m_nServersResponded = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerDetailsMsgHandler::~CServerDetailsMsgHandler()
{
	m_nServersResponded = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CServerDetailsMsgHandler::Process(const netadr_t& from, bf_read& msg) {
	if (from.GetIPHostByteOrder() == 0 || 
		from.GetPort() == 0 || 
		!from.IsValid()
		)
		return false;

	double recvTime = Plat_FloatTime();

	// Reset servers count
	if (g_pServersInfo->GetStartRequestTime() < recvTime - LIST_REFRESH_TIMEOUT)
		m_nServersResponded = 0;

	serveritem_t server{};

	char c = msg.ReadByte();

	// set server address
	server.m_NetAdr = from;

	switch (c)
	{

	case S2A_INFO_REPLY:
	{
		if (!ProcessInfo(msg, server))
			Warning("Failed processing info for server %s\n", server.m_NetAdr.ToString());

		break;
	}
	case S2A_PLAYER_REPLY:
	{
		break;
	}
	case S2A_RULES_REPLY:
	{
		break;
	}
	case S2C_CHALLENGE:
	{
		break;
	}
	default:
		break;
	}

	m_nServersResponded++;

	// Update it
	if (m_pServerList)
		m_pServerList->UpdateServer(server.m_NetAdr, server, recvTime);

	if (m_nServersResponded & 1)
		g_pServersInfo->UpdateStartRequestTime();

	return true;
}

// process info
bool CServerDetailsMsgHandler::ProcessInfo(bf_read& msg, serveritem_t& server) {
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

	// EDF - Extra data flags, only Source2007+
	// ********

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
		msg.ReadString(server.m_szGameTags, sizeof(server.m_szGameTags));
	}

	if (server.m_iFlags & S2A_EDF_GAMEID)
	{
		uint64 ulGameID;
		msg.ReadBytes(&ulGameID, 8);
	}

	return true;
}
