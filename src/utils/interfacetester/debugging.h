#pragma once

typedef int (*DEBUG_MSG)(const char* fmt, ...);

extern DEBUG_MSG Msg;
extern DEBUG_MSG Warning;
extern DEBUG_MSG Success;
extern DEBUG_MSG Info;
extern DEBUG_MSG Error;

// initialize all debugging functions
void Debugging_Init();

void Debugging_Shutdown();