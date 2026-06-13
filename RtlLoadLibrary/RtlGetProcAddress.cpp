#include "pch.h"
#include "RtlGetProcAddress.h"
#include <variant>
#include <type_traits>
#include <shared_mutex>
#include "RtlGetModuleFileName.h"
#include "RtlGetModuleHandle.h"
#include "ApisetQuery.h"
#include "LdrLink.h"

using ExportNameMap = std::unordered_map<std::string, DWORD>;
static std::unordered_map<HMODULE, ExportNameMap> g_ModuleExportCache;
static std::shared_mutex g_ExportCacheMutex;

static const ExportNameMap* GetOrBuildExportCache(__in HMODULE hModule)
{
	{
		std::shared_lock<std::shared_mutex> lock(g_ExportCacheMutex);
		auto it = g_ModuleExportCache.find(hModule);
		if (it != g_ModuleExportCache.end())
			return &it->second;
	}

	ExportNameMap cache;
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hModule;
	PIMAGE_NT_HEADERS pNt = nullptr;
	PIMAGE_EXPORT_DIRECTORY pExp = nullptr;
	DWORD* pFuncs = nullptr;
	DWORD* pNames = nullptr;
	WORD* pOrds = nullptr;

	if (!pDos || pDos->e_magic != IMAGE_DOS_SIGNATURE)
		goto store_empty;

	pNt = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDos->e_lfanew);
	if (!pNt || pNt->Signature != IMAGE_NT_SIGNATURE)
		goto store_empty;

	{
		auto& expDir = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		if (expDir.VirtualAddress == 0 || expDir.Size == 0)
			goto store_empty;

		pExp = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hModule + expDir.VirtualAddress);
		if (!pExp || !pExp->NumberOfNames)
			goto store_empty;

		pFuncs = (DWORD*)((BYTE*)hModule + pExp->AddressOfFunctions);
		pNames = (DWORD*)((BYTE*)hModule + pExp->AddressOfNames);
		pOrds  = (WORD*)((BYTE*)hModule + pExp->AddressOfNameOrdinals);

		if (!pFuncs || !pNames || !pOrds)
			goto store_empty;

		for (DWORD i = 0; i < pExp->NumberOfNames; i++) {
			const char* pszName = (const char*)((BYTE*)hModule + pNames[i]);
			if (pszName) {
				DWORD dwRva = pFuncs[pOrds[i]];
				cache[pszName] = dwRva;
			}
		}
	}

store_empty:
	{
		std::unique_lock<std::shared_mutex> lock(g_ExportCacheMutex);
		auto [newIt, _] = g_ModuleExportCache.emplace(hModule, std::move(cache));
		return &newIt->second;
	}
}

