#pragma once
#include "pch.h"
#include "KernelStruct.h"
#include "RtlGetModuleHandle.h"
extern "C" {

	inline DWORD
    WINAPI
    RtlGetModuleFileNameW(
        __in_opt HMODULE hModule,
        __out LPWSTR lpFilename,
        __in DWORD dwSize
	)
	{
		if (lpFilename == nullptr || dwSize == 0) {
			return 0; //                     
		}

		if (hModule == nullptr) {
			hModule = RtlGetModuleHandleW(nullptr); //                                                           
		}

		static PMY_PEB pPeb = GetCurrentPeb();

		if (!pPeb) {
			DebugPrint(LEVEL_ERROR, "RtlGetModuleFileNameW: PEB is null");
			return 0; // PEB                
		}

		if (pPeb && pPeb->Ldr) {
			PMY_LDR_DATA_TABLE_ENTRY pEntry = (PMY_LDR_DATA_TABLE_ENTRY)pPeb->Ldr->InLoadOrderModuleList.Flink;

			if (!pEntry) {

				DebugPrint(LEVEL_ERROR, "RtlGetModuleFileNameW: InLoadOrderModuleList is empty");

				return 0;
			}
			while (pEntry && pEntry->DllBase) {

				if (pEntry->DllBase == hModule) {
					DebugPrint(LEVEL_INFO, "RtlGetModuleFileNameW: Found module %ls", pEntry->FullDllName.Buffer);
					if (wcslen(pEntry->FullDllName.Buffer) < dwSize) {

						wcscpy_s(lpFilename, dwSize, pEntry->FullDllName.Buffer);

						return static_cast<DWORD>(wcslen(lpFilename));
					}
					else {

						DebugPrint(LEVEL_WARN, "RtlGetModuleFileNameW:Buffer too small for module name");
						
						return dwSize; //                       
					}
				}
			

				pEntry = (PMY_LDR_DATA_TABLE_ENTRY)pEntry->InLoadOrderLinks.Flink;

				if (!pEntry) {
					DebugPrint(LEVEL_INFO, "Foreach every LDR pEntry end");
				}
			}
		}
		//                                                           
		return 0;
	}
	inline DWORD
    WINAPI
    RtlGetModuleFileNameA(
        __in_opt HMODULE hModule,
        __out LPSTR lpFilename,
        __in DWORD dwSize
	)
	{

		std::unique_ptr<wchar_t[]> pwszBuffer(new wchar_t[dwSize]);

		if (!pwszBuffer) {

			return 0; //                          

		}

		DWORD dwResult = RtlGetModuleFileNameW(hModule, pwszBuffer.get(), dwSize);

		if (dwResult > 0) {

			size_t convertedChars = 0;

			wcstombs_s(&convertedChars, lpFilename, dwSize, pwszBuffer.get(), _TRUNCATE);
		}

		return dwResult;
	}
};
