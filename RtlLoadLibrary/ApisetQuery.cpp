#include "pch.h"
#include "ApisetQuery.h"



/**
* @brief          API Set                                                              DLL                           string      
* @param apisetName API Set                      L"api-ms-win-core-file-l1-1-0.dll"      
* @return std::vector<std::string>          DLL                                  ["kernel32.dll"]      
*/

/**
* @brief          API Set                                                          DLL                           string      
* @param apisetName API Set                      "api-ms-win-core-file-l1-1-0.dll"      
* @return std::vector<std::string>          DLL                                  ["kernel32.dll"]      
*/


std::vector<std::string> ApisetQuery::QueryForwardDlls(__in const std::string& strApisetName) {
    std::wstring wstrName = NarrowToWide(strApisetName);
    return QueryForwardDlls(wstrName);
}
std::vector<std::string> ApisetQuery::QueryForwardDlls(__in const std::wstring& wstrApisetName) {
    std::wstring wstrNormName = NormalizeName(wstrApisetName);

    //         1:                            
    auto it = m_ApiSetMap.find(wstrNormName);
    if (it != m_ApiSetMap.end()) {
        //                  -                             
        std::vector<std::string> vecResult;
        vecResult.reserve(it->second.size());
        for (const auto& ws : it->second) {
            vecResult.emplace_back(WideToNarrow(ws));
        }
        return vecResult;
    }

    //         2:                
    static std::unordered_map<std::wstring, std::vector<std::string>> mapLongestMatchCache;
    auto cacheIt = mapLongestMatchCache.find(wstrNormName);
    if (cacheIt != mapLongestMatchCache.end()) {
        return cacheIt->second;
    }

    //         3:                       
    size_t nPrefixLen = (wstrNormName.size() < PREFIX_INDEX_LEN)
        ? wstrNormName.size() : PREFIX_INDEX_LEN;
    std::wstring wstrQueryPrefix = wstrNormName.substr(0, nPrefixLen);

    auto bucketIt = m_PrefixIndex.find(wstrQueryPrefix);

    size_t nMaxPrefix = 0;
    const std::vector<std::wstring>* pvecBestValue = nullptr;
    size_t nNormSize = wstrNormName.size();

    if (bucketIt != m_PrefixIndex.end()) {
        for (const std::wstring* pKey : bucketIt->second) {
            size_t keySize = pKey->size();
            size_t minLen = (keySize < nNormSize) ? keySize : nNormSize;

            size_t nLen = 0;
            while (nLen < minLen && (*pKey)[nLen] == wstrNormName[nLen])
                ++nLen;

            if (nLen > nMaxPrefix) {
                nMaxPrefix = nLen;
                auto mapIt = m_ApiSetMap.find(*pKey);
                if (mapIt != m_ApiSetMap.end())
                    pvecBestValue = &mapIt->second;
            }
        }
    }

    std::vector<std::string> vecResult;

    //                       
    const double dblSimilarity = (double)nMaxPrefix / nNormSize;
    if (pvecBestValue && dblSimilarity >= 0.5) {
        vecResult.reserve(pvecBestValue->size());
        for (const auto& ws : *pvecBestValue) {
            vecResult.emplace_back(WideToNarrow(ws));
        }

        //         4:                   
        mapLongestMatchCache[wstrNormName] = vecResult;
    }

    return vecResult;
}


//                                         
std::wstring ApisetQuery::NormalizeName(__in const std::wstring& _wstrName) {
    std::wstring wstrName = _wstrName;
    for (auto& wchar : wstrName) wchar = towlower(wchar);
    return wstrName;
}

