//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// net_ws.c
// Windows IP Support layer.

#include "net_ws_headers.h"
#include "net_ws_queued_packet_sender.h"
#include "..\utils\bzip2\bzlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NET_COMPRESSION_STACKBUF_SIZE 4096 

static ConVar net_showsplits( "net_showsplits", "0", 0, "Show info about packet splits" );
static ConVar net_compresssplits( "net_compresssplits", "1", 0, "Compress splitpacket parts before sending" );

static ConVar ipname        ( "ip", "localhost", 0, "Overrides IP for multihomed hosts" );
static ConVar hostport      ( "hostport", va("%i",PORT_SERVER), 0, "Host game server port" );
static ConVar hostip		( "hostip", "", 0, "Host game server ip" );
static ConVar clientport    ( "clientport", va("%i",PORT_CLIENT), 0, "Host game client port" );
static ConVar hltvport		( "tv_port", va("%i",PORT_HLTV), 0, "Host SourceTV port" );
static ConVar matchmakingport( "matchmakingport", va("%i",PORT_MATCHMAKING), 0, "Host Matchmaking port" );
static ConVar systemlinkport( "systemlinkport", va("%i",PORT_SYSTEMLINK), 0, "System Link port" );

static ConVar fakelag		( "net_fakelag", "0", FCVAR_CHEAT, "Lag all incoming network data (including loopback) by this many milliseconds." );
static ConVar fakeloss		( "net_fakeloss", "0", FCVAR_CHEAT, "Simulate packet loss as a percentage (negative means drop 1/n packets)" ); 
static ConVar droppackets	( "net_droppackets", "0", FCVAR_CHEAT, "Drops next n packets on client" ); 
static ConVar fakejitter	( "net_fakejitter", "0", FCVAR_CHEAT, "Jitter fakelag packet time" );

static ConVar net_compressvoice( "net_compressvoice", "0", 0, "Attempt to compress out of band voice payloads (360 only)." );
ConVar net_usesocketsforloopback( "net_usesocketsforloopback", "0", 0, "Use network sockets layer even for listen server local player's packets (multiplayer only)." );

#ifdef _DEBUG
static ConVar fakenoise		( "net_fakenoise", "0", FCVAR_CHEAT, "Simulate corrupt network packets (changes n bits per packet randomly)" ); 
static ConVar fakeshuffle	( "net_fakeshuffle", "0", FCVAR_CHEAT, "Shuffles order of every nth packet (needs net_fakelag)" ); 
static ConVar recvpackets	( "net_recvpackets", "-1", FCVAR_CHEAT, "Receive exactly next n packets if >= 0" ); 
static ConVar	net_savelargesplits( "net_savelargesplits", "-1", 0, "If not -1, then if a split has this many or more split parts, save the entire packet to disc for analysis." );
#endif

#ifdef _X360
static void NET_LogServerCallback( IConVar *var, const char *pOldString, float flOldValue );
static ConVar net_logserver( "net_logserver", "0", 0,  "Dump server stats to a file", NET_LogServerCallback );
static ConVar net_loginterval( "net_loginterval", "1", 0, "Time in seconds between server logs" );
#endif

//-----------------------------------------------------------------------------
// Toggle Xbox 360 network security to allow cross-platform testing
//-----------------------------------------------------------------------------
#if !defined( _X360 )
#define X360SecureNetwork() false
#define IPPROTO_VDP	IPPROTO_UDP
#elif defined( _RETAIL )
#define X360SecureNetwork() true
#else
bool X360SecureNetwork( void )
{
	if ( CommandLine()->FindParm( "-xnet_bypass_security" ) )
	{
		return false;
	}
	return true;
}
#endif

extern ConVar net_showudp;
extern ConVar net_showtcp;
extern ConVar net_blocksize;
extern ConVar host_timescale;
extern int host_framecount;

void NET_ClearQueuedPacketsForChannel( INetChannel *chan );

#define DEF_LOOPBACK_SIZE 2048

typedef struct
{
	int			nPort;		// UDP/TCP use same port number
	bool		bListening;	// true if TCP port is listening
	int			hUDP;		// handle to UDP socket from socket()
	int			hTCP;		// handle to TCP socket from socket()
} netsocket_t;

typedef struct 
{
	int				newsock;	// handle of new socket
	int				netsock;	// handle of listen socket
	float			time;
	netadr_t		addr;
} pendingsocket_t;


#include "tier0/memdbgoff.h"

struct loopback_t
{
	char		*data;		// loopback buffer
	int			datalen;	// current data length
	char		defbuffer[ DEF_LOOPBACK_SIZE ];

	DECLARE_FIXEDSIZE_ALLOCATOR( loopback_t );
};

#include "tier0/memdbgon.h"

DEFINE_FIXEDSIZE_ALLOCATOR( loopback_t, 2, CUtlMemoryPool::GROW_SLOW );

// Split long packets.  Anything over 1460 is failing on some routers
typedef struct
{
	int		currentSequence;
	int		splitCount;
	int		totalSize;
	char	buffer[NET_MAX_MESSAGE];
} LONGPACKET;

// Use this to pick apart the network stream, must be packed
#pragma pack(1)
typedef struct
{
	int		netID;
	int		sequenceNumber;
	short	packetID;
} SPLITPACKET;

typedef struct
{
	int decompressedSize;
	int crc;
} SPLITPACKET_COMPRESSED;
#pragma pack()

#define MIN_USER_MAXROUTABLE_SIZE	576  // ( X.25 Networks )
#define MAX_USER_MAXROUTABLE_SIZE	MAX_ROUTABLE_PAYLOAD

#define MAX_ROUTABLE_PACKET		1400
#define SPLIT_SIZE		(MAX_ROUTABLE_PACKET - sizeof(SPLITPACKET)) // 1390

static ConVar sv_maxroutable
	( 
	"sv_maxroutable", 
	"1260", 
	0, 
	"Server upper bound on net_maxroutable that a client can use.", 
	true, MIN_USER_MAXROUTABLE_SIZE, 
	true, MAX_USER_MAXROUTABLE_SIZE 
	);

ConVar net_maxroutable
	( 
	"net_maxroutable", 
	"1260", 
	FCVAR_ARCHIVE | FCVAR_USERINFO, 
	"Requested max packet size before packets are 'split'.", 
	true, MIN_USER_MAXROUTABLE_SIZE, 
	true, MAX_USER_MAXROUTABLE_SIZE 
	);

netadr_t	net_local_adr;
double		net_time = 0.0f;	// current time, updated each frame

static	CUtlVector<netsocket_t> net_sockets;	// the 4 sockets, Server, Client, HLTV, Matchmaking
static	CUtlVector<netpacket_t>	net_packets;

static	bool net_multiplayer = false;	// if true, configured for Multiplayer
static	bool net_noip = false;	// Disable IP support, can't switch to MP mode
static	bool net_nodns = false;	// Disable DNS request to avoid long timeouts
static  bool net_notcp = true;	// Disable TCP support
static	bool net_nohltv = false; // disable HLTV support
static	bool net_dedicated = false;	// true is dedicated system
static	int  net_error = 0;			// global error code updated with NET_GetLastError()


static CUtlVectorMT< CUtlVector< CNetChan* > >			s_NetChannels;
static CUtlVectorMT< CUtlVector< pendingsocket_t > >	s_PendingSockets;

CTSQueue<loopback_t *> s_LoopBacks[LOOPBACK_SOCKETS];
static netpacket_t*	s_pLagData[MAX_SOCKETS];  // List of lag structures, if fakelag is set.

unsigned short NET_HostToNetShort( unsigned short us_in )
{
	return htons( us_in );
}

unsigned short NET_NetToHostShort( unsigned short us_in )
{
	return ntohs( us_in );
}

// This macro is used to capture the return value of a function call while recording
// a VCR file. During playback, it will get the return value out of the VCR file
// instead of actually calling the function.
#if !defined( NO_VCR )
#define VCR_NONPLAYBACKFN( call, resultVar, eventName ) \
	{ \
		if ( VCRGetMode() != VCR_Playback ) \
			resultVar = call; \
		\
		VCRGenericValue( eventName, &resultVar, sizeof( resultVar ) ); \
	}
#else
#define VCR_NONPLAYBACKFN( call, resultVar, eventName ) \
	{ \
		if ( VCRGetMode() != VCR_Playback ) \
			resultVar = call; \
		\
	}
#endif

