#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include "debugging.h"

#define FMT_FUNCTION(name) int name(const char* fmt, ...)
#define FMT_PRINTF va_list args; va_start(args, fmt); int ret = vprintf(fmt, args); va_end(args); resetcolor(); return ret

DEBUG_MSG Msg;
DEBUG_MSG Warning;
DEBUG_MSG Success;
DEBUG_MSG Info;
DEBUG_MSG Error;

namespace gDbg { 

	HANDLE hStdout;

	void setcolor(short Color)
	{
		if (!SetConsoleTextAttribute(hStdout, Color))
			puts("[Debugging]: (warning) setcolor failed\n"); // _msg or other aren't initialized now.
			///(void)0;
	}

	void resetcolor()
	{
		setcolor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // 7
	}

	FMT_FUNCTION(_msg)
	{
		resetcolor();
		FMT_PRINTF;
	}
	FMT_FUNCTION(_warn)
	{
		setcolor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		FMT_PRINTF;
	}
	FMT_FUNCTION(_success)
	{
		setcolor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		FMT_PRINTF;
	}
	FMT_FUNCTION(_info)
	{
		setcolor(FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		FMT_PRINTF;
	}
	FMT_FUNCTION(_err)
	{
		setcolor(FOREGROUND_RED | FOREGROUND_INTENSITY);
		char buf[2048];
		va_list args; va_start(args, fmt); vsprintf(buf, fmt, args); va_end(args);
		puts(buf);
		resetcolor();
		::MessageBox(0, buf, "Engine Error", MB_OK);
		return TerminateProcess(GetCurrentProcess(), -1);
	}

}


void Debugging_Init() {
	using namespace gDbg;

	Msg = _msg;
	Warning = _warn;
	Success = _success;
	Info = _info;
	Error = _err; // will always throw messagebox

	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	Success("[Debugging]: (start)\n");

	if (hStdout == INVALID_HANDLE_VALUE)
		Error("[Debugging]: (error) invalid stdout handle\n");

	return;
}

void Debugging_Shutdown() {
	using namespace gDbg;

	if (!Success("[Debugging]: (end)\n"))
		puts("[Debugging]: (end)\n");

	Msg = 0;
	Warning = 0;
	Success = 0;
	Info = 0;
	Error = 0;

	resetcolor();

	return;
}