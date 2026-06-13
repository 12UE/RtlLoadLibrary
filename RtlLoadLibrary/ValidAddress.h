#ifndef VALIDADDRESS_H
#define VALIDADDRESS_H

#include"pch.h"
#ifdef _WIN64
#define USER_MAXADDR 0x00007FFFFFFFFFFF
#else
#define USER_MAXADDR 0x7FFFFFFF
#endif
#define USER_MINADDR 0x10000

bool  CheckMask(__in const DWORD dwValue, __in const DWORD dwMask);

bool CheckAddrMask(__in LPCVOID lpAddress, __in DWORD mask, __in SIZE_T nSize = 0x1000);

bool CheckPtrReadable(__in LPCVOID lpAddress, __in SIZE_T dwSize = 0x1000);

bool CheckPtrWritable(__in LPCVOID lpAddress, __in SIZE_T dwSize = 0x1000);

bool CheckPtrExecutable(__in LPCVOID lpAddress, __in SIZE_T dwSize = 0x1000);
#endif // VALIDADDRESS_H