/*
====================
NET_ErrorString
====================
*/
const char *NET_ErrorString (int code)
{
#if defined( _WIN32 )
	switch (code)
	{
	case WSAEINTR: return "WSAEINTR";
	case WSAEBADF: return "WSAEBADF";
	case WSAEACCES: return "WSAEACCES";
	case WSAEDISCON: return "WSAEDISCON";
	case WSAEFAULT: return "WSAEFAULT";
	case WSAEINVAL: return "WSAEINVAL";
	case WSAEMFILE: return "WSAEMFILE";
	case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
	case WSAEINPROGRESS: return "WSAEINPROGRESS";
	case WSAEALREADY: return "WSAEALREADY";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE: return "WSAEMSGSIZE";
	case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE: return "WSAEADDRINUSE";
	case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
	case WSAENETDOWN: return "WSAENETDOWN";
	case WSAENETUNREACH: return "WSAENETUNREACH";
	case WSAENETRESET: return "WSAENETRESET";
	case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
	case WSAECONNRESET: return "WSAECONNRESET";
	case WSAENOBUFS: return "WSAENOBUFS";
	case WSAEISCONN: return "WSAEISCONN";
	case WSAENOTCONN: return "WSAENOTCONN";
	case WSAESHUTDOWN: return "WSAESHUTDOWN";
	case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
	case WSAETIMEDOUT: return "WSAETIMEDOUT";
	case WSAECONNREFUSED: return "WSAECONNREFUSED";
	case WSAELOOP: return "WSAELOOP";
	case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSASYSNOTREADY: return "WSASYSNOTREADY";
	case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED: return "WSANOTINITIALISED";
	case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN: return "WSATRY_AGAIN";
	case WSANO_RECOVERY: return "WSANO_RECOVERY";
	case WSANO_DATA: return "WSANO_DATA";
	default: return "UNKNOWN ERROR";
	}
#else
	return strerror( code );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*sadr - 
// Output : bool	NET_StringToSockaddr
//-----------------------------------------------------------------------------
bool NET_StringToSockaddr( const char *s, struct sockaddr *sadr )
{
	char	*colon;
	char	copy[128];
	
	Q_memset (sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	Q_strncpy (copy, s, sizeof( copy ) );
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
	{
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = NET_HostToNetShort((short)atoi(colon+1));	
		}
	}
	
	if (copy[0] >= '0' && copy[0] <= '9' && Q_strstr( copy, "." ) )
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		if ( net_nodns )
			return false;	// DNS names disabled

		struct hostent	*h;
		if ( (h = gethostbyname(copy)) == NULL )
			return false;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}

int NET_GetLastError( void )
{
#if defined( _WIN32 )
	net_error = WSAGetLastError();
#else
	net_error = errno;
#endif
#if !defined( NO_VCR )
	VCRGenericValue( "WSAGetLastError", &net_error, sizeof( net_error ) );
#endif
	return net_error;
}

/*
==================
NET_ClearLaggedList

==================
*/
void NET_ClearLaggedList(netpacket_t **pList)
{
	netpacket_t * p = (*pList);

	while ( p )
	{
		netpacket_t * n = p->pNext;

		if ( p->data )
		{
			delete[] p->data;
			p->data = NULL;
		}
		delete p;
		p = n;
	}

	(*pList) = NULL;
}

void NET_ClearLagData( int sock )
{
	if ( sock < MAX_SOCKETS && s_pLagData[sock] )
	{
		NET_ClearLaggedList( &s_pLagData[sock] );
	}
}

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
bool NET_StringToAdr ( const char *s, netadr_t *a)
{
	struct sockaddr saddr;

	char address[128];
	
	Q_strncpy( address, s, sizeof(address) );

	if ( !Q_strncmp( address, "localhost", 10 ) || !Q_strncmp( address, "localhost:", 10 ) )
	{
		// subsitute 'localhost' with '127.0.0.1", both have 9 chars
		// this way we can resolve 'localhost' without DNS and still keep the port
		Q_memcpy( address, "127.0.0.1", 9 );
	}

	
	if ( !NET_StringToSockaddr (address, &saddr) )
		return false;
		
	a->SetFromSockadr( &saddr );	

	return true;
}

CNetChan *NET_FindNetChannel(int socket, netadr_t &adr)
{
	AUTO_LOCK_FM( s_NetChannels );

	int numChannels = s_NetChannels.Count();

	for ( int i = 0; i < numChannels; i++ )
	{
		CNetChan * chan = s_NetChannels[i];

		// sockets must match
		if ( socket != chan->GetSocket() )
			continue;

		// and the IP:Port address 
		if ( adr.CompareAdr( chan->GetRemoteAddress() )  )
		{
			return chan;	// found it
		}
	}

	return NULL;	// no channel found
}

void NET_CloseSocket( int hSocket, int sock = -1)
{
	if ( !hSocket )
		return;

	// close socket handle
	int ret;
	VCR_NONPLAYBACKFN( closesocket( hSocket ), ret, "closesocket" );
	if ( ret == -1 )
	{
		NET_GetLastError();
		ConMsg ("WARNING! NET_CloseSocket: %s\n", NET_ErrorString(net_error));
	}

	// if hSocket mapped to hTCP, clear hTCP
	if ( sock >= 0 )
	{
		if ( net_sockets[sock].hTCP == hSocket )
		{
			net_sockets[sock].hTCP = 0;
			net_sockets[sock].bListening = false;
		}
	}
}

/*
====================
NET_IPSocket
====================
*/
int NET_OpenSocket ( const char *net_interface, int& port, int protocol )
{
	struct sockaddr_in	address;
	unsigned int		opt;
	int					newsocket = -1;

	if ( protocol == IPPROTO_TCP )
	{
		VCR_NONPLAYBACKFN( socket (PF_INET, SOCK_STREAM, IPPROTO_TCP), newsocket, "socket()" );
	}
	else // as UDP or VDP
	{
		VCR_NONPLAYBACKFN( socket (PF_INET, SOCK_DGRAM, protocol), newsocket, "socket()" );
	}

	if ( newsocket == -1 )
	{
		NET_GetLastError(); 
		if ( net_error != WSAEAFNOSUPPORT )
			Msg ("WARNING: NET_OpenSockett: socket failed: %s", NET_ErrorString(net_error));

		return 0;
	}

	
	opt =  1; // make it non-blocking
	int ret;
	VCR_NONPLAYBACKFN( ioctlsocket (newsocket, FIONBIO, (unsigned long*)&opt), ret, "ioctlsocket" );
	if ( ret == -1 )
	{
		NET_GetLastError();
		Msg ("WARNING: NET_OpenSocket: ioctl FIONBIO: %s\n", NET_ErrorString(net_error) );
	}
	
	if ( protocol == IPPROTO_TCP )
	{
		if ( !IsX360() ) // SO_KEEPALIVE unsupported on the 360
		{
			opt = 1; // set TCP options: keep TCP connection alive
			VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt)), ret, "setsockopt" );
			if (ret == -1)
			{
				NET_GetLastError();		
				Msg ("WARNING: NET_OpenSocket: setsockopt SO_KEEPALIVE: %s\n", NET_ErrorString(net_error));
				return 0;
			}
		}

		linger optlinger;	// set TCP options: Does not block close waiting for unsent data to be sent
		optlinger.l_linger = 0;
		optlinger.l_onoff = 0;
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_LINGER, (char *)&optlinger, sizeof(optlinger)), ret, "setsockopt" );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_LINGER: %s\n", NET_ErrorString(net_error));
			return 0;
		}

		opt = 1; // set TCP options: Disables the Nagle algorithm for send coalescing.
		VCR_NONPLAYBACKFN( setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)), ret, "setsockopt" );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt TCP_NODELAY: %s\n", NET_ErrorString(net_error));
			return 0;
		}

		opt = NET_MAX_MESSAGE; // set TCP options: set send buffer size
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_SNDBUF, (char *)&opt, sizeof(opt)), ret, "setsockopt" );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_SNDBUF: %s\n", NET_ErrorString(net_error));
			return 0;
		}

		opt = NET_MAX_MESSAGE; // set TCP options: set receive buffer size
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_RCVBUF, (char *)&opt, sizeof(opt)), ret, "setsockopt" );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_RCVBUF: %s\n", NET_ErrorString(net_error));
			return 0;
		}
		

		return newsocket;	// don't bind TCP sockets by default
	}

	// rest is UDP only
	
	// VPD protocol (Xbox 360 secure network) doesn't support SO_BROADCAST
 	if ( !X360SecureNetwork() || protocol != IPPROTO_VDP )
 	{
		opt = 1; // set UDP options: make it broadcast capable
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt)), ret, "setsockopt" );
		if (ret == -1)
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString(net_error));
			return 0;
		}
	}
	
	if ( CommandLine()->FindParm( "-reuse" ) )
	{
		opt = 1; // make it reusable
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)), ret, "setsockopt" );
		if (ret == -1)
		{
			NET_GetLastError();
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_REUSEADDR: %s\n", NET_ErrorString(net_error));
			return 0;
		}
	}

	if (!net_interface || !net_interface[0] || !Q_strcmp(net_interface, "localhost"))
	{
		address.sin_addr.s_addr = INADDR_ANY;
	}
	else
	{
		NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);
	}

	address.sin_family = AF_INET;

	int port_offset;	// try binding socket to port, try next 10 is port is already used

	for ( port_offset = 0; port_offset < PORT_TRY_MAX; port_offset++ )
	{
		if ( port == PORT_ANY )
		{
			address.sin_port = 0;	// = INADDR_ANY
		}
		else
		{
			address.sin_port = NET_HostToNetShort((short)( port + port_offset ));
		}

		VCR_NONPLAYBACKFN( bind (newsocket, (struct sockaddr *)&address, sizeof(address)), ret, "bind" );
		if ( ret != -1 )
		{
			if ( port != PORT_ANY && port_offset != 0 )
			{
				port += port_offset;	// update port
				ConDMsg( "Socket bound to non-default port %i because original port was already in use.\n", port );
			}
			break;
		}

		NET_GetLastError();

		if ( port == PORT_ANY || net_error != WSAEADDRINUSE )
		{
			Msg ("WARNING: NNET_OpenSocket: bind: %s\n", NET_ErrorString(net_error));
			NET_CloseSocket(newsocket,-1);
			return 0;
		}

		// Try next port
	}

	if ( port_offset == PORT_TRY_MAX )
	{
		Msg( "WARNING: UDP_OpenSocket: unable to bind socket\n" );
		NET_CloseSocket( newsocket,-1 );
		return 0;
	}
	
	return newsocket;
}

int NET_ConnectSocket( int sock, const netadr_t &addr )
{
	Assert( (sock >= 0) && (sock < net_sockets.Count()) );

	netsocket_t *netsock = &net_sockets[sock];

	if ( netsock->hTCP )
	{
		NET_CloseSocket( netsock->hTCP, sock );
	}

	if ( net_notcp )
		return 0;

	sockaddr saddr;

	addr.ToSockadr( &saddr );

	int anyport = PORT_ANY;

	netsock->hTCP = NET_OpenSocket( ipname.GetString(), anyport, true );

	if ( !netsock->hTCP )
	{
		Msg( "Warning! NET_ConnectSocket failed opening socket %i, port %i.\n", sock, net_sockets[sock].nPort );
		return false;
	}

	int ret;
	VCR_NONPLAYBACKFN( connect( netsock->hTCP, &saddr, sizeof(saddr) ), ret, "connect" );
	if ( ret == -1 )
	{
		NET_GetLastError();

		if ( net_error != WSAEWOULDBLOCK )
		{
			Msg ("NET_ConnectSocket: %s\n", NET_ErrorString( net_error ) );
			return 0;
		}
	}

	return net_sockets[sock].hTCP;
}

int NET_SendStream( int nSock, const char * buf, int len, int flags )
{
	//int ret = send( nSock, buf, len, flags );
	int ret = VCRHook_send( nSock, buf, len, flags );
	if ( ret == -1 )
	{
		NET_GetLastError();

		if ( net_error == WSAEWOULDBLOCK )
		{
			return 0; // ignore EWOULDBLOCK
		}

		Msg ("NET_SendStream: %s\n", NET_ErrorString( net_error ) );
	}

	return ret;
}

int NET_ReceiveStream( int nSock, char * buf, int len, int flags )
{
	int ret = VCRHook_recv( nSock, buf, len, flags );
	if ( ret == -1 )
	{
		NET_GetLastError();

		if ( net_error == WSAEWOULDBLOCK || 
			 net_error == WSAENOTCONN )
		{
			return 0; // ignore EWOULDBLOCK
		}

		Msg ("NET_ReceiveStream: %s\n", NET_ErrorString( net_error ) );
	}

	return ret;
}

INetChannel *NET_CreateNetChannel(int socket, netadr_t *adr, const char * name, INetChannelHandler * handler, bool bForceNewChannel)
{
	CNetChan *chan = NULL;

	if ( !bForceNewChannel && adr != NULL )
	{
		// try to find real network channel if already existing
		if ( ( chan = NET_FindNetChannel( socket, *adr ) ) != NULL )
		{
			// channel already known, clear any old stuff before Setup wipes all
			chan->Clear();
		}
	}

	if ( !chan )
	{
		// create new channel
		chan = new CNetChan();

		AUTO_LOCK_FM( s_NetChannels );
		s_NetChannels.AddToTail( chan );
	}

	NET_ClearLagData( socket );

	// just reset and return
	chan->Setup( socket, adr, name, handler );

	return chan;
}

void NET_RemoveNetChannel(INetChannel *netchan, bool bDeleteNetChan)
{
	if ( !netchan )
	{
		return;
	}

	AUTO_LOCK_FM( s_NetChannels );
	if ( s_NetChannels.Find( static_cast<CNetChan*>(netchan) ) == s_NetChannels.InvalidIndex() )
	{
		DevMsg(1, "NET_CloseNetChannel: unknown channel.\n");
		return;
	}

	s_NetChannels.FindAndRemove( static_cast<CNetChan*>(netchan) );

	NET_ClearQueuedPacketsForChannel( netchan );
	
	if ( bDeleteNetChan )
		delete netchan;
}


