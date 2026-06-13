#include "pch.h"
#include "ErrorShow.h"
#include "RtlGetProcAddress.h"
#include "ReloadSystemLibrary.h"

std::string GetLastErrorAsString(__in DWORD dwErrorCode)
{
    if (dwErrorCode == 0)
    {
        return std::string();        
    }

    LPSTR pMessageBuffer = nullptr;
    size_t nSize = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        dwErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&pMessageBuffer,
        0,
        nullptr
    );

    std::string strMessage(pMessageBuffer, nSize);

                        
    LocalFree(pMessageBuffer);

                                            
    if (!strMessage.empty() && strMessage.back() == '\n')
    {
        strMessage.pop_back();
    }
    if (!strMessage.empty() && strMessage.back() == '\r')
    {
        strMessage.pop_back();
    }

    return strMessage;
}

NTSTATUS RtlGetLastNtStatus() {
    typedef NTSTATUS(NTAPI* pRtlGetLastNtStatus)(void);
    static pRtlGetLastNtStatus pRtlGetLastNtStatusFunc = (pRtlGetLastNtStatus)RtlGetProcAddress(CopySystemDllA(XORSTRA("ntdll.dll")), XORSTRA("RtlGetLastNtStatus"));
    if (pRtlGetLastNtStatusFunc) {
        return pRtlGetLastNtStatusFunc();
    }
    return STATUS_UNSUCCESSFUL;                                                                         
}

std::string GetNtLastErrorAsString(__in NTSTATUS lErrorCode)
{
	DWORD dwError = RtlNtStatusToDosError(lErrorCode);
    return GetLastErrorAsString(dwError);
}
