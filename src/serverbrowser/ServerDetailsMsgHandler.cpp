//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// normally pragma warning is disabled in vgui.h
#pragma warning( disable: 4800 )	// disables 'performance warning converting int to bool'

#include "pch_serverbrowser.h"
#include "..\utils\bzip2\bzlib.h" // BZ2_bzBuffToBuffDecompress

#define SPLITPACKET_HEADER -2
#define SPLIT_FLAG_COMPRESSED 0x80000000
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
	m_pResponseTarget = 0;
	m_nServersResponded = 0;
}

CServerDetailsMsgHandler::CServerDetailsMsgHandler(IServerQueryResponse* response)
	: CMsgHandler()
{
	m_pServerList = 0;
	m_pResponseTarget = response;
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

	// check connectionless header
	if (msg.ReadLong() != CONNECTIONLESS_HEADER)
		return false;

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
		if (!ProcessPlayers(msg))
			Warning("Failed processing players for server %s\n", server.m_NetAdr.ToString());

		break;
	}
	case S2A_RULES_REPLY:
	{
		if (!ProcessRules(msg))
			Warning("Failed processing rules for server %s\n", server.m_NetAdr.ToString());

		break;
	}
	case S2C_CHALLENGE:
	{
		int challenge = ProcessChallenge(msg);

		if (challenge == -1)
			return false; // invalid response
		
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

// process multi packet response
bool CServerDetailsMsgHandler::ProcessLong(const netadr_t& from, bf_read& msg) {
	// Check if this packet is split
	if (msg.ReadLong() != SPLITPACKET_HEADER)
	{
		msg.Seek(0);
		return false;
	}

	int netID = msg.ReadLong();

	// is this split compressed?
	bool isCompressed = (netID & SPLIT_FLAG_COMPRESSED) != 0;

	if (isCompressed)
		netID &= ~SPLIT_FLAG_COMPRESSED;

	byte totalPackets = msg.ReadByte();
	byte packetNumber = msg.ReadByte();

	unsigned long uncompressedSize = 0;
	unsigned long crc32 = 0;

	// first packet, read size and crc32 sum
	if (packetNumber == 0)
	{

	}

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

	if (m_pResponseTarget)
		m_pResponseTarget->ServerResponded(server);

	return true;
}

// process rules
bool CServerDetailsMsgHandler::ProcessRules(bf_read& msg) {
	int numRules = msg.ReadShort();

	if (m_pResponseTarget) {
		if (!numRules) {
			m_pResponseTarget->RulesFailedToRespond();
			return false;
		}
	}

	char name[64];
	char value[64];

	for (int i = 0; i < numRules; i++)
	{
		if (msg.IsOverflowed())
			break; // we've reached end of the message

		memset(name, 0, sizeof(name)); memset(value, 0, sizeof(value));
		msg.ReadString(name, sizeof(name));
		msg.ReadString(value, sizeof(value));

		if (m_pResponseTarget)
			m_pResponseTarget->RulesResponded(name, value);
	}

	if (m_pResponseTarget)
		m_pResponseTarget->RulesRefreshComplete();
}

// something
bool CServerDetailsMsgHandler::ProcessPlayers(bf_read& msg) {
	int numPlayers = msg.ReadByte();

	if (m_pResponseTarget) {
		if (!numPlayers) {
			m_pResponseTarget->PlayersFailedToRespond();
			return false;
		}
	}

	char name[64];
	long score = 0;
	float duration = 0.0f;
	for (int i = 0; i < numPlayers; i++)
	{
		// id
		msg.ReadByte();

		memset(name, 0, sizeof(name));
		msg.ReadString(name, sizeof(name));
		
		score = msg.ReadLong();
		duration = msg.ReadFloat();

		m_pResponseTarget->AddPlayerToList(name, score, duration);
	}

	delete[] name;

	if (m_pResponseTarget)
	{
		m_pResponseTarget->PlayersRefreshComplete();
	}

	return true;
}

// returns processed challenge
int CServerDetailsMsgHandler::ProcessChallenge(bf_read& msg) {
	int chnr = -1;

	if (msg.m_pData[4] == 'A')
		chnr = msg.ReadLong();

	if (m_pResponseTarget)
		m_pResponseTarget->ChallengeReceived(chnr);

	return chnr;
}