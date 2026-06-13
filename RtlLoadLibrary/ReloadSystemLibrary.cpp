#include"pch.h"
#include <mutex>
#include "ReloadSystemLibrary.h"
#include "KernelStruct.h"

HMODULE CopySystemDllA(
    __in const char* lpszDllName)
{
    if (!lpszDllName)return nullptr;
	std::wstring wstrLibraryName = NarrowToWide(lpszDllName);
	if (wstrLibraryName.empty()) return nullptr;
	return CopySystemDllW(wstrLibraryName.c_str());
}
//                              
static bool ValidateDllName(
    __in const wchar_t* lpwszDllName) 
{
    if (!lpwszDllName) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: nullptr DLL name provided");
        return false;
    }
    return true;
}

//                                            
static HMODULE CheckModuleCache(
    __in const wchar_t* lpwszDllName,
    __inout std::wstring& outLowerName) 
{
    UNREFERENCED_PARAMETER(lpwszDllName);
    UNREFERENCED_PARAMETER(outLowerName);
    return nullptr;
}

//                         
static bool GetModuleInfo(
    __in const wchar_t* lpwszDllName,
    __inout HMODULE& hOutModule,
    __inout MODULEINFO& outModInfo) 
{
    static PMY_PEB_LDR_DATA pLdr = GetCurrentLdrData();
    if (pLdr) {
        PLIST_ENTRY pEntry = pLdr->InLoadOrderModuleList.Flink;
        while (pEntry != &pLdr->InLoadOrderModuleList) {
            PMY_LDR_DATA_TABLE_ENTRY pLdrEntry =
                CONTAINING_RECORD(pEntry, MY_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
            if (pLdrEntry && pLdrEntry->BaseDllName.Buffer) {
                int nLen = pLdrEntry->BaseDllName.Length / sizeof(wchar_t);
                if (nLen > 0 && nLen < MAX_PATH) {
                    wchar_t wAryModuleName[MAX_PATH] = {0};
                    memcpy(wAryModuleName, pLdrEntry->BaseDllName.Buffer, nLen * sizeof(wchar_t));
                    wAryModuleName[nLen] = L'\0';
                    if (_wcsicmp(wAryModuleName, lpwszDllName) == 0) {
                        hOutModule = (HMODULE)pLdrEntry->DllBase;
                        outModInfo.lpBaseOfDll = pLdrEntry->DllBase;
                        outModInfo.SizeOfImage = pLdrEntry->SizeOfImage;
                        outModInfo.EntryPoint = pLdrEntry->EntryPoint;
                        return true;
                    }
                }
            }
            pEntry = pEntry->Flink;
        }
    }

    hOutModule = GetModuleHandleW(lpwszDllName);
    if (!hOutModule) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: GetModuleHandleW failed for %ls",
            lpwszDllName);
        return false;
    }

    if (!GetModuleInformation(GetCurrentProcess(), hOutModule, &outModInfo, sizeof(MODULEINFO))) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: GetModuleInformation failed for %ls",
            lpwszDllName);
        return false;
    }

    return true;
}

    //         PE  
static PIMAGE_NT_HEADERS ValidatePEHeaders(
    __in BYTE* pBaseAddress,
    __in const wchar_t* lpwszDllName,
    __inout BOOL& outIs64Bit) 
{
    UNREFERENCED_PARAMETER(lpwszDllName);
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pBaseAddress;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: Invalid DOS header for %ls", lpwszDllName);
        return nullptr;
    }

    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(pBaseAddress + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: Invalid NT headers for %ls", lpwszDllName);
        return nullptr;
    }

    //               32              64  PE
    if (pNtHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        outIs64Bit = TRUE;
    }
    else if (pNtHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        outIs64Bit = FALSE;
    }
    else {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: Unknown PE format for %ls", lpwszDllName);
        return nullptr;
    }

    return pNtHeaders;
}

//                                       
static DWORD GetSectionProtection(__in DWORD dwCharacteristics)
{
    DWORD dwProtect = PAGE_NOACCESS;

    if (dwCharacteristics & IMAGE_SCN_MEM_EXECUTE) {
        if (dwCharacteristics & IMAGE_SCN_MEM_WRITE) {
            dwProtect = PAGE_EXECUTE_READWRITE;
        }
        else {
            dwProtect = PAGE_EXECUTE_READ;
        }
    }
    else if (dwCharacteristics & IMAGE_SCN_MEM_WRITE) {
        dwProtect = PAGE_READWRITE;
    }
    else {
        dwProtect = PAGE_READONLY;
    }

    return dwProtect;
}

