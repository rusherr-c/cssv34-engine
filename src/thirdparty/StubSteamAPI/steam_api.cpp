#define _CRT_SECURE_NO_WARNINGS
#define STEAM_API_EXPORTS
#define NULL 0
#include "steam\steam_api.h"
#define NOTE_UNUSED(x) (void)(x)
S_API void *g_pSteamClientGameServer;
void *g_pSteamClientGameServer = NULL;

//steam_api.h
S_API bool SteamAPI_Init() {
	return true;
}

S_API bool SteamAPI_InitSafe() {
	return true;
}

S_API void SteamAPI_Shutdown() {

}

S_API bool SteamAPI_RestartAppIfNecessary() {
	return false;
}

S_API void SteamAPI_ReleaseCurrentThreadMemory() {

}

S_API void SteamAPI_WriteMiniDump() {

}

S_API void SteamAPI_SetMiniDumpComment() {

}

S_API void SteamAPI_RunCallbacks() {
}

S_API void SteamAPI_RegisterCallback( class CCallbackBase* pCallback, int iCallback ) {
	NOTE_UNUSED(pCallback); NOTE_UNUSED(iCallback);
}

S_API void SteamAPI_UnregisterCallback( class CCallbackBase* pCallback ) {
	NOTE_UNUSED(pCallback);
}

S_API void SteamAPI_RegisterCallResult() {

}

S_API void SteamAPI_UnregisterCallResult() {

}

S_API bool SteamAPI_IsSteamRunning() {
	return false;
}

S_API void Steam_RunCallbacks( HSteamPipe hSteamPipe, bool bGameServerCallbacks ) {
	NOTE_UNUSED(hSteamPipe); NOTE_UNUSED(bGameServerCallbacks);
}

S_API void Steam_RegisterInterfaceFuncs(void* hModule) {
	NOTE_UNUSED(hModule);
}

S_API int Steam_GetHSteamUserCurrent() {
	return 0;
}

S_API const char *SteamAPI_GetSteamInstallPath() {
	return NULL;
}

S_API int SteamAPI_GetHSteamPipe() {
	return 0;
}

S_API void SteamAPI_SetTryCatchCallbacks() {

}

S_API void SteamAPI_SetBreakpadAppID() {

}

S_API void SteamAPI_UseBreakpadCrashHandler() {

}

S_API int GetHSteamPipe() {
	return 0;
}

S_API int GetHSteamUser() {
	return 0;
}

S_API int SteamAPI_GetHSteamUser() {
	return 0;
}

S_API void *SteamInternal_ContextInit() {
	return NULL;
}

S_API void *SteamInternal_CreateInterface() {
	return NULL;
}

S_API void *SteamApps() {
	return NULL;
}

S_API ISteamClient *SteamClient() {
	return NULL;
}

S_API ISteamFriends *SteamFriends() {
	return NULL;
}

S_API void *SteamHTTP() {
	return NULL;
}

S_API void *SteamMatchmaking() {
	return NULL;
}

S_API void *SteamMatchmakingServers() {
	return NULL;
}

S_API void *SteamNetworking() {
	return NULL;
}

S_API void *SteamRemoteStorage() {
	return NULL;
}

S_API void *SteamScreenshots() {
	return NULL;
}

S_API ISteamUser *SteamUser() {
	return NULL;
}

S_API void *SteamUserStats() {
	return NULL;
}

S_API ISteamUtils *SteamUtils() {
	return NULL;
}

S_API ISteamGameServer* SteamGameServer() {
	return NULL;
}

S_API ISteamUtils* SteamGameServerUtils() {
	return NULL;
}

S_API int SteamGameServer_GetHSteamPipe() {
	return 0;
}

S_API int SteamGameServer_GetHSteamUser() {
	return 0;
}

S_API int SteamGameServer_GetIPCCallCount() {
	return 0;
}

S_API bool SteamGameServer_Init(uint32 unIP, uint16 usPort, uint16 usGamePort, void* eServerMode, int nGameAppId, const char* pchGameDir, const char* pchVersionString) {
	NOTE_UNUSED(unIP);
	NOTE_UNUSED(usPort);
	NOTE_UNUSED(usGamePort);
	NOTE_UNUSED(eServerMode);
	NOTE_UNUSED(nGameAppId);
	NOTE_UNUSED(pchGameDir);
	NOTE_UNUSED(pchVersionString);

	return false;
}

S_API int SteamGameServer_InitSafe() {
	return 0;
}

S_API void SteamGameServer_RunCallbacks() {
}

S_API void SteamGameServer_Shutdown() {
}
