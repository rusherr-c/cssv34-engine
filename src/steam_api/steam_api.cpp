#define _CRT_SECURE_NO_WARNINGS
#define S_API extern "C" __declspec(dllexport)
#define NULL 0

S_API bool SteamAPI_Init() { return true; }
S_API bool SteamAPI_InitSafe() { return true; }
S_API bool SteamAPI_IsSteamRunning() { return true; }