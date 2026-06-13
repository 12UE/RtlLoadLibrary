#ifndef GLOBALOBJECT_H382FAE48DC41F287EB1E7E8933C9B0
#define GLOBALOBJECT_H382FAE48DC41F287EB1E7E8933C9B0
#include"pch.h"
#include <winsvc.h>
#pragma comment(lib, "Advapi32.lib")
#include <comdef.h>
#include <oleauto.h>
#pragma comment(lib, "OleAut32.lib")
#include <roapi.h>
#pragma comment(lib,"RuntimeObject.lib")
#pragma comment(lib, "winspool.lib")



EXCEPTION_DISPOSITION NTAPI ExceptionHandler(
    __in PEXCEPTION_RECORD  ExceptionRecord,
    __in PVOID              EstablisherFrame,
    __inout PCONTEXT           ContextRecord,
    __in PVOID              DispatcherContext);
void InitializeExceptionHandler();
typedef struct _SEHInfo{
    WCHAR m_wAryFullName[MAX_PATH];
    uintptr_t m_pStartAddr;
    uintptr_t m_pEndAddr;
    _SEHInfo(__in const WCHAR* lpFullName, __in size_t nCount, __in uintptr_t StartAddr, __in uintptr_t EndAddr)
    {
        m_pStartAddr = StartAddr;
        m_pEndAddr = EndAddr;

        if (lpFullName != nullptr && nCount > 0)
        {
            wcsncpy_s(
                m_wAryFullName,
                _countof(m_wAryFullName),
                lpFullName,
                nCount
            );
            m_wAryFullName[_countof(m_wAryFullName) - 1] = L'\0';
        }
        else
        {
            m_wAryFullName[0] = L'\0';
        }
    }
}SEHInfo,*PSEHInfo;
struct GlobalObject : public Singleton<GlobalObject> {
    int m_MaxForwardDllCount = 100;
    std::unordered_map<std::wstring, void*> SelfLoadModules;
    std::unordered_map<void*, std::wstring> SelfLoadModulesReverse;
    std::vector<_SEHInfo> SEHTables;
    CRITICAL_SECTION m_LoaderLock;
    SRWLOCK m_SelfLoadModulesLock;
    SRWLOCK m_SEHTablesLock;
    BOOL m_bLoadState = TRUE;
    GlobalObject() {
        InitializeCriticalSection(&m_LoaderLock);
        InitializeSRWLock(&m_SelfLoadModulesLock);
        InitializeSRWLock(&m_SEHTablesLock);
        InitializeExceptionHandler();
        std::vector < std::string > systemlib = { "OleAut32.dll","Advapi32.dll","Ole32.dll","winspool.drv","windows.storage.dll"};
        for (auto& dlls : systemlib) {
            LoadLibraryA(dlls.c_str());
        }
        volatile static auto res = CoInitialize(nullptr);
    }

    void LockSelfLoadModulesShared() { AcquireSRWLockShared(&m_SelfLoadModulesLock); }
    void UnlockSelfLoadModulesShared() { ReleaseSRWLockShared(&m_SelfLoadModulesLock); }
    void LockSelfLoadModulesExclusive() { AcquireSRWLockExclusive(&m_SelfLoadModulesLock); }
    void UnlockSelfLoadModulesExclusive() { ReleaseSRWLockExclusive(&m_SelfLoadModulesLock); }

    void LockSEHTablesShared() { AcquireSRWLockShared(&m_SEHTablesLock); }
    void UnlockSEHTablesShared() { ReleaseSRWLockShared(&m_SEHTablesLock); }
    void LockSEHTablesExclusive() { AcquireSRWLockExclusive(&m_SEHTablesLock); }
    void UnlockSEHTablesExclusive() { ReleaseSRWLockExclusive(&m_SEHTablesLock); }
};

void RemoveLdr();
inline BOOL WINAPI ConsoleCtrlHandler(__in DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_CLOSE_EVENT:
        exit(0);
        break;
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        exit(0);
        break;
    default:
        break;
    }
    return TRUE;
}
inline bool Init() {
    WCHAR CurrentFile[MAX_PATH]{ 0 };
    GetModuleFileNameW(nullptr, CurrentFile, MAX_PATH);
    atexit(RemoveLdr);
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        printf("Regist exit callback failed: %d\n", GetLastError());
        return false;
    }
    return true;
}
static bool g_bInit = Init();
extern "C" CRITICAL_SECTION* GetLorderLock();
#endif // !GLOBALOBJECT_H382FAE48DC41F287EB1E7E8933C9B0
