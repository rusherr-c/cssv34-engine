//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined( PROTO_VERSION_H )
#define PROTO_VERSION_H
#ifdef _WIN32
#pragma once
#endif

// The current network protocol version.  Changing this makes clients and servers incompatible
#define PROTOCOL_VERSION    7

#define DEMO_BACKWARDCOMPATABILITY

// For backward compatability of demo files
#define PROTOCOL_VERSION_13		13
#define PROTOCOL_VERSION_12		12
#define PROTOCOL_VERSION_11		11
#define PROTOCOL_VERSION_10		10
#define PROTOCOL_VERSION_9		9
#define PROTOCOL_VERSION_8		8
#define PROTOCOL_VERSION_7		7
#define PROTOCOL_VERSION_6		6

#endif