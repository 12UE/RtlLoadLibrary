#include "pch.h"
#include "RtlGetModuleHandle.h"
#include "KernelStruct.h"
#include "GlobalObject.h"
#include "ApisetQuery.h"

int _mywcsicmp(__in const wchar_t* pwszStr1, __in const wchar_t* pwszStr2)
{
    wchar_t c1, c2;

    do {
        c1 = towlower(*pwszStr1++);
        c2 = towlower(*pwszStr2++);

        if (c1 == L'\0')
            return c1 - c2;

    } while (c1 == c2);

    return c1 - c2;
}
size_t mywcslen(__in const wchar_t* pwszStr)
{
    const wchar_t* s = pwszStr;

    
    while (1) {
        if (s[0] == L'\0') return s - pwszStr;
        if (s[1] == L'\0') return s - pwszStr + 1;
        if (s[2] == L'\0') return s - pwszStr + 2;
        if (s[3] == L'\0') return s - pwszStr + 3;
        s += 4;
    }
}

int mywcsncpy_s(__out wchar_t* dest, __in size_t destSize, __in const wchar_t* src, __in size_t count)
{
    
    if (dest == nullptr || destSize == 0) {
        errno = EINVAL;
        return EINVAL;
    }

    if (src == nullptr) {
        dest[0] = L'\0';
        errno = EINVAL;
        return EINVAL;
    }

    if (destSize > (size_t)-1 / sizeof(wchar_t)) {
        dest[0] = L'\0';
        errno = ERANGE;
        return ERANGE;
    }

    
    size_t copyCount = (count < destSize - 1) ? count : destSize - 1;
    size_t i;

    
    for (i = 0; i < copyCount && src[i] != L'\0'; i++) {
        dest[i] = src[i];
    }

    
    dest[i] = L'\0';

    
    if (count != (size_t)-1 && i < count && src[i] != L'\0') {
        
        return 0;
    }

    return 0;
}
HMODULE WINAPI RtlGetModuleHandleW(__in_opt const wchar_t* pwszTargetDllName) {
    if (!pwszTargetDllName) {
        return nullptr;
    }
    wchar_t* pwszDllStr = (wchar_t*)pwszTargetDllName;

    if(g_GetModuleHandleCallback) {

        return g_GetModuleHandleCallback(pwszTargetDllName);

	}
	{
		std::wstring wstrLower = pwszTargetDllName;
		for (auto& ch : wstrLower) ch = towlower(ch);
		GlobalObject::GetInstance().LockSelfLoadModulesShared();
		auto& refSelfLoad = GlobalObject::GetInstance().SelfLoadModules;
		auto it = refSelfLoad.find(wstrLower);
		if (it != refSelfLoad.end()) {
			HMODULE hFound = (HMODULE)it->second;
			GlobalObject::GetInstance().UnlockSelfLoadModulesShared();
			return hFound;
		}
		GlobalObject::GetInstance().UnlockSelfLoadModulesShared();
	}
    static PMY_PEB_LDR_DATA pLdr = GetCurrentLdrData();
    if (!pLdr) {

        DebugPrint(LEVEL_ERROR, "RtlGetModuleHandleW: Get LdrData failed");

        return nullptr;
    }
    PLIST_ENTRY pCurrentEntry = pLdr->InLoadOrderModuleList.Flink;

    if (pCurrentEntry == &pLdr->InLoadOrderModuleList) {

        DebugPrint(LEVEL_ERROR, "RtlGetModuleHandleW: InLoadOrderModuleList is empty");

        return nullptr; 
	}
    PMY_LDR_DATA_TABLE_ENTRY pCurrentLdrEntry = nullptr;

    if (pwszDllStr == nullptr) {
        pCurrentLdrEntry = CONTAINING_RECORD(pCurrentEntry, MY_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        return (HMODULE)pCurrentLdrEntry->DllBase;
    }

    if (mywcslen(pwszTargetDllName) >= MAX_PATH) {
       DebugPrint(LEVEL_ERROR, "RtlGetModuleHandleW: Library name over size");
        return nullptr; 
	}

    std::wstring wstrTargetName(pwszTargetDllName);
    
    if (wstrTargetName.size() < 4 || wstrTargetName.substr(wstrTargetName.size() - 4) != L".dll") {
        wstrTargetName += L".dll";
    }
    
    size_t nLastSlash = wstrTargetName.find_last_of(L"\\/");

    if (nLastSlash != std::wstring::npos) {

        wstrTargetName = wstrTargetName.substr(nLastSlash + 1);
    }
    
    std::vector<std::string> vecstrResults;

    std::vector<std::wstring> vecwstrResults;

    if (wstrTargetName.find(L"ext-") == 0 || wstrTargetName.find(L"api-") == 0) {

        vecstrResults=ApisetQuery::GetInstance().QueryForwardDlls(wstrTargetName);
	}
    if (!vecstrResults.empty()) {
        
        for (const auto& szForwardDll : vecstrResults) {

            std::wstring wstrForwardDll = NarrowToWide(szForwardDll.c_str());

            PLIST_ENTRY pEntry = pLdr->InLoadOrderModuleList.Flink;

            while (pEntry != &pLdr->InLoadOrderModuleList) {

                PMY_LDR_DATA_TABLE_ENTRY pLdrEntry = CONTAINING_RECORD(pEntry, MY_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

                if (!pLdrEntry){
                    DebugPrint(LEVEL_ERROR, "RtlGetModuleHandleW: pLdrEntry is nullptr");
					return nullptr;
                }

                if (pLdrEntry->BaseDllName.Buffer) {

                    wchar_t wAryCurrentDllName[MAX_PATH] = {0};

                    int nLen = pLdrEntry->BaseDllName.Length / sizeof(wchar_t);

                    if (nLen > 0 && nLen < MAX_PATH) {

                        mywcsncpy_s(wAryCurrentDllName, MAX_PATH, pLdrEntry->BaseDllName.Buffer, nLen);

                        wAryCurrentDllName[nLen] = L'\0';

                        if (_mywcsicmp(wAryCurrentDllName, wstrForwardDll.c_str()) == 0) {

                            return (HMODULE)pLdrEntry->DllBase;
                        }
                    }
                }

                pEntry = pEntry->Flink;
            }
        }
    }
    

    while (pCurrentEntry != &pLdr->InLoadOrderModuleList) {

        pCurrentLdrEntry = CONTAINING_RECORD(pCurrentEntry, MY_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        if (!pCurrentLdrEntry) {
			DebugPrint(LEVEL_ERROR, "RtlGetModuleHandleW: pCurrentLdrEntry is nullptr");
			return nullptr;
        }

        if (pCurrentLdrEntry->BaseDllName.Buffer) {

            wchar_t wszAryCurrentDllName[MAX_PATH] = {0};

            int nLen = pCurrentLdrEntry->BaseDllName.Length / sizeof(wchar_t);

            if (nLen > 0 && nLen < MAX_PATH) {

                mywcsncpy_s(wszAryCurrentDllName, MAX_PATH, pCurrentLdrEntry->BaseDllName.Buffer, nLen);

                wszAryCurrentDllName[nLen] = L'\0';

                if (_mywcsicmp(wszAryCurrentDllName, wstrTargetName.c_str()) == 0) {

                    return (HMODULE)pCurrentLdrEntry->DllBase;
                }
            }
        }
        pCurrentEntry = pCurrentEntry->Flink;
    }
    return nullptr;
}

HMODULE WINAPI RtlGetModuleHandleA(__in_opt const char* pszTargetDllName)
{
    if (pszTargetDllName == nullptr) {

        return RtlGetModuleHandleW(nullptr);
	}

	std::wstring wstrName = NarrowToWide(pszTargetDllName);

    if (wstrName.empty()) {

		DebugPrint(LEVEL_ERROR, "RtlGetModuleHandleA: Failed to convert  to wide string");

        return nullptr;
    }

	DebugPrint(LEVEL_INFO, "RtlGetModuleHandleA: Searching for module %s", pszTargetDllName);

    return RtlGetModuleHandleW(wstrName.c_str());
}