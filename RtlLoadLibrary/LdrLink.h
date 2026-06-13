#pragma once
#include"pch.h"
#include <mutex>
#include "KernelStruct.h"
//                                                         
#define RemoveEntryList(Entry)                      \
do {                                                \
    PLIST_ENTRY _EX_Entry = (Entry);                \
    PLIST_ENTRY _EX_Blink = _EX_Entry->Blink;       \
    PLIST_ENTRY _EX_Flink = _EX_Entry->Flink;       \
    _EX_Blink->Flink = _EX_Flink;                   \
    _EX_Flink->Blink = _EX_Blink;                   \
} while (0)
inline std::mutex g_LdrLinkMutex;
extern "C" {
    bool WINAPI AddLdrLink(__in const _MY_LDR_DATA_TABLE_ENTRY& Entry, __in BOOL bInitialize);
    bool WINAPI RemoveLdrLink(__in_opt const UNICODE_STRING* BaseName, __in_opt const UNICODE_STRING* FullPath);
    BOOL LdrUnlinkModule(__in HMODULE hTargetModule);
}