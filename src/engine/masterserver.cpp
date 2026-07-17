//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================
#include <WinSock2.h>
#include <random>
#include "quakedef.h"
#include "server.h"
#include "master.h"
#include "proto_oob.h"
#include "host.h"
#include "eiface.h"
#include "server.h"
#include "cdll_int.h"
#include "utlmap.h"
#include "utlbuffer.h"
#include "sys_dll.h"
#include "info_key.h"

extern ConVar sv_tags;
extern ConVar sv_lan;
extern ConVar sv_region;

#define MASTER_SERVER_PROTOCOL_VERSION 7

#define S2A_EDF_GAMEPORT 0x80
#define S2A_EDF_STEAMID 0x10
#define S2A_EDF_SOURCETV 0x40
#define S2A_EDF_GAMETAGS 0x20
#define S2A_EDF_GAMEID 0x01

#define RETRY_INFO_REQUEST_TIME 0.4 // seconds
#define MASTER_RESPONSE_TIMEOUT 1.5 // seconds
#define INFO_REQUEST_TIMEOUT 5.0 // seconds

const int g_iMasterServersVDF_Maximum = 6;

// IPv4 only
static char g_MasterServers[][32] =
{
	"80.78.244.170:27011",
	//"78.154.103.37:10232",
};

#ifdef DEDICATED
#define IsLan() false
#else
#define IsLan() sv_lan.GetInt()
#endif

//-----------------------------------------------------------------------------
// Purpose: List of master servers and some state info about them
//-----------------------------------------------------------------------------
typedef struct adrlist_s
{
	// Next master in chain
	struct adrlist_s	*next;
	// Challenge request sent to master
	qboolean			heartbeatwaiting;
	// Challenge request send time
	float				heartbeatwaitingtime; 
	// Last one is Main master
	int					heartbeatchallenge;
	// Time we sent last heartbeat
	double				last_heartbeat;
	// Master server address
	netadr_t			adr;
} adrlist_t;

//-----------------------------------------------------------------------------
// Purpose: Implements the master server interface
//-----------------------------------------------------------------------------
class CMaster : public IMaster, public IServersInfo
{
	friend class IServerList;
	// This is very dirty
	// TODO: Move serversinfo stuff to ServerBrowser
public:
	CMaster( void );
	virtual ~CMaster( void );

	// Heartbeat functions.
	void Init( void );
	void Shutdown( void );
	// Sets up master address

	void InitConnection(void);
	void ShutdownConnection(void);
	void SendHeartbeat( adrlist_t *p );
	void AddServer( netadr_t *adr );
	void UseDefault ( void );
	void CheckHeartbeat (void);
	void RespondToHeartbeatChallenge( netadr_t &from, bf_read &msg );
	void PingServer(netadr_t& svadr) {}

	void ProcessConnectionlessPacket(netpacket_t*packet );
	void ProcessConnectionless_GameServer(netpacket_t* packet);
	void ProcessConnectionless_GameClient(netpacket_t* packet);

	void AddMaster_f( const CCommand &args );
	void Heartbeat_f( void );

	void RunFrame();
	void RetryServersInfoRequest();

	void ReplyInfo( const netadr_t &adr );
	newgameserver_t &ProcessInfo( bf_read &buf );

	void ReplyChallenge(const netadr_t& adr);

	void ReplyPlayers(const netadr_t& adr);
	//a2s_player_t &ProcessPlayers(bf_read& buf);

	// ServersInfo
	void RequestInternetServerList(const char* gamedir, IServerListResponse* response);
	void RequestLANServerList(const char* gamedir, IServerListResponse* response);
	void RequestFavoritesServerList(const char* gamedir, IServerListResponse* response);
	void RequestHistoryServerList(const char* gamedir, IServerListResponse* response);

	void AddFavoriteServer(uint32 unIP, uint16 usPort);
	void AddHistoryServer(uint32 unIP, uint16 usPort, time_t timeLastPlayed);

	void RemoveFavoriteServer(uint32 unIP, uint16 usPort);
	void RemoveHistoryServer(uint32 unIP, uint16 usPort);

	void AddServerAddresses( netadr_t **adr, int count );
	void RequestServerInfo( const netadr_t &adr );
	void StopRefresh();

	void PingServer(uint32 unIP, uint16 usPort, IServerPingResponse* response);
	void PlayerDetails(uint32 unIP, uint16 usPort, IServerPlayersResponse* response);
	bool CancelServerQuery(EServerQuery type, uint32 unIP, uint16 usPort);

