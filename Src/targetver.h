#pragma once

// // Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <SDKDDKVer.h>
#if !defined(_WIN64) && !defined(RC_INVOKED)
#error Winip supports only 64-bit Windows.
#endif