int mystrcmp(__in const char* str1, __in const char* str2)
{
    
    unsigned char c1, c2;

    do {
        c1 = (unsigned char)*str1++;
        c2 = (unsigned char)*str2++;

        if (c1 == '\0')
            return c1 - c2;

    } while (c1 == c2);

    return c1 - c2;
}
void* RtlGetProcAddressImpl(
    __in HMODULE hModule,
    __in const char* pszProcName,
    __in int nDepth,
    __inout std::set<CallIdentifier>& sCallHistory
) {
    if (nDepth >= MAX_FORWARD_DEPTH) {
       DebugPrint(LEVEL_ERROR, "RtlGetProcAddress: Maximum recursion depth reached");
        return nullptr;
    }

    if (!hModule || !pszProcName) {
       DebugPrint(LEVEL_ERROR, "RtlGetProcAddress: Invalid parameters");
        return nullptr;
    }

    CallIdentifier tagCurrentCall{};
    tagCurrentCall.hModule = hModule;
    tagCurrentCall.functionName = ((ULONG_PTR)pszProcName <= 0xFFFF) ?
        ("#" + std::to_string((ULONG_PTR)pszProcName)) : pszProcName;

    if (sCallHistory.find(tagCurrentCall) != sCallHistory.end()) {
        DebugPrint(LEVEL_INFO, "RtlGetProcAddress: Circular forwarding detected for %s in module %p",
            tagCurrentCall.functionName.c_str(), hModule);
        return nullptr;
    }

    sCallHistory.insert(tagCurrentCall);

    bool bOrdinal = IS_INTRESOURCE(pszProcName);
    DWORD dwFuncRva = 0;

    if (!bOrdinal) {
        static std::unordered_map<std::string, void*> SelfApis{
            std::pair("GetModuleFileNameW", (void*)RtlGetModuleFileNameW),
            std::pair("GetModuleFileNameA", (void*)RtlGetModuleFileNameA),
            std::pair("GetModuleHandleA",   (void*)RtlGetModuleHandleA),
            std::pair("GetModuleHandleW",   (void*)RtlGetModuleHandleW),
        };
        auto iter = SelfApis.find(pszProcName);
        if (iter != SelfApis.end()) {
            sCallHistory.erase(tagCurrentCall);
            return iter->second;
        }

        const ExportNameMap* pCache = GetOrBuildExportCache(hModule);
        if (!pCache) {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }
        auto nameIt = pCache->find(pszProcName);
        if (nameIt == pCache->end()) {
            DebugPrint(LEVEL_WARN, "RtlGetProcAddress: Function %s not found in module %p", pszProcName, hModule);
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }
        dwFuncRva = nameIt->second;
    } else {
        DWORD dwOrdinalValue = (DWORD)(ULONG_PTR)pszProcName;
        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
        if (!pDosHeader || pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }
        PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);
        if (!pNtHeaders || pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }
        IMAGE_DATA_DIRECTORY& tagExpDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (tagExpDir.VirtualAddress == 0 || tagExpDir.Size == 0) {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }
        PIMAGE_EXPORT_DIRECTORY pExportDir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hModule + tagExpDir.VirtualAddress);
        if (!pExportDir) {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }
        DWORD* pdwFuncTable = (DWORD*)((BYTE*)hModule + pExportDir->AddressOfFunctions);
        if (!pdwFuncTable) {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }
        if (dwOrdinalValue >= pExportDir->Base &&
            dwOrdinalValue < pExportDir->Base + pExportDir->NumberOfFunctions) {
            dwFuncRva = pdwFuncTable[dwOrdinalValue - pExportDir->Base];
        } else {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }
    }

    if (dwFuncRva == 0) {
        sCallHistory.erase(tagCurrentCall);
        return nullptr;
    }

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);
    IMAGE_DATA_DIRECTORY& tagExpDirEntry = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

    if (dwFuncRva >= tagExpDirEntry.VirtualAddress &&
        dwFuncRva < tagExpDirEntry.VirtualAddress + tagExpDirEntry.Size) {

        const char* pszForwardStr = (const char*)((BYTE*)hModule + dwFuncRva);
        if (!pszForwardStr) {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }

        const char* pszDot = strchr(pszForwardStr, '.');
        if (!pszDot || pszDot == pszForwardStr) {
            sCallHistory.erase(tagCurrentCall);
            return nullptr;
        }

        std::string strDllName(pszForwardStr, pszDot - pszForwardStr);
        const char* pszTargetFunc = pszDot + 1;
        std::vector<std::string> vecTargetDlls;

        if (strDllName.find("api-") == 0 || strDllName.find("ext-") == 0) {
            std::wstring wtrDllName(strDllName.begin(), strDllName.end());
            vecTargetDlls = ApisetQuery::GetInstance().QueryForwardDlls(wtrDllName);
            if (vecTargetDlls.empty()) {
                sCallHistory.erase(tagCurrentCall);
                return nullptr;
            }
        } else {
            vecTargetDlls.push_back(strDllName);
        }

        for (const auto& strRealDll : vecTargetDlls) {
            std::string strDllFile = strRealDll;
            if (strDllFile.size() < 4 || strDllFile.substr(strDllFile.size() - 4) != ".dll") {
                strDllFile += ".dll";
            }

            HMODULE hTarget = RtlGetModuleHandleA(strDllFile.c_str());
            if (!hTarget) {
                continue;
            }

            ULONG ulOrdinal = 0;
            char* pszEnd = nullptr;
            BOOL bIsOrdinalFwd = (pszTargetFunc[0] == '#');
            ulOrdinal = strtoul(pszTargetFunc + bIsOrdinalFwd, &pszEnd, 10) * bIsOrdinalFwd;
            bIsOrdinalFwd &= (*pszEnd == '\0') & (ulOrdinal > 0) & (ulOrdinal <= 0xFFFF);

            const char* pszParam = bIsOrdinalFwd ? (const char*)(ULONG_PTR)ulOrdinal : pszTargetFunc;

            void* pTargetAddress = RtlGetProcAddressImpl(
                hTarget, pszParam, nDepth + 1, sCallHistory);

            if (pTargetAddress) {
                sCallHistory.erase(tagCurrentCall);
                return pTargetAddress;
            }
        }

        sCallHistory.erase(tagCurrentCall);
        return nullptr;
    }

    sCallHistory.erase(tagCurrentCall);
    return (void*)((BYTE*)hModule + dwFuncRva);
}



