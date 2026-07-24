#include "tier0/dbg.h"
#include "convar.h"
#include "color.h"
#include "steamCommon.h"
#include "steam/steamclientpublic.h"
#include "steam/steam_api.h"
#include "SteamIDConfig.h"
#include "RevEmu.h"
#include "ExternIP.h"

#include "sys_dll.h"
#include <ctime>

#define SIDCVARS_FLAGS 0 //FCVAR_DEVELOPMENTONLY

static bool g_bSidCfg_FirstStart = true;
extern int g_iSteamAppID;
bool g_bIsESTEAMATiON = true;
static Color SteamIDCfg_LogColor(100, 255, 100, 255);

ConVar steam_gen("steam_gen", "4", SIDCVARS_FLAGS, "Sets steam gen (development only, 0 = use default)");
ConVar steam_uid("steam_uid", "0", SIDCVARS_FLAGS, "Sets custom steam id (development only, 0 = use default)");
ConVar steam_special("steam_special", "0", SIDCVARS_FLAGS, "Special number");
ConVar steam_new("steam_new", "0", SIDCVARS_FLAGS, "Indicates to use new steam id instance or not");

/*
* Get account id
* 
* @output       Account ID as an integer.
*/

int get_accountid()
{
	DWORD volumeSerial = 0;
	DWORD maxComponentLen = 0;

	GetVolumeInformationA(
		"C:\\",
		nullptr,
		0,
		&volumeSerial,
		&maxComponentLen,
		&maxComponentLen,
		nullptr,
		0
	);

	char id[16];
	sprintf_s(id, sizeof(id), "%u", volumeSerial);

	uint32_t hash = 1315423911u;

	for (int i = 0; id[i] != '\0'; i++)
	{
		uint8_t c = static_cast<uint8_t>(id[i]);
		hash ^= (hash >> 2) + 32u * hash + c;
	}

	return static_cast<int>(hash);
}

SteamIDConfig::SteamIDConfig() : steamID(0) {

	if (g_bSidCfg_FirstStart) {
		srand((unsigned)_time64(0));
		
		steamID = get_accountid();

		g_bSidCfg_FirstStart = false;

		ConColorMsg(SteamIDCfg_LogColor, "[SteamIDConfig] ");
		Msg("Using SteamID: STEAM_0:0:%i\n", steamID); 
	}
	else {
		steamID = get_accountid();
	}
}

SteamIDConfig::~SteamIDConfig() {
	steamID = 0;
}

int SteamIDConfig::CreateOriginalTicket(void* pData, CSteamID sid, uint32 ip, uint16 port, bool secure) {

	Msg("[SteamIDConfig] Creating ticket via SteamUser()\n");
	Ticket = RevGenerator::GenerateRevEmu4(pData, steamID, 0);
	if (SteamUser())
		Ticket = SteamUser()->InitiateGameConnection(pData, 2048, sid, CGameID(g_iSteamAppID), ntohl(ip), ntohs(port), secure);

	auto pTicket = (int*)pData;
	
	ConColorMsg(SteamIDCfg_LogColor, "[SteamIDConfig] ");
	Msg("SteamID: %i\n", (pTicket[1]));

	return Ticket;
}

int SteamIDConfig::CreateTicket(void* pData, int gen, int special) {
	int nGen = steam_gen.GetInt() ? steam_gen.GetInt() : gen;
	int nSpecial = steam_special.GetInt() ? steam_special.GetInt() : special;
	int nSteamID = steam_uid.GetInt() ? steam_uid.GetInt() : steamID;

	if (steam_new.GetBool())
		if (!(nSteamID & 1))
			nSteamID -= 1;

	if (nGen == 1)
		Ticket = RevGenerator::GenerateRevEmu1(pData, nSteamID, nSpecial);
	if (nGen == 2)
		Ticket = RevGenerator::GenerateRevEmu2(pData, nSteamID);
	if (nGen == 3)
		Ticket = RevGenerator::GenerateRevEmu3(pData, nSteamID, nSpecial);
	if (nGen == 4)
		Ticket = RevGenerator::GenerateRevEmu4(pData, nSteamID, nSpecial);

	ConColorMsg(SteamIDCfg_LogColor, "[SteamIDConfig] ");
	Msg("Generated: gen %i, special %i, uid %i, ticket %i\n", nGen, nSpecial, nSteamID, Ticket);

	return Ticket;
}