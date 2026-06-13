#pragma once
#include"pch.h"
std::wstring GetFullPathW(__in const std::wstring& wstrFileName);
std::string GetFullPathA(__in const std::string& strFileName);
#ifdef UNICODE
#define GetFullPath GetFullPathW
#else
#define GetFullPath GetFullPathA
#endif // !UNICODE