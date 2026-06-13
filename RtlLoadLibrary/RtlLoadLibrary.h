#pragma once
#include"pch.h"
#include <future>
extern "C" {
	HMODULE WINAPI RtlLoadLibraryExW(__in LPCWSTR lpLibFileName, __in HANDLE hFile, __in DWORD dwFlags);
	HMODULE WINAPI RtlLoadLibraryW(__in LPCWSTR lpLibFileName);
	HMODULE WINAPI RtlLoadLibraryExA(__in LPCSTR lpLibFileName, __in HANDLE hFile, __in DWORD dwFlags);
	HMODULE WINAPI RtlLoadLibraryA(__in LPCSTR lpLibFileName);
	HMODULE WINAPI RtlLoadLibraryInMemory(__in PVOID lpBytes,__in_opt DWORD dwSize);
	HMODULE WINAPI RtlLoadLibraryInMemoryEx(
		__in PVOID lpBytes,
		__in DWORD dwSize,
		__in DWORD dwFlags,
		__in LPWSTR lpFileName,
		__in LPWSTR lpFullname
	);
	PVOID WINAPI RtlGetSectionDataA(__in HMODULE hMod, __in LPCSTR lpSecName);
	PVOID WINAPI RtlGetSectionDataW(__in HMODULE hMod, __in LPCWSTR lpSecName);
	PVOID WINAPI RtlGetEntryPoint(__in HMODULE hMod);
	
	// Loader Lock API - wraps LdrpLoaderLock critical section
	// Call RtlLoadLibraryInMemoryEx within a locked scope for safety
	void WINAPI RtlLockLoaderLock();
	void WINAPI RtlUnlockLoaderLock();
	
};
SIZE_T GetContinuousReadableMemorySize(__in LPVOID address);
std::future<HMODULE> AsyncRtlLoadLibraryA(__in LPCSTR lpLibFileName);
bool IsApiSetName(__in const std::wstring& name);
BOOL FileExists(__in LPCWSTR szPath);
std::wstring GetFileExtension(__in const std::wstring& filePath);
std::vector<std::wstring> GetSafeDirectories();
std::pair<std::wstring, std::wstring> GetFileNameAndPath(__in const std::wstring& fullPath);
