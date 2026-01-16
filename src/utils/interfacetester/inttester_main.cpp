#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <direct.h>
#include <cstdio>

#include "tier1\interface.h"
#include "tier1\strtools.h"
#include "tier0\platform.h"

#include "vtables.h"
#include "debugging.h"

void* RequestInterface(const char* mod, const char* interface, char* pOutFullInterfaceName = 0) {
	char fullInterfaceName[1024];
	void* pOutputInterface = nullptr;
	Success("[RequestInterface]: (start) mod.%s req_iface.%s\n", mod, interface);
	
	HMODULE module = LoadLibrary(mod);
	if (!module) {
		Error("[RequestInterface]: (error) %s, LAST DWORD: %i", mod, GetLastError());
		return 0;
	}

	void* try_ = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(module, CREATEINTERFACE_PROCNAME))(interface, nullptr);
	if (try_ == nullptr)
		Warning("[RequestInterface]: iFace %s not available\n", interface);
	else {
		pOutputInterface = try_;
		V_strcpy(fullInterfaceName, interface);
	}

	for (int i = 0; i < 99; i++) {
		memset(fullInterfaceName, 0, sizeof(fullInterfaceName));
		if (i < 10)
			sprintf(fullInterfaceName, "%s00%i", interface, i);
		else
			sprintf(fullInterfaceName, "%s0%i", interface, i);

		void* pIFace = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(module, CREATEINTERFACE_PROCNAME))(fullInterfaceName, nullptr);
		if (pIFace == nullptr)
			Warning("[RequestInterface]: iFace %s not available\n", fullInterfaceName);
		else {
			pOutputInterface = pIFace;
			break;
		}
	}

	if (pOutputInterface == nullptr)
		Error("[RequestInterface]: (end) last iFace = %s\n", fullInterfaceName);
	else
		Success("[RequestInterface]: (end) last iFace = %s\n", fullInterfaceName);

	if (pOutFullInterfaceName)
		V_strcpy(pOutFullInterfaceName, fullInterfaceName);

	return pOutputInterface;
}

static void usage(int iReturnCode = 0)
{
	fflush(stderr);
	Msg(
		"Usage: interfacetester <module> <interfacename>\n"
		"\n"
		"<module>\n" 
		"is a dll relative to bin folder\n"
		"e.g: gameui.dll\n"
		"\n"
		"<interfacename>\n" 
		"is the name of interface you want to test\n"
		"e.g: GameUI or GameUI011\n"
		"\n"
	);
	exit(iReturnCode);
}

int main(int argc, char** argv) {
	// Must add 'bin' to the path....
	char* pPath = getenv("PATH");
	char BinPath[MAX_PATH];
	char szBuf[4096];

	(void)_getcwd(BinPath, sizeof(BinPath));

	if (argc > 3 && strcmp(argv[3], "customEnv") == 0) {
		if (argc < 5)
			puts("main: customEnv skipped\n");

		sprintf(szBuf, "PATH=%s;%s", argv[4], pPath);
		szBuf[sizeof(szBuf) - 1] = '\0';
		printf("main: customEnv set to %s\n", argv[4]);
	}
	else {
		sprintf(szBuf, "PATH=%s;%s", BinPath, pPath);
		szBuf[sizeof(szBuf) - 1] = '\0';
	}

	_putenv(szBuf);

	Debugging_Init();

	if (argc < 2)
		Warning("No command specified.  Try 'interfacetester -?' for info.\n");
	if (argc < 3 && argc != 1)
		Warning("Argument 2 is missing.  Try 'interfacetester -?' for info.\n");

	if (strcmp(argv[1], "-?") == 0)
		usage();

	char outIntName[1024];
	void* pInterface = RequestInterface(argv[1], argv[2], outIntName);
	vtable pInterfaceVTable;
	InitVTable(pInterface, pInterfaceVTable);

	Info(
		"// ------------------------ //\n"
		"// MODULE NAME %s\n"
		"// INTERFACE NAME %s\n"
		"// VPOINTER COUNT %i\n"
		"// ------------------------ //\n",
		argv[1],
		outIntName,
		pInterfaceVTable.count
	);

	Debugging_Shutdown();
	return 1;
}