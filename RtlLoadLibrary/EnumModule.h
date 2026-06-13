#pragma once
#include"pch.h"
using EnumModuleCallback = const std::function<bool(const MODULEENTRY32W&)>&;
bool EnumModule(__in EnumModuleCallback pCallBack, __in DWORD pid = GetCurrentProcessId());

template<typename T>
inline T ReadMemory(__in HANDLE hProcess, __in uintptr_t Addr, __out_opt SIZE_T* PSIZE = nullptr) {
    T data{};
    SIZE_T dwRead=0;
    if (!ReadProcessMemory(hProcess, (LPCVOID)Addr, &data, sizeof(data), &dwRead)&& dwRead!=sizeof(T)) {
        DebugPrint(LEVEL_ERROR, "Read memory error for %s", GetLastErrorAsString().c_str());
    }
    if (PSIZE) {
        *PSIZE = dwRead;
    }
    return data;
}

inline void PrintModules(__in DWORD PID = GetCurrentProcessId()) {
    printf("\n");
    printf("========================================================================================================\n");
    printf("                                    LOADED MODULES (PID: %lu)\n", PID);
    printf("========================================================================================================\n");
    printf("%-30s %-18s %-12s %-8s %-8s\n",
        "Module Name", "Base Address", "Size", "GlblCnt", "ProcCnt");
    printf("--------------------------------------------------------------------------------------------------------\n");

    int nModuleCount = 0;

    EnumModule(
        [&nModuleCount](const MODULEENTRY32W& tagModuleEntry) -> bool {
            nModuleCount++;

            char cAryModuleName[MAX_MODULE_NAME32 + 1] = { 0 };
            char cAryFullPath[MAX_PATH] = { 0 };

            WideCharToMultiByte(CP_ACP, 0, tagModuleEntry.szModule, -1,
                cAryModuleName, sizeof(cAryModuleName), nullptr, nullptr);
            WideCharToMultiByte(CP_ACP, 0, tagModuleEntry.szExePath, -1,
                cAryFullPath, sizeof(cAryFullPath), nullptr, nullptr);

            printf("%-30s 0x%zx %-12u %-8u %-8u\n",
                cAryModuleName,
                (uintptr_t)tagModuleEntry.modBaseAddr,
                tagModuleEntry.modBaseSize,
                tagModuleEntry.GlblcntUsage,
                tagModuleEntry.ProccntUsage);

            printf("  Path: %s\n", cAryFullPath);
            printf("\n");

            return true;
        },
        PID
            );

    printf("========================================================================================================\n");
    printf("Total Modules: %d\n", nModuleCount);
    printf("========================================================================================================\n\n");
}