/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/


void NET_SendLoopPacket (int sock, int length, const unsigned char *data)
{
	loopback_t	*loop;

	if ( length > NET_MAX_PAYLOAD )
	{
		DevMsg( "NET_SendLoopPacket:  packet too big (%i).\n", length );
		return;
	}

	loop = new loopback_t;

	if ( length <= DEF_LOOPBACK_SIZE )
	{
		loop->data = loop->defbuffer;
	}
	else
	{
		loop->data  = new char[ length ];
	}

	Q_memcpy (loop->data, data, length);
	loop->datalen = length;

	if ( sock == NS_SERVER )
	{
		s_LoopBacks[NS_CLIENT].PushItem( loop );
	}
	else if ( sock == NS_CLIENT )
	{
		s_LoopBacks[NS_SERVER].PushItem( loop );
	}
	else
	{
		DevMsg( "NET_SendLoopPacket:  invalid socket (%i).\n", sock );
		return;
	}
}

//=============================================================================

int NET_CountLaggedList( netpacket_t *pList )
{
	int c = 0;
	netpacket_t *p = pList;
	
	while ( p )
	{
		c++;
		p = p->pNext;
	}

	return c;
}

/*
===================
NET_AddToLagged

===================
*/
void NET_AddToLagged( netpacket_t **pList, netpacket_t *pPacket )
{
	if ( pPacket->pNext )
	{
		Msg("NET_AddToLagged::Packet already linked\n");
		return;
	}

	// first copy packet

	netpacket_t *newPacket = new netpacket_t;

	(*newPacket) = (*pPacket);  // copy packet infos
	newPacket->data = new unsigned char[ pPacket->size ];	// create new data buffer
	Q_memcpy( newPacket->data, pPacket->data, pPacket->size ); // copy packet data
	newPacket->pNext = NULL;

	// if list is empty, this is our first element
	if ( (*pList) == NULL )
	{
		(*pList) = newPacket;	// put packet in top of list
	}
	else
	{
		netpacket_t *last = (*pList);

		while ( last->pNext )
		{
			// got to end of list
			last = last->pNext;
		}

		// add at end
		last->pNext = newPacket;
	}
}

// Actual lag to use in msec
static float s_FakeLag = 0.0;

float NET_GetFakeLag()
{
	return s_FakeLag;
}

// How quickly we converge to a new value for fakelag
#define FAKELAG_CONVERGE	200  // ms per second

/*
==============================
NET_AdjustLag

==============================
*/
void NET_AdjustLag( void )
{
	static double s_LastTime = 0;
	
	// Bound time step
	
	float dt = clamp( net_time - s_LastTime, 0.0f, 0.2f );
	
	s_LastTime = net_time;

	// Already converged?
	if ( fakelag.GetFloat() == s_FakeLag )
		return;

	// Figure out how far we have to go
	float diff = fakelag.GetFloat() - s_FakeLag;

	// How much can we converge this frame
	float converge = FAKELAG_CONVERGE * dt;

	// Last step, go the whole way
	if ( converge > fabs( diff ) )
	{
		converge = fabs( diff );
	}

	// Converge toward fakelag.GetFloat()
	if ( diff < 0.0 )
	{
		// Converge toward fakelag.GetFloat()
		s_FakeLag -= converge;
	}
	else
	{
		s_FakeLag += converge;
	}
}


bool NET_LagPacket (bool newdata, netpacket_t * packet) // without newdata I guess
{
	static int losscount[MAX_SOCKETS];

	if ( packet->source >= MAX_SOCKETS )
		return newdata; // fake lag not supported for extra sockets

	if ( (droppackets.GetInt() > 0)  && newdata && (packet->source == NS_CLIENT) )
	{
		droppackets.SetValue( droppackets.GetInt() - 1 );
		return false;
	}

	if ( fakeloss.GetFloat() && newdata )
	{
		losscount[packet->source]++;

		if ( fakeloss.GetFloat() > 0.0f )
		{
			// Act like we didn't hear anything if we are going to lose the packet.
			// Depends on random # generator.
			if (RandomInt(0,100) <= (int)fakeloss.GetFloat())
				return false;
		}
		else
		{
			int ninterval;

			ninterval = (int)(fabs( fakeloss.GetFloat() ) );
			ninterval = max( 2, ninterval );

			if ( !( losscount[packet->source] % ninterval ) )
			{
				return false;
			}
		}
	}

	if (s_FakeLag <= 0.0)
	{
		// Never leave any old msgs around
		for ( int i=0; i<MAX_SOCKETS; i++ )
		{
			NET_ClearLagData( i );
		}
		return newdata;
	}

	// if new packet arrived in fakelag list
	if ( newdata )	// this block didnt exist in src2006
	{
		NET_AddToLagged( &s_pLagData[packet->source], packet );
	}

	// Now check the correct list and feed any message that is old enough.
	netpacket_t *p = s_pLagData[packet->source]; // current packet

	if ( !p )
		return false;	// no packet in lag list

	float target = s_FakeLag;
	float maxjitter = min( fakejitter.GetFloat(), target * 0.5f );
	target += RandomFloat( -maxjitter, maxjitter );

	if ( (p->received + (target/1000.0f)) > net_time )
		return false;	// not time yet for this packet

#ifdef _DEBUG
	if ( fakeshuffle.GetInt() && p->pNext )
	{
		if ( !RandomInt( 0, fakeshuffle.GetInt() ) )
		{
			// swap p and p->next
			netpacket_t * t = p->pNext;
			p->pNext = t->pNext;
			t->pNext = p;
			p = t; 
		}
	}
#endif
	
	// remove packet p from list (is head)
	s_pLagData[packet->source] = p->pNext;
		
	// copy & adjust content
	packet->source	= p->source;	
	packet->from	= p->from;		
	packet->pNext	= NULL;			// no next
	packet->received = net_time;	// new time
	packet->size	= p->size;		
	packet->stream	= p->stream;
			
	Q_memcpy( packet->data, p->data, p->size );

	// free lag packet
					
	delete[] p->data;
	delete p;
			
	return true;
}

// Calculate MAX_SPLITPACKET_SPLITS according to the smallest split size
#define MAX_SPLITPACKET_SPLITS ( NET_MAX_MESSAGE / SPLIT_SIZE ) // 69
#define SPLIT_PACKET_STALE_TIME		15.0f
#define SPLIT_FLAG_COMPRESSED 0x80000000

class CSplitPacketEntry
{
public:
	CSplitPacketEntry()
	{
		memset( &from, 0, sizeof( from ) );

		int i;
		for ( i = 0; i < MAX_SPLITPACKET_SPLITS; i++ )
		{
			splitflags[ i ] = -1;
		}

		memset( &netsplit, 0, sizeof( netsplit ) );
		lastactivetime = 0.0f;
	}

public:
	netadr_t		from;
	int				splitflags[ MAX_SPLITPACKET_SPLITS ];
	LONGPACKET		netsplit;
	// host_time the last time any entry was received for this entry
	float			lastactivetime;
};

