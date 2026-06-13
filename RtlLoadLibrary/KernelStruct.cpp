#include "pch.h"
#include "KernelStruct.h"

WCHAR UpCase(__in WCHAR c)
{
    if (c >= L'a' && c <= L'z')
        return c - (L'a' - L'A');
    return c;
}




BOOLEAN RtlEqualUnicodeString(__in const UNICODE_STRING* puString1, __in const UNICODE_STRING* puString2, __in BOOLEAN bCaseInSensitive)
{
    if (!puString1 || !puString2)
        return FALSE;

    if (puString1->Length != puString2->Length)
        return FALSE;

    USHORT usLen = puString1->Length;
    if (usLen == 0)
        return TRUE;

    PCWSTR pcwszS1 = puString1->Buffer;
    PCWSTR pcwszS2 = puString2->Buffer;
    if (!pcwszS1 || !pcwszS2)
        return FALSE;

    USHORT usChars = usLen / sizeof(WCHAR);

    if (bCaseInSensitive)
    {
        for (USHORT i = 0; i < usChars; ++i)
        {
            if (UpCase(pcwszS1[i]) != UpCase(pcwszS2[i]))
                return FALSE;
        }
    }
    else
    {
        if (memcmp(pcwszS1, pcwszS2, usLen) != 0)
            return FALSE;
    }

    return TRUE;
}


BOOL CopyUnicodeString(__in const UNICODE_STRING& tagSrc, __out UNICODE_STRING& tagDest)
{
    if (!tagSrc.Buffer || !tagSrc.Length)
    {
        DebugPrint(LEVEL_INFO, "CopyUniCodeString:Empty Unicode String");
        return FALSE;
    }
    const SIZE_T nTotalBytes = tagSrc.Length + sizeof(WCHAR);
    PWSTR pNewStr = (PWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,nTotalBytes);
    if (!pNewStr&& tagSrc.Length) {
		DebugPrint(LEVEL_ERROR, "CopyUnicodeString: Memory allocation failed");
        return FALSE;
    }
    memcpy_s(pNewStr, nTotalBytes, tagSrc.Buffer, tagSrc.Length);
    RtlInitUnicodeString(&tagDest, pNewStr);
    return TRUE;
}


PMY_PEB GetCurrentPeb()
{
    PMY_PEB pPEB = nullptr;
#if defined(_WIN64)
    pPEB = (PMY_PEB)__readgsqword(0x60);
#else
    pPEB = (PMY_PEB)__readfsdword(0x30);
#endif
    if(!pPEB) {
        DebugPrint(LEVEL_ERROR, "GetCurrentPeb: Failed to get PEB");
	}
	return pPEB;
}

PLIST_ENTRY GetInLoadOrderModuleList()
{
	static PMY_PEB pPEB = GetCurrentPeb();
    if (!pPEB) {
        DebugPrint(LEVEL_ERROR, "GetInLoadOrderModuleList: PEB is null");
        return nullptr;
	}
    if (!pPEB->Ldr) {
        DebugPrint(LEVEL_ERROR, "GetInLoadOrderModuleList: Ldr is null");
        return nullptr;
    }
    PLIST_ENTRY pList = &pPEB->Ldr->InLoadOrderModuleList;
    if (!pList) {
        DebugPrint(LEVEL_ERROR, "GetInLoadOrderModuleList: InLoadOrderModuleList is null");
        return nullptr;
    }
	return pList;
}

PLIST_ENTRY GetInMemoryOrderModuleList()
{
	static PMY_PEB pPEB = GetCurrentPeb();
    if (!pPEB) {
        DebugPrint(LEVEL_ERROR, "GetInMemoryOrderModuleList: PEB is null");
        return nullptr;
    }
    if (!pPEB->Ldr) {
        DebugPrint(LEVEL_ERROR, "GetInMemoryOrderModuleList: Ldr is null");
        return nullptr;
    }
    PLIST_ENTRY pList = &pPEB->Ldr->InMemoryOrderModuleList;
    if (!pList) {
        DebugPrint(LEVEL_ERROR, "GetInMemoryOrderModuleList: InMemoryOrderModuleList is null");
        return nullptr;
	}
	return pList;
}