	static DWORD WINAPI MasterServersVDFLoading_Thread(LPVOID thisptr);

private:
	// List of known master servers
	adrlist_t *m_pMasterAddresses;

	bool m_bInitialized;
	bool m_bRefreshing;

	int m_iServersResponded;
	long m_iC2S_ServerChallengeNum;
	long m_iS2C_ServerChallengeNum;

	double m_flStartRequestTime;
	double m_flRetryRequestTime;
	double m_flMasterRequestTime;

	uint m_iInfoSequence;
	char m_szGameDir[256];

	// If nomaster is true, the server will not send heartbeats to the master server
	bool	m_bNoMasters;

	CUtlMap<netadr_t, bool> m_serverAddresses;
	CUtlMap<netadr_t, double> m_serversRequestTime;

	netadr_t m_lastServerAdr;

	IServerListResponse *m_serverListResponse;
};

static CMaster s_MasterServer;
IMaster *master = (IMaster *)&s_MasterServer;

IServersInfo *g_pServersInfo = (IServersInfo*)&s_MasterServer;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMaster, IServersInfo, SERVERLIST_INTERFACE_VERSION, s_MasterServer );

#define	HEARTBEAT_SECONDS	140.0
#define MASTER_PARSE_FILE "masterservers.vdf"

#define MAX_SINFO 2048

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMaster::CMaster( void )
{
	m_pMasterAddresses	= NULL;
	m_bNoMasters		= false;
	m_bInitialized = false;
	m_iServersResponded = 0;
	m_iC2S_ServerChallengeNum = -1;
	m_iS2C_ServerChallengeNum = -1;

	m_serverListResponse = NULL;
	SetDefLessFunc( m_serverAddresses );
	SetDefLessFunc( m_serversRequestTime );

	m_bRefreshing = false;
	m_iInfoSequence = 1;

	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMaster::~CMaster( void )
{
	m_bRefreshing = 0;

	Shutdown();
	ShutdownConnection();
}

// Purpose: Runs every frame
void CMaster::RunFrame()
{
	CheckHeartbeat();

	if( !m_bRefreshing )
		return;

	if( m_serverListResponse &&
		m_flStartRequestTime < Plat_FloatTime() - INFO_REQUEST_TIMEOUT )
	{
		StopRefresh();
		m_serverListResponse->RefreshComplete( NServerResponse::nServerFailedToRespond );
		return;
	}

	if( m_iServersResponded > 0 &&
			m_iServersResponded >= m_serverAddresses.Count() &&
			m_flMasterRequestTime < Plat_FloatTime() - MASTER_RESPONSE_TIMEOUT )
	{
		StopRefresh();
		m_serverListResponse->RefreshComplete( NServerResponse::nServerResponded );
		return;
	}

	if( m_flRetryRequestTime < Plat_FloatTime() - RETRY_INFO_REQUEST_TIME )
	{
		m_flRetryRequestTime = Plat_FloatTime();

		if( m_serverAddresses.Count() == 0 ) // Retry masterserver request
		{
			g_pServersInfo->RequestInternetServerList(m_szGameDir, NULL);
			return;
		}

		if( m_iServersResponded < m_serverAddresses.Count() )
			RetryServersInfoRequest();
	}
}

// Purpose: stop refreshing server list
void CMaster::StopRefresh()
{
	if( !m_bRefreshing )
		return;

	m_iServersResponded = 0;
	m_bRefreshing = false;
	m_serverAddresses.RemoveAll();
	m_serversRequestTime.RemoveAll();

	// stop all refresh operations
	favoriteservers->StopRefresh();
	lanservers->StopRefresh();
	historyservers->StopRefresh();
	monitoringservers->StopRefresh();
}

//-----------------------------------------------------------------------------
// Purpose: Server replies with S2A_INFO_REPLY
//-----------------------------------------------------------------------------
void CMaster::ReplyInfo( const netadr_t &adr )
{
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	CUtlBuffer buf;
	buf.EnsureCapacity( 2048 );

	buf.PutUnsignedInt( CONNECTIONLESS_HEADER );
	buf.PutUnsignedChar( S2A_INFOREPLY );

	//buf.PutUnsignedInt(sequence);
	buf.PutUnsignedChar( PROTOCOL_VERSION ); // Hardcoded protocol version number
	buf.PutString( sv.GetName() );
	buf.PutString( sv.GetMapName() );
	buf.PutString( gamedir );
	buf.PutString( serverGameDLL->GetGameDescription() );
	buf.PutShort( GetSteamInfIDVersionInfo().AppID );

	// player info
	buf.PutUnsignedChar( sv.GetNumClients() );
	buf.PutUnsignedChar( sv.GetMaxClients() );
	buf.PutUnsignedChar( sv.GetNumFakeClients() );

	// server type
	buf.PutUnsignedChar(sv.IsDedicated() ? 'd' : 'l');

	// environment
	if (IsWindows())
		buf.PutUnsignedChar('w');
	else if (IsLinux())
		buf.PutUnsignedChar('l');
	else
		buf.PutUnsignedChar('m');

	// Password?
	buf.PutUnsignedChar( sv.GetPassword() != NULL ? 1 : 0 );

	// VAC
	buf.PutUnsignedChar(false);

	// Game version
	buf.PutString(GetSteamInfIDVersionInfo().szVersionString);

	// Write a byte with some flags that describe what is to follow.
	const char *pchTags = sv_tags.GetString();
	int nFlags = 0;

	if ( pchTags && pchTags[0] != '\0' )
		nFlags |= S2A_EDF_GAMETAGS;

	buf.PutUnsignedInt( nFlags );

	if ( nFlags & S2A_EDF_GAMETAGS )
		buf.PutString( pchTags );

	MasterNetHandler()->NET_SendPacket( NS_SERVER, adr, (unsigned char *)buf.Base(), buf.TellPut() );
}

//-----------------------------------------------------------------------------
// Purpose: Process info about server
//-----------------------------------------------------------------------------
newgameserver_t &CMaster::ProcessInfo(bf_read &buf)
{
	static newgameserver_t s;
	memset( &s, 0, sizeof(s) );

	s.m_nProtocolVersion = buf.ReadByte();

	buf.ReadString( s.m_szServerName, sizeof(s.m_szServerName) );
	buf.ReadString( s.m_szMap, sizeof(s.m_szMap) );
	buf.ReadString( s.m_szGameDir, sizeof(s.m_szGameDir) );

	buf.ReadString( s.m_szGameDescription, sizeof(s.m_szGameDescription) );
	s.m_nAppID = buf.ReadShort();

	// player info
	s.m_nPlayers = buf.ReadByte();
	s.m_nMaxPlayers = buf.ReadByte();
	s.m_nBotPlayers = buf.ReadByte();

	// Password?
	buf.ReadByte(); // server type
	buf.ReadByte(); // env

	s.m_bPassword = buf.ReadByte();
	s.m_bSecure = buf.ReadByte();
	buf.ReadString(s.m_szGameVersion, sizeof(s.m_szGameVersion));

	s.m_iFlags = buf.ReadByte();

	if (s.m_iFlags & S2A_EDF_GAMEPORT)
	{
		s.m_NetAdr.SetPort(buf.ReadShort());
	}

	if (s.m_iFlags & S2A_EDF_STEAMID)
	{
		uint64 ulSteamID;
		buf.ReadBytes(&ulSteamID, 8);
	}

	if (s.m_iFlags & S2A_EDF_SOURCETV)
	{
		char str[64];
		buf.ReadShort(); // spectator port (unused)
		buf.ReadString(str, sizeof(str)); // spectator sv name
	}

	if( s.m_iFlags & S2A_EDF_GAMETAGS )
	{
		buf.ReadString( s.m_szGameTags, sizeof(s.m_szGameTags) );
	}

	if (s.m_iFlags & S2A_EDF_GAMEID)
	{
		uint64 ulGameID;
		buf.ReadBytes(&ulGameID, 8);
	}
	Msg("CMaster: server processed\n%s\n", s.toString());

	return s;
}

//-----------------------------------------------------------------------------
// Purpose: Server replies with S2C_CHALLENGE
//-----------------------------------------------------------------------------
void CMaster::ReplyChallenge(const netadr_t& adr) {
	char buf[256];
	bf_write msg(buf, sizeof(buf));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(S2C_CHALLENGE);
	msg.WriteLong(m_iS2C_ServerChallengeNum);

	MasterNetHandler()->NET_SendPacket(NS_SERVER, adr, msg.GetData(), msg.GetNumBytesWritten());
}

//-----------------------------------------------------------------------------
// Purpose: Server replies with S2A_PLAYER_REPLY
//-----------------------------------------------------------------------------
void CMaster::ReplyPlayers(const netadr_t& adr) {
	char buf[256];
	bf_write msg(buf, sizeof(buf));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(S2A_PLAYER_REPLY);
	msg.WriteByte(sv.GetNumClients());
	// chunks starting
	for (int i = 0; i < sv.GetNumClients(); i++) {

		IClient *client = sv.GetClient(i);
		INetChannel *nci = client->GetNetChannel();

		msg.WriteByte(i);
		msg.WriteString(client->GetClientName());
		msg.WriteLong(i); // we dont know player's score
		msg.WriteFloat(nci->GetTimeConnected());
	}

	MasterNetHandler()->NET_SendPacket(NS_SERVER, adr, msg.GetData(), msg.GetNumBytesWritten());
}

//-----------------------------------------------------------------------------
// Purpose: Process connectionless packet
//-----------------------------------------------------------------------------
void CMaster::ProcessConnectionlessPacket(netpacket_t*packet )
{
	static ALIGN4 char string[2048] ALIGN4_POST;    // Buffer for sending heartbeat

	uint32 ip; uint16 port;

	bf_read msg = packet->message;
	byte c = msg.ReadByte();

	if ( c == 0  )
		return;

	ProcessConnectionless_GameServer(packet);
	ProcessConnectionless_GameClient(packet);

	switch( c )
	{
		case M2A_CHALLENGE:
		{
			RespondToHeartbeatChallenge( packet->from, msg );
			break;
		}
		// master reply starts with FF FF FF FF 66 0A
		case M2C_QUERY:
		{
			if (msg.ReadByte() != 0x0A)
				break;

			if (!m_bRefreshing)
				break;

			ip = htonl(msg.ReadLong());
			port = htons(msg.ReadShort());

			while (ip != 0 && port != 0)
			{
				netadr_t adr(ip, port);

				unsigned short index = m_serverAddresses.Find(adr);
				if (index != m_serverAddresses.InvalidIndex())
				{
					ip = msg.ReadLong();
					port = msg.ReadShort();
					continue;
				}

				m_serverAddresses.Insert(adr, false);
				RequestServerInfo(adr);

				ip = msg.ReadLong();
				port = msg.ReadShort();
			}

			if (m_lastServerAdr.GetIPHostByteOrder() == 0 || m_lastServerAdr.GetPort() == 0)
				m_lastServerAdr.SetIPAndPort(ip, port);

			RequestInternetServerList(m_szGameDir, m_serverListResponse);

			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Process connectionless packet (server)
//-----------------------------------------------------------------------------
void CMaster::ProcessConnectionless_GameServer(netpacket_t* packet) {
	bf_read msg = packet->message;
	byte c = msg.ReadByte();

	switch (c)
	{
		case A2S_INFOREQUEST:
		{
			char string[32];
			msg.ReadString(string, sizeof(string));

			if (strcmp(string, A2S_KEY_STRING) != 0) // invalid request
				break;

			ReplyInfo(packet->from);
			break;
		}

		case A2S_PLAYER_REQUEST:
		{
			srand((uint)_time64(0));
			if (m_iS2C_ServerChallengeNum == -1)
				m_iS2C_ServerChallengeNum = (rand() % (0xFFFFFF - 0xFF + 1)) + 0xFF;

			int received_challenge = msg.ReadLong();
			if (received_challenge != m_iS2C_ServerChallengeNum || received_challenge == -1) {
				ReplyChallenge(packet->from);
				break;
			}

			ReplyPlayers(packet->from);

			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Process connectionless packet (client)
//-----------------------------------------------------------------------------
void CMaster::ProcessConnectionless_GameClient(netpacket_t* packet) {
	bf_read msg = packet->message;
	byte c = msg.ReadByte();

	switch (c)
	{
		case S2A_INFOREPLY:
		{
			if (!m_bRefreshing)
				break;

			newgameserver_t& s = ProcessInfo(msg);

			unsigned short index = m_serverAddresses.Find(packet->from);
			unsigned short rindex = m_serversRequestTime.Find(packet->from);

			if (index == m_serverAddresses.InvalidIndex())
				break;

			if (rindex == m_serversRequestTime.InvalidIndex())
				break;

			double requestTime = m_serversRequestTime[rindex];

			if (m_serverAddresses[index]) // shit happens
				return;

			m_serverAddresses[index] = true;
			s.m_nPing = (Plat_FloatTime() - requestTime) * 1000.0; // calculate ping here

			s.m_NetAdr = packet->from;

			m_serverListResponse->ServerResponded(s);
			m_iServersResponded++;

			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Request info from single server
//-----------------------------------------------------------------------------
void CMaster::RequestServerInfo( const netadr_t &adr )
{
	char string[256];
	bf_write msg( string, sizeof(string) );

	msg.WriteLong( CONNECTIONLESS_HEADER ); // 0xFFFFFFFF
	msg.WriteByte( A2S_INFOREQUEST ); // 'T'
	//msg.WriteLong( m_iInfoSequence );
	msg.WriteString(A2S_KEY_STRING); // "Source Engine Query"
	//m_serversRequestTime.Insert(m_iInfoSequence, Plat_FloatTime());
	msg.WriteByte(0);

	m_iInfoSequence++;
	if (m_serversRequestTime.Find(adr) == m_serversRequestTime.InvalidIndex())
	{
		m_serversRequestTime.Insert(adr, Plat_FloatTime());
		MasterNetHandler()->NET_SendPacket(NS_CLIENT, adr, msg.GetData(), msg.GetNumBytesWritten());
	}
	//MasterNetHandler()->NET_SendPacket(NS_CLIENT, adr, msg.GetData(), msg.GetNumBytesWritten() );
}

//-----------------------------------------------------------------------------
// Purpose: Retry ServersInfo request
//-----------------------------------------------------------------------------
void CMaster::RetryServersInfoRequest()
{
	FOR_EACH_MAP_FAST( m_serverAddresses, i )
	{
		bool bResponded = m_serverAddresses.Element(i);
		if( bResponded )
			continue;

		const netadr_t adr = m_serverAddresses.Key(i);
		RequestServerInfo( adr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sends a heartbeat to the master server
// Input  : *p - 
//-----------------------------------------------------------------------------
void CMaster::SendHeartbeat ( adrlist_t *p )
{
	static char	message[MAX_SINFO + 5];    // Buffer for sending heartbeat
	int			players;          // Number of active client connections
	char        szGD[MAX_OSPATH];
	char		info[MAX_SINFO];
	char		szOS[2];

	if ( !p )
		return;

	// Still waiting on challenge response?
	if ( p->heartbeatwaiting )
		return;

	// Waited too long
	if ( (realtime - p->heartbeatwaitingtime ) >= HB_TIMEOUT )
		return;

	// Send to master
	//Q_FileBase( com_gamedir, szGD, sizeof( szGD ) );

	//bf_write buf( string, sizeof(string) );
	//buf.WriteByte( S2M_HEARTBEAT );
	//buf.WriteLong( p->heartbeatchallenge );
	//buf.WriteShort( PROTOCOL_VERSION );
	//buf.WriteString( szGD );

	//NET_SendPacket( NULL, NS_SERVER, p->adr, buf.GetData(), buf.GetNumBytesWritten() );
	
	//
	// count active users
	//
	players = sv.GetNumClients();

	Q_memset(message, 0, sizeof(message));

	Q_FileBase(com_gamedir, szGD, sizeof(szGD));

	bool bHasPW = sv.GetPassword() != NULL;

#if _WIN32
	Q_strcpy(szOS, "w");
#else
	Q_strcpy(szOS, "l");
#endif

	info[0] = '\0';

	Info_SetValueForKey(info, "protocol", va("%i", MASTER_SERVER_PROTOCOL_VERSION), MAX_SINFO);
	Info_SetValueForKey(info, "challenge", va("%i", p->heartbeatchallenge), MAX_SINFO);
	Info_SetValueForKey(info, "players", va("%i", players), MAX_SINFO);
	Info_SetValueForKey(info, "max", va("%i", sv.GetMaxClients()), MAX_SINFO);
	Info_SetValueForKey(info, "bots", va("%i", sv.GetNumFakeClients()), MAX_SINFO);
	Info_SetValueForKey(info, "gamedir", szGD, MAX_SINFO);
	Info_SetValueForKey(info, "map", sv.GetMapName(), MAX_SINFO);
	Info_SetValueForKey(info, "password", bHasPW ? "1" : "0", MAX_SINFO);
	Info_SetValueForKey(info, "os", szOS, MAX_SINFO);
	Info_SetValueForKey(info, "lan", IsLan() ? "1" : "0", MAX_SINFO);
	Info_SetValueForKey(info, "region", sv_region.GetString(), MAX_SINFO);
	Info_SetValueForKey(info, "gametype", sv_tags.GetString(), MAX_SINFO);

	if (sv.IsHLTV())
	{
		Info_SetValueForKey(info, "type", "p", MAX_SINFO); // p = HLTV proxy
		Info_SetValueForKey(info, "secure", "0", MAX_SINFO); // never requires a secure client
		Info_SetValueForKey(info, "version", "1.0.0.0", MAX_SINFO);
		Info_SetValueForKey(info, "product", "srctv", MAX_SINFO);

		// Info_SetValueForKey( info, "proxy", hltv->IsMasterProxy()?"1":"2", MAX_SINFO );
	}
	else // game server
	{
		Info_SetValueForKey(info, "type", sv.IsDedicated() ? "d" : "l", MAX_SINFO);
		Info_SetValueForKey(info, "secure", "0", MAX_SINFO); // never requires a secure client
		Info_SetValueForKey(info, "version", GetSteamInfIDVersionInfo().szVersionString, MAX_SINFO);
		Info_SetValueForKey(info, "product", GetSteamInfIDVersionInfo().szProductString, MAX_SINFO);
	}


	// Send to master
	Q_snprintf(message, sizeof(message), "%c\n%s\n", S2M_HEARTBEAT2, info);

	MasterNetHandler()->NET_SendPacket(NS_SERVER, p->adr, (unsigned char*)message, Q_strlen(message));
}

//-----------------------------------------------------------------------------
// Purpose: Requests a challenge so we can then send a heartbeat
//-----------------------------------------------------------------------------
void CMaster::CheckHeartbeat (void)
{
	adrlist_t *p;

	unsigned char c;

	if ( m_bNoMasters ||      // We are ignoring heartbeats
		IsLan() ||           // Lan servers don't heartbeat
		(sv.GetMaxClients() <= 1) ||  // not a multiplayer server.
		!sv.IsActive() )			  // only heartbeat if a server is running.
		return;

	p = m_pMasterAddresses;
	while ( p )
	{
		// Time for another try?
		if ( ( realtime - p->last_heartbeat) < HEARTBEAT_SECONDS)  // not time to send yet
		{
			p = p->next;
			continue;
		}

		// Should we resend challenge request?
		if ( p->heartbeatwaiting &&
			( ( realtime - p->heartbeatwaitingtime ) < HB_TIMEOUT ) )
		{
			p = p->next;
			continue;
		}

		//int32 challenge = RandomInt( 0, INT_MAX );

		p->heartbeatwaiting     = true;
		p->heartbeatwaitingtime = realtime;

		p->last_heartbeat       = realtime;  // Flag at start so we don't just keep trying for hb's when
		//p->heartbeatchallenge = challenge;

		c = A2S_GETCHALLENGE;

		// Send to master asking for a challenge #
		MasterNetHandler()->NET_SendPacket( NS_SERVER, p->adr, &c, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Server is shutting down, unload master servers list, tell masters that we are closing the server
//-----------------------------------------------------------------------------
void CMaster::ShutdownConnection( void )
{
	adrlist_t *p;

	if ( !host_initialized )
		return;

	if ( m_bNoMasters ||      // We are ignoring heartbeats
		IsLan() ||           // Lan servers don't heartbeat
		(sv.GetMaxClients() <= 1) )   // not a multiplayer server.
		return;

	const char packet = S2M_SHUTDOWN;

	p = m_pMasterAddresses;
	while ( p )
	{
		MasterNetHandler()->NET_SendPacket(NS_SERVER, p->adr, (unsigned char*)&packet, 1);
		p->last_heartbeat = -99999.0;
		p = p->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add server to the master list
// Input  : *adr - 
//-----------------------------------------------------------------------------
void CMaster::AddServer( netadr_t *adr )
{
	adrlist_t *n;

	// See if it's there
	n = m_pMasterAddresses;
	while ( n )
	{
		if ( n->adr.CompareAdr(*adr) )
			break;
		n = n->next;
	}

	// Found it in the list.
	if ( n )
		return;

	n = ( adrlist_t * ) malloc ( sizeof( adrlist_t ) );
	if ( !n )
		Sys_Error( "Error allocating %zd bytes for master address.", sizeof( adrlist_t ) );

	memset( n, 0, sizeof( adrlist_t ) );

	n->adr = *adr;

	// Queue up a full heartbeat to all master servers.
	n->last_heartbeat = -99999.0;

	// Link it in.
	n->next = m_pMasterAddresses;
	m_pMasterAddresses = n;

	Msg("CMaster: Added master server %s\n", n->adr.ToString());
}

//-----------------------------------------------------------------------------
// Purpose: Add built-in default master if woncomm.lst doesn't parse
//-----------------------------------------------------------------------------
void CMaster::UseDefault ( void )
{
	netadr_t adr;

	Msg("Using default master addresses\n");

	for( int i = 0; i < ARRAYSIZE(g_MasterServers);i++ )
	{
		// Convert to netadr_t
		if (NET_StringToAdr(g_MasterServers[i], &adr))
		{
			// Add to master list
			AddServer(&adr);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaster::RespondToHeartbeatChallenge( netadr_t &from, bf_read &msg )
{
	adrlist_t* p;

	// No masters, just ignore.
	if ( !m_pMasterAddresses )
		return;

	p = m_pMasterAddresses;
	while ( p )
	{
		if ( from.CompareAdr(p->adr) )
			break;

		p = p->next;
	}

	// Not a known master server.
	if ( !p )
		return;

	uint32 challenge = msg.ReadLong();

	// Kill timer
	p->heartbeatwaiting   = false;
	p->heartbeatchallenge = challenge;

	// Send the actual heartbeat request to this master server.
	SendHeartbeat( p );
}

//-----------------------------------------------------------------------------
// Purpose: Add/remove master servers
//-----------------------------------------------------------------------------
void CMaster::AddMaster_f(const CCommand& args)
{
	CUtlString cmd((args.ArgC() > 1) ? args[1] : "");

	netadr_t adr;

	if (!NET_StringToAdr(cmd.String(), &adr))
	{
		Warning("Invalid address\n");
		return;
	}

	this->AddServer(&adr);
}


//-----------------------------------------------------------------------------
// Purpose: Send a new heartbeat to the master
//-----------------------------------------------------------------------------
void CMaster::Heartbeat_f (void)
{
	adrlist_t *p;

	p = m_pMasterAddresses;
	while ( p )
	{
		// Queue up a full hearbeat
		p->last_heartbeat = -9999.0;
		p->heartbeatwaitingtime = -9999.0;
		p = p->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AddMaster_f( const CCommand &args )
{
	master->AddMaster_f( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Heartbeat1_f( void )
{
	master->Heartbeat_f();
}

static ConCommand setmaster("addmaster", AddMaster_f );
static ConCommand heartbeat("heartbeat", Heartbeat1_f, "Force heartbeat of master servers" ); 

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
KeyValues* LoadMasterServersConfig()
{
	KeyValues* kv = new KeyValues("");

	if (!kv->LoadFromFile(g_pFullFileSystem, "masterservers.vdf", "CONFIG"))
	{
		kv->deleteThis();
		return nullptr;
	}

	return kv;
}

//-----------------------------------------------------------------------------
// Purpose: Creates default MasterServers config
//-----------------------------------------------------------------------------
void CreateDefaultMasterServersConfig()
{
	KeyValues* kv = new KeyValues("MasterServers");

	KeyValues* entry = kv->FindKey("0", true);
	entry->SetString("addr", "default");

	kv->SaveToFile(g_pFullFileSystem, "masterservers.vdf", "CONFIG");
	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: extracts master addresses from masterservers.vdf config
//-----------------------------------------------------------------------------
CUtlVector<netadr_t>* MasterServersConfig_GetAddresses(KeyValues* kv)
{
	CUtlVector<netadr_t>* addresses = new CUtlVector<netadr_t>();

	if (!kv)
		return addresses;

	for (int i = 0; i < g_iMasterServersVDF_Maximum; i++)
	{
		char keyName[16];
		Q_snprintf(keyName, sizeof(keyName), "%d", i);

		KeyValues* numberKey = kv->FindKey(keyName);
		if (!numberKey)
			continue;

		KeyValues* addrKey = numberKey->FindKey("addr");
		if (!addrKey)
			continue;

		const char* addrStr = addrKey->GetString();
		if (!addrStr || !addrStr[0])
			continue;

		netadr_t adr;

		if (!Q_stricmp(addrStr, "default"))
		{
			adr.SetType(NA_LOOPBACK);
		}
		else if (!Q_strstr(addrStr, ":"))
		{
			adr.SetType(NA_NULL);
		}
		else
		{
			adr.SetFromString(addrStr);
		}

		addresses->AddToTail(adr);
	}

	return addresses;
}

// FIXME: Dirty!
DWORD WINAPI CMaster::MasterServersVDFLoading_Thread(LPVOID param)
{
	CMaster* pThis = (CMaster*)param;

	while (!g_pFullFileSystem)
	{
		Sleep(100);
	}

	g_pFullFileSystem->AddSearchPath("platform\\config", "CONFIG");

	KeyValues* kv = LoadMasterServersConfig();
	if (!kv)
	{
		CreateDefaultMasterServersConfig();
		Warning("Couldn't find masterservers.vdf, using default master addresses\n");

		pThis->UseDefault();
		return 0;
	}

	CUtlVector<netadr_t>* addresses = MasterServersConfig_GetAddresses(kv);

	for (int i = 0; i < addresses->Count(); i++)
	{
		netadr_t& adr = (*addresses)[i];

		if (adr.GetType() == NA_NULL)
			continue;

		if (adr.GetType() == NA_LOOPBACK)
		{
			pThis->UseDefault();
			continue;
		}

		pThis->AddServer(&adr);
	}

	delete addresses;
	kv->deleteThis();

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void CMaster::Init(void)
{
	if (m_bInitialized)
		return;

	m_bInitialized = true;

	Msg("%f: CMaster Init\n", Plat_FloatTime());

	// FIXME: Dirty!
	HANDLE hThread = CreateThread(
		nullptr,
		0,
		MasterServersVDFLoading_Thread,
		this,
		0,
		nullptr
	);

	if (hThread)
		CloseHandle(hThread);
}

//-----------------------------------------------------------------------------
// Purpose: Shutting down
//-----------------------------------------------------------------------------
void CMaster::Shutdown(void)
{
	adrlist_t *p, *n;

	// Free the master list now.
	p = m_pMasterAddresses;
	while ( p )
	{
		n = p->next;
		free( p );
		p = n;
	}

	m_pMasterAddresses = NULL;
	Msg("%f: CMaster Shutdown\n", Plat_FloatTime());
}

// ServersInfo
void CMaster::RequestInternetServerList(const char *gamedir, IServerListResponse *response)
{
#if 1
	if (!m_lastServerAdr.IsValid())
		m_lastServerAdr.SetIPAndPort(0, 0);

	if( m_bNoMasters ) return;
	strncpy( m_szGameDir, gamedir, sizeof(m_szGameDir) );
	
	if( response )
	{
		StopRefresh();
		m_bRefreshing = true;
		m_serverListResponse = response;
		m_flRetryRequestTime = m_flStartRequestTime = m_flMasterRequestTime = Plat_FloatTime();
	}

	ALIGN4 char buf[256] ALIGN4_POST;
	bf_write msg(buf, sizeof(buf));

	msg.WriteByte( C2M_CLIENTQUERY ); 
	msg.WriteByte(0xFF); // region is always 255
	msg.WriteString(m_lastServerAdr.ToString());

	char gamedir_filter[256];
	strcpy(gamedir_filter, "\\gamedir\\");
	strcat(gamedir_filter, gamedir);

	msg.WriteString(gamedir_filter);

	adrlist_t *p;

	p = m_pMasterAddresses;
	while ( p )
	{
		MasterNetHandler()->NET_SendPacket( NS_CLIENT, p->adr, msg.GetData(), msg.GetNumBytesWritten() );
		p = p->next;
	}
#else
	monitoringservers->RequestServerList(0, response);
#endif
}

//
// Other functions controlled by IServerList interfaces
//

void CMaster::RequestLANServerList(const char *gamedir, IServerListResponse *response)
{
	lanservers->RequestServerList(gamedir, response);
}

void CMaster::RequestFavoritesServerList(const char* gamedir, IServerListResponse* response)
{
	favoriteservers->RequestServerList(gamedir, response);
}

void CMaster::RequestHistoryServerList(const char* gamedir, IServerListResponse* response)
{
	historyservers->RequestServerList(gamedir, response);
}

void CMaster::AddFavoriteServer(uint32 unIP, uint16 usPort) 
{
	favoriteservers->AddServer(unIP, usPort);
}

void CMaster::AddHistoryServer(uint32 unIP, uint16 usPort, time_t timeLastPlayed) 
{
	historyservers->AddServer(unIP, usPort, timeLastPlayed);
}

void CMaster::RemoveFavoriteServer(uint32 unIP, uint16 usPort)
{
	favoriteservers->RemoveServer(unIP, usPort);
}

void CMaster::RemoveHistoryServer(uint32 unIP, uint16 usPort)
{
	historyservers->RemoveServer(unIP, usPort);
}

void CMaster::AddServerAddresses( netadr_t **adr, int count )
{
	// what this function supposed to be?
}

//
// These three are defined in serverqueries
//

void CMaster::PingServer(uint32 unIP, uint16 usPort, IServerPingResponse* response) {
	if (unIP == 0 || usPort == 0 || response == 0)
		return;

	serverqueries->PingServer(unIP, usPort, response);
}
void CMaster::PlayerDetails(uint32 unIP, uint16 usPort, IServerPlayersResponse* response) {
	if (unIP == 0 || usPort == 0 || response == 0)
		return;

	serverqueries->PlayerDetails(unIP, usPort, response);
}
bool CMaster::CancelServerQuery(EServerQuery type, uint32 unIP, uint16 usPort) {
	if (unIP == 0 || usPort == 0)
		return false;

	return serverqueries->CancelServerQuery(type, unIP, usPort);
}