FARPROC WINAPI RtlGetProcAddressInternal(__in HMODULE hModule, __in const char* szProcName) {
    if(g_fnGetProcAddreCallback) {
        return g_fnGetProcAddreCallback(hModule, szProcName);
	}
    std::set<CallIdentifier> setCallHistory;
    return (FARPROC)RtlGetProcAddressImpl(hModule, szProcName, 0, setCallHistory);
}


using ProcNameOrOrdinal = std::variant<std::string, WORD>;

struct ProcKey {
    HMODULE               m_hMod;
    ProcNameOrOrdinal     m_NameOrOrd;

    bool operator==(__in const ProcKey& rhs) const noexcept {
        return m_hMod == rhs.m_hMod && m_NameOrOrd == rhs.m_NameOrOrd;
    }
};

struct ProcKeyHasher {
    std::size_t operator()(__in const ProcKey& k) const noexcept {
        std::size_t h1 = std::hash<HMODULE>()(k.m_hMod);

        std::size_t h2 = std::visit([](const auto& v) -> std::size_t {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
                return std::hash<std::string>()(v);
            else
                return std::hash<WORD>()(v);
            }, k.m_NameOrOrd);

        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

using ProcCache = std::unordered_map<ProcKey, FARPROC, ProcKeyHasher>;
static ProcCache g_ProcCache;
static std::shared_mutex g_ProcCacheMutex;

static inline bool IsOrdinal(__in LPCSTR name) {
    return reinterpret_cast<ULONG_PTR>(name) <= 0xFFFF;
}


FARPROC WINAPI RtlGetProcAddress(__in HMODULE hMod, __in LPCSTR lpszProcName) {
    if (!hMod || !lpszProcName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    ProcKey key{};
    key.m_hMod = hMod;

    if (IsOrdinal(lpszProcName)) {
        key.m_NameOrOrd = static_cast<WORD>(reinterpret_cast<ULONG_PTR>(lpszProcName));
    }
    else {
        key.m_NameOrOrd = std::string(lpszProcName);
    }

    {
        std::shared_lock<std::shared_mutex> lock(g_ProcCacheMutex);
        auto it = g_ProcCache.find(key);
        if (it != g_ProcCache.end())
            return it->second;
    }

    FARPROC pProc = RtlGetProcAddressInternal(hMod, lpszProcName);
    if (pProc) {
        std::unique_lock<std::shared_mutex> lock(g_ProcCacheMutex);
        g_ProcCache.emplace(key, pProc);
    }
    return pProc;
}

void WINAPI RtlInvalidateProcCache(__in HMODULE hMod)
{
    if (!hMod) return;
    {
        std::unique_lock<std::shared_mutex> lock(g_ProcCacheMutex);
        for (auto it = g_ProcCache.begin(); it != g_ProcCache.end(); ) {
            if (it->first.m_hMod == hMod) {
                it = g_ProcCache.erase(it);
            } else {
                ++it;
            }
        }
    }
    {
        std::unique_lock<std::shared_mutex> lock(g_ExportCacheMutex);
        g_ModuleExportCache.erase(hMod);
    }
    DebugPrint(LEVEL_INFO, "RtlInvalidateProcCache: Cache cleared for module 0x%p", hMod);
}