PLIST_ENTRY GetInInitializationOrderModuleList()
{
	static PMY_PEB pPEB = GetCurrentPeb();
    if (!pPEB) {
        DebugPrint(LEVEL_ERROR, "GetInInitializationOrderModuleList: PEB is null");
        return nullptr;
    }
    if (!pPEB->Ldr) {
        DebugPrint(LEVEL_ERROR, "GetInInitializationOrderModuleList: Ldr is null");
        return nullptr;
    }
    PLIST_ENTRY pList = &pPEB->Ldr->InInitializationOrderModuleList;
    if (!pList) {
        DebugPrint(LEVEL_ERROR, "GetInInitializationOrderModuleList: InInitializationOrderModuleList is null");
        return nullptr;
	}
	return pList;
}

PMY_PEB_LDR_DATA GetCurrentLdrData()
{
    PMY_PEB pPEB = GetCurrentPeb();
    if (!pPEB) {
        DebugPrint(LEVEL_ERROR, "GetLdrData: PEB is null");
        return nullptr;
    }
    if (!pPEB->Ldr) {
        DebugPrint(LEVEL_ERROR, "GetLdrData: Ldr is null");
        return nullptr;
    }
    return pPEB->Ldr;
}

MY_LDR_DATA_TABLE_ENTRY GetPartialExeLdrEntry()
{
	PLIST_ENTRY pList = GetInLoadOrderModuleList();
    if (!pList)
    {
        DebugPrint(LEVEL_ERROR, "Get inLoadOrderModuleList failed");
		return MY_LDR_DATA_TABLE_ENTRY{};
    }
	PLIST_ENTRY pCurrent = pList->Flink;
    if (pCurrent == pList || !pCurrent->Flink) {
        DebugPrint(LEVEL_ERROR, "No loaded modules found in InLoadOrderModuleList");
        return MY_LDR_DATA_TABLE_ENTRY{};
	}
    return *CONTAINING_RECORD(pCurrent, MY_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
}

NTSTATUS NtQuerySystemInformationEx(__in SYSTEM_INFORMATION_CLASS SystemClass, __inout std::unique_ptr<CHAR[]>& pSystemInfo, __out_opt PULONG pulSize)
{
    ULONG ulBufferSize = 0x1000;

    auto pBuffer = std::make_unique<CHAR[]>(ulBufferSize);

    if (!pBuffer) {

        return STATUS_NO_MEMORY;

    }

    ULONG ulReturnLength = 0;

    NTSTATUS lStatus=STATUS_UNSUCCESSFUL;

    do {
        lStatus = NtQuerySystemInformation(
            SystemClass,
            pBuffer.get(),
            ulBufferSize,
            &ulReturnLength
        );

        if (lStatus == STATUS_INFO_LENGTH_MISMATCH) {
            pBuffer = std::make_unique<CHAR[]>(ulReturnLength);

            ulBufferSize = ulReturnLength;
        }

    } while (lStatus == STATUS_INFO_LENGTH_MISMATCH);

    if (!NT_SUCCESS(lStatus)) {
        DebugPrint(LEVEL_ERROR, "NtQuerySystemInformation failed for %s", GetNtLastErrorAsString().c_str());
        return STATUS_UNSUCCESSFUL;
    }
    pSystemInfo = std::move(pBuffer);

    if (pulSize) {
        if (ulReturnLength == 0) {
            DebugPrint(LEVEL_ERROR, "NtQuerySystemInformationEx: No data returned for SystemClass %d", SystemClass);
            return STATUS_UNSUCCESSFUL;
		}
        *pulSize = ulReturnLength;
    }
    else {
		DebugPrint(LEVEL_INFO, "Waring- NtQuerySystemInformationEx: pnSize is null");
    }

    return lStatus;
}
