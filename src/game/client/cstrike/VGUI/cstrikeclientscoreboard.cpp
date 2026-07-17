//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "CstrikeClientScoreBoard.h"
#include "c_team.h"
#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include "backgroundpanel.h"

#include <KeyValues.h>

#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVgui.h>
#include <vgui_controls/SectionedListPanel.h>

#include "voice_status.h"

using namespace vgui;

enum EScoreboardSections
{
	SCORESECTION_TERRORIST = 1,
	SCORESECTION_CT,
	SCORESECTION_SPECTATOR
};

static char s_szServerName[256] = "";

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSClientScoreBoardDialog::CCSClientScoreBoardDialog(IViewPort* pViewPort) : CClientScoreBoardDialog(pViewPort)
{
	Panel* control = FindChildByName("ServerName");
	if (control)
	{
		PostMessage(control, new KeyValues("SetText", "text", s_szServerName));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSClientScoreBoardDialog::~CCSClientScoreBoardDialog()
{
}

void CCSClientScoreBoardDialog::FireGameEvent(IGameEvent* event)
{
	BaseClass::FireGameEvent(event);

	const char* type = event->GetName();

	if (Q_strcmp(type, "server_spawn") == 0)
	{
		Q_strncpy(s_szServerName, event->GetString("hostname"), sizeof(s_szServerName));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Paint background for rounded corners
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::PaintBackground()
{
	int wide, tall;
	GetSize(wide, tall);

	DrawRoundedBackground(m_bgColor, wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: Paint border for rounded corners
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::PaintBorder()
{
	int wide, tall;
	GetSize(wide, tall);

	DrawRoundedBorder(m_borderColor, wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: Apply scheme settings
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_bgColor = GetSchemeColor("SectionedListPanel.BgColor", GetBgColor(), pScheme);
	m_borderColor = pScheme->GetColor("FgColor", Color(0, 0, 0, 0));

	SetBgColor(m_bgColor);
	SetBorder(pScheme->GetBorder("BaseBorder"));
}

//-----------------------------------------------------------------------------
// Purpose: sets up base sections
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::InitScoreboardSections()
{
	m_pPlayerList->SetBgColor(Color(0, 0, 0, 0));
	m_pPlayerList->SetBorder(NULL);

	// fill out the structure of the scoreboard
	AddHeader();

	// add the team sections
	AddSection(TYPE_TEAM, TEAM_TERRORIST);
	AddSection(TYPE_TEAM, TEAM_CT);
	AddSection(TYPE_TEAM, TEAM_SPECTATOR);
}

//-----------------------------------------------------------------------------
// Purpose: Used for sorting players
//-----------------------------------------------------------------------------
bool CCSClientScoreBoardDialog::CSPlayerSortFunc(KeyValues* it1, KeyValues* it2)
{
	Assert(it1 && it2);

	// first compare score
	int v1 = it1->GetInt("frags");
	int v2 = it2->GetInt("frags");
	if (v1 > v2)
		return true;
	else if (v1 < v2)
		return false;

	// second compare deaths
	v1 = it1->GetInt("deaths");
	v2 = it2->GetInt("deaths");
	if (v1 > v2)
		return false;
	else if (v1 < v2)
		return true;

	// if score and deaths are the same, use player index to get deterministic sort
	int iPlayerIndex1 = it1->GetInt("playerIndex");
	int iPlayerIndex2 = it2->GetInt("playerIndex");
	return (iPlayerIndex1 > iPlayerIndex2);
}

void CCSClientScoreBoardDialog::CSPlayerSortFunc()
{
	CUtlVector<KeyValues*>& teamSorted = m_teamPlayers[0];

	for (int teamIndex = SCORESECTION_TERRORIST; teamIndex <= SCORESECTION_SPECTATOR; ++teamIndex)
	{
		CUtlVector<KeyValues*>& teamPlayers = m_teamPlayers[teamIndex];

		for (int i = 0; i < teamPlayers.Count(); ++i)
		{
			int insertionPoint = 0;
			for (; insertionPoint < teamSorted.Count(); ++insertionPoint)
			{
				if (CSPlayerSortFunc(teamPlayers[i], teamSorted[insertionPoint]))
					break;
			}

			if (insertionPoint == teamSorted.Count())
			{
				teamSorted.AddToTail(teamPlayers[i]);
			}
			else
			{
				teamSorted.InsertBefore(insertionPoint, teamPlayers[i]);
			}
		}

		teamPlayers.RemoveAll();
		teamPlayers.Swap(teamSorted);
		teamSorted.RemoveAll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates information about teams
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::UpdateTeamInfo()
{
	// update the team sections in the scoreboard
	for (int teamIndex = TEAM_SPECTATOR; teamIndex <= TEAM_CT; teamIndex++)
	{
		wchar_t* teamName = NULL;
		int sectionID = 0;
		C_Team* team = GetGlobalTeam(teamIndex);

		if (team)
		{
			sectionID = GetSectionFromTeamNumber(teamIndex);

			switch (teamIndex) {
			case TEAM_TERRORIST:
				teamName = g_pVGuiLocalize->Find("#Cstrike_ScoreBoard_Ter");
				break;
			case TEAM_CT:
				teamName = g_pVGuiLocalize->Find("#Cstrike_ScoreBoard_CT");
				break;
			case TEAM_SPECTATOR:
				teamName = g_pVGuiLocalize->Find("#Spectators");
				break;
			default:
				Assert(false);
				break;
			}

			// update # of players on each team
			wchar_t name[64];
			wchar_t string1[1024];
			wchar_t val[6];

			_snwprintf(val, ARRAYSIZE(val), L"%i", team->Get_Number_Players());

			if (!teamName && team)
			{
				g_pVGuiLocalize->ConvertANSIToUnicode(team->Get_Name(), name, sizeof(name));
				teamName = name;
			}

			if (team->Get_Number_Players() == 1)
			{
				g_pVGuiLocalize->ConstructString(string1, sizeof(string1), g_pVGuiLocalize->Find("#Cstrike_ScoreBoard_Player"), 2, teamName, val);
			}
			else
			{
				g_pVGuiLocalize->ConstructString(string1, sizeof(string1), g_pVGuiLocalize->Find("#Cstrike_ScoreBoard_Players"), 2, teamName, val);
			}

			// set # of players for team in dialog
			m_pPlayerList->ModifyColumn(sectionID, "name", string1);

			if (teamIndex != TEAM_SPECTATOR)
			{
				// set team score in dialog
				_snwprintf(val, ARRAYSIZE(val), L"%i", team->Get_Score());
				m_pPlayerList->ModifyColumn(sectionID, "frags", val);
			}

			int pingsum = 0;
			int numcounted = 0;
			for (int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++)
			{
#ifdef _CLIENT_ADD
				if (g_PR->IsConnected(playerIndex) && GetSectionFromTeamNumber(g_PR->GetTeam(playerIndex)) == sectionID)
#else
				if (teamIndex > TEAM_SPECTATOR && g_PR->IsConnected(playerIndex) && g_PR->GetTeam(playerIndex) == teamIndex)
#endif // _CLIENT_ADD
				{
					int ping = g_PR->GetPing(playerIndex);

					if (ping >= 1)
					{
						pingsum += ping;
						numcounted++;
					}
				}
			}

#ifdef _CLIENT_ADD
			if (numcounted > 0)
#else
			if (numcounted > 0 && teamIndex > TEAM_SPECTATOR)
#endif // _CLIENT_ADD
			{
				int ping = (int)((float)pingsum / (float)numcounted);
				_snwprintf(val, ARRAYSIZE(val), L"%d", ping);
				m_pPlayerList->ModifyColumn(sectionID, "ping", val);
			}
			else
			{
				m_pPlayerList->ModifyColumn(sectionID, "ping", L"");
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds the top header of the scoreboars
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::AddHeader()
{
	// add the top header
	m_pPlayerList->AddSection(0, "");
	m_pPlayerList->SetSectionAlwaysVisible(0);
	m_pPlayerList->AddColumnToSection(0, "name", "", 0, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_NAME_WIDTH));
	m_pPlayerList->AddColumnToSection(0, "class", "", 0, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_CLASS_WIDTH));
	m_pPlayerList->AddColumnToSection(0, "frags", "#PlayerScore", 0 | SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_SCORE_WIDTH));
	m_pPlayerList->AddColumnToSection(0, "deaths", "#PlayerDeath", 0 | SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_DEATH_WIDTH));
	m_pPlayerList->AddColumnToSection(0, "ping", "#PlayerPing", 0 | SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_PING_WIDTH));
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new section to the scoreboard (i.e the team header)
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::AddSection(int teamType, int teamNumber)
{
	int sectionID = GetSectionFromTeamNumber(teamNumber);
	if (teamType == TYPE_TEAM)
	{
		m_pPlayerList->AddSection(sectionID, "");

		// setup the columns
		m_pPlayerList->AddColumnToSection(sectionID, "name", "", 0, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_NAME_WIDTH));
		m_pPlayerList->AddColumnToSection(sectionID, "class", "", 0, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_CLASS_WIDTH));
		m_pPlayerList->AddColumnToSection(sectionID, "frags", "", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_SCORE_WIDTH));
		m_pPlayerList->AddColumnToSection(sectionID, "deaths", "", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_DEATH_WIDTH));
		m_pPlayerList->AddColumnToSection(sectionID, "ping", "", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_PING_WIDTH));

		// set the section to have the team color
#ifdef _CLIENT_FIXES
		if (teamNumber)
#else
		if (teamNumber > TEAM_SPECTATOR)
#endif // _CLIENT_FIXES
		{
			if (GameResources())
				m_pPlayerList->SetSectionFgColor(sectionID, GameResources()->GetTeamColor(teamNumber));
		}

		m_pPlayerList->SetSectionAlwaysVisible(sectionID);
	}
	else if (teamType == TYPE_SPECTATORS)
	{
		m_pPlayerList->AddSection(sectionID, "");
		m_pPlayerList->AddColumnToSection(sectionID, "name", "#Spectators", 0, scheme()->GetProportionalScaledValueEx(GetScheme(), CSTRIKE_NAME_WIDTH));
		m_pPlayerList->AddColumnToSection(sectionID, "class", "", 0, scheme()->GetProportionalScaledValueEx(GetScheme(), 100));
	}
}

int CCSClientScoreBoardDialog::GetSectionFromTeamNumber(int teamNumber)
{
	switch (teamNumber)
	{
	case TEAM_TERRORIST:
		return SCORESECTION_TERRORIST;
	case TEAM_CT:
		return SCORESECTION_CT;
	}
	return SCORESECTION_SPECTATOR;
}

enum {
	MAX_PLAYERS_PER_TEAM = 16,
	MAX_SCOREBOARD_PLAYERS = 32
};

//-----------------------------------------------------------------------------
// Purpose: Updates the player list
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::UpdatePlayerInfo()
{
	m_pPlayerList->RemoveAll();

	C_CS_PlayerResource* cs_PR = dynamic_cast<C_CS_PlayerResource*>(g_PR);
	if (!cs_PR)
		return;

	C_CSPlayer* pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (!pLocalPlayer)
		return;

	int selectedRow = -1;

	for (int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++)
	{
		if (g_PR->IsConnected(playerIndex))
		{
			KeyValues* pKeyValues = new KeyValues("data");
			GetPlayerScoreInfo(playerIndex, pKeyValues);

			CUtlVector<KeyValues*>& teamPlayers = m_teamPlayers[GetSectionFromTeamNumber(g_PR->GetTeam(playerIndex))];
			teamPlayers.AddToTail(pKeyValues);
		}
	}

	CSPlayerSortFunc();

	int maxPlayers = MAX_SCOREBOARD_PLAYERS;
	for (int teamIndex = SCORESECTION_TERRORIST; teamIndex <= SCORESECTION_SPECTATOR; ++teamIndex)
	{
		CUtlVector<KeyValues*>& teamPlayers = m_teamPlayers[teamIndex];
		int maxPlayersTeam = MAX_PLAYERS_PER_TEAM;

		for (int i = 0; i < teamPlayers.Count(); ++i, --maxPlayersTeam)
		{
			bool isLocalPlayer = (teamPlayers[i]->GetInt("playerIndex") == pLocalPlayer->entindex());

			if ((maxPlayers > 0 && maxPlayersTeam > 0) || (teamIndex == SCORESECTION_SPECTATOR && isLocalPlayer))
			{
				int itemID = m_pPlayerList->AddItem(teamIndex, teamPlayers[i]);
				Color clr = g_PR->GetTeamColor(teamIndex == SCORESECTION_SPECTATOR ? TEAM_SPECTATOR : (teamIndex == SCORESECTION_TERRORIST ? TEAM_TERRORIST : TEAM_CT));
				m_pPlayerList->SetItemFgColor(itemID, clr);

				if (isLocalPlayer)
				{
					selectedRow = itemID;	// this is the local player, hilight this row
				}

				--maxPlayers;
			}

			teamPlayers[i]->deleteThis();
		}

		teamPlayers.RemoveAll();
	}

	if (selectedRow != -1)
	{
		m_pPlayerList->SetSelectedItem(selectedRow);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new row to the scoreboard, from the playerinfo structure
//-----------------------------------------------------------------------------
bool CCSClientScoreBoardDialog::GetPlayerScoreInfo(int playerIndex, KeyValues* kv)
{
	// Clean up the player name
	const char* oldName = g_PR->GetPlayerName(playerIndex);
	int bufsize = strlen(oldName) * 2 + 1;
	char* newName = (char*)_alloca(bufsize);
	UTIL_MakeSafeName(oldName, newName, bufsize);

	kv->SetString("name", newName);
	kv->SetInt("playerIndex", playerIndex);

	if (g_PR->GetTeam(playerIndex) > TEAM_SPECTATOR)
	{
		kv->SetInt("frags", g_PR->GetPlayerScore(playerIndex));
		kv->SetInt("deaths", g_PR->GetDeaths(playerIndex));
		kv->SetString("class", "");
	}

#ifdef _CLIENT_ADD
	if (g_PR->GetPing(playerIndex) < 1)
#else
	if (g_PR->GetPing(playerIndex) < 1 || g_PR->GetTeam(playerIndex) <= TEAM_SPECTATOR)
#endif // _CLIENT_ADD
	{
		if (g_PR->IsFakePlayer(playerIndex))
		{
			kv->SetString("ping", "BOT");
		}
		else
		{
			kv->SetString("ping", "");
		}
	}
	else
	{
		kv->SetInt("ping", g_PR->GetPing(playerIndex));
	}

	// get CS specific infos
	C_CS_PlayerResource* cs_PR = dynamic_cast<C_CS_PlayerResource*>(g_PR);

	C_CSPlayer* me = C_CSPlayer::GetLocalCSPlayer();

	if (!cs_PR || !me)
		return true;

	bool bShowExtraInfo =
		(me->GetTeamNumber() == TEAM_UNASSIGNED) || // we're not spawned yet
		(me->GetTeamNumber() == TEAM_SPECTATOR) || // we are a spectator
		(me->IsPlayerDead()) ||					  // we are dead
		(me->GetTeamNumber() == g_PR->GetTeam(playerIndex)); // we're on the same team

	if (g_PR->IsHLTV(playerIndex))
	{
		// show #spectators in class field, it's transmitted as player's score
		char numspecs[32];
#ifdef _CLIENT_FIXES
		vgui::localize()->ConvertUnicodeToANSI(vgui::localize()->Find("#Spectators"), numspecs, sizeof(numspecs));
		Q_snprintf(numspecs, sizeof(numspecs), "%s: %i", numspecs, m_HLTVSpectators);
#else
		Q_snprintf(numspecs, sizeof(numspecs), "%i Spectators", m_HLTVSpectators);
#endif
		kv->SetString("class", numspecs);
	}
	else if (!g_PR->IsAlive(playerIndex) && g_PR->GetTeam(playerIndex) > TEAM_SPECTATOR)
	{
		kv->SetString("class", "#Cstrike_DEAD");
	}
	else if (cs_PR->HasC4(playerIndex) && bShowExtraInfo)
	{
		kv->SetString("class", "#Cstrike_BOMB");
	}
	else if (cs_PR->IsVIP(playerIndex) && bShowExtraInfo)
	{
		kv->SetString("class", "#Cstrike_VIP");
	}

	return true;
}