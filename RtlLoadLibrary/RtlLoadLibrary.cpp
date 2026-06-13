#include "pch.h"
#include "RtlLoadLibrary.h"
#include "ApisetQuery.h"
#include "RtlGetModuleHandle.h"
#include "RtlMemory.h"
#include "GlobalObject.h"
#include <thread>
static const std::wstring& SystemDirectory()
{
    static std::wstring wstrDir = [] {
        wchar_t waryBuf[MAX_PATH]={0};
        DWORD dwLen = ::GetSystemDirectoryW(waryBuf, _countof(waryBuf));
        return std::wstring(waryBuf, dwLen);
        }();
    return wstrDir;
}
HMODULE WINAPI RtlLoadLibraryExW(__in LPCWSTR lpLibFileName, __in HANDLE hFile, __in DWORD dwFlags){
    if (!lpLibFileName || !*lpLibFileName) {
       DebugPrint(LEVEL_ERROR, "RtlLoadLibraryExW: Invalid library name");
        return nullptr;
    }
    if (hFile != nullptr) {
       DebugPrint(LEVEL_ERROR, "RtlLoadLibraryExW:hFile is Not Null");
        return nullptr;
    }
        
    /* Step 0: Resolve API set names first */
    std::vector<std::string> vecResults;
	std::wstring wstrLibFileName(lpLibFileName);
    if(IsApiSetName(wstrLibFileName)) {
        vecResults = ApisetQuery::GetInstance().QueryForwardDlls(wstrLibFileName);
	}
    
    /* Step 1: Fast load path - check if already loaded */
    std::wstring wstrBaseName = L"";
    if (!vecResults.empty()) {
        // Use the first API set resolved DLL name
        wstrLibFileName = std::wstring(vecResults[0].begin(), vecResults[0].end());
    }
    wstrBaseName=GetFileName(wstrLibFileName);
    HMODULE hOldModule = RtlGetModuleHandleW(wstrBaseName.c_str());
    if (hOldModule) {
        return hOldModule;
    }
    LPCWSTR lpcwszSearchPath = nullptr;
    switch (dwFlags)
    {
    case LOAD_LIBRARY_SEARCH_SYSTEM32:
        lpcwszSearchPath = SystemDirectory().c_str();
        break;
    case LOAD_LIBRARY_SEARCH_APPLICATION_DIR:
        lpcwszSearchPath = L".";
        break;
    default:
        lpcwszSearchPath = nullptr;
        break;
    }

    /* Step 3: Search for the file path */
    WCHAR wszAryFullPath[MAX_PATH + 1]{};
    DWORD dwLen = SearchPathW(lpcwszSearchPath, wstrLibFileName.c_str(), nullptr,
        _countof(wszAryFullPath), wszAryFullPath, nullptr);
    if (!dwLen || dwLen >= _countof(wszAryFullPath)) {
        return nullptr;
    }
    if (!PathFileExistsW(wszAryFullPath)) {
        DebugPrint(LEVEL_ERROR, "RtlLoadLibraryExW failed file not exists for %ls", wszAryFullPath);
        return nullptr;
    }
    /* Step 4: Map the file and load it */
    THANDLE hFileReal = CreateFileW(wszAryFullPath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFileReal) {
        DebugPrint(LEVEL_ERROR, "RtlLoadLibraryExW: CreateFileW %ls failed, le=%d",
            wszAryFullPath, GetLastError());
        return nullptr;
    }
    FileMap tagFileMap(static_cast<HANDLE>(hFileReal));
    LARGE_INTEGER tagFileSize{};
    if (!GetFileSizeEx(static_cast<HANDLE>(hFileReal), &tagFileSize)) {
        DebugPrint(LEVEL_ERROR, "Invalid FileSize for %s!",GetLastErrorAsString().c_str());
        return nullptr;
    }
    return RtlLoadLibraryInMemoryEx(tagFileMap.GetMap(), (DWORD)tagFileSize.QuadPart, dwFlags, (LPWSTR)wstrBaseName.c_str(), wszAryFullPath);
}

HMODULE WINAPI RtlLoadLibraryW(__in LPCWSTR lpLibFileName)
{
    if (!lpLibFileName) {
        DebugPrint(LEVEL_ERROR, "Invalid library name!");
        return nullptr;
    }
    std::wstring wstrLibNameA(lpLibFileName);
    if (wstrLibNameA.empty()) {
        DebugPrint(LEVEL_ERROR, "Empty library name,please check again");
        return nullptr;
    }
	return RtlLoadLibraryExW(lpLibFileName, nullptr, 0);
}

