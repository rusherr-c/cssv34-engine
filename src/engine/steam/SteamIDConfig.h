/*
 *
 * Copyright (c) 2026 RuSHeRR
 *
 * Purpose: Steam ID Helper
 *	for shmelle's RevEmu
 *
*/

#ifndef STEAMID_CONFIG_H_
#define STEAMID_CONFIG_H_
#include "checksum_crc.h"
#include "strtools.h"
#include "steam/steamclientpublic.h"
#include "baseclientstate.h"

extern bool g_bIsESTEAMATiON;
extern ConVar steam_gen;
extern ConVar steam_uid;
extern ConVar steam_special;
extern ConVar steam_new;

// Special class that generates SteamID
class SteamIDConfig {
public:
	SteamIDConfig();
	~SteamIDConfig();

	// Calls SteamUser()->InitiateGameConnection
	int	CreateOriginalTicket(void* pData, CSteamID sid = 0ull, uint32 ip = 0u, uint16 port = 0u, bool secure = false);
	
	// Generates ticket for given gen
	int	CreateTicket(void* pData, int gen = 4, int special = 0);

private:
	int steamID;
	int Ticket;
};

#endif // STEAMID_CONFIG_H_