BOOL ApisetQuery::Init(__in const std::wstring& wstrSchemaPath) {
#ifndef _WIN64
    // 32                              FS               ,                       System32
    Wow64DisableWow64FsRedirection(&m_pOldValue);
#endif

    //        schema         
    m_hFile = CreateFileW(wstrSchemaPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (static_cast<HANDLE>(m_hFile) == INVALID_HANDLE_VALUE) {
        DebugPrint(LEVEL_ERROR, "ApisetQuery::Init Create file error! Path: %ws, Error: 0x%08X", wstrSchemaPath.c_str(), GetLastError());
#ifndef _WIN64
        Wow64RevertWow64FsRedirection(m_pOldValue);
#endif
        return FALSE;
    }

    FileMap tagMapFile(static_cast<HANDLE>(m_hFile));
    if (!tagMapFile) {
        DebugPrint(LEVEL_ERROR, "ApisetQuery::Init Create file map error! Error: 0x%08X", GetLastError());
        CloseHandle(static_cast<HANDLE>(m_hFile));
#ifndef _WIN64
        Wow64RevertWow64FsRedirection(m_pOldValue);
#endif
        return FALSE;
    }

    //                               
    m_lpBase = tagMapFile.GetMap();
    if (!m_lpBase) {
        DebugPrint(LEVEL_ERROR, "ApisetQuery::Init Get map view error!");
        CloseHandle(static_cast<HANDLE>(m_hFile));
#ifndef _WIN64
        Wow64RevertWow64FsRedirection(m_pOldValue);
#endif
        return FALSE;
    }

    //              DOS   
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)m_lpBase;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        DebugPrint(LEVEL_ERROR, "ApisetQuery::Init Invalid DOS signature: 0x%04X (expected 0x%04X)",
            pDosHeader->e_magic, IMAGE_DOS_SIGNATURE);
        CloseHandle(static_cast<HANDLE>(m_hFile));
#ifndef _WIN64
        Wow64RevertWow64FsRedirection(m_pOldValue);
#endif
        return FALSE;
    }

    //              NT   
    PIMAGE_NT_HEADERS64 pNtHeader = (PIMAGE_NT_HEADERS64)((BYTE*)m_lpBase + pDosHeader->e_lfanew);
    if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
        DebugPrint(LEVEL_ERROR, "ApisetQuery::Init Invalid NT signature: 0x%08X (expected 0x%08X)",
            pNtHeader->Signature, IMAGE_NT_SIGNATURE);
        CloseHandle(static_cast<HANDLE>(m_hFile));
#ifndef _WIN64
        Wow64RevertWow64FsRedirection(m_pOldValue);
