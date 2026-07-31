/*
 *
 * Copyright (c) 2026 RuSHeRR
 * 
 * Purpose: build information
 * 
*/

#ifndef BUILD_INFO_H_
#define BUILD_INFO_H_

#include "tier0/platform.h"
#include "git_info.h"

struct commit_info_t
{
	char sha[41] {};		//< sha1
	char sha_short[8]{};	//< sha1 (short)
	char author[64] {};		//< author of the commit
	char message[64] {};	//< commit message
	char date[32] {};		//< date
};

class CBuildInfo
{
public:
	CBuildInfo();
	~CBuildInfo();

private:
	// internal initialize function
	void Init();

public:
	// returns days since Nov 07 1998
	int GetBuildNumber();

	// gives the UNIX timestamp
	uint32 GetBuildTimestamp();
	
	// converts UNIX timestamp to hex format
	char* GetHexTimestamp();

	// GITHUB REST API START //
	///////////////////////////

	// gets commit information
	commit_info_t& GetCommitInfo(const char* branch = "dev", const char* until = "today");

private:
	// process raw json data into commit info
	void ProcessRawCommitData(const char* pData, commit_info_t& info);

	///////////////////////////
	//  GITHUB REST API END  //

private:
	int			m_nBuildNumber;
	uint32		m_nTimestamp;
};

// Global pointer
extern CBuildInfo* g_pBuildInfo;

inline int build_number()
{
	return g_pBuildInfo->GetBuildNumber();
}

#endif // BUILD_INFO_H_