#pragma once
#ifndef RTLGETPROCADDRESS_H
#define RTLGETPROCADDRESS_H
#include "pch.h"
#include <mutex>

const int MAX_FORWARD_DEPTH = 32;

struct CallIdentifier {
    HMODULE hModule;
    std::string functionName;

    bool operator<(__in const CallIdentifier& other) const {
        if (hModule != other.hModule) return hModule < other.hModule;
        return functionName < other.functionName;
    }
};
static std::mutex g_CallHistoryMutex;
extern "C" FARPROC WINAPI RtlGetProcAddress(__in HMODULE hMod, __in const char* lpszProcName);
extern "C" void WINAPI RtlInvalidateProcCache(__in HMODULE hMod);
static std::function<FARPROC(HMODULE, const char*)> g_fnGetProcAddreCallback = nullptr;

inline void SetGetProcAddressCallback(__in const std::function<FARPROC(HMODULE, const char*)>& callback) {
    if (!g_fnGetProcAddreCallback) {
        g_fnGetProcAddreCallback = callback;
    }
}

inline void ClearGetProcAddressCallback() {
    g_fnGetProcAddreCallback = nullptr;
}
#endif // RTLGetProcAddress.h
