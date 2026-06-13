#include "pch.h"
#include "GetFullPath.h"
std::wstring GetFullPathW(__in const std::wstring& wstrFileName)
{
    wchar_t waryBuffer[MAX_PATH]={0};
    wchar_t* pwszFilePart = nullptr;

    DWORD dwLen = ::SearchPathW(nullptr,
        wstrFileName.c_str(),
        nullptr,
        MAX_PATH,
        waryBuffer,
        &pwszFilePart);

    return (dwLen && dwLen < MAX_PATH) ? std::wstring(waryBuffer) : std::wstring();
}

std::string GetFullPathA(__in const std::string& strFileName)
{
    //        UTF-8 -> UTF-16
    int nWideLen = ::MultiByteToWideChar(CP_UTF8, 0,
        strFileName.c_str(), -1,
        nullptr, 0);
    if (nWideLen <= 0) return {};

    std::wstring wstrName(nWideLen, 0);
    ::MultiByteToWideChar(CP_UTF8, 0,
        strFileName.c_str(), -1,
        &wstrName[0], nWideLen);

    //             
    std::wstring wstrPath = GetFullPathW(wstrName);
    if (wstrPath.empty()) return {};

    // UTF-16 -> UTF-8
    int nAsciiLen = ::WideCharToMultiByte(CP_UTF8, 0,
        wstrPath.c_str(), -1,
        nullptr, 0,
        nullptr, nullptr);
    if (nAsciiLen <= 0) return {};

    std::string strPath(nAsciiLen, 0);
    ::WideCharToMultiByte(CP_UTF8, 0,
        wstrPath.c_str(), -1,
        &strPath[0], nAsciiLen,
        nullptr, nullptr);

    //                        '\0'
    strPath.pop_back();
    return strPath;
}