HMODULE WINAPI RtlLoadLibraryExA(__in LPCSTR lpLibFileName, __in HANDLE hFile, __in DWORD dwFlags){
    if (!lpLibFileName) {
        DebugPrint(LEVEL_ERROR, "Invalid library name!");
        return nullptr;
    }
    std::string strLibNameA(lpLibFileName);
    if (strLibNameA.empty()) {
        DebugPrint(LEVEL_ERROR, "Empty library name,please check again");
        return nullptr;
    }
	return RtlLoadLibraryExW(NarrowToWide(lpLibFileName).c_str(), hFile, dwFlags);
}

HMODULE WINAPI RtlLoadLibraryA(__in LPCSTR lpLibFileName)
{
    if (!lpLibFileName) {
        DebugPrint(LEVEL_ERROR, "Invalid library name!");
        return nullptr;
    }
    std::string strLibNameA(lpLibFileName);
    if (strLibNameA.empty()) {
        DebugPrint(LEVEL_ERROR, "Empty library name,please check again");
        return nullptr;
    }
	return RtlLoadLibraryExW(NarrowToWide(lpLibFileName).c_str(), nullptr, 0);
}

HMODULE __stdcall RtlLoadLibraryInMemory(__in PVOID lpBytes, __in_opt DWORD dwSize)
{   
    DWORD nSize = dwSize;
    if (!nSize) {
        nSize = static_cast<DWORD>(GetContinuousReadableMemorySize(lpBytes));
    }
    return RtlLoadLibraryInMemoryEx(lpBytes, nSize, 0,nullptr,nullptr);
}

HMODULE WINAPI RtlLoadLibraryInMemoryEx(
    __in PVOID lpBytes,
    __in DWORD dwSize, 
    __in DWORD dwFlags,
    __in LPWSTR lpFileName,
    __in LPWSTR lpFullName
)
{
    if (!lpBytes || !dwSize) {
        DebugPrint(LEVEL_ERROR, "RtlLoadLibraryInMemoryEx Invalid parameters!");
        return nullptr;
    }
    RtlMemory tagPe(dwSize);
    if (lpFileName) {
        tagPe.m_wstrFileName = lpFileName;
    }
    if (lpFullName) {
        tagPe.m_wstrFullPath = lpFullName;
    }
    // Acquire Loader Lock (LdrpLoaderLock)
    // Critical section to protect FixImportTable during DLL loading across threads
    RtlLockLoaderLock();
    HMODULE hResult = (HMODULE)tagPe.RunImage(lpBytes, dwSize, dwFlags);
    RtlUnlockLoaderLock();
    return hResult;
}

PVOID __stdcall RtlGetSectionDataA(__in HMODULE hMod, __in LPCSTR lpSecName)
{
    if (!hMod) {
        DebugPrint(LEVEL_ERROR, "RtlGetSectionDataA::Invalid hMod");
        return nullptr;
    }
    if (!lpSecName) {
        DebugPrint(LEVEL_ERROR, "RtlGetSectionDataA::Invalid lpSecName");
        return nullptr;
    }
    PBYTE pBase = (PBYTE)hMod;

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pBase;
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE) {
        DebugPrint(LEVEL_ERROR, "RtlGetSectionDataA::Invalid DOS signature");
        return nullptr;
    }
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pBase + pDos->e_lfanew);
    if (pNt->Signature != IMAGE_NT_SIGNATURE) {
        DebugPrint(LEVEL_ERROR, "RtlGetSectionDataA::Invalid NT signature");
        return nullptr;
    }

    WORD wSecCount = pNt->FileHeader.NumberOfSections;
    PIMAGE_SECTION_HEADER pSec = IMAGE_FIRST_SECTION(pNt);

    for (WORD i = 0; i < wSecCount; i++, pSec++) {
        if (!_strnicmp((LPCSTR)pSec->Name, lpSecName, 8)) {
            PVOID pSecData = pBase + pSec->VirtualAddress;

            DebugPrint(LEVEL_INFO, "RtlGetSectionDataA::Found section: %s", pSec->Name);
            return pSecData;
        }
    }

    DebugPrint(LEVEL_ERROR, "RtlGetSectionDataA::Section not found: %s", lpSecName);
    return nullptr;
}

PVOID __stdcall RtlGetSectionDataW(__in HMODULE hMod, __in LPCWSTR lpSecName)
{
    if (!hMod) {
        DebugPrint(LEVEL_ERROR, "RtlGetSectionDataW::Invalid hMod");
        return nullptr;
    }
    if (!lpSecName) {
        DebugPrint(LEVEL_ERROR, "RtlGetSectionDataW::Invalid lpSecName");
        return nullptr;
    }
    return  RtlGetSectionDataA(hMod, WideToNarrow(lpSecName).c_str());
}

