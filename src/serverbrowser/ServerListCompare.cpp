//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

//-----------------------------------------------------------------------------
// Purpose: List password column sort function
//-----------------------------------------------------------------------------
int __cdecl PasswordCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	serveritem_t* s1 = &ServerBrowserDialog().GetServer(p1.userData);
	serveritem_t* s2 = &ServerBrowserDialog().GetServer(p2.userData);

	if (!s1 && s2)
		return -1;
	if (!s2 && s1)
		return 1;
	if (!s1 && !s2)
		return 0;

	if (s1->password < s2->password)
		return 1;
	else if (s1->password > s2->password)
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
int __cdecl BotsCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	serveritem_t* s1 = &ServerBrowserDialog().GetServer(p1.userData);
	serveritem_t* s2 = &ServerBrowserDialog().GetServer(p2.userData);

	if (!s1 && s2)
		return -1;
	if (!s2 && s1)
		return 1;
	if (!s1 && !s2)
		return 0;

	if (s1->botPlayers < s2->botPlayers)
		return 1;
	else if (s1->botPlayers > s2->botPlayers)
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
int __cdecl SecureCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
int __cdecl IPAddressCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	serveritem_t* s1 = &ServerBrowserDialog().GetServer(p1.userData);
	serveritem_t* s2 = &ServerBrowserDialog().GetServer(p2.userData);

	if (!s1 && s2)
		return -1;
	if (!s2 && s1)
		return 1;
	if (!s1 && !s2)
		return 0;

	if (s1->ip < s2->ip)
		return -1;
	else if (s2->ip < s1->ip)
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Ping comparison function
//-----------------------------------------------------------------------------
int __cdecl PingCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	serveritem_t* s1 = &ServerBrowserDialog().GetServer(p1.userData);
	serveritem_t* s2 = &ServerBrowserDialog().GetServer(p2.userData);

	if (!s1 && s2)
		return -1;
	if (!s2 && s1)
		return 1;
	if (!s1 && !s2)
		return 0;

	int ping1 = s1->ping;
	int ping2 = s2->ping;

	if (ping1 < ping2)
		return -1;
	else if (ping1 > ping2)
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Map comparison function
//-----------------------------------------------------------------------------
int __cdecl MapCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	serveritem_t* s1 = &ServerBrowserDialog().GetServer(p1.userData);
	serveritem_t* s2 = &ServerBrowserDialog().GetServer(p2.userData);

	if (!s1 && s2)
		return -1;
	if (!s2 && s1)
		return 1;
	if (!s1 && !s2)
		return 0;

	return Q_stricmp(s1->map, s2->map);
}

//-----------------------------------------------------------------------------
// Purpose: Game dir comparison function
//-----------------------------------------------------------------------------
int __cdecl GameCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	serveritem_t* s1 = &ServerBrowserDialog().GetServer(p1.userData);
	serveritem_t* s2 = &ServerBrowserDialog().GetServer(p2.userData);

	if (!s1 && s2)
		return -1;
	if (!s2 && s1)
		return 1;
	if (!s1 && !s2)
		return 0;

	// make sure we haven't added the same server to the list twice
	Assert(p1.userData != p2.userData);

	return Q_stricmp(s1->gameDescription, s2->gameDescription);
}

//-----------------------------------------------------------------------------
// Purpose: Server name comparison function
//-----------------------------------------------------------------------------
int __cdecl ServerNameCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	serveritem_t* s1 = &ServerBrowserDialog().GetServer(p1.userData);
	serveritem_t* s2 = &ServerBrowserDialog().GetServer(p2.userData);

	if (!s1 && s2)
		return -1;
	if (!s2 && s1)
		return 1;
	if (!s1 && !s2)
		return 0;

	return Q_stricmp(s1->name, s2->name);
}

//-----------------------------------------------------------------------------
// Purpose: Player number comparison function
//-----------------------------------------------------------------------------
int __cdecl PlayersCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	serveritem_t* s1 = &ServerBrowserDialog().GetServer(p1.userData);
	serveritem_t* s2 = &ServerBrowserDialog().GetServer(p2.userData);

	if (!s1 && s2)
		return -1;
	if (!s2 && s1)
		return 1;
	if (!s1 && !s2)
		return 0;

	int s1p = max(0, s1->players - s1->botPlayers);
	int s1m = max(0, s1->maxPlayers - s1->botPlayers);
	int s2p = max(0, s2->players - s2->botPlayers);
	int s2m = max(0, s2->maxPlayers - s2->botPlayers);

	// compare number of players
	if (s1p > s2p)
		return -1;
	if (s1p < s2p)
		return 1;

	// compare max players if number of current players is equal
	if (s1m > s2m)
		return -1;
	if (s1m < s2m)
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Player number comparison function
//-----------------------------------------------------------------------------
int __cdecl LastPlayedCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Tag comparison function
//-----------------------------------------------------------------------------
int __cdecl TagsCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
	return GameCompare(pPanel, p1, p2); // Tags are not supported!
}

