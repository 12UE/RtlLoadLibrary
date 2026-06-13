#include "pch.h"
#include "EnumModule.h"

bool EnumModule(__in EnumModuleCallback pfnCallBack, __in DWORD pid) {
    if (!pfnCallBack) {
        DebugPrint(LEVEL_ERROR, "Invalid callbacks");
        return false;
    }
    THANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);

    THANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (!hModuleSnap) {
        DebugPrint(LEVEL_ERROR, "CreateToolhelp32Snapshot FAILED!");
        return false;
    }

    MODULEENTRY32W tagModuleEntry = { sizeof(MODULEENTRY32W), };

    for (BOOL bOk = Module32FirstW(static_cast<HANDLE>(hModuleSnap), &tagModuleEntry); bOk; bOk = Module32NextW(static_cast<HANDLE>(hModuleSnap), &tagModuleEntry)) {

        if (!pfnCallBack(tagModuleEntry)) {
            return false;
        }
    }
    return true;
}
