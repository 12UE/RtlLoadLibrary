#pragma once
#ifndef  BREAKDUMPDBF42358_83F_4E76_A114_A87AA15BAD17
#define BREAKDUMPDBF42358_83F_4E76_A114_A87AA15BAD17


#ifndef _WINDOWS_
#include<Windows.h>
#endif
#ifndef _MINIDUMP_H
#include<DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#endif

inline bool IsCriticalException(__in DWORD dwExceptionCode) {
    switch (dwExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
        return true;
    default:
        return false;
    }
}

inline int GenerateMiniDump(__in const PEXCEPTION_POINTERS& pExceptionPointers) {
    wchar_t wAryFileName[MAX_PATH] = { 0 };
    SYSTEMTIME tagLocalTime;
    GetLocalTime(&tagLocalTime);
    swprintf_s(wAryFileName, L"crash_%04d%02d%02d-%02d%02d%02d.dmp",
        tagLocalTime.wYear, tagLocalTime.wMonth, tagLocalTime.wDay,
        tagLocalTime.wHour, tagLocalTime.wMinute, tagLocalTime.wSecond);

    HANDLE hDumpFile = CreateFileW(wAryFileName, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    if (INVALID_HANDLE_VALUE == hDumpFile)
        return EXCEPTION_CONTINUE_SEARCH;

    MINIDUMP_EXCEPTION_INFORMATION tagExpParam{
        GetCurrentThreadId(), pExceptionPointers, TRUE
    };

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
        hDumpFile, MiniDumpWithFullMemory,
        (pExceptionPointers ? &tagExpParam : 0), 0, 0);

    CloseHandle(hDumpFile);
    exit(0);
    return EXCEPTION_EXECUTE_HANDLER;
}

inline LONG WINAPI DbgExceptionFilter(__in LPEXCEPTION_POINTERS lpExceptionInfo) {
    if (!lpExceptionInfo)
        return EXCEPTION_CONTINUE_SEARCH;
    if (IsDebuggerPresent())
        return EXCEPTION_CONTINUE_SEARCH;

    DWORD dwExceptionCode = lpExceptionInfo->ExceptionRecord->ExceptionCode;

    if (IsCriticalException(dwExceptionCode)) {
        return GenerateMiniDump(lpExceptionInfo);
    }


    return EXCEPTION_CONTINUE_EXECUTION;
}
inline int	InitHandleException() {
	//AddVectoredExceptionHandler(true, DbgExceptionFilter);
	return 0;
}
static int nInitProcess = InitHandleException();

#endif // ! BREAKDUMPDBF42358_83F_4E76_A114_A87AA15BAD17