typedef CUtlVector< CSplitPacketEntry > vecSplitPacketEntries_t;
static CUtlVector<vecSplitPacketEntries_t> net_splitpackets;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void NET_DiscardStaleSplitpackets( const int sock )
{
	vecSplitPacketEntries_t &splitPacketEntries = net_splitpackets[sock];
	int i;
	for ( i = splitPacketEntries.Count() - 1; i >= 0; i-- )
	{
		CSplitPacketEntry *entry = &splitPacketEntries[ i ];
		Assert( entry );

		if ( net_time < ( entry->lastactivetime + SPLIT_PACKET_STALE_TIME ) )
			continue;

		splitPacketEntries.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *from - 
// Output : CSplitPacketEntry
//-----------------------------------------------------------------------------
CSplitPacketEntry *NET_FindOrCreateSplitPacketEntry( const int sock, netadr_t *from )
{
	vecSplitPacketEntries_t &splitPacketEntries = net_splitpackets[sock];
	int i, count = splitPacketEntries.Count();
	CSplitPacketEntry *entry = NULL;
	for ( i = 0; i < count; i++ )
	{
		entry = &splitPacketEntries[ i ];
		Assert( entry );

		if ( from->CompareAdr(entry->from) )
			break;
	}

	if ( i >= count )
	{
		CSplitPacketEntry newentry;
		newentry.from = *from;

		splitPacketEntries.AddToTail( newentry );

		entry = &splitPacketEntries[ splitPacketEntries.Count() - 1 ];
	}

	Assert( entry );
	return entry;
}

static char const *DescribeSocket( int sock )
{
	switch ( sock )
	{
	default:
		break;
	case NS_CLIENT:
		return "cl ";
	case NS_SERVER:
		return "sv ";
	case NS_HLTV:
		return "htv";
	case NS_MATCHMAKING:
		return "mat";
	case NS_SYSTEMLINK:
		return "lnk";
	}

	return "??";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock
//			packet
// 
// Output : bool
// 
// Prototype from IDA: char __cdecl NET_GetLong(int a1)
//-----------------------------------------------------------------------------
bool NET_GetLong(const int sock, netpacket_t* packet)
{
	int				packetNumber, packetCount, sequenceNumber, offset;
	short			packetID;
	SPLITPACKET* pHeader;

	if (packet->size < sizeof(SPLITPACKET))
	{
		Msg( "Invalid split packet length %i\n", packet->size );
		return false;
	}

	CSplitPacketEntry* entry = NET_FindOrCreateSplitPacketEntry( sock, &packet->from );
	Assert(entry);
	if (!entry)
		return false;

	entry->lastactivetime = net_time;
	Assert( packet->from.CompareAdr( entry->from ) );

	pHeader = (SPLITPACKET*)packet->data;
	// pHeader is network endian correct
	sequenceNumber = LittleLong( pHeader->sequenceNumber );
	packetID = LittleShort( pHeader->packetID );
	// High byte is packet number
	packetNumber = (packetID >> 8);
	// Low byte is number of total packets
	packetCount = (packetID & 0xff);

	if (packetNumber >= MAX_SPLITPACKET_SPLITS ||
		packetCount > MAX_SPLITPACKET_SPLITS)
	{
		Msg( "NET_GetLong:  Split packet from %s with too many split parts (number %i/ count %i) where %i is max count allowed\n",
			packet->from.ToString(),
			packetNumber,
			packetCount,
			MAX_SPLITPACKET_SPLITS );
		return false;
	}

	// First packet in split series?
	if (entry->netsplit.currentSequence == -1 ||
		sequenceNumber != entry->netsplit.currentSequence)
	{
		entry->netsplit.currentSequence = sequenceNumber;
		entry->netsplit.splitCount = packetCount;
	}

	int size = packet->size - sizeof(SPLITPACKET);

	if (entry->splitflags[packetNumber] != sequenceNumber)
	{
		// Last packet in sequence? set size
		if (packetNumber == (packetCount - 1))
		{
			entry->netsplit.totalSize = size + SPLIT_SIZE * packetCount - SPLIT_SIZE;
		}

		entry->netsplit.splitCount--;		// Count packet
		entry->splitflags[packetNumber] = sequenceNumber;

		if (net_showsplits.GetInt() && net_showsplits.GetInt() != 3)
		{
			Msg( "<-- Split packet %i of %i, seq %i, size %i from %s\n",
				packetNumber + 1,
				packetCount,
				sequenceNumber,
				size,
				packet->from.ToString());
		}
	}
	else
	{
		Msg( "NET_GetLong:  Ignoring duplicated split packet %i of %i ( %i bytes ) from %s\n", packetNumber + 1, packetCount, size, packet->from.ToString() );
	}


	// Copy the incoming data to the appropriate place in the buffer
	offset = SPLIT_SIZE * packetNumber;
	memcpy(entry->netsplit.buffer + offset, packet->data + sizeof(SPLITPACKET), size);

	// Have we received all of the pieces to the packet?
	if (entry->netsplit.splitCount <= 0)
	{
		int sequence = entry->netsplit.currentSequence;

		entry->netsplit.currentSequence = -1;	// Clear packet
		if (entry->netsplit.totalSize > sizeof(entry->netsplit.buffer))
		{
			Msg("Split packet too large! %d bytes from %s\n", entry->netsplit.totalSize, packet->from.ToString());
			return false;
		}

		if (sequence >= 0)
		{
			Q_memcpy( packet->data, entry->netsplit.buffer, entry->netsplit.totalSize );
			packet->size = entry->netsplit.totalSize;
			return true;
		}
		else
		{
			SPLITPACKET_COMPRESSED* pCompressedHeader = ( SPLITPACKET_COMPRESSED* )entry->netsplit.buffer;
			int decompressedSize = pCompressedHeader->decompressedSize;
			int expectedCRC = pCompressedHeader->crc;

			char* decompressBuffer = new char[NET_MAX_MESSAGE];
			
			unsigned int destLen = NET_MAX_MESSAGE;
			int result = BZ2_bzBuffToBuffDecompress(
				decompressBuffer,
				&destLen,
				entry->netsplit.buffer + sizeof(SPLITPACKET_COMPRESSED),
				entry->netsplit.totalSize - sizeof(SPLITPACKET_COMPRESSED),
				0, 0);

			if ( result != BZ_OK || destLen != decompressedSize )
			{
				Msg( "Error decompressing split packet %d bytes from %s (%i)\n", entry->netsplit.totalSize, packet->from.ToString(), result );
				delete[] decompressBuffer;
				return false;
			}

			if ( CRC32_ProcessSingleBuffer( decompressBuffer, decompressedSize ) != expectedCRC )
			{
				Msg( "Error decompressing split packet %d bytes from %s, crc's don't match\n", entry->netsplit.totalSize, packet->from.ToString() );
				delete[] decompressBuffer;
				return false;
			}

			memcpy(packet->data, decompressBuffer, decompressedSize);
			packet->size = decompressedSize;
			delete[] decompressBuffer;

			return true;
		}
	}

	return false;
}


bool NET_GetLoopPacket ( netpacket_t * packet )
{
	Assert ( packet );

	loopback_t	*loop;

	if ( packet->source > NS_SERVER )
		return false;
		
	if ( !s_LoopBacks[packet->source].PopItem( &loop ) )
	{
		return false;
	}

	if (loop->datalen == 0)
	{
		// no packet in loopback buffer
		delete loop;
		return ( NET_LagPacket( false, packet ) );
	}

	// copy data from loopback buffer to packet 
	packet->from.SetType( NA_LOOPBACK );
	packet->size = loop->datalen;
	Q_memcpy ( packet->data, loop->data, packet->size );
	
	loop->datalen = 0; // buffer is avalibale again

	if ( loop->data != loop->defbuffer )
	{
		delete loop->data;
		loop->data = loop->defbuffer;
	}

	delete loop;

	// allow lag system to modify packet
	return ( NET_LagPacket( true, packet ) );	
}

bool NET_ReceiveDatagram ( const int sock, netpacket_t * packet )
{
	Assert ( packet );
	Assert ( net_multiplayer );

	struct sockaddr	from;
	int				fromlen = sizeof(from);
	int				net_socket = net_sockets[packet->source].hUDP;

#ifdef _DEBUG
	if ( recvpackets.GetInt() >= 0 )
	{
		unsigned long bytes;

		ioctlsocket( net_socket, FIONREAD, &bytes );

		if ( bytes <= 0 )
			return false;

		if ( recvpackets.GetInt() == 0 )
			return false;

		recvpackets.SetValue( recvpackets.GetInt() - 1 );
	}
#endif

	int ret = VCRHook_recvfrom(net_socket, (char *)packet->data, NET_MAX_MESSAGE, 0, (struct sockaddr *)&from, (int *)&fromlen );
	if ( ret > 0 )
	{
		packet->size = ret;

		MEM_ALLOC_CREDIT();
		CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > bufVoice( NET_COMPRESSION_STACKBUF_SIZE );

		unsigned int nVoiceBits = 0u;

		packet->from.SetFromSockadr( &from );
		packet->size = ret;
		

		if ( ret < NET_MAX_MESSAGE )
		{
			// Check for split message
			if ( LittleLong( *(int *)packet->data ) == NET_HEADER_FLAG_SPLITPACKET )	
			{
				if ( !NET_GetLong( sock, packet ) )
					return false;
			}

			return NET_LagPacket( true, packet );
		}
		else
		{
			ConDMsg ( "NET_ReceiveDatagram:  Oversize packet from %s\n", packet->from.ToString() );
		}
	}
	else if ( ret == -1  )									// error?
	{
		NET_GetLastError();

		switch ( net_error )
		{
		case WSAEWOULDBLOCK:
		case WSAECONNRESET:
		case WSAECONNREFUSED:
			break;
		case WSAEMSGSIZE:
			ConDMsg ("NET_ReceivePacket: %s\n", NET_ErrorString(net_error));
			break;
		default:
			// Let's continue even after errors
			ConDMsg ("NET_ReceivePacket: %s\n", NET_ErrorString(net_error));
			break;
		}
	}

	return false;
}

netpacket_t *NET_GetPacket (int sock, byte *scratch )
{
	// Each socket has its own netpacket to allow multithreading
	netpacket_t &inpacket = net_packets[sock];

	NET_AdjustLag();
	NET_DiscardStaleSplitpackets( sock );

	// setup new packet
	inpacket.from.SetType( NA_IP );
	inpacket.from.Clear();
	inpacket.received = net_time;
	inpacket.source = sock;	
	inpacket.data = scratch;
	inpacket.size = 0;
	inpacket.pNext = NULL;
	inpacket.message.SetDebugName("inpacket.message");

	// Check loopback first
	if ( !NET_GetLoopPacket( &inpacket ) )
	{
		if (!NET_IsMultiplayer())
		{
			return NULL;
		}

		// then check UDP data 
		if ( !NET_ReceiveDatagram( sock, &inpacket ) )
		{
			// at last check if the lag system has a packet for us
			if ( !NET_LagPacket (false, &inpacket) )
			{
				return NULL;	// we don't have any new packet
			}
		}
	}
	
	Assert ( inpacket.size ); 

#ifdef _DEBUG
	if ( fakenoise.GetInt() > 0 )
	{
		COM_AddNoise( inpacket.data, inpacket.size, fakenoise.GetInt() );
	}
#endif
	
	// prepare bitbuffer for reading packet with new size
	inpacket.message.StartReading( inpacket.data, inpacket.size );

	return &inpacket;
}

void NET_ProcessPending( void )
{
	AUTO_LOCK_FM( s_PendingSockets );
	for ( int i=0; i<s_PendingSockets.Count();i++ )
	{
		pendingsocket_t * psock = &s_PendingSockets[i];

		char	headerBuf[5];

		if ( (net_time - psock->time) > TCP_CONNECT_TIMEOUT )
		{
			NET_CloseSocket( psock->newsock );
			s_PendingSockets.Remove( i );
			continue;
		}

		int ret = NET_ReceiveStream( psock->newsock, headerBuf, sizeof(headerBuf), 0 );

		if ( ret == 0 )
		{
			continue;	// nothing received
		}
		else if ( ret == -1 )
		{
			NET_CloseSocket( psock->newsock );
			s_PendingSockets.Remove( i );
			continue;	// connection closed somehow
		}
		
		bf_read		header( headerBuf, sizeof(headerBuf) );

		int cmd = header.ReadByte();
		unsigned long challengeNr = header.ReadLong();
		bool bOK = false;	

		if ( cmd == STREAM_CMD_ACKN )
		{
			AUTO_LOCK_FM( s_NetChannels );
			for ( int j = 0; j < s_NetChannels.Count(); j++ )
			{
				CNetChan * chan = s_NetChannels[j];

				if ( chan->GetSocket() != psock->netsock )
					continue;

				if ( challengeNr == chan->GetChallengeNr() && !chan->m_StreamSocket )
				{
					if ( psock->addr.CompareAdr( chan->remote_address, true ) )
					{
						chan->m_StreamSocket = psock->newsock;
						chan->m_StreamActive = true;
						
						chan->ResetStreaming();

						bOK = true;

						if ( net_showtcp.GetInt() )
						{
							Msg ("TCP <- %s: connection accepted\n", psock->addr.ToString() );
						}
						
						break;
					}
					else
					{
						Msg ("TCP <- %s: IP address mismatch.\n", psock->addr.ToString() );
					}
				}
			}
		}

		if ( !bOK )
		{
			Msg ("TCP <- %s: invalid connection request.\n", psock->addr.ToString() );
			NET_CloseSocket( psock->newsock );
		}

		s_PendingSockets.Remove( i );
	}
}

void NET_ProcessListen(int sock)
{
	netsocket_t * netsock = &net_sockets[sock];
		
	if ( !netsock->bListening )
		return;

	sockaddr sa;
	int nLengthAddr = sizeof(sa);
		
	int newSocket;

	VCR_NONPLAYBACKFN( accept( netsock->hTCP, &sa, (socklen_t*)&nLengthAddr), newSocket, "accept" );
#if !defined( NO_VCR )
	VCRGenericValue( "sockaddr", &sa, sizeof( sa ) );
#endif
	if ( newSocket == -1 )
	{
		NET_GetLastError();

		if ( net_error != WSAEWOULDBLOCK )
		{
			ConDMsg ("NET_ThreadListen: %s\n", NET_ErrorString(net_error));
		}
		return;
	}

	// new connection TCP request, put in pending queue

	pendingsocket_t psock;

	psock.newsock = newSocket;
	psock.netsock = sock;
	psock.addr.SetFromSockadr( &sa );
	psock.time = net_time;

	AUTO_LOCK_FM( s_PendingSockets );
	s_PendingSockets.AddToTail( psock );

	// tell client to send challenge number to identify

	char authcmd = STREAM_CMD_AUTH;

	NET_SendStream( newSocket, &authcmd, 1 , 0 );	

	if ( net_showtcp.GetInt() )
	{
		Msg ("TCP <- %s: connection request.\n", psock.addr.ToString() );
	}
}

CTSList<byte *> g_NetScratchBuffers;

void NET_ProcessSocket( int sock, IConnectionlessPacketHandler *handler )
{
	netpacket_t * packet;
	
	Assert ( (sock >= 0) && (sock<net_sockets.Count()) );

	// Scope for the auto_lock
	{
		AUTO_LOCK_FM( s_NetChannels );

		// get streaming data from channel sockets
		int numChannels = s_NetChannels.Count();

		for ( int i = (numChannels-1); i >= 0 ; i-- )
		{
			CNetChan *netchan = s_NetChannels[i];

			// sockets must match
			if ( sock != netchan->GetSocket() )
				continue;

			if ( !netchan->ProcessStream() )
			{
				netchan->GetMsgHandler()->ConnectionCrashed("TCP connection failed.");
			}
		}
	}

	// now get datagrams from sockets
	byte *scratch;
	if ( !g_NetScratchBuffers.PopItem( &scratch ) )
	{
		scratch = new byte[NET_MAX_MESSAGE];
	}
	while ( ( packet = NET_GetPacket ( sock, scratch ) ) != NULL )
	{
		if ( Filter_ShouldDiscard ( packet->from ) )	// filtering is done by network layer
		{
			Filter_SendBan( packet->from );	// tell them we aren't listening...
			continue;
		} 

		// check for connectionless packet (0xffffffff) first
		if ( LittleLong( *(unsigned int *)packet->data ) == CONNECTIONLESS_HEADER )
		{
			packet->message.ReadLong();	// read the -1

			if ( net_showudp.GetInt() )
			{
				Msg("UDP <- %s: sz=%i OOB '%c'\n", packet->from.ToString(), packet->size, packet->data[4]);
			}

			handler->ProcessConnectionlessPacket( packet );
			continue;
		}

		// check for packets from connected clients
		
		CNetChan * netchan = NET_FindNetChannel( sock, packet->from );

		if ( netchan )
		{
			netchan->ProcessPacket( packet, true );
		}
		/* else	// Not an error that may happen during connect or disconnect. Didnt exist in src2006.
		{
			Msg ("Sequenced packet without connection from %s\n" , packet->from.ToString() );
		}*/
	}
	g_NetScratchBuffers.PushItem( scratch );
}

void NET_LogBadPacket(netpacket_t * packet)
{
	FileHandle_t fp;
	int i = 0;
	char filename[ MAX_OSPATH ];
	bool done = false;

	while ( i < 1000 && !done )
	{
		Q_snprintf( filename, sizeof( filename ), "badpacket%03i.dat", i );
		fp = g_pFileSystem->Open( filename, "rb" );
		if ( !fp )
		{
			fp = g_pFileSystem->Open( filename, "wb" );
			g_pFileSystem->Write( packet->data, packet->size, fp );
			done = true;
		}
		if ( fp )
		{
			g_pFileSystem->Close( fp );
		}
		i++;
	}

	if ( i < 1000 )
	{
		Msg( "Error buffer for %s written to %s\n", packet->from.ToString(), filename );
	}
	else
	{
		Msg( "Couldn't write error buffer, delete error###.dat files to make space\n" );
	}
}

int NET_SendToImpl( SOCKET s, const char FAR * buf, int len, const struct sockaddr FAR * to, int tolen )
{
	int nSend = 0;
	nSend = sendto( s, buf, len, 0, to, tolen );

	return nSend;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			s - 
//			buf - 
//			len - 
//			flags - 
//			to - 
//			tolen - 
// Output : int
//-----------------------------------------------------------------------------
bool CL_IsHL2Demo();
bool CL_IsPortalDemo();
// First argument is INetChannel* actually but, it's not used anyways so im not gonna add it :)
int NET_SendTo( bool verbose, SOCKET s, const char FAR * buf, int len, const struct sockaddr FAR * to, int tolen )
{	
	int nSend = 0;
	
	
	// If it's 0.0.0.0:0, then it's a fake player + sv_stressbots and we've plumbed everything all 
	// the way through here, where we finally bail out.
	sockaddr_in *pInternetAddr = (sockaddr_in*)to;
#ifdef _WIN32
	if ( pInternetAddr->sin_addr.S_un.S_addr == 0
#else
	if ( pInternetAddr->sin_addr.s_addr == 0 
#endif
		&& pInternetAddr->sin_port == 0 )
	{		
		return len;
	}

	// Normally, we shouldn't need to write this data to the file, but it can help catch
	// out-of-sync errors earlier.
	if ( VCRGetMode() != VCR_Disabled && vcr_verbose.GetInt() )
	{
#if !defined( NO_VCR )
		VCRGenericValue( "senddata", &len, sizeof( len ) );
		VCRGenericValue( "senddata2", (char*)buf, len );
#endif
	}

	// Don't send anything out in VCR mode.. it just annoys other people testing in multiplayer.
	if ( VCRGetMode() != VCR_Playback )
	{
#ifndef SWDS
		if ( ( CL_IsHL2Demo() || CL_IsPortalDemo() ) && !net_dedicated )
		{
			Error("");
		}
#endif // _WIN32

		nSend = NET_SendToImpl
		( 
			s, 
			buf,
			len,
			to, 
			tolen
		);
	}

#if defined( _DEBUG )
	if ( verbose && 
		( nSend > 0 ) && 
		( len > MAX_ROUTABLE_PAYLOAD ) )
	{
		ConDMsg( "NET_SendTo:  Packet length (%i) > (%i) bytes\n", len, MAX_ROUTABLE_PAYLOAD );
	}
#endif
	return nSend;
}

#if defined( _DEBUG )

#include "FileSystem.h"
#include "FileSystem_Engine.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *NET_GetDebugFilename( char const *prefix )
{
	static char filename[ MAX_OSPATH ];

	int i;

	for ( i = 0; i < 10000; i++ )
	{
		Q_snprintf( filename, sizeof( filename ), "debug/%s%04i.dat", prefix, i );
		if ( g_pFileSystem->FileExists( filename ) )
			continue;

		return filename;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*buf - 
//			len - 
//-----------------------------------------------------------------------------
void NET_StorePacket( char const *filename, byte const *buf, int len )
{
	FileHandle_t fh;

	g_pFileSystem->CreateDirHierarchy( "debug/", "DEFAULT_WRITE_PATH" );
	fh = g_pFileSystem->Open( filename, "wb" );
	if ( FILESYSTEM_INVALID_HANDLE != fh )
	{
		g_pFileSystem->Write( buf, len, fh );
		g_pFileSystem->Close( fh );
	}
}

#endif // _DEBUG

struct SendQueueItem_t
{
	SendQueueItem_t() :
		m_pChannel( NULL ),
		m_Socket( (SOCKET)-1 )
	{
	}

	CNetChan	*m_pChannel;
	SOCKET		m_Socket;
	CUtlBuffer	m_Buffer;
	CUtlBuffer	m_To;
};

struct SendQueue_t
{
	SendQueue_t() : 
		m_nHostFrame( 0 )
	{
	}
	int									m_nHostFrame;
	CUtlLinkedList< SendQueueItem_t >	m_SendQueue;
};

static SendQueue_t g_SendQueue;

int NET_QueuePacketForSend( CNetChan *chan, SOCKET s, const char FAR *buf, int len, const struct sockaddr FAR * to, int tolen, uint32 msecDelay )
{
	// If net_queued_packet_thread was -1 at startup, then we don't even have a thread.
	if ( net_queued_packet_thread.GetInt() && g_pQueuedPackedSender->IsRunning() )
	{
		g_pQueuedPackedSender->QueuePacket( chan, s, buf, len, to, tolen, msecDelay );
	}
	else
	{
		Assert( chan );
		// Set up data structure
		SendQueueItem_t *sq = &g_SendQueue.m_SendQueue[ g_SendQueue.m_SendQueue.AddToTail() ];
		sq->m_Socket = s;
		sq->m_pChannel = chan;
		sq->m_Buffer.Put( (const void *)buf, len );
		sq->m_To.Put( (const void *)to, tolen );
		sq->m_pChannel->IncrementQueuedPackets();
	}
	
	return len;
}

void NET_SendQueuedPacket( SendQueueItem_t *sq )
{
	// Msg( "Send queued packet %d\n", sq->m_Buffer.TellPut() );
	NET_SendTo
	( 
		false, 
		sq->m_Socket, 
		( const char FAR * )sq->m_Buffer.Base(), 
		sq->m_Buffer.TellPut(), 
		( const struct sockaddr FAR * )sq->m_To.Base(), 
		sq->m_To.TellPut()
	);

	sq->m_pChannel->DecrementQueuedPackets();
}

void NET_ClearQueuedPacketsForChannel( INetChannel *channel )
{
	CUtlLinkedList< SendQueueItem_t >& list = g_SendQueue.m_SendQueue;

	for ( unsigned short i = list.Head(); i != list.InvalidIndex();  )
	{
		unsigned short n = list.Next( i );
		SendQueueItem_t &e = list[ i ];
		if ( e.m_pChannel == channel )
		{
			list.Remove( i );
		}
		i = n;
	}
}

void NET_SendQueuedPackets()
{
	// Only do this once per frame
	if ( host_framecount == g_SendQueue.m_nHostFrame )
		return;
	g_SendQueue.m_nHostFrame = host_framecount;

	CUtlLinkedList< SendQueueItem_t >& list = g_SendQueue.m_SendQueue;
	if ( list.IsValidIndex( list.Head() ) )
	{
		SendQueueItem_t *sq = &list[ list.Head() ];
		NET_SendQueuedPacket( sq );
		list.Remove( list.Head() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			s - 
//			buf - 
//			len - 
//			flags - 
//			to - 
//			tolen - 
// Output : int
//-----------------------------------------------------------------------------
static volatile int32 s_SplitPacketSequenceNumber[ MAX_SOCKETS ] = {1};

int NET_SendLong( INetChannel *chan, int sock, SOCKET s, const char FAR * buf, int len, const struct sockaddr FAR * to, int tolen )
{
	CNetChan *netchan = dynamic_cast< CNetChan * >( chan );

	bool bCompress = net_compresssplits.GetBool() && (len > MAX_ROUTABLE_PAYLOAD);
	char* sendbuf = new char[NET_MAX_PAYLOAD];
	int sendlen = len;

	if (bCompress)
	{
		SPLITPACKET_COMPRESSED* compressedHeader = (SPLITPACKET_COMPRESSED*)sendbuf;
		compressedHeader->decompressedSize = len;
		compressedHeader->crc = (int)CRC32_ProcessSingleBuffer(buf, len);

		unsigned int destLen = NET_MAX_PAYLOAD - sizeof(SPLITPACKET_COMPRESSED);
		BZ2_bzBuffToBuffCompress(
			sendbuf + sizeof(SPLITPACKET_COMPRESSED),
			&destLen, (char*)buf, len, 9, 0, 0
		);
		sendlen = destLen+sizeof(SPLITPACKET_COMPRESSED);
	}
	else {
		memcpy(sendbuf, buf, len);
	}

	int nSequenceNumber = ThreadInterlockedIncrement( &s_SplitPacketSequenceNumber[ sock ] );

	char			packet[ MAX_ROUTABLE_PACKET ];
	SPLITPACKET		*pPacket = (SPLITPACKET *)packet;

	// Make pPacket data network endian correct
	pPacket->netID = LittleLong( NET_HEADER_FLAG_SPLITPACKET );
	pPacket->sequenceNumber = LittleLong( nSequenceNumber );
	if (bCompress)
	{
		pPacket->sequenceNumber |= 0x80000000;
	}
	
	int nPacketCount = (sendlen + SPLIT_SIZE - 1) / SPLIT_SIZE;

	int nBytesLeft = sendlen;
	int nPacketNumber = 0;
	int nTotalBytesSent = 0;
	bool bFirstSend = true;

	while ( nBytesLeft > 0 )
	{
		int size = min( SPLIT_SIZE, nBytesLeft );

		pPacket->packetID = LittleShort( (short)(( nPacketNumber << 8 ) + nPacketCount) );
		
		Q_memcpy( packet + sizeof(SPLITPACKET), sendbuf + (nPacketNumber * SPLIT_SIZE), size );
		
		int ret = 0;

		// Setting net_queued_packet_thread to NET_QUEUED_PACKET_THREAD_DEBUG_VALUE goes into a mode where all packets are queued.. can be used to stress-test it.
		// Linux threads aren't prioritized well enough for this to work well (i.e. the queued packet thread doesn't get enough
		// attention to flush itself well). Plus, Linux doesn't have the problem that Windows has where, if you send 6 UDP packets
		// without giving up your timeslice, it'll just discard the 7th and later packets until you Sleep(). Linux doesn't
		// have that problem so it has no need for a queue.
#ifndef _LINUX 
		if ( netchan && (!bFirstSend || net_queued_packet_thread.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE) )
		{
			uint32 delay = (int)( 1000.0f * ( (float)( nPacketNumber * ( SPLIT_SIZE + UDP_HEADER_SIZE ) ) / (float)netchan->GetDataRate() ) + 0.5f );
			ret = NET_QueuePacketForSend( netchan, s, packet, size + sizeof(SPLITPACKET), to, tolen, delay );
		}
		else
#endif
		{
			// Also, we send the first packet no matter what
			// w/o a netchan, if there are too many splits, its possible the packet can't be delivered.  However, this would only apply to out of band stuff like
			//  server query packets, which should never require splitting anyway.
			ret = NET_SendTo( false, s, packet, size + sizeof(SPLITPACKET), to, tolen );
		}

		// First split send
		bFirstSend = false;

		if ( ret < 0 )
		{
			return ret;
		}

		if ( ret >= size )
		{
			nTotalBytesSent += size;
		}

		nBytesLeft -= size;
		++nPacketNumber;

		// Always bitch about split packets in debug
		if ( net_showsplits.GetInt() && net_showsplits.GetInt() != 2 )
		{
			netadr_t adr;
			
			adr.SetFromSockadr( (struct sockaddr*)to );

			if (bCompress)
			{
				Msg("Split packet [compressed] %i/%i, size %i uncompressed %i, dest %s\n",
						nPacketNumber, nPacketCount, size, len, adr.ToString());
			}
			else
			{
				Msg("Split packet [uncompressed] %i/%i, size %i, dest %s\n",
					nPacketNumber, nPacketCount, size, len, adr.ToString());
			}
		}
	}
	delete[] sendbuf;
	return nTotalBytesSent;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			length - 
//			*data - 
//			to - 
// Output : void NET_SendPacket
//-----------------------------------------------------------------------------

void NET_SendPacket ( INetChannel *chan, int sock,  const netadr_t &to, const unsigned char *data, int length )
{
	struct sockaddr	addr;
	int		net_socket;

	if ( net_showudp.GetInt() && (*(unsigned int*)data == CONNECTIONLESS_HEADER) )
	{
		Msg("UDP -> %s: sz=%i OOB '%c'\n", to.ToString(), length, data[4] );
	}

	if ( !NET_IsMultiplayer() )	// Just !NET_IsMultiplayer
	{
		NET_SendLoopPacket (sock, length, data);
		return;
	}

	if ( to.type == NA_BROADCAST || to.type == NA_IP)
	{
		net_socket = net_sockets[sock].hUDP;
	}
	else
	{
		DevMsg("NET_SendPacket: bad address type (%i)\n", to.type );
		return;
	}

	if ( (droppackets.GetInt() < 0)  && sock == NS_CLIENT )
	{
		droppackets.SetValue( droppackets.GetInt() + 1 );
		return;
	}

	if ( fakeloss.GetFloat() > 0.0f )
	{
		// simulate sending this packet
		if (RandomInt(0,100) <= (int)fakeloss.GetFloat())
			return;
	}

	to.ToSockadr ( &addr );

	int nSend;

	// Do we need to break this packet up?
	if ( length <= MAX_ROUTABLE_PACKET && 
		!(net_queued_packet_thread.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE && chan ) )	
	{
		// simple case, small packet, just send it
		nSend = NET_SendTo( true, net_socket, (const char *)data, length, &addr, sizeof(addr) );
	}
	else
	{
		// split packet into smaller pieces
		nSend = NET_SendLong( chan, sock, net_socket, (const char *)data, length, &addr, sizeof(addr) );
	}
	
	if (nSend == -1)
	{
		NET_GetLastError();
		
		// wouldblock is silent
		if ( net_error == WSAEWOULDBLOCK )
			return;

		if ( net_error == WSAECONNRESET )
			return;

		// some PPP links dont allow broadcasts
		if ( ( net_error == WSAEADDRNOTAVAIL) && ( to.type == NA_BROADCAST ) )
			return;

		ConDMsg ("NET_SendPacket Warning: %s : %s\n", NET_ErrorString(net_error), to.ToString(0) );
	}
}

void NET_OutOfBandPrintf(int sock, const netadr_t &adr, const char *format, ...)
{
	va_list		argptr;
	char		string[MAX_ROUTABLE_PAYLOAD];
	
	*(unsigned int*)string = CONNECTIONLESS_HEADER;

	va_start (argptr, format);
	Q_vsnprintf (string+4, sizeof( string ) - 4, format,argptr);
	va_end (argptr);

	int length = Q_strlen(string+4) + 5;

	NET_SendPacket ( NULL, sock, adr, (byte *)string, length );
}

/*
====================
NET_CloseAllSockets
====================
*/
void NET_CloseAllSockets (void)
{
	// shut down any existing and open sockets
	for (int i=0 ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].nPort )
		{
			NET_CloseSocket( net_sockets[i].hUDP );
			NET_CloseSocket( net_sockets[i].hTCP );

			net_sockets[i].nPort = 0;
			net_sockets[i].bListening = false;
			net_sockets[i].hUDP = 0;
			net_sockets[i].hTCP = 0;
		}
	}

	// shut down all pending sockets
	AUTO_LOCK_FM( s_PendingSockets );
	for(int j=0; j<s_PendingSockets.Count();j++ )
	{
		NET_CloseSocket( s_PendingSockets[j].newsock );
	}

	s_PendingSockets.RemoveAll();
}

/*
====================
NET_FlushAllSockets
====================
*/
void NET_FlushAllSockets( void )
{
	// drain any packets that my still lurk in our incoming queue
	char data[2048];
	struct sockaddr	from;
	int	fromlen = sizeof(from);
	
	for (int i=0 ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].hUDP )
		{
			int bytes = 1;

			// loop until no packets are pending anymore
			while ( bytes > 0  )
			{
				bytes = VCRHook_recvfrom( net_sockets[i].hUDP, data, sizeof(data), 0, (struct sockaddr *)&from, (int *)&fromlen );
			}
		}
	}
}

// It seems that this func doesnt exist in src2006
// not deleting, i dont want to mess up with OpenSockets
static void OpenSocketInternal( int nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny )
{
	int port = nSetPort ? nSetPort : nDefaultPort;
	int *handle = NULL;
	if( nProtocol == IPPROTO_TCP )
	{
		handle = &net_sockets[nModule].hTCP;
	}
	else if ( nProtocol == IPPROTO_UDP || nProtocol == IPPROTO_VDP )
	{
		handle = &net_sockets[nModule].hUDP;
	}
	else
	{
		Sys_Error( "Unrecognized protocol type %d", nProtocol );
		return;
	}

	if ( !net_sockets[nModule].nPort )
	{
		*handle = NET_OpenSocket (ipname.GetString(), port, nProtocol );

		if ( !*handle && bTryAny )
		{
			port = PORT_ANY;	// try again with PORT_ANY
			*handle = NET_OpenSocket (ipname.GetString(), port, nProtocol );
		}

		if ( !*handle )
		{
			Sys_Error( "Couldn't allocate any %s IP port", pName );
			return;
		}

		net_sockets[nModule].nPort = port;
	}
	else
	{
		Msg("WARNING: NET_OpenSockets: %s port %i already open.\n", pName, net_sockets[nModule].nPort );
	}
}

/*
====================
NET_OpenSockets
====================
*/
void NET_OpenSockets (void)
{	
	// Xbox 360 uses VDP protocol to combine encrypted game data with clear voice data
	const int nProtocol = X360SecureNetwork() ? IPPROTO_VDP : IPPROTO_UDP;

	OpenSocketInternal( NS_SERVER, hostport.GetInt(), PORT_SERVER, "server", nProtocol, false );
	OpenSocketInternal( NS_CLIENT, clientport.GetInt(), PORT_SERVER, "client", nProtocol, true );

	if ( !net_nohltv )
	{
		OpenSocketInternal( NS_HLTV, hltvport.GetInt(), PORT_HLTV, "hltv", nProtocol, false );
	}

	if ( IsX360() )
	{
		OpenSocketInternal( NS_MATCHMAKING, matchmakingport.GetInt(), PORT_MATCHMAKING, "matchmaking", nProtocol, false );
		OpenSocketInternal( NS_SYSTEMLINK, systemlinkport.GetInt(), PORT_SYSTEMLINK, "systemlink", IPPROTO_UDP, false );
	}
}

int NET_AddExtraSocket( int port )
{
	int newSocket = net_sockets.AddToTail();

	Q_memset( &net_sockets[newSocket], 0, sizeof(netsocket_t) );

	OpenSocketInternal( newSocket, port, PORT_ANY, "extra", IPPROTO_UDP, true );

	net_packets.EnsureCount( newSocket+1 );
	net_splitpackets.EnsureCount( newSocket+1 );

	return newSocket;
}

void NET_RemoveAllExtraSockets()
{
	for (int i=MAX_SOCKETS ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].nPort )
		{
			NET_CloseSocket( net_sockets[i].hUDP );
			NET_CloseSocket( net_sockets[i].hTCP );
		}
	}
	net_sockets.RemoveMultiple( MAX_SOCKETS, net_sockets.Count()-MAX_SOCKETS );

	Assert( net_sockets.Count() == MAX_SOCKETS );
}

unsigned short NET_GetUDPPort(int socket)
{
	if ( socket < 0 || socket >= net_sockets.Count() )
		return 0;

	return net_sockets[socket].nPort;
}


/*
================
NET_GetLocalAddress

Returns the servers' ip address as a string.
================
*/
void NET_GetLocalAddress (void)
{
	net_local_adr.Clear();

	if ( net_noip )
	{
		Msg("TCP/UDP Disabled.\n");
	}
	else
	{
		char	buff[512];

		// If we have changed the ip var from the command line, use that instead.
		if ( Q_strcmp(ipname.GetString(), "localhost") )
		{
			Q_strncpy(buff, ipname.GetString(), sizeof( buff ) );	// use IP set with ipname
		}
		else
		{
			gethostname( buff, sizeof(buff) );	// get own IP address
			buff[sizeof(buff)-1] = 0;			// Ensure that it doesn't overrun the buffer
		}

		NET_StringToAdr (buff, &net_local_adr);

		int ipaddr = ( net_local_adr.ip[0] << 24 ) + 
					 ( net_local_adr.ip[1] << 16 ) + 
					 ( net_local_adr.ip[2] << 8 ) + 
					   net_local_adr.ip[3];

		hostip.SetValue( ipaddr );
	}
}


/*
====================
NET_IsConfigured

Is winsock ip initialized?
====================
*/
bool NET_IsMultiplayer( void )
{
	return net_multiplayer;
}

bool NET_IsDedicated( void )
{
	return net_dedicated;
}

#ifdef _X360
#include "iengine.h"
static FileHandle_t g_fh;
void NET_LogServerStatus( void )
{
	if ( !g_fh )
		return;

	static float fNextTime = 0.f;
	float fCurrentTime = eng->GetCurTime();

	if ( fCurrentTime >= fNextTime )
	{
		fNextTime = fCurrentTime + net_loginterval.GetFloat();
	}
	else
	{
		return;
	}

	AUTO_LOCK_FM( s_NetChannels );
	int numChannels = s_NetChannels.Count();

	if ( numChannels == 0 )
	{
		ConMsg( "No active net channels.\n" );
		return;
	}

	enum
	{
		NET_LATENCY,
		NET_LOSS,
		NET_PACKETS_IN,
		NET_PACKETS_OUT,
		NET_CHOKE_IN,
		NET_CHOKE_OUT,
		NET_FLOW_IN,
		NET_FLOW_OUT,
		NET_TOTAL_IN,
		NET_TOTAL_OUT,
		NET_LAST,
	};
	float fStats[NET_LAST] = {0.f};

	for ( int i = 0; i < numChannels; ++i )
	{
		INetChannel *chan = s_NetChannels[i];
		fStats[NET_LATENCY] += chan->GetAvgLatency(FLOW_OUTGOING);
		fStats[NET_LOSS] += chan->GetAvgLoss(FLOW_INCOMING);
		fStats[NET_PACKETS_IN] += chan->GetAvgPackets(FLOW_INCOMING);
		fStats[NET_PACKETS_OUT] += chan->GetAvgPackets(FLOW_OUTGOING);
		fStats[NET_CHOKE_IN] += chan->GetAvgChoke(FLOW_INCOMING);
		fStats[NET_CHOKE_OUT] += chan->GetAvgChoke(FLOW_OUTGOING);
		fStats[NET_FLOW_IN] += chan->GetAvgData(FLOW_INCOMING);
		fStats[NET_FLOW_OUT] += chan->GetAvgData(FLOW_OUTGOING);
		fStats[NET_TOTAL_IN] += chan->GetTotalData(FLOW_INCOMING);
		fStats[NET_TOTAL_OUT] += chan->GetTotalData(FLOW_OUTGOING);
	}

	for ( int i = 0; i < NET_LAST; ++i )
	{
		fStats[i] /= numChannels;
	}

	const unsigned int size = 128;
	char msg[size];
	Q_snprintf( msg, size, "%.0f,%d,%.0f,%.0f,%.0f,%.1f,%.1f,%.1f,%.1f,%.1f\n", 
				fCurrentTime,
				numChannels,
				fStats[NET_LATENCY],
				fStats[NET_LOSS],
				fStats[NET_PACKETS_IN], 
				fStats[NET_PACKETS_OUT],
				fStats[NET_FLOW_IN]/1024.0f, 
				fStats[NET_FLOW_OUT]/1024.0f,
				fStats[NET_CHOKE_IN],
				fStats[NET_CHOKE_OUT]
			 );

	g_pFileSystem->Write( msg, Q_strlen( msg ), g_fh );
}

void NET_LogServerCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );

	if ( var.GetBool() )
	{
		if ( g_fh )
		{
			g_pFileSystem->Close( g_fh );
			g_fh = 0;
		}

		g_fh = g_pFileSystem->Open( "dump.csv", "wt" );
		if ( !g_fh )
		{
			Msg( "Failed to open log file\n" );
			pConVar->SetValue( 0 );
			return;
		}

		char msg[128];
		Q_snprintf( msg, 128, "Time,Channels,Latency,Loss,Packets In,Packets Out,Flow In(kB/s),Flow Out(kB/s),Choke In,Choke Out\n" );
		g_pFileSystem->Write( msg, Q_strlen( msg ), g_fh );
	}
	else
	{
		if ( g_fh )
		{
			g_pFileSystem->Close( g_fh );
			g_fh = 0;
		}
	}
}
#endif

/*
====================
NET_SetTime

Updates net_time
====================
*/
void NET_SetTime( double realtime )
{
	static double s_last_realtime = 0;

	double frametime = realtime - s_last_realtime;
	s_last_realtime = realtime;

	if ( frametime > 1.0f )
	{
		// if we have very long frame times because of loading stuff
		// don't apply that to net time to avoid unwanted timeouts
		frametime = 1.0f;
	}
	else if ( frametime < 0.0f )
	{
		frametime = 0.0f;
	}

	// adjust network time so fakelag works with host_timescale
	net_time += frametime * host_timescale.GetFloat();
}

/*
====================
NET_RunFrame

RunFrame must be called each system frame before reading/sending on any socket
====================
*/
void NET_RunFrame( double realtime )
{
	NET_SetTime( realtime );

	RCONServer().RunFrame();
	RPTServer().RunFrame();
#ifndef SWDS
	RCONClient().RunFrame();
	RPTClient().RunFrame();
#endif
#ifdef _X360
	if ( net_logserver.GetInt() )
	{
		NET_LogServerStatus();
	}
	g_pMatchmaking->RunFrame();
#endif
	if ( !NET_IsMultiplayer() || net_notcp )
		return;

	// process TCP sockets:
	for ( int i=0; i< net_sockets.Count(); i++ )
	{
		if ( net_sockets[i].hTCP && net_sockets[i].bListening )
		{
			NET_ProcessListen( i );
		}
	}

	NET_ProcessPending();
}

void NET_ClearLoopbackBuffers()
{
	for (int i = 0; i < LOOPBACK_SOCKETS; i++)
	{
		loopback_t *loop;

		while ( s_LoopBacks[i].PopItem( &loop ) )
		{
			if ( loop->data && loop->data != loop->defbuffer )
			{
				delete [] loop->data;
			}
			delete loop;
		}
	}
}

void NET_ConfigLoopbackBuffers( bool bAlloc )
{
	NET_ClearLoopbackBuffers();
}

/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/

void NET_Config ( void )
{
	// free anything
	NET_CloseAllSockets();	// close all UDP/TCP sockets

	net_time = 0.0f;

	// now reconfiguare

	if ( net_multiplayer )
	{	
		// don't allocate loopback buffers
		NET_ConfigLoopbackBuffers( false );

		// get localhost IP address
		NET_GetLocalAddress();

		// reopen sockets if in MP mode
		NET_OpenSockets();

		// setup the rcon server sockets
		if ( net_dedicated || CommandLine()->FindParm( "-usercon" ) )
		{
			netadr_t rconAddr = net_local_adr;
			rconAddr.SetPort( net_sockets[NS_SERVER].nPort );
			RCONServer().SetAddress( rconAddr.ToString() );
			RCONServer().CreateSocket();
		}
	}
	else
	{
		// allocate loopback buffers
		NET_ConfigLoopbackBuffers( true );
	}

	Msg( "Network: IP %s, mode %s, dedicated %s, ports %i SV / %i CL\n", 
		net_local_adr.ToString(true), net_multiplayer?"MP":"SP", net_dedicated?"Yes":"No", 
		net_sockets[NS_SERVER].nPort, net_sockets[NS_CLIENT].nPort );
}

/*
====================
NET_SetDedicated

A single player game will only use the loopback code
====================
*/

inline void NET_SetDedicated()
{
	if ( net_noip )
	{
		Msg( "Warning! Dedicated not possible with -noip parameter.\n");
		return;		
	}

	net_dedicated = true;
}

void NET_ListenSocket( int sock, bool bListen )
{
	Assert( (sock >= 0) && (sock < net_sockets.Count()) );

	netsocket_t * netsock = &net_sockets[sock];

	if ( netsock->hTCP )
	{
		NET_CloseSocket( netsock->hTCP, sock );
	}

	if (!NET_IsMultiplayer() || net_notcp)
		return;

	if ( bListen )
	{
		const char * net_interface = ipname.GetString();

		netsock->hTCP = NET_OpenSocket( net_interface, netsock->nPort, true );

		if ( !netsock->hTCP )
		{
			Msg( "Warning! NET_ListenSocket failed opening socket %i, port %i.\n", sock, net_sockets[sock].nPort );
			return;
		}

		struct sockaddr_in	address;

		if (!net_interface || !net_interface[0] || !Q_strcmp(net_interface, "localhost"))
		{
			address.sin_addr.s_addr = INADDR_ANY;
		}
		else
		{
			NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);
		}

		address.sin_family = AF_INET;
		address.sin_port = NET_HostToNetShort((short)( netsock->nPort ));

		int ret;
		VCR_NONPLAYBACKFN( bind( netsock->hTCP, (struct sockaddr *)&address, sizeof(address)), ret, "bind" );
		if ( ret == -1 )
		{
			NET_GetLastError();
			Msg ("WARNING: NET_ListenSocket bind failed on socket %i, port %i.\n", netsock->hTCP, netsock->nPort );
			return;
		}

		VCR_NONPLAYBACKFN( listen( netsock->hTCP, TCP_MAX_ACCEPTS), ret, "listen" );
		if ( ret == -1 )
		{
			NET_GetLastError();
			Msg ("WARNING: NET_ListenSocket listen failed on socket %i, port %i.\n", netsock->hTCP, netsock->nPort );
			return;
		}

		netsock->bListening = true;
	}
}

void NET_SetMutiplayer(bool multiplayer)
{
	if ( net_noip && multiplayer )
	{
		Msg( "Warning! Multiplayer mode not available with -noip parameter.\n");
		return;		
	}

	if ( net_dedicated && !multiplayer )
	{
		Msg( "Warning! Singleplayer mode not available on dedicated server.\n");
		return;		
	}

	// reconfigure if changed
	if ( net_multiplayer != multiplayer )
	{
		net_multiplayer = multiplayer;
		NET_Config();
	}

	// clear loopback buffer in single player mode
	if ( !multiplayer )
	{
		NET_ClearLoopbackBuffers();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bIsDedicated - 
//-----------------------------------------------------------------------------
void NET_Init( bool bIsDedicated )
{
#ifndef _LINUX
	if ( CommandLine()->FindParm( "-NoQueuedPacketThread" ) )
		Warning( "Found -NoQueuedPacketThread, so no queued packet thread will be created.\n" );
	else
		g_pQueuedPackedSender->Setup();
#endif

	if (CommandLine()->FindParm("-nodns"))
	{
		net_nodns = true;
	}

	if (CommandLine()->FindParm("-usetcp"))
	{
		net_notcp = false;
	}

	if (CommandLine()->FindParm("-nohltv"))
	{
		net_nohltv = true;
	}

	if (CommandLine()->FindParm("-noip"))
	{
		net_noip = true;
	}
	else
	{
#if defined(_WIN32)

#if defined(_X360)
		XNetStartupParams xnsp;
		memset( &xnsp, 0, sizeof( xnsp ) );
		xnsp.cfgSizeOfStruct = sizeof( XNetStartupParams );
		if ( X360SecureNetwork() )
		{
			Msg( "Xbox 360 network is Secure\n" );
		}
		else
		{
			// Allow cross-platform communication
			xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
			Msg( "Xbox 360 network is Unsecure\n" );
		}

		net_reduce_payloadsize.SetValue( true );

		INT err = XNetStartup( &xnsp );
		if ( err )
		{
			ConMsg( "Error! Failed to set XNET Security Bypass.\n");
		}
		err = XOnlineStartup();
		if ( err != ERROR_SUCCESS )
		{
			ConMsg( "Error! XOnlineStartup failed.\n");
		}
#else
		// initialize winsock 2.0
		WSAData wsaData;
		if ( WSAStartup( MAKEWORD(2,0), &wsaData ) != 0 )
		{
			ConMsg( "Error! Failed to load network socket library.\n");
			net_noip = true;
		}
#endif	// _X360
#endif	// _WIN32
	}

	Assert( SVC_LASTMSG < (1<<NETMSG_TYPE_BITS) );
	Assert( NET_MAX_PAYLOAD < (1<<NET_MAX_PALYLOAD_BITS) );
	Assert( MAX_FILE_SIZE < (1<<MAX_FILE_SIZE_BITS) );

	net_time = 0.0f;

	
	int hPort = CommandLine()->ParmValue( "-port", -1 );
	if ( hPort == -1 )
	{
		hPort = CommandLine()->ParmValue( "+port", -1 ); // check if they used +port by mistake
	}

	if ( hPort != -1 )
	{
		hostport.SetValue( hPort );
	}

	// clear static stuff
	net_sockets.EnsureCount( MAX_SOCKETS );
	net_packets.EnsureCount( MAX_SOCKETS );
	net_splitpackets.EnsureCount( MAX_SOCKETS );

	for ( int i = 0; i < MAX_SOCKETS; ++i )
	{
		s_pLagData[i] = NULL;
		Q_memset( &net_sockets[i], 0, sizeof(netsocket_t) );
	}

	const char *ip = CommandLine()->ParmValue( "-ip" );

	if ( ip ) // if they had a command line option for IP
	{
		ipname.SetValue( ip );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}

	if ( bIsDedicated )
	{
		// set dedicated MP mode
		NET_SetDedicated();
	}
	else
	{
		// set SP mode
		NET_ConfigLoopbackBuffers( true );
	}
}

/*
====================
NET_Shutdown

====================
*/
void NET_Shutdown (void)
{
	int nError = 0;

	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		NET_ClearLaggedList( &s_pLagData[i] );
	}

	g_pQueuedPackedSender->Shutdown();

	net_multiplayer = false;
	net_dedicated = false;

	NET_CloseAllSockets();
	NET_ConfigLoopbackBuffers( false );

#if defined(_WIN32)
	if ( !net_noip )
	{
		nError = WSACleanup();
		if ( nError )
		{
			Msg("Failed to complete WSACleanup = 0x%x.\n", nError );
		}
#if defined(_X360)
		nError = XOnlineCleanup();
		if ( nError != ERROR_SUCCESS )
		{
			Msg( "Warning! Failed to complete XOnlineCleanup = 0x%x.\n", nError );
		}
#endif	// _X360
	}
#endif	// _WIN32

	Assert( s_NetChannels.Count() == 0 );
	Assert( s_PendingSockets.Count() == 0);
}

void NET_PrintChannelStatus( INetChannel * chan )
{
	Msg( "NetChannel '%s':\n", chan->GetName() );
	Msg( "- remote IP: %s\n", chan->GetAddress(), chan->IsPlayback()?"(Demo`)":"" );
	Msg( "- online: %s\n", COM_FormatSeconds( chan->GetTimeConnected() ) );
	Msg( "- reliable: %s\n", chan->HasPendingReliableData()?"pending data":"available" );
	Msg( "- latency: %.1f, loss %.2f\n", chan->GetAvgLatency(FLOW_OUTGOING), chan->GetAvgLoss(FLOW_INCOMING) );
	Msg( "- packets: in %.1f/s, out %.1f/s\n", chan->GetAvgPackets(FLOW_INCOMING), chan->GetAvgPackets(FLOW_OUTGOING) );
	Msg( "- choke: in %.2f, out %.2f\n", chan->GetAvgChoke(FLOW_INCOMING), chan->GetAvgChoke(FLOW_OUTGOING) );
	Msg( "- flow: in %.1f, out %.1f kB/s\n", chan->GetAvgData(FLOW_INCOMING)/1024.0f, chan->GetAvgData(FLOW_OUTGOING)/1024.0f );
	Msg( "- total: in %.1f, out %.1f MB\n\n", (float)chan->GetTotalData(FLOW_INCOMING)/(1024*1024), (float)chan->GetTotalData(FLOW_OUTGOING)/(1024*1024) );
}

CON_COMMAND( net_channels, "Shows net channel info" )
{
	int numChannels = s_NetChannels.Count();

	if ( numChannels == 0 )
	{
		ConMsg( "No active net channels.\n" );
		return;
	}

	AUTO_LOCK_FM( s_NetChannels );
	for ( int i = 0; i < numChannels; i++ )
	{
		NET_PrintChannelStatus( s_NetChannels[i] );
	}
}

CON_COMMAND( net_start, "Inits multiplayer network sockets" )
{
	net_multiplayer = true;
	NET_Config();
}

CON_COMMAND( net_status, "Shows current network status" )
{
	AUTO_LOCK_FM( s_NetChannels );
	int numChannels = s_NetChannels.Count();

	ConMsg("Net status for host %s:\n", 
		net_local_adr.ToString(true) );

	ConMsg("- Config: %s, %s, %i connections\n",
		net_multiplayer?"Multiplayer":"Singleplayer",
		net_dedicated?"dedicated":"listen",
		numChannels	);
		

	ConMsg("- Ports: Client %u, Server %u, HLTV %u, Matchmaking %u, Systemlink %u\n",
		net_sockets[NS_CLIENT].nPort,
		net_sockets[NS_SERVER].nPort, 
		net_sockets[NS_HLTV].nPort, 
		net_sockets[NS_MATCHMAKING].nPort, 
		net_sockets[NS_SYSTEMLINK].nPort ); 

	if ( numChannels <= 0 )
	{
		return;
	}

	// gather statistics:

	float avgLatencyOut = 0;
	float avgLatencyIn = 0;
	float avgPacketsOut = 0;
	float avgPacketsIn = 0;
	float avgLossOut = 0;
	float avgLossIn = 0;
	float avgDataOut = 0;
	float avgDataIn = 0;

	for ( int i = 0; i < numChannels; i++ )
	{
		CNetChan *chan = s_NetChannels[i];

		avgLatencyOut += chan->GetAvgLatency(FLOW_OUTGOING);
		avgLatencyIn += chan->GetAvgLatency(FLOW_INCOMING);

		avgLossIn += chan->GetAvgLoss(FLOW_INCOMING);
		avgLossOut += chan->GetAvgLoss(FLOW_OUTGOING);

		avgPacketsIn += chan->GetAvgPackets(FLOW_INCOMING);
		avgPacketsOut += chan->GetAvgPackets(FLOW_OUTGOING);
		
		avgDataIn += chan->GetAvgData(FLOW_INCOMING);
		avgDataOut += chan->GetAvgData(FLOW_OUTGOING);
	}

	ConMsg( "- Latency: avg out %.2fs, in %.2fs\n",  avgLatencyOut/numChannels, avgLatencyIn/numChannels );
 	ConMsg( "- Loss:    avg out %.1f, in %.1f\n", avgLossOut/numChannels, avgLossIn/numChannels );
	ConMsg( "- Packets: net total out  %.1f/s, in %.1f/s\n", avgPacketsOut, avgPacketsIn );
	ConMsg( "           per client out %.1f/s, in %.1f/s\n", avgPacketsOut/numChannels, avgPacketsIn/numChannels );
	ConMsg( "- Data:    net total out  %.1f, in %.1f kB/s\n", avgDataOut/1024.0f, avgDataIn/1024.0f );
	ConMsg( "           per client out %.1f, in %.1f kB/s\n", (avgDataOut/numChannels)/1024.0f, (avgDataIn/numChannels)/1024.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Generic buffer compression from source into dest (BZip2)
// Input  :	
//			*dest - 
//			*destLen - 
//			*source - 
//			sourceLen - 
// 
// Output : int
// Note : didnt exist in src2006!!!
//-----------------------------------------------------------------------------
bool NET_BufferToBufferCompress(char* dest, unsigned int* destLen, char* source, unsigned int sourceLen)
{
	Assert(dest);
	Assert(destLen);
	Assert(source);

	unsigned int outLen = *destLen;

	int ret = BZ2_bzBuffToBuffCompress(
		dest,              // output buffer
		&outLen,           // in: max size, out: actual size
		source,            // input buffer
		sourceLen,         // input size
		9,                 // blockSize100k (9 = max compression)
		0,                 // verbosity
		30                 // workFactor (0x1Eu = default)
	);

	if (ret != BZ_OK)
	{
		// fallback — sending non compressed
		if (*destLen >= sourceLen)
		{
			Q_memcpy(dest, source, sourceLen);
			*destLen = sourceLen;
		}

		DevMsg("BZ2_bzBuffToBuffCompress error code = %i\n", ret);
		return false;
	}

	*destLen = outLen;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Generic buffer decompression from source into dest (BZip2)
// Input  : 
//			*dest - 
//			*destLen - 
//			*source - 
//			sourceLen - 
// 
// Output : int
// Note : didnt exist in src2006!!!
//-----------------------------------------------------------------------------
bool NET_BufferToBufferDecompress(char* dest, unsigned int* destLen, char* source, unsigned int sourceLen)
{
	Assert(dest);
	Assert(destLen);
	Assert(source);

	unsigned int outLen = *destLen;

	int ret = BZ2_bzBuffToBuffDecompress(
		dest,              // output buffer
		&outLen,           // in: max size, out: actual size
		source,            // compressed data (BZh...)
		sourceLen,         // compressed size
		0,                 // small (0 = normal mode)
		0                  // verbosity
	);

	if (ret != BZ_OK)
	{
		DevMsg("BZ2_bzBuffToBuffDecompress error code = %i\n", ret);
		return false;
	}

	*destLen = outLen;
	return true;
}
