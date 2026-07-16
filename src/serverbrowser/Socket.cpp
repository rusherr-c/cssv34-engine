//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#if !defined( _X360 )
#define FD_SETSIZE 1024
#endif

#include <assert.h>
#include "winlite.h"
#if !defined( _X360 )
#include "winsock.h"
#else
#include "winsockx.h"
#endif
#include "socket.h"
#include "tier0/vcrmode.h"

#include <VGUI/IVGui.h>

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: Default message handler for received messages
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMsgHandler::Process( const netadr_t &from, bf_read &msg )
{
	// Swallow message by default
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a non-blocking, broadcast capable, UDP socket.  If port is
//  specified, binds it to listen on that port, otherwise, chooses a random port.
//-----------------------------------------------------------------------------
CSocket::CSocket(uint16 port)
{
	m_hSocket = INVALID_SOCKET;

	Open(port);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSocket::~CSocket()
{
	Close();
}

bool CSocket::IsValid() const
{
	return m_hSocket != INVALID_SOCKET;
}

//-----------------------------------------------------------------------------
// Purpose: Open socket on given port
// Input  : port -
// 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSocket::Open(uint16 port)
{
	Close();

	m_hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (m_hSocket == INVALID_SOCKET)
		return false;

	BOOL broadcast = TRUE;

	setsockopt(
		m_hSocket,
		SOL_SOCKET,
		SO_BROADCAST,
		(char*)&broadcast,
		sizeof(broadcast));

	u_long mode = 1;

	ioctlsocket(
		m_hSocket,
		FIONBIO,
		&mode);

	sockaddr_in addr{};

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(
		m_hSocket,
		(sockaddr*)&addr,
		sizeof(addr)) == SOCKET_ERROR)
	{
		Close();
		return false;
	}

	sockaddr_in local{};
	int len = sizeof(local);

	getsockname(
		m_hSocket,
		(sockaddr*)&local,
		&len);

	m_Address.SetFromSockadr(
		(sockaddr*)&local);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Close this socket
//-----------------------------------------------------------------------------
void CSocket::Close()
{
	if (m_hSocket != INVALID_SOCKET)
	{
		closesocket(m_hSocket);
		m_hSocket = INVALID_SOCKET;
	}

	m_Address.Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Send message to specified address
// Input  : to - 
//			data -
//			length -
// 
// Output : int - number of bytes sent
//-----------------------------------------------------------------------------
int CSocket::Send(
	const netadr_t& to,
	const void* data,
	int length)
{
	sockaddr addr{};

	to.ToSockadr(&addr);

	return sendto(
		m_hSocket,
		(const char*)data,
		length,
		0,
		&addr,
		sizeof(sockaddr_in))
		!= SOCKET_ERROR;
}

//-----------------------------------------------------------------------------
// Purpose: Send message to specified address
// Input  : to - 
//			msg -
// 
// Output : int - number of bytes sent
//-----------------------------------------------------------------------------
int CSocket::Send(const netadr_t& to, bf_write& msg)
{
	return Send(
		to,
		msg.GetData(),
		msg.GetNumBytesWritten());
}

//-----------------------------------------------------------------------------
// Purpose: Send broadcast message on specified port
// Input  : port - 
//			data -
//			length -
// 
// Output : int - number of bytes sent
//-----------------------------------------------------------------------------
int CSocket::Broadcast(
	uint16 port,
	const void* data,
	int length)
{
	sockaddr_in addr{};

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_BROADCAST;

	return sendto(
		m_hSocket,
		(const char*)data,
		length,
		0,
		(sockaddr*)&addr,
		sizeof(addr))
		!= SOCKET_ERROR;
}

//-----------------------------------------------------------------------------
// Purpose: Send broadcast message on specified port
// Input  : port - 
//			msg - 
// 
// Output : int - number of bytes sent
//-----------------------------------------------------------------------------
int CSocket::Broadcast(uint16 port, bf_write& msg)
{
	return Broadcast(
		port,
		msg.GetData(),
		msg.GetNumBytesWritten());
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame (outside of the socket thread) to allow socket to receive incoming messages
//  and route them as appropriate
//-----------------------------------------------------------------------------
void CSocket::Frame()
{
	if (!IsValid())
		return;

	byte buffer[65536];

	while (true)
	{
		sockaddr_in from{};
		int fromlen = sizeof(from);

		int bytes =
			recvfrom(
				m_hSocket,
				(char*)buffer,
				sizeof(buffer),
				0,
				(sockaddr*)&from,
				&fromlen);

		if (bytes == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				break;

			return;
		}

		if (bytes <= 0)
			break;

		netadr_t adr;
		adr.SetFromSockadr((sockaddr*)&from);

		bf_read msg(
			buffer,
			bytes);

		for (int i = 0; i < m_Handlers.Count(); i++)
		{
			if (m_Handlers[i]->Process(
				adr,
				msg))
			{
				break;
			}

			msg.Seek(0);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add hander to head of chain
// Input  : *handler - 
//-----------------------------------------------------------------------------
void CSocket::AddHandler(CMsgHandler* h)
{
	if (!m_Handlers.HasElement(h))
		m_Handlers.AddToTail(h);
}

//-----------------------------------------------------------------------------
// Purpose: Removed indicated handler
// Input  : *handler - 
//-----------------------------------------------------------------------------
void CSocket::RemoveHandler(CMsgHandler* h)
{
	m_Handlers.FindAndRemove(h);
}

//-----------------------------------------------------------------------------
// Purpose: Get socket
// Output : SOCKET
//-----------------------------------------------------------------------------
SOCKET CSocket::GetSocket(void) const
{
	return m_hSocket;
}

//-----------------------------------------------------------------------------
// Purpose: Resolves the socket address
// Output : const netadr_t
//-----------------------------------------------------------------------------
const netadr_t& CSocket::GetAddress() const
{
	return m_Address;
}