PVOID __stdcall RtlGetEntryPoint(__in HMODULE hMod)
{
    if (!hMod) {
        DebugPrint(LEVEL_ERROR, "RtlGetEntryPoint::Invalid module handle!");
        return nullptr;
    }
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)hMod;

    if (pDosHdr->e_magic != IMAGE_DOS_SIGNATURE) {
        DebugPrint(LEVEL_ERROR, "RtlGetEntryPoint::Invalid DOS header!\n");
        return nullptr;
    }

    PIMAGE_NT_HEADERS pNtHdrs = (PIMAGE_NT_HEADERS)((BYTE*)hMod + pDosHdr->e_lfanew);

    if (pNtHdrs->Signature != IMAGE_NT_SIGNATURE) {
        DebugPrint(LEVEL_ERROR, "RtlGetEntryPoint::Invalid NT header!\n");
        return nullptr;
    }

    DWORD dwEntryPointRva = pNtHdrs->OptionalHeader.AddressOfEntryPoint;
    if (!dwEntryPointRva) {
        DebugPrint(LEVEL_WARN, "Empty Entry Point!");
        return nullptr;
    }
    PVOID pEntryPoint = (PVOID)((BYTE*)hMod + dwEntryPointRva);

    return pEntryPoint;
}

static bool IsMemoryReadable(__in DWORD dwProtect)
{
    switch (dwProtect)
    {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;

    case PAGE_NOACCESS:
    case PAGE_GUARD:
    default:
        return false;
    }
}
const BOOL FindPattern(
    __in const BYTE* pData,
    __in size_t nDataSize,
    __in const BYTE* pPattern,
    __in size_t nPatternSize,
    __in const char* pszMask,
    __out BYTE** ppRet
) {
    if (!pData || !pPattern || !pszMask || !ppRet) return FALSE;
    if (nPatternSize == 0 || nDataSize < nPatternSize) return FALSE;

    // Boyer-Moore bad character skip table
    size_t nAryBadCharSkip[256] = {};
    const size_t defaultSkip = nPatternSize;
    for (size_t i = 0; i < 256; ++i) {
        nAryBadCharSkip[i] = defaultSkip;
    }
    for (size_t j = 0; j < nPatternSize; ++j) {
        if (pszMask[j] != '?') {
            nAryBadCharSkip[pPattern[j]] = nPatternSize - 1 - j;
        }
    }

    const size_t nSearchLimit = nDataSize - nPatternSize;
    size_t i = 0;

    while (i <= nSearchLimit) {
        size_t j = nPatternSize;
        bool bMismatch = false;

        while (j-- > 0) {
            if (pszMask[j] != '?' && pData[i + j] != pPattern[j]) {
                bMismatch = true;
                break;
            }
        }

        if (!bMismatch) {
            *ppRet = const_cast<BYTE*>(&pData[i]);
            return TRUE;
        }

        BYTE ucBadChar = pData[i + j];
        size_t nSkip = nAryBadCharSkip[ucBadChar];
        i += (nSkip > 1) ? nSkip : 1;
    }

    return FALSE;
}
SIZE_T GetContinuousReadableMemorySize(__in LPVOID address)
{
    MEMORY_BASIC_INFORMATION tagMemInfo = { 0 };

    if (VirtualQuery(address, &tagMemInfo, sizeof(tagMemInfo)) == 0)
    {
        DebugPrint(LEVEL_ERROR, "VirtualQuery failed: %s\n", GetLastErrorAsString().c_str());
        return 0;
    }

    if (tagMemInfo.State != MEM_COMMIT || !IsMemoryReadable(tagMemInfo.Protect))
    {
        return 0;
    }

    const LPVOID pAllocationBase = tagMemInfo.AllocationBase;
    LPVOID pCurrentScanAddr = (LPVOID)((ULONG_PTR)tagMemInfo.BaseAddress + tagMemInfo.RegionSize);

    while (VirtualQuery(pCurrentScanAddr, &tagMemInfo, sizeof(tagMemInfo)) != 0)
    {
        const bool bSameAllocation = (tagMemInfo.AllocationBase == pAllocationBase);
        const bool bCommitted = (tagMemInfo.State == MEM_COMMIT);
        const bool bReadable = IsMemoryReadable(tagMemInfo.Protect);

        if (!bSameAllocation || !bCommitted || !bReadable)
            break;

        pCurrentScanAddr = (LPVOID)((ULONG_PTR)pCurrentScanAddr + tagMemInfo.RegionSize);
    }

    return (SIZE_T)((ULONG_PTR)pCurrentScanAddr - (ULONG_PTR)pAllocationBase);
}

