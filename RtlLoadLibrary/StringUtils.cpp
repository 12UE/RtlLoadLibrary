#include "pch.h"
#include "StringUtils.h"

std::string WideToNarrow(__in const std::wstring& wstrWide) {
    int size = WideCharToMultiByte(CP_ACP, 0, wstrWide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowStr(size, 0);
    WideCharToMultiByte(CP_ACP, 0, wstrWide.c_str(), -1, &narrowStr[0], size, nullptr, nullptr);
    return narrowStr;
}

std::wstring NarrowToWide(__in const std::string& strNarrow) {
    int size = MultiByteToWideChar(CP_ACP, 0, strNarrow.c_str(), -1, nullptr, 0);
    std::wstring wideStr(size, 0);
    MultiByteToWideChar(CP_ACP, 0, strNarrow.c_str(), -1, &wideStr[0], size);
    return wideStr;
}

std::wstring GetFileName(__in const std::wstring& wstrFullName) {
    const wchar_t* lastSlash = wcsrchr(wstrFullName.c_str(), L'\\');
    if (!lastSlash) lastSlash = wcsrchr(wstrFullName.c_str(), L'/');
    return lastSlash ? (lastSlash + 1) : wstrFullName;
}


