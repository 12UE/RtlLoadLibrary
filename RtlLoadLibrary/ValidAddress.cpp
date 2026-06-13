#include "pch.h"
#include "ValidAddress.h"

bool CheckMask(__in const DWORD dwValue, __in const DWORD dwMask) {
	return (dwMask && (dwValue & dwMask)) && (dwValue <= dwMask);
}

#undef USER_MINADDR
#undef USER_MAXADDR
#define USER_MINADDR ((LPCVOID)0x00010000)
#define USER_MAXADDR ((LPCVOID)0x7FFFFFFE)

/**
 * @brief                                           +                                           
 *        1.                       
 *        2.             
 *        3.                 
 *        4.                             
 *        5.                                                                     
 */
bool CheckAddrMask(__in LPCVOID lpAddress, __in DWORD dwMask, __in SIZE_T nSize)
{
    if (!lpAddress || nSize == 0) {
        DebugPrint(LEVEL_WARN, "CheckAddrMask:                           ");
        return false;
    }
    if (lpAddress > USER_MAXADDR || lpAddress < USER_MINADDR) {
        DebugPrint(LEVEL_WARN, "CheckAddrMask:                                       ");
        return false;
    }
    ULONG_PTR start = (ULONG_PTR)lpAddress;
    ULONG_PTR end = start + nSize;
    if (end < start) {
        DebugPrint(LEVEL_WARN, "CheckAddrMask:                              ");
        return false;
    }

    LPCVOID currentAddr = lpAddress;
    SIZE_T remainingSize = nSize;
    while (remainingSize > 0)
    {
        MEMORY_BASIC_INFORMATION mbi = { 0 };
        if (VirtualQuery(currentAddr, &mbi, sizeof(mbi)) == 0) {
            return false;
        }


        if (!CheckMask(mbi.AllocationProtect, dwMask)) {
            DebugPrint(LEVEL_WARN, "CheckAddrMask:                                   ");
            return false;
        }

        //                          +                 
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE) {
            DebugPrint(LEVEL_WARN, "CheckAddrMask:                                             ");
            return false;
        }

        //                                                 
        SIZE_T pageAvail = mbi.RegionSize - ((ULONG_PTR)currentAddr - (ULONG_PTR)mbi.BaseAddress);

        //                 
        currentAddr = (LPCVOID)((ULONG_PTR)currentAddr + pageAvail);
        remainingSize -= pageAvail;
    }

    //                                      
    return true;
}

//                    
bool CheckPtrReadable(__in LPCVOID lpAddress, __in SIZE_T nSize) {
	return CheckAddrMask(lpAddress, PAGE_READWRITE |PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE, nSize);
}

//                    
bool CheckPtrWritable(__in LPCVOID lpAddress, __in SIZE_T nSize) {
	return CheckAddrMask(lpAddress, PAGE_READWRITE | PAGE_EXECUTE_READWRITE, nSize);
}

//                          
bool CheckPtrExecutable(__in LPCVOID lpAddress, __in SIZE_T dwSize) {
	return CheckAddrMask(lpAddress, PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE, dwSize);
}