std::future<HMODULE> AsyncRtlLoadLibraryA(__in LPCSTR lpcLibFileName) {

    std::promise<HMODULE> tagPromise;
    std::future<HMODULE> tagFuture = tagPromise.get_future();
    if (!lpcLibFileName) {
        tagPromise.set_value(nullptr);
        return  tagFuture;
    }
    std::thread([lpcLibFileName, p = std::move(tagPromise)]() mutable {
        HMODULE hMod = RtlLoadLibraryA(lpcLibFileName);
        p.set_value(hMod);
    }).detach();

    return tagFuture;
}

bool IsApiSetName(__in const std::wstring& wstrName) {
    if (wstrName.empty()) {
        return false;
    }
    const std::wstring wstrPreFix1 = L"api-";
    const std::wstring wstrPreFix2 = L"ext-";
    const std::wstring wstrSuffix = L".dll";

    bool bPrefix = wstrName.compare(0, wstrPreFix1.size(), wstrPreFix1) == 0 ||
        wstrName.compare(0, wstrPreFix2.size(), wstrPreFix2) == 0;

    bool bSuffix = wstrName.size() >= wstrSuffix.size() &&
        wstrName.compare(wstrName.size() - wstrSuffix.size(), wstrSuffix.size(), wstrSuffix) == 0;

    bool bSecondDash = wstrName.find(L'-', 4) != std::wstring::npos;

    return bPrefix && bSuffix && bSecondDash;
}


BOOL FileExists(__in LPCWSTR lpwszPath)
{
    if (!lpwszPath) {
        return FALSE;
    }
	DWORD dwAttrib = GetFileAttributesW(lpwszPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring GetFileExtension(__in const std::wstring& wstrFilePath)
{
    if (wstrFilePath.empty()) return{};
	size_t nDotPos = wstrFilePath.find_last_of(L".");
	size_t nSlashPos = wstrFilePath.find_last_of(L"/\\");
	if (nDotPos != std::wstring::npos && (nSlashPos == std::wstring::npos || nDotPos > nSlashPos)) {
		return wstrFilePath.substr(nDotPos + 1);
	}
	return L"";
}

std::vector<std::wstring> GetSafeDirectories()
{
	std::vector<std::wstring> vecSafeDirs;
    wchar_t wszArySystemRoot[MAX_PATH]={ 0 };
	GetEnvironmentVariableW(L"SystemRoot", wszArySystemRoot, MAX_PATH);
	vecSafeDirs.push_back(std::wstring(wszArySystemRoot) + L"\\System32");
	vecSafeDirs.push_back(std::wstring(wszArySystemRoot) + L"\\System32\\downlevel");

	wchar_t wszAryProgramFiles[MAX_PATH] = { 0 };
	GetEnvironmentVariableW(L"ProgramFiles", wszAryProgramFiles, MAX_PATH);
	vecSafeDirs.push_back(std::wstring(wszAryProgramFiles) + L"\\SomeApp");

	wchar_t wszAryProgramFilesX86[MAX_PATH] = { 0 };
	GetEnvironmentVariableW(L"ProgramFiles(x86)", wszAryProgramFilesX86, MAX_PATH);
	vecSafeDirs.push_back(std::wstring(wszAryProgramFilesX86) + L"\\SomeApp");
	
	return vecSafeDirs;
}

std::pair<std::wstring, std::wstring> GetFileNameAndPath(__in const std::wstring& wstrFullPath)
{
    if (wstrFullPath.empty()) {
        DebugPrint(LEVEL_ERROR, "empty file name");
        return std::pair<std::wstring, std::wstring>{L"", L""};
    }
    size_t nSlashPos = wstrFullPath.find_last_of(L"/\\");

	std::wstring wstrFileName = (nSlashPos != std::wstring::npos) ? wstrFullPath.substr(nSlashPos + 1) : wstrFullPath;

	std::wstring wstrDirectoryPath = (nSlashPos != std::wstring::npos) ? wstrFullPath.substr(0, nSlashPos) : L"";

	return { wstrFileName, wstrDirectoryPath };
}


// ============ Loader Lock Management ============
// Wraps LdrpLoaderLock critical section.
// Must be held when calling FixImportTable during RtlLoadLibraryInMemoryEx
// to prevent concurrent DLL loading from corrupting PE fixups.

void WINAPI RtlLockLoaderLock()
{
    CRITICAL_SECTION* pLock = GetLorderLock();
    if (pLock) {
        EnterCriticalSection(pLock);
    }
}

void WINAPI RtlUnlockLoaderLock()
{
    CRITICAL_SECTION* pLock = GetLorderLock();
    if (pLock) {
        LeaveCriticalSection(pLock);
    }
}
