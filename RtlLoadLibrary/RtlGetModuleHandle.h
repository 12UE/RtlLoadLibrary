#ifndef RLTGETMODULEHANDLE_H
#define RLTGETMODULEHANDLE_H
#include"pch.h"
static std::function<HMODULE(const wchar_t*)> g_GetModuleHandleCallback = nullptr;
inline void SetGetModuleHandleCallback(__in const std::function<HMODULE(const wchar_t*)>& callback) {
	g_GetModuleHandleCallback = callback;
}
inline void ClearGetModuleHandleCallback() {
	g_GetModuleHandleCallback = nullptr;
}

extern "C"  HMODULE WINAPI RtlGetModuleHandleW(__in_opt const wchar_t* targetDllName=nullptr);

extern "C"  HMODULE WINAPI RtlGetModuleHandleA(__in_opt const char* targetDllName=nullptr);

#ifdef UNICODE
#define RtlGetModuleHandle RtlGetModuleHandleW
#else
#define RtlGetModuleHandle RtlGetModuleHandleA
#endif // !UNICODE

#endif // RLTGETMODULEHANDLE_H
