#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "tier0\platform.h"

// basic vtable helper
struct vtable {
	friend bool InitVTable(void* interface, vtable& vtable_obj);

private:
	typedef void* (*func)(void*);
	void** vt;

public:
	int32 count; // out

	func operator[](int num) { // out
		return (func)vt[num];
	}
};

inline void DumpVTable(void** vt) {
    for (int i = 0; i < 16; i++) {
        printf("[%02d] %p\n", i, vt[i]);
    }
}

inline int CountVTable(void** vt) {
    // !FIXME!FIXME!
    // 
    // This is bad as hell, need to rework counting mechanism
    const int MAX = 256;
    int i = 0;

    for (; i < MAX; i++) {
        if (!vt[i])
            break;

        if (vt[i] < (void*)0x10000)
            break;
    }

    return i;
}

inline bool InitVTable(void* interface, vtable& vtable_obj) {
	if (!interface)
		return false;

	void** vtable = *reinterpret_cast<void***>(interface);
	vtable_obj.vt = vtable;

	// here we need to count all functions.
    int count = CountVTable(vtable); 
    //DumpVTable(vtable);

	vtable_obj.count = count;

	return true;
}

