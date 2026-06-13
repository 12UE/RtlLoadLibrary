// ApiSetResolve.h
// Resolves Windows API Set Schema to real DLL names

#pragma once
#ifndef _APISETRESOLVE_H_A62A08BF_8098_436A_9BD3_75AA561BD5A5
#define _APISETRESOLVE_H_A62A08BF_8098_436A_9BD3_75AA561BD5A5
#include"pch.h"


// 64-bit NT header section (used to locate .apiset section in PE)
typedef struct _IMAGE_SECTION_HEADER64 {
    BYTE  Name[8];
    union {
        DWORD PhysicalAddress;
        DWORD VirtualSize;
    } Misc;
    DWORD VirtualAddress;
    DWORD SizeOfRawData;
    DWORD PointerToRawData;
    DWORD PointerToRelocations;
    DWORD PointerToLinenumbers;
    WORD  NumberOfRelocations;
    WORD  NumberOfLinenumbers;
    DWORD Characteristics;
} IMAGE_SECTION_HEADER64, * PIMAGE_SECTION_HEADER64;



// API Set Namespace header
typedef struct _API_SET_NAMESPACE {
    uint32_t Version;
    uint32_t Size;
    uint32_t Flags;
    uint32_t Count;
    uint32_t EntryOffset;
    uint32_t HashOffset;
    uint32_t HashFactor;
} API_SET_NAMESPACE, * PAPI_SET_NAMESPACE;

// API Set Namespace Entry
typedef struct _API_SET_NAMESPACE_ENTRY {
    uint32_t Flags;
    uint32_t NameOffset;
    uint32_t NameLength;
    uint32_t HashedLength;
    uint32_t ValueOffset;
    uint32_t ValueCount;
} API_SET_NAMESPACE_ENTRY, * PAPI_SET_NAMESPACE_ENTRY;

// API Set Value Entry (maps to target DLL)
typedef struct _API_SET_VALUE_ENTRY {
    uint32_t Flags;
    uint32_t NameOffset;
    uint32_t NameLength;
    uint32_t ValueOffset;
    uint32_t ValueLength;
} API_SET_VALUE_ENTRY, * PAPI_SET_VALUE_ENTRY;

// Parses apisetschema.dll to extract API Set mappings
class ApisetQuery:public Singleton<ApisetQuery> {

public:
    ~ApisetQuery() {
        
        
    }
    ApisetQuery() {
        WCHAR wszArySystemRoot[MAX_PATH] = { 0 };

        DWORD dwRet = GetEnvironmentVariableW(L"SystemRoot", wszArySystemRoot, MAX_PATH);
        if (dwRet == 0 || dwRet >= MAX_PATH) {
            DebugPrint(LEVEL_ERROR, "Failed to get SystemRoot\n");
            return;
        }

        WCHAR wszAryFullDllPath[MAX_PATH] = { 0 };
        swprintf_s(wszAryFullDllPath, MAX_PATH, L"%s\\System32\\apisetschema.dll", wszArySystemRoot);

        if (!Init(wszAryFullDllPath)) {
            DebugPrint(LEVEL_ERROR, "load apiset info failed");
        }
    }
    std::vector<std::string> QueryForwardDlls(__in const std::string& apisetName);

    std::vector<std::string> QueryForwardDlls(__in const std::wstring& apisetName);



    bool IsLoaded() const { return m_bLoaded; }

private:
    // API Set map: key = API Set name (lowercase), value = target DLL list
    std::unordered_map<std::wstring, std::vector<std::wstring>> m_ApiSetMap;

    std::unordered_map<std::wstring, std::vector<const std::wstring*>> m_PrefixIndex;
    static constexpr size_t PREFIX_INDEX_LEN = 24;

    LPVOID m_lpBase = nullptr;
    THANDLE m_hMapping = nullptr;
    THANDLE m_hFile = nullptr;
    PVOID m_pOldValue = nullptr;
    bool m_bLoaded = false;

    std::wstring NormalizeName(__in const std::wstring& name);

    BOOL Init(__in const std::wstring& schemaPath);
};

#endif // _APISETRESOLVE_H_A62A08BF_8098_436A_9BD3_75AA561BD5A5