#endif
        return FALSE;
    }

    DebugPrint(LEVEL_INFO, "ApisetQuery::Init PE file loaded, NumberOfSections: %d", pNtHeader->FileHeader.NumberOfSections);

    //                       
    PIMAGE_SECTION_HEADER pSection = (PIMAGE_SECTION_HEADER)((BYTE*)pNtHeader +
        sizeof(IMAGE_NT_HEADERS64) - sizeof(IMAGE_OPTIONAL_HEADER64) +
        pNtHeader->FileHeader.SizeOfOptionalHeader);

    //              .apiset       
    PIMAGE_SECTION_HEADER pTargetSection = nullptr;
    for (int i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++) {
        if (memcmp(pSection[i].Name, ".apiset", 7) == 0) {
            pTargetSection = &pSection[i];
            DebugPrint(LEVEL_INFO, "Found .apiset section at index %d, RawData: 0x%08X, Size: 0x%08X",
                i, pTargetSection->PointerToRawData, pTargetSection->SizeOfRawData);
            break;
        }
    }

    if (!pTargetSection) {
        DebugPrint(LEVEL_ERROR, ".apiset section not found in PE file");
        CloseHandle(static_cast<HANDLE>(m_hFile));
#ifndef _WIN64
        Wow64RevertWow64FsRedirection(m_pOldValue);
#endif
        return FALSE;
    }

    //          API Set                   
    PUCHAR pApiSet = (PUCHAR)m_lpBase + pTargetSection->PointerToRawData;
    PAPI_SET_NAMESPACE pNamespace = (PAPI_SET_NAMESPACE)pApiSet;

    DebugPrint(LEVEL_INFO, "API Set Namespace Version: %d, Count: %d",
        pNamespace->Version, pNamespace->Count);

    UINT_PTR unptrNamespaceBase = (UINT_PTR)pNamespace;
    PAPI_SET_NAMESPACE_ENTRY pNamespaceEntry =
        (PAPI_SET_NAMESPACE_ENTRY)(unptrNamespaceBase + pNamespace->EntryOffset);

    //                          API Set       ,          apiSetMap
    DWORD dwSuccessCount = 0;
    DWORD dwEmptyCount = 0;

    for (DWORD i = 0; i < pNamespace->Count; i++) {
        //                - NameOffset
        if (pNamespaceEntry->NameOffset >= pTargetSection->SizeOfRawData) {
            DebugPrint(LEVEL_ERROR, "Invalid NameOffset at entry %d: 0x%08X (section size: 0x%08X)",
                i, pNamespaceEntry->NameOffset, pTargetSection->SizeOfRawData);
            pNamespaceEntry++;
            continue;
        }

        //          API Set             
        WCHAR* pwszApiName = (WCHAR*)(unptrNamespaceBase + pNamespaceEntry->NameOffset);
        if (!pwszApiName) {
            DebugPrint(LEVEL_ERROR, "API name pointer is nullptr at entry %d", i);
            pNamespaceEntry++;
            continue;
        }

        //          NameLength
        if (pNamespaceEntry->NameLength == 0 || pNamespaceEntry->NameLength > 260 * sizeof(WCHAR)) {
            DebugPrint(LEVEL_ERROR, "Invalid NameLength at entry %d: %d bytes",
                i, pNamespaceEntry->NameLength);
            pNamespaceEntry++;
            continue;
        }

        //                - NameOffset + NameLength
        if (pNamespaceEntry->NameOffset + pNamespaceEntry->NameLength > pTargetSection->SizeOfRawData) {
            DebugPrint(LEVEL_ERROR, "Name data exceeds section boundary at entry %d", i);
            pNamespaceEntry++;
            continue;
        }

        std::wstring wstrApiSetName(pwszApiName, pNamespaceEntry->NameLength / sizeof(WCHAR));
        wstrApiSetName += L".dll"; //          .dll         

        std::vector<std::wstring> vecDlls;

        //          ValueOffset        ValueCount
        if (pNamespaceEntry->ValueCount > 0) {
            if (pNamespaceEntry->ValueOffset >= pTargetSection->SizeOfRawData) {
                DebugPrint(LEVEL_ERROR, "Invalid ValueOffset at entry %d: 0x%08X",
                    i, pNamespaceEntry->ValueOffset);
                pNamespaceEntry++;
                continue;
            }

            //                              DLL
            PAPI_SET_VALUE_ENTRY pValueEntry =
                (PAPI_SET_VALUE_ENTRY)(unptrNamespaceBase + pNamespaceEntry->ValueOffset);

            for (DWORD j = 0; j < pNamespaceEntry->ValueCount; j++) {
                //                      Value         (ValueLength    0                 ,                      )
                if (pValueEntry->ValueLength == 0) {
                    DebugPrint(LEVEL_DEBUG, "  [%d.%d] %ws -> (empty mapping, skipped)",
                        i, j, wstrApiSetName.c_str());
                    pValueEntry++;
                    continue;
                }

                //          ValueLength                     
                if (pValueEntry->ValueLength > 260 * sizeof(WCHAR)) {
                    DebugPrint(LEVEL_ERROR, "Invalid ValueLength at entry %d, value %d: %d bytes",
                        i, j, pValueEntry->ValueLength);
                    pValueEntry++;
                    continue;
                }

                //          ValueOffset         
                if (pValueEntry->ValueOffset >= pTargetSection->SizeOfRawData) {
                    DebugPrint(LEVEL_ERROR, "Invalid ValueOffset at entry %d, value %d: 0x%08X",
                        i, j, pValueEntry->ValueOffset);
                    pValueEntry++;
                    continue;
                }

                //          ValueOffset + ValueLength               
                if (pValueEntry->ValueOffset + pValueEntry->ValueLength > pTargetSection->SizeOfRawData) {
                    DebugPrint(LEVEL_ERROR, "Value data exceeds section boundary at entry %d, value %d",
                        i, j);
                    pValueEntry++;
                    continue;
                }

                WCHAR* pwszTargetDll = (WCHAR*)(unptrNamespaceBase + pValueEntry->ValueOffset);
                if (!pwszTargetDll) {
                    DebugPrint(LEVEL_ERROR, "Target DLL pointer is nullptr at entry %d, value %d", i, j);
                    pValueEntry++;
                    continue;
                }

                std::wstring wstrTargetDllName(pwszTargetDll, pValueEntry->ValueLength / sizeof(WCHAR));
                vecDlls.emplace_back(wstrTargetDllName);

                DebugPrint(LEVEL_DEBUG, "  [%d.%d] %ws -> %ws",
                    i, j, wstrApiSetName.c_str(), wstrTargetDllName.c_str());

                pValueEntry++;
            }
        }

        //                                                       apiSetMap
        if (!vecDlls.empty()) {
            m_ApiSetMap.insert(std::make_pair(NormalizeName(wstrApiSetName), vecDlls));
            dwSuccessCount++;
        }
        else {
            dwEmptyCount++;
            DebugPrint(LEVEL_DEBUG, "Skipping empty API Set: %ws", wstrApiSetName.c_str());
        }

        pNamespaceEntry++;
    }

    DebugPrint(LEVEL_INFO, "API Set initialization complete: %d entries loaded, %d empty entries skipped, Total processed: %d/%d",
        dwSuccessCount, dwEmptyCount, dwSuccessCount + dwEmptyCount, pNamespace->Count);

    m_PrefixIndex.clear();
    for (const auto& entry : m_ApiSetMap) {
        size_t nPrefixLen = (entry.first.size() < PREFIX_INDEX_LEN)
            ? entry.first.size() : PREFIX_INDEX_LEN;
        std::wstring wstrPrefix = entry.first.substr(0, nPrefixLen);
        m_PrefixIndex[wstrPrefix].push_back(&entry.first);
    }

    m_bLoaded = true;

#ifndef _WIN64
    Wow64RevertWow64FsRedirection(m_pOldValue);
#endif

    return TRUE;
}
