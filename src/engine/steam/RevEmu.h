#pragma once

#include "RevSpoofer.h"
#include "Encryption\CRijndael.h"
#include "Encryption\SHA.h"
#include <Windows.h>

// Some defines

#define LOBYTE(x)   (*((_BYTE*)&(x)))
#define HIBYTE(x)   (*((_BYTE*)&(x)+1))

#define LOWORD(x)   (*((_WORD*)&(x)))
#define HIWORD(x)   (*((_WORD*)&(x)+1))

#define BYTE0(x)    (*((_BYTE*)&(x)+0))
#define BYTE1(x)    (*((_BYTE*)&(x)+1))
#define BYTE2(x)    (*((_BYTE*)&(x)+2))
#define BYTE3(x)    (*((_BYTE*)&(x)+3))

static void CreateRandomString(char *pszDest, int nLength)
{
	static const char c_szAlphaNum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	
	for (int i = 0; i < nLength; ++i)
		pszDest[i] = c_szAlphaNum[rand() % (sizeof(c_szAlphaNum) - 1)];

	pszDest[nLength] = '\0'; 
}

namespace RevGenerator
{
	int GenerateSteamEmu(void* pData, int nSteamID)
	{
		auto pTicket = (int*)pData;

		unsigned i = 0;

		pTicket[20] = -1;									// header
		pTicket[21] = (nSteamID ^ 0xC9710266) & 0x7FFFFFFF; // SteamId

		return 768;
	}

	int GenerateRevEmu1(void* pData, int nSteamID, int nSpecial)
	{
		if (nSpecial == 1)
			return GenerateSteamEmu(pData, nSteamID);

		auto pTicket = (int*)pData;
		auto pbTicket = (unsigned char*)pData;

		pTicket[0] = 0xFFFF;			// +0, header
		pTicket[1] = (nSteamID) << 1;	// +4, SteamId
		*(short*)&pbTicket[8] = 0;		// +8, unknown, in original emulator must be 0
		return 10;
	}

	int GenerateRevEmu2(void* pData, int nSteamID)
	{
		char szhwid[64];

		CreateRandomString(szhwid, 16);
		if (!RevSpoofer::Spoof(szhwid, nSteamID))
			return 0;

		auto pTicket = (int*)pData;
		auto revHash = RevSpoofer::Hash(szhwid);

		pTicket[0] = '.';			// +0, header
		pTicket[1] = 20;			// +4, unknown number, must be 20
		pTicket[2] = 'rev';         // +8, magic number (low part)
		pTicket[3] = 0;             // +12, magic number (high part)
		pTicket[4] = revHash * 2;	// +16, SteamId

		return 24;
	}

	int GenerateRevEmu3(void* pData, int nSteamID, int nSpecial)
	{
		char szhwid[64];

		CreateRandomString(szhwid, 16);
		if (!RevSpoofer::Spoof(szhwid, nSteamID))
			return 0;

		auto pTicket = (int*)pData;
		auto revHash = RevSpoofer::Hash(szhwid);

		pTicket[0] = 'J';           //  +0, header
		pTicket[1] = revHash;       //  +4, hash of string at +24 offset
		pTicket[2] = 'rev';         //  +8, magic number
		pTicket[3] = 0;             // +12, unknown number, must be 0

		pTicket[4] = revHash * 2;	// +16, SteamId, Low part
		pTicket[5] = 0x01100001;    // +20, SteamId, High part

		strcpy((char*)&pTicket[6], szhwid); // +24, string for hash

		if (nSpecial == 1)
			return 152;
		else
			return 164;
	}

	int GenerateRevEmu4(void* pData, int nSteamID, int nSpecial)
	{
		char szhwid[64];

		CreateRandomString(szhwid, 16);
		if (!RevSpoofer::Spoof(szhwid, nSteamID))
			return 0;

		auto pTicket = (int*)pData;
		auto pbTicket = (byte*)pData;
		auto revHash = RevSpoofer::Hash(szhwid);

		pTicket[0] = 'S';           //  +0, header
		pTicket[1] = revHash;       //  +4, hash of string at +24 offset
		pTicket[2] = 'rev';         //  +8, magic number
		pTicket[3] = 0;             // +12, unknown number, must always be 0

		pTicket[4] = revHash * 2;	// +16, SteamId, Low part
		pTicket[5] = 0x01100001;    // +20, SteamId, High part

		/* Encrypt HWID with AESKeyRand key and save it in the ticket. */
		static const char AESKeyRand[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
		char AESHashRand[32];
		auto AESRand = CRijndael();
		AESRand.MakeKey(AESKeyRand, CRijndael::sm_chain0, 32, 32);
		AESRand.EncryptBlock(szhwid, AESHashRand);
		memcpy(&pbTicket[24], AESHashRand, 32);

		/* Encrypt AESKeyRand with AESKeyRev key and save it in the ticket.
		 * AESKeyRev key is identical to the key in dproto/reunion. */
		static const char AESKeyRev[] = "_YOU_SERIOUSLY_NEED_TO_GET_LAID_";
		char AESHashRev[32];
		auto AESRev = CRijndael();
		AESRev.MakeKey(AESKeyRev, CRijndael::sm_chain0, 32, 32);
		AESRev.EncryptBlock(AESKeyRand, AESHashRev);
		memcpy(&pbTicket[56], AESHashRev, 32);

		/* Perform HWID hashing and save hash to the ticket. */
		char SHAHash[32];
		auto sha = CSHA(CSHA::SHA256);
		sha.AddData(szhwid, 32);
		sha.FinalDigest(SHAHash);
		memcpy(&pbTicket[88], SHAHash, 32);

		if (nSpecial == 1)
			return 152;
		else
			return 164;
	}
}
