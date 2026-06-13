#include "pch.h"
#include "DebugPrint.h"


static DebugLevel g_eDebugLevel = LEVEL_ALL;

DebugLevel SetGlobalLogLevel(__in DebugLevel eLevel) {
    DebugLevel eOldLevel = g_eDebugLevel;
    g_eDebugLevel = eLevel;
	return eOldLevel;
}

static std::string Level2String(__in DebugLevel nLevel) {
    if (nLevel == LEVEL_NONE) return "|NONE|";
    static const std::pair<DebugLevel, const char*> kLevelMap[] = {
        { LEVEL_INFO,  "|INFO|" },
        { LEVEL_WARN,  "|WARN|" },
        { LEVEL_ERROR, "|EROR|" },
        { LEVEL_DEBUG, "|DBUG|" }
    };
    std::string strResult;
    for (const auto& [level, str] : kLevelMap) {
        if (nLevel & level) {
            strResult += str;
        }
    }
    return strResult.empty() ? "|NONE|" : strResult;
}

void DebugPrintA(__in int nLevel, __in const char* pszFmt, ...)
{
    if ((nLevel & g_eDebugLevel) == 0)
        return;
    char szBuffer[1024];
    int nLen = sprintf_s(szBuffer, sizeof(szBuffer),
        "[%s] ", Level2String((DebugLevel)nLevel).c_str());
    va_list tagArgs;
    va_start(tagArgs, pszFmt);
    nLen += vsnprintf_s(szBuffer + nLen, sizeof(szBuffer) - nLen,
        _TRUNCATE, pszFmt, tagArgs);
    va_end(tagArgs);
    if (nLen < (int)sizeof(szBuffer) - 2)
    {
        szBuffer[nLen] = '\n';
        szBuffer[nLen + 1] = '\0';
    }
    //OutputDebugStringA(szBuffer);
    printf(szBuffer);
}