//                                   
static BYTE* AllocateModuleMemory(
    __in SIZE_T nImageSize,
    __in const wchar_t* lpwszDllName,
    __in BYTE* pOldBase,
    __in PIMAGE_NT_HEADERS pNtHeaders)
{
    UNREFERENCED_PARAMETER(lpwszDllName);
    UNREFERENCED_PARAMETER(pOldBase);
    UNREFERENCED_PARAMETER(pNtHeaders);
    if (nImageSize == 0 || nImageSize > 512 * 1024 * 1024) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: Invalid image size %zu for %ls",
            nImageSize, lpwszDllName);
        return nullptr;
    }

    BYTE* pNewModule = (BYTE*)VirtualAlloc(nullptr, nImageSize,
        MEM_RESERVE,
        PAGE_NOACCESS);
    if (!pNewModule) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: VirtualAlloc reserve failed for %ls (size: %zu)",
            lpwszDllName, nImageSize);
        return nullptr;
    }

    SIZE_T nHeadersSize = pNtHeaders->OptionalHeader.SizeOfHeaders;
    if (!VirtualAlloc(pNewModule, nHeadersSize, MEM_COMMIT, PAGE_READWRITE)) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: VirtualAlloc commit headers failed for %ls",
            lpwszDllName);
        VirtualFree(pNewModule, 0, MEM_RELEASE);
        return nullptr;
    }

    PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
    WORD wNumberOfSections = pNtHeaders->FileHeader.NumberOfSections;

    for (WORD i = 0; i < wNumberOfSections; i++) {
        DWORD dwVirtualAddress = pSectionHeader[i].VirtualAddress;
        DWORD dwVirtualSize = pSectionHeader[i].Misc.VirtualSize;

        if (dwVirtualSize == 0) continue;
        if (dwVirtualAddress >= nImageSize) continue;
        if (dwVirtualAddress + dwVirtualSize > nImageSize) {
            dwVirtualSize = (DWORD)(nImageSize - dwVirtualAddress);
        }

        if (!VirtualAlloc(pNewModule + dwVirtualAddress, dwVirtualSize, MEM_COMMIT, PAGE_READWRITE)) {
            DebugPrint(LEVEL_ERROR, "CopySystemDllW: VirtualAlloc commit section %d failed for %ls",
                i, lpwszDllName);
            VirtualFree(pNewModule, 0, MEM_RELEASE);
            return nullptr;
        }
    }

    return pNewModule;
}

//             PE  
static bool CopyPEHeaders(
    __inout BYTE* pNewModule,
    __in BYTE* pOldBase,
    __in PIMAGE_NT_HEADERS pNtHeaders,
    __in SIZE_T nImageSize,
    __in const wchar_t* lpwszDllName) 
{
    UNREFERENCED_PARAMETER(lpwszDllName);
    SIZE_T nHeadersSize = pNtHeaders->OptionalHeader.SizeOfHeaders;
    if (nHeadersSize > nImageSize) {
        DebugPrint(LEVEL_ERROR, "CopySystemDllW: Invalid headers size for %ls", lpwszDllName);
        return false;
    }
    memcpy(pNewModule, pOldBase, nHeadersSize);
    return true;
}

