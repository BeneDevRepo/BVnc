/*
Ensure that every file includes the same WinAPI Configuration
*/

#define WIN32_LEAN_AND_MEAN 1 // Exclude Networking etc. from WinAPI

// #ifdef WINVER
// 	#undef WINVER
// #endif
// #define WINVER 0x0602 // Windows 8
// #define WINVER 0x0A00 // Windows 10 ?

#include <windows.h>
#include <windowsx.h>

// #define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#define MOD_NOREPEAT 0x4000