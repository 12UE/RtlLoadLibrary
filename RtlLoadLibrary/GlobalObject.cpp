#include "pch.h"
#include "GlobalObject.h"
#include "LdrLink.h"
#include "RtlGetProcAddress.h"
#ifdef _WIN64
static EXCEPTION_DISPOSITION NTAPI ExceptionHandler(
    __in PEXCEPTION_RECORD  ExceptionRecord,
    __in PVOID              EstablisherFrame,
    __inout PCONTEXT           ContextRecord,
    __in PVOID              DispatcherContext)
{
    UNREFERENCED_PARAMETER(DispatcherContext);
    DebugPrint(LEVEL_ALL,
        "TopLevelSEHandler: Exception at %p (TID stack ESP=0x%08X)",
        ExceptionRecord->ExceptionAddress, ContextRecord->Rsp);
    if (!ExceptionRecord || !ContextRecord || !EstablisherFrame) {
        DebugPrint(LEVEL_ERROR, "TopLevelSEHandler: Invalid parameters");
        return ExceptionContinueSearch;
    }

    const PVOID pExceptionAddr = ExceptionRecord->ExceptionAddress;
    const DWORD64 dwEsp = ContextRecord->Rsp;
    EXCEPTION_REGISTRATION_RECORD* pThisRec = reinterpret_cast<EXCEPTION_REGISTRATION_RECORD*>(EstablisherFrame);
    EXCEPTION_REGISTRATION_RECORD* pPrevRec = (pThisRec ? pThisRec->Next : nullptr);
    DebugPrint(LEVEL_ALL,
        "TopLevelSEHandler: No valid handler found for exception at %p (TID stack ESP=0x%08X)",
        pExceptionAddr, dwEsp);
    return ExceptionContinueSearch;
}
#endif
void InitializeExceptionHandler()
{
#ifdef _WIN64
	AddVectoredExceptionHandler(1, (PVECTORED_EXCEPTION_HANDLER)ExceptionHandler);
#endif
}
void RemoveLdr()
{
    GlobalObject::GetInstance().LockSelfLoadModulesExclusive();
    auto pSelfloadList = &GlobalObject::GetInstance().SelfLoadModules;
    if (CheckPtrReadable(pSelfloadList, 4) &&!pSelfloadList->empty()) {
        DebugPrint(LEVEL_INFO, "========================================");
        DebugPrint(LEVEL_INFO, "Starting cleanup of %d self-loaded modules", pSelfloadList->size());
        DebugPrint(LEVEL_INFO, "========================================");

        for (auto it = pSelfloadList->begin(); it != pSelfloadList->end();it++ ) {
            void* pModuleBase = it->second;
            const wchar_t* lpwszModuleName = it->first.c_str();
            if (!pModuleBase||!lpwszModuleName) continue;
            DebugPrint(LEVEL_INFO, "Processing module: %ls (Base: 0x%p)", lpwszModuleName, pModuleBase);
            if (!LdrUnlinkModule((HMODULE)pModuleBase)) {
                DebugPrint(LEVEL_ERROR, "Unlink module failed! for 0x%x", pModuleBase);
                continue;
            }
            DebugPrint(LEVEL_INFO, "Successfully unlinked %ls from LDR", lpwszModuleName);
            RtlInvalidateProcCache((HMODULE)pModuleBase);
        }
        pSelfloadList->clear();
    }
    GlobalObject::GetInstance().UnlockSelfLoadModulesExclusive();
}
CRITICAL_SECTION* GetLorderLock()
{
    return &GlobalObject::GetInstance().m_LoaderLock;
}