//                                                                             
static BYTE* MapCleanDllFromDisk(__in const wchar_t* lpwszDllName, __inout SIZE_T& outImageSize)
{
    wchar_t wszSystemPath[MAX_PATH] = { 0 };
    
    //                 
    if (GetSystemDirectoryW(wszSystemPath, MAX_PATH) == 0) {
        return nullptr;
    }
    
    //                                 
    wcscat_s(wszSystemPath, MAX_PATH, L"\\");
    wcscat_s(wszSystemPath, MAX_PATH, lpwszDllName);
    
    //               
    HANDLE hFile = CreateFileW(wszSystemPath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    
    //                         
    DWORD dwFileSize = GetFileSize(hFile, nullptr);
    if (dwFileSize == INVALID_FILE_SIZE || dwFileSize == 0) {
        CloseHandle(hFile);
        return nullptr;
    }
    
    //                                           
    BYTE* pFileBuffer = (BYTE*)VirtualAlloc(nullptr, dwFileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pFileBuffer) {
        CloseHandle(hFile);
        return nullptr;
    }
    
    DWORD dwBytesRead = 0;
    if (!ReadFile(hFile, pFileBuffer, dwFileSize, &dwBytesRead, nullptr) || dwBytesRead != dwFileSize) {
        VirtualFree(pFileBuffer, 0, MEM_RELEASE);
        CloseHandle(hFile);
        return nullptr;
    }
    
    CloseHandle(hFile);
    
    //         PE  
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pFileBuffer;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        VirtualFree(pFileBuffer, 0, MEM_RELEASE);
        return nullptr;
    }
    
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(pFileBuffer + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        VirtualFree(pFileBuffer, 0, MEM_RELEASE);
        return nullptr;
    }
    
    //                      
    SIZE_T nImageSize = pNtHeaders->OptionalHeader.SizeOfImage;
    
    //                             
    BYTE* pImageBase = (BYTE*)VirtualAlloc(nullptr, nImageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pImageBase) {
        VirtualFree(pFileBuffer, 0, MEM_RELEASE);
        return nullptr;
    }
    
    //             PE  
    SIZE_T nHeadersSize = pNtHeaders->OptionalHeader.SizeOfHeaders;
    memcpy(pImageBase, pFileBuffer, nHeadersSize);
    
    //                           
    PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
    WORD wNumberOfSections = pNtHeaders->FileHeader.NumberOfSections;
    
    for (WORD i = 0; i < wNumberOfSections; i++) {
        if (pSectionHeader[i].SizeOfRawData == 0) continue;
        
        BYTE* pDest = pImageBase + pSectionHeader[i].VirtualAddress;
        BYTE* pSrc = pFileBuffer + pSectionHeader[i].PointerToRawData;
        DWORD dwCopySize = min(pSectionHeader[i].SizeOfRawData, pSectionHeader[i].Misc.VirtualSize);
        
        memcpy(pDest, pSrc, dwCopySize);
    }
    
    //                                   
    VirtualFree(pFileBuffer, 0, MEM_RELEASE);
    
    outImageSize = nImageSize;
    return pImageBase;
}


static void ProcessRelocationEntry(
    __in WORD wRelocEntry,
    __in DWORD dwBlockRVA,
    __inout BYTE* pNewModule,
    __in DWORD_PTR dwptrOldBase,
    __in DWORD_PTR dwptrNewBase,
    __in DWORD_PTR dwptrOriginalImageBase,
    __in SIZE_T nImageSize,
    __in const wchar_t* lpwszDllName);

static void RelocateCleanImageToBase(
    __inout BYTE* pCleanBase,
    __in SIZE_T nCleanImageSize,
    __in DWORD_PTR dwptrTargetBase,
    __in const wchar_t* lpwszDllName)
{
    PIMAGE_DOS_HEADER pCleanDos = (PIMAGE_DOS_HEADER)pCleanBase;
    PIMAGE_NT_HEADERS pCleanNt = (PIMAGE_NT_HEADERS)(pCleanBase + pCleanDos->e_lfanew);
    PIMAGE_DATA_DIRECTORY pRelocDir =
        &pCleanNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (pRelocDir->Size == 0 || pRelocDir->VirtualAddress == 0) {
        return;
    }

    PIMAGE_BASE_RELOCATION pReloc =
        (PIMAGE_BASE_RELOCATION)(pCleanBase + pRelocDir->VirtualAddress);
    DWORD_PTR dwptrOriginalImageBase = pCleanNt->OptionalHeader.ImageBase;

    DWORD dwRelocBlockCount = 0;
    const DWORD dwMAX_RELOC_BLOCKS = 10000;

    while (pReloc->VirtualAddress != 0 &&
        pReloc->SizeOfBlock > 0 &&
        dwRelocBlockCount < dwMAX_RELOC_BLOCKS) {
        dwRelocBlockCount++;

        if ((BYTE*)pReloc + pReloc->SizeOfBlock > pCleanBase + nCleanImageSize) {
            break;
        }

        if (pReloc->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION)) {
            pReloc = (PIMAGE_BASE_RELOCATION)((BYTE*)pReloc + sizeof(IMAGE_BASE_RELOCATION));
            continue;
        }

        DWORD dwNumEntries = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        WORD* pRelocData = (WORD*)((BYTE*)pReloc + sizeof(IMAGE_BASE_RELOCATION));

        for (DWORD j = 0; j < dwNumEntries; j++) {
            ProcessRelocationEntry(pRelocData[j], pReloc->VirtualAddress,
                pCleanBase, 0, dwptrTargetBase,
                dwptrOriginalImageBase, nCleanImageSize, lpwszDllName);
        }

        pReloc = (PIMAGE_BASE_RELOCATION)((BYTE*)pReloc + pReloc->SizeOfBlock);
    }
}

static void CopySections(
    __inout BYTE* pNewModule,
    __in BYTE* pOldBase,
    __in PIMAGE_NT_HEADERS pNtHeaders,
    __in SIZE_T nImageSize,
    __in const wchar_t* lpwszDllName) 
{
    UNREFERENCED_PARAMETER(lpwszDllName);
    PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
    WORD wNumberOfSections = pNtHeaders->FileHeader.NumberOfSections;

    SIZE_T nCleanImageSize = 0;
    BYTE* pCleanBase = MapCleanDllFromDisk(lpwszDllName, nCleanImageSize);

    if (pCleanBase) {
        RelocateCleanImageToBase(pCleanBase, nCleanImageSize,
            (DWORD_PTR)pOldBase, lpwszDllName);
    }

    for (WORD i = 0; i < wNumberOfSections; i++) {
        DWORD dwVirtualAddress = pSectionHeader[i].VirtualAddress;
        DWORD dwVirtualSize = pSectionHeader[i].Misc.VirtualSize;

        if (dwVirtualSize == 0) continue;
        if (dwVirtualAddress >= nImageSize) continue;

        DWORD dwEffectiveSize = dwVirtualSize;
        if (dwVirtualAddress + dwVirtualSize > nImageSize) {
            DebugPrint(LEVEL_WARN,
                "CopySystemDllW: Section %d exceeds image boundary, truncating", i);
            dwEffectiveSize = (DWORD)(nImageSize - dwVirtualAddress);
        }

        BYTE* pDest = pNewModule + dwVirtualAddress;
        BYTE* pSrc = pOldBase + dwVirtualAddress;

        memcpy(pDest, pSrc, dwEffectiveSize);

        if (pCleanBase
            && (pSectionHeader[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)
            && dwVirtualAddress < nCleanImageSize) {
            BYTE* pCleanSrc = pCleanBase + dwVirtualAddress;
            DWORD dwCleanAvail = (DWORD)(nCleanImageSize - dwVirtualAddress);
            DWORD dwScanSize = min(dwEffectiveSize, dwCleanAvail);
            _Analysis_assume_(dwScanSize <= dwEffectiveSize);

            DWORD dwFixedCount = 0;
            for (DWORD j = 0; j < dwScanSize; j++) {
                if (pDest[j] != pCleanSrc[j]) {
                    pDest[j] = pCleanSrc[j];
                    dwFixedCount++;
                }
            }

            if (dwFixedCount > 0) {
                DebugPrint(LEVEL_INFO,
                    "CopySystemDllW: Section %d fixed %u modified bytes from disk image",
                    i, dwFixedCount);
            }
        }
    }

    if (pCleanBase) {
        VirtualFree(pCleanBase, 0, MEM_RELEASE);
    }
}

//                                           
static void ProcessRelocationEntry(
    __in WORD wRelocEntry,
    __in DWORD dwBlockRVA,
    __inout BYTE* pNewModule,
    __in DWORD_PTR dwptrOldBase,
    __in DWORD_PTR dwptrNewBase,
    __in DWORD_PTR dwptrOriginalImageBase,
    __in SIZE_T nImageSize,
    __in const wchar_t* lpwszDllName)
{
    UNREFERENCED_PARAMETER(dwptrOldBase);
    UNREFERENCED_PARAMETER(lpwszDllName);
    if (wRelocEntry == 0) return;

    WORD wRelocType = wRelocEntry >> 12;
    WORD wRelocOffset = wRelocEntry & 0xFFF;
    DWORD_PTR dwptrPatchRVA = dwBlockRVA + wRelocOffset;

    if (dwptrPatchRVA >= nImageSize) {
        DebugPrint(LEVEL_WARN,
            "CopySystemDllW: Relocation RVA 0x%llx exceeds image size",
            (unsigned long long)dwptrPatchRVA);
        return;
    }

    DWORD_PTR dwptrPatchAddress = (DWORD_PTR)pNewModule + dwptrPatchRVA;

    switch (wRelocType) {
    case IMAGE_REL_BASED_DIR64:
        if (dwptrPatchRVA + sizeof(DWORD_PTR) <= nImageSize) {
            DWORD_PTR* pdwptrPatch = (DWORD_PTR*)dwptrPatchAddress;
            DWORD_PTR originalValue = *pdwptrPatch;
            *pdwptrPatch = dwptrNewBase + (originalValue - dwptrOriginalImageBase);
        }
        break;

    case IMAGE_REL_BASED_HIGHLOW:
        if (dwptrPatchRVA + sizeof(DWORD) <= nImageSize) {
            DWORD* pdwPatch = (DWORD*)dwptrPatchAddress;
            DWORD dwOriginalValue = *pdwPatch;
            DWORD dwOriginalImageBase32 = (DWORD)dwptrOriginalImageBase;
            *pdwPatch = (DWORD)(dwptrNewBase + (dwOriginalValue - dwOriginalImageBase32));
        }
        break;

    case IMAGE_REL_BASED_HIGH:
        if (dwptrPatchRVA + sizeof(WORD) <= nImageSize) {
            WORD* pwPatch = (WORD*)dwptrPatchAddress;
            WORD wOriginalValue = *pwPatch;
            WORD wOriginalImageBaseHigh = (WORD)(dwptrOriginalImageBase >> 16);
            *pwPatch = (WORD)((dwptrNewBase >> 16) + (wOriginalValue - wOriginalImageBaseHigh));
        }
        break;

    case IMAGE_REL_BASED_LOW:
        if (dwptrPatchRVA + sizeof(WORD) <= nImageSize) {
            WORD* pwPatch = (WORD*)dwptrPatchAddress;
            WORD wOriginalValue = *pwPatch;
            WORD wOriginalImageBaseLow = (WORD)(dwptrOriginalImageBase & 0xFFFF);
            *pwPatch = (WORD)((dwptrNewBase & 0xFFFF) + (wOriginalValue - wOriginalImageBaseLow));
        }
        break;

    case IMAGE_REL_BASED_ABSOLUTE:
        //                   
        break;

    default:
        DebugPrint(LEVEL_WARN,
            "CopySystemDllW: Unsupported relocation type %d for %ls",
            wRelocType, lpwszDllName);
        break;
    }
}

//                             
static void ProcessRelocations(
    __inout BYTE* pNewModule,
    __in BYTE* pOldBase,
    __in PIMAGE_NT_HEADERS pNtHeaders,
    __in SIZE_T nImageSize,
    __in const wchar_t* lpwszDllName)
{
    UNREFERENCED_PARAMETER(lpwszDllName);
    PIMAGE_DOS_HEADER pNewDosHeader = (PIMAGE_DOS_HEADER)pNewModule;
    PIMAGE_NT_HEADERS pNewNtHeaders = (PIMAGE_NT_HEADERS)(pNewModule + pNewDosHeader->e_lfanew);
    PIMAGE_DATA_DIRECTORY pRelocDir =
        &pNewNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (pRelocDir->Size == 0 || pRelocDir->VirtualAddress == 0) {
        return;
    }

    PIMAGE_BASE_RELOCATION pReloc =
        (PIMAGE_BASE_RELOCATION)(pNewModule + pRelocDir->VirtualAddress);
    DWORD_PTR dwptrOldBase = (DWORD_PTR)pOldBase;
    DWORD_PTR dwptrNewBase = (DWORD_PTR)pNewModule;
    DWORD_PTR originalImageBase = pNtHeaders->OptionalHeader.ImageBase;

    DWORD dwRelocBlockCount = 0;
    const DWORD dwMAX_RELOC_BLOCKS = 10000;

    while (pReloc->VirtualAddress != 0 &&
        pReloc->SizeOfBlock > 0 &&
        dwRelocBlockCount < dwMAX_RELOC_BLOCKS) {
        dwRelocBlockCount++;

        if ((BYTE*)pReloc + pReloc->SizeOfBlock > pNewModule + nImageSize) {
            DebugPrint(LEVEL_ERROR,
                "CopySystemDllW: Relocation block exceeds image boundary");
            break;
        }

        if (pReloc->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION)) {
            pReloc = (PIMAGE_BASE_RELOCATION)((BYTE*)pReloc + sizeof(IMAGE_BASE_RELOCATION));
            continue;
        }

        DWORD dwNumEntries = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        WORD* pRelocData = (WORD*)((BYTE*)pReloc + sizeof(IMAGE_BASE_RELOCATION));

        for (DWORD j = 0; j < dwNumEntries; j++) {
            ProcessRelocationEntry(pRelocData[j], pReloc->VirtualAddress,
                pNewModule, dwptrOldBase, dwptrNewBase,
                originalImageBase, nImageSize, lpwszDllName);
        }

        pReloc = (PIMAGE_BASE_RELOCATION)((BYTE*)pReloc + pReloc->SizeOfBlock);
    }
}

//                          
static void UpdateImageBase(
    __inout BYTE* pNewModule)
{
    PIMAGE_DOS_HEADER pNewDosHeader = (PIMAGE_DOS_HEADER)pNewModule;
    PIMAGE_NT_HEADERS pNewNtHeaders = (PIMAGE_NT_HEADERS)(pNewModule + pNewDosHeader->e_lfanew);
    pNewNtHeaders->OptionalHeader.ImageBase = (DWORD_PTR)pNewModule;
}

static void AdjustSectionProtections(
    __inout BYTE* pModule,
    __in PIMAGE_NT_HEADERS pNtHeaders,
    __in SIZE_T nImageSize,
    __in const wchar_t* lpwszDllName)
{
    UNREFERENCED_PARAMETER(lpwszDllName);
    PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
    WORD wNumberOfSections = pNtHeaders->FileHeader.NumberOfSections;

    for (WORD i = 0; i < wNumberOfSections; i++) {
        DWORD dwVirtualAddress = pSectionHeader[i].VirtualAddress;
        DWORD dwVirtualSize = pSectionHeader[i].Misc.VirtualSize;

        if (dwVirtualSize == 0) continue;
        if (dwVirtualAddress >= nImageSize) continue;
        if (dwVirtualAddress + dwVirtualSize > nImageSize) {
            dwVirtualSize = (DWORD)(nImageSize - dwVirtualAddress);
        }

        DWORD dwNewProtect = GetSectionProtection(pSectionHeader[i].Characteristics);
        DWORD dwOldProtect = 0;

        if (!VirtualProtect(pModule + dwVirtualAddress, dwVirtualSize, dwNewProtect, &dwOldProtect)) {
            DebugPrint(LEVEL_WARN,
                "CopySystemDllW: VirtualProtect section %d failed for %ls (err=%u)",
                i, lpwszDllName, GetLastError());
        }
    }
}
std::mutex g_CopyModulemtx;


