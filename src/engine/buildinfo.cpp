/*
 *
 * Copyright (c) 2026 RuSHeRR
 *
 * Purpose: build information
 *
*/

#include "quakedef.h"
#include "winlite.h"
#include "tier1/strtools.h"
#include "buildinfo.h"
#include "unixtime.h"
#include "http.h"
#include "../thirdparty/nlohmann/json.hpp"

using json = nlohmann::json;

static const char* months[] =
{
	"Jan","Feb","Mar","Apr","May","Jun",
	"Jul","Aug","Sep","Oct","Nov","Dec"
};

static const char github_api_url[] = "https://api.github.com/repos/rusherr-c/cssv34-engine/";

/*
 * converts date
 * into iso8601 format
 * 
 * @output char*
 */
void get_date_iso8601(const char* date, char* out)
{
    char month[4];
    int day, year;

    sscanf(date, "%3s %d %d", month, &day, &year);

    int monthNum = 0;
    for (int i = 0; i < 12; ++i)
    {
        if (strcmp(month, months[i]) == 0)
        {
            monthNum = i + 1;
            break;
        }
    }

    sprintf(out, "%04d-%02d-%02dT00:00:00Z",
        year, monthNum, day);
}

// Constructor
//
CBuildInfo::CBuildInfo()
{
	Init();
}

// Destructor
//
CBuildInfo::~CBuildInfo()
{

}

// returns days since Nov 07 1998
int CBuildInfo::GetBuildNumber()
{
	return m_nBuildNumber;
}

// gives the UNIX timestamp
uint32 CBuildInfo::GetBuildTimestamp()
{
	return m_nTimestamp;
}

// converts UNIX timestamp to hex format
char* CBuildInfo::GetHexTimestamp()
{
    char *out = new char[9];
	sprintf(out, "%x", m_nTimestamp);

    return out;
}

// gets commit information
commit_info_t& CBuildInfo::GetCommitInfo(const char* branch, const char* until) {
    commit_info_t info{};
    char    request[128];
    bool    bQueryLast = false;

    byte   *resultData = 0;
    int     resultSize = 0;
    int     returnCode = 0;

    if (!until || until[0] == 0 || !branch || branch[0] == 0)
        return info;

    if (!strcmp(until, "today"))
        bQueryLast = true;

    snprintf(request, sizeof(request), "%scommits?sha=%s&", github_api_url, branch);
    if (bQueryLast)
        strcat(request, "per_page=1");
    else
    {
        strcat(request, "until=");

        char until_date[32];
        get_date_iso8601(until, until_date);
        strcat(request, until_date);
        strcat(request, "&per_page=1");
    }

    resultData = HTTPGetRequest(request, &resultSize, &returnCode);

    if (resultData && resultSize && returnCode == 200)
        ProcessRawCommitData((const char*)resultData, info);
    else
        Warning("HTTP GET Request to %s failed with code %i\n", request, returnCode);

    free(resultData);

    return info;
}

// process raw json data into commit info
void CBuildInfo::ProcessRawCommitData(const char* pData, commit_info_t& info) {
    json j = json::parse(pData);

    const auto& commit = j[0];

    std::string sha = commit["sha"];
    std::string author = commit["commit"]["author"]["name"];
    std::string message = commit["commit"]["message"];
    std::string date = commit["commit"]["author"]["date"];

    strncpy(info.sha, sha.c_str(), sizeof(info.sha));
    strncpy(info.sha_short, sha.c_str(), sizeof(info.sha_short));
    strncpy(info.author, author.c_str(), sizeof(info.author));
    strncpy(info.message, message.c_str(), sizeof(info.message));
    strncpy(info.date, date.c_str(), sizeof(info.date));
    info.sha_short[7] = 0;
}

// internal initialize function
void CBuildInfo::Init()
{
	m_nTimestamp = UNIX_TIMESTAMP;
	m_nBuildNumber = m_nTimestamp / 86400; // days since Jan 1 1970

	m_nBuildNumber -= 10439;  // Nov 7 1998 (HL1 Gold Date)
}


// Singleton
static CBuildInfo s_buildInfo;

// Global pointer
CBuildInfo* g_pBuildInfo = &s_buildInfo;
