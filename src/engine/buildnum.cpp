//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "quakedef.h"
#include "winlite.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

//char *date = "Nov 07 1998"; // "Oct 24 1996";
char *date = __DATE__ ;

char *mon[12] = 
{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char mond[12] = 
{ 31,    28,    31,    30,    31,    30,    31,    31,    30,    31,    30,    31 };

class CBuildInfo
{
public:
	CBuildInfo()
	{
		Init();
	}

	~CBuildInfo()
	{

	}

	// returns days since Nov 07 1998
	int GetBuildNumber()
	{
		return m_nBuildNumber;
	}

	uint32 GetBuildTimestamp()
	{
		return m_nTimestamp;
	}

	void GetHexTimestamp(char* out)
	{
		sprintf(out, "%x", m_nTimestamp);
	}

private:
	void Init()
	{
		auto base = (BYTE*)&__ImageBase;

		auto dos = (PIMAGE_DOS_HEADER)base;
		auto nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);

		m_nTimestamp = nt->FileHeader.TimeDateStamp;
		m_nBuildNumber = m_nTimestamp / 86400;

		//m_nBuildNumber -= 34995; // Oct 24 1996
		m_nBuildNumber -= 35739;  // Nov 7 1998 (HL1 Gold Date)
	}

	int			m_nBuildNumber;
	uint32		m_nTimestamp;
};

// Singleton
static CBuildInfo s_buildInfo;
CBuildInfo* g_pBuildInfo = &s_buildInfo;

///////////////////////////////////////////////////////////////////////////////
///////// New style build number implementation using timestamp. //////////////
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// Purpose: Get timestamp from nt header
// Output : unsigned int
//-----------------------------------------------------------------------------
uint32 build_timestamp()
{
	auto base = (BYTE*)&__ImageBase;

	auto dos = (PIMAGE_DOS_HEADER)base;
	auto nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);

	uint32 timestamp = nt->FileHeader.TimeDateStamp;

	return timestamp;
}

//-----------------------------------------------------------------------------
// Purpose: Convert timestamp(build number) to hex string.
// Output : char*
//-----------------------------------------------------------------------------
char* build_hex()
{
	char buffer[256];

	uint32 timestamp = build_timestamp();

	sprintf(buffer, "%x", timestamp);

	return buffer;
}