HMODULE CopySystemDllW(
    __in const wchar_t* lpwszDllName) 
{
    //                     
    if (!ValidateDllName(lpwszDllName)) {
        return nullptr;
    }
    std::unique_lock<std::mutex> mtxLock(g_CopyModulemtx);//                                                             
    //                
    std::wstring wstrDllNameLower;
    HMODULE hCachedModule = CheckModuleCache(lpwszDllName, wstrDllNameLower);
    if (hCachedModule) {
        return hCachedModule;
    }

    //                         
    HMODULE hOldModule = nullptr;
    MODULEINFO tagModInfo = { 0 };
    if (!GetModuleInfo(lpwszDllName, hOldModule, tagModInfo)) {
        return nullptr;
    }

    BYTE* pOldBase = (BYTE*)tagModInfo.lpBaseOfDll;
    if (!pOldBase) {
        return nullptr;
    }

    //         PE  
    BOOL b64Bit = FALSE;
    PIMAGE_NT_HEADERS pNtHeaders = ValidatePEHeaders(pOldBase, lpwszDllName, b64Bit);
    if (!pNtHeaders) {
        return nullptr;
    }

    SIZE_T nImageSize = pNtHeaders->OptionalHeader.SizeOfImage;

    //                     
    BYTE* pNewModule = AllocateModuleMemory(nImageSize, lpwszDllName, pOldBase, pNtHeaders);
    if (!pNewModule) {
        return nullptr;
    }

    //             PE  
    if (!CopyPEHeaders(pNewModule, pOldBase, pNtHeaders, nImageSize, lpwszDllName)) {
        VirtualFree(pNewModule, 0, MEM_RELEASE);
        return nullptr;
    }

    //                           
    CopySections(pNewModule, pOldBase, pNtHeaders, nImageSize, lpwszDllName);

    //                       
    ProcessRelocations(pNewModule, pOldBase, pNtHeaders, nImageSize, lpwszDllName);

    //              PE            ImageBase                               
    UpdateImageBase(pNewModule);

    AdjustSectionProtections(pNewModule, pNtHeaders, nImageSize, lpwszDllName);

    return (HMODULE)pNewModule;
}
