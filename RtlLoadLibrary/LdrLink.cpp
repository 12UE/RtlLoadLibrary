#include "pch.h"
#include "LdrLink.h"
#include "RtlGetProcAddress.h"
static constexpr DWORD LDRP_PROCESS_ATTACH_CALLED = 0x00080000;
bool WINAPI AddLdrLink(__in const MY_LDR_DATA_TABLE_ENTRY& tagEntry, __in BOOL bInitialize)
{
    std::lock_guard<std::mutex> mtxLock(g_LdrLinkMutex);

    // Validate parameters
    if (!tagEntry.BaseDllName.Buffer || !tagEntry.FullDllName.Buffer || !tagEntry.DllBase) {
        DebugPrint(LEVEL_ERROR, "AddLdrLink: Invalid entry data");
        return false;
    }

    PMY_PEB_LDR_DATA pLdr = GetCurrentLdrData();
    if (!pLdr) {
        DebugPrint(LEVEL_ERROR, "AddLdrLink: GetCurrentLdrData failed");
        return false;
    }

    /* 1. Check if already exists */
    auto FindExisting = [&]() -> PMY_LDR_DATA_TABLE_ENTRY {
        PLIST_ENTRY pHead = &pLdr->InLoadOrderModuleList;
        if (pHead->Flink == pHead) {
            return nullptr;
        }

        for (PLIST_ENTRY pCur = pHead->Flink; pCur && pCur != pHead; pCur = pCur->Flink) {
            PMY_LDR_DATA_TABLE_ENTRY pItem = CONTAINING_RECORD(pCur, MY_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
            if (pItem->BaseDllName.Buffer &&
                RtlEqualUnicodeString(&pItem->BaseDllName, &tagEntry.BaseDllName, TRUE)) {
                return pItem;
            }
        }
        return nullptr;
        };

    if (PMY_LDR_DATA_TABLE_ENTRY pExist = FindExisting()) {
        if (pExist->ObsoleteLoadCount != 0xFFFF) {
            pExist->ObsoleteLoadCount++;
            DebugPrint(LEVEL_INFO, "Module %ls load count: %u",
                pExist->BaseDllName.Buffer, pExist->ObsoleteLoadCount);
        }
        return true;
    }

    /* 2. Allocate new entry */
    PMY_LDR_DATA_TABLE_ENTRY pNew = (PMY_LDR_DATA_TABLE_ENTRY)
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MY_LDR_DATA_TABLE_ENTRY));
    if (!pNew) {
        DebugPrint(LEVEL_ERROR, "AddLdrLink: Memory allocation failed");
        return false;
    }

    /* 3. Copy template and initialize */
    MY_LDR_DATA_TABLE_ENTRY tagTemplate = GetPartialExeLdrEntry();
    memcpy_s(pNew, sizeof(*pNew), &tagTemplate, sizeof(tagTemplate));

    // Clear list nodes
    ZeroMemory(&pNew->InLoadOrderLinks, sizeof(LIST_ENTRY));
    ZeroMemory(&pNew->InMemoryOrderLinks, sizeof(LIST_ENTRY));
    ZeroMemory(&pNew->InInitializationOrderLinks, sizeof(LIST_ENTRY));

    // Clear template string pointers (avoid false free)
    pNew->BaseDllName.Buffer = nullptr;
    pNew->FullDllName.Buffer = nullptr;

    // Deep copy strings
    if (!CopyUnicodeString(tagEntry.BaseDllName, pNew->BaseDllName) ||
        !CopyUnicodeString(tagEntry.FullDllName, pNew->FullDllName)) {
        HeapFree(GetProcessHeap(), 0, pNew);
        return false;
    }

    // Set fields
    pNew->DllBase = tagEntry.DllBase;
    pNew->EntryPoint = tagEntry.EntryPoint;
    pNew->SizeOfImage = tagEntry.SizeOfImage;
    pNew->ObsoleteLoadCount = 1;
    pNew->TlsIndex = 0;
    if (bInitialize) {
        pNew->Flags |= LDRP_PROCESS_ATTACH_CALLED;
    }
    /* 4. Insert into linked lists */
    auto InsertAfterFirst = [](PLIST_ENTRY pHead, PLIST_ENTRY pNewNode) {
        if (!pHead || !pNewNode) {
            DebugPrint(LEVEL_ERROR, "InsertAfterFirst: nullptr pointer");
            return;
        }

        if (pHead->Flink == pHead) {
            // Empty list: insert at head
            DebugPrint(LEVEL_INFO, "InsertAfterFirst: Empty list, inserting at head");
            pNewNode->Flink = pHead;
            pNewNode->Blink = pHead;
            pHead->Flink = pNewNode;
            pHead->Blink = pNewNode;
            return;
        }

        // Insert after the first node
        PLIST_ENTRY pFirst = pHead->Flink;
        pNewNode->Flink = pFirst->Flink;
        pNewNode->Blink = pFirst;
        pFirst->Flink->Blink = pNewNode;
        pFirst->Flink = pNewNode;
    };

    InsertAfterFirst(&pLdr->InLoadOrderModuleList, &pNew->InLoadOrderLinks);
    InsertAfterFirst(&pLdr->InMemoryOrderModuleList, &pNew->InMemoryOrderLinks);
    InsertAfterFirst(&pLdr->InInitializationOrderModuleList, &pNew->InInitializationOrderLinks);

    DebugPrint(LEVEL_INFO, "Successfully added %ls to Ldr", pNew->BaseDllName.Buffer);
    return true;
}

bool WINAPI RemoveLdrLink(__in_opt const UNICODE_STRING* puBaseName, __in_opt const UNICODE_STRING* puFullPath)
{
    std::lock_guard<std::mutex> mtxlLock(g_LdrLinkMutex);

    if (!puBaseName && !puFullPath)
        return false;

    PMY_PEB_LDR_DATA pLdr = GetCurrentLdrData();
    if (!pLdr) {
        DebugPrint(LEVEL_ERROR, "RemoveLdrLink: Failed to get Ldr");
        return false;
    }

    PLIST_ENTRY pHead = &pLdr->InLoadOrderModuleList;
    PLIST_ENTRY pCur = pHead->Flink;
    bool bFound = false;

    // Skip the first node (main module)
    if (pCur != pHead)
        pCur = pCur->Flink;

    while (pCur != pHead)
    {
        PMY_LDR_DATA_TABLE_ENTRY pEntry =
            CONTAINING_RECORD(pCur, MY_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        // Save next node to avoid broken links after deletion
        PLIST_ENTRY pNext = pCur->Flink;

        // Match conditions
        bool bMatch = false;
        if (puBaseName && puBaseName->Buffer)
            bMatch = RtlEqualUnicodeString(&pEntry->BaseDllName, puBaseName, TRUE);
        if (puFullPath && puFullPath->Buffer) {
            bool bFullPathMatch = RtlEqualUnicodeString(&pEntry->FullDllName, puFullPath, TRUE);
            bMatch = puBaseName && puBaseName->Buffer ? (bMatch && bFullPathMatch) : bFullPathMatch;
        }
        // Require at least one criterion to actually match
        if (!bMatch) {
            pCur = pNext;
            continue;
        }
        // Safety: verify this entry was added by AddLdrLink (LoadCount == 1)
        // System-loaded entries have LoadCount == 0xFFFF or varying non-1 values
        if (pEntry->ObsoleteLoadCount != 1) {
            DebugPrint(LEVEL_WARN, "RemoveLdrLink: Skipping system entry %wZ (LoadCount=%u)",
                &pEntry->BaseDllName, pEntry->ObsoleteLoadCount);
            pCur = pNext;
            continue;
        }

        if (bMatch)
        {
            // Remove from all three lists
            RemoveEntryList(&pEntry->InLoadOrderLinks);
            RemoveEntryList(&pEntry->InMemoryOrderLinks);
            RemoveEntryList(&pEntry->InInitializationOrderLinks);

            // Free memory allocated via HeapAlloc in AddLdrLink
            if (pEntry->BaseDllName.Buffer)
                HeapFree(GetProcessHeap(), 0, pEntry->BaseDllName.Buffer);
            if (pEntry->FullDllName.Buffer)
                HeapFree(GetProcessHeap(), 0, pEntry->FullDllName.Buffer);
            HeapFree(GetProcessHeap(), 0, pEntry);

            bFound = true;
        }

        pCur = pNext;
    }

    if (!bFound) {
        DebugPrint(LEVEL_WARN, "RemoveLdrLink: Module not found");
    }
    return bFound;
}
BOOL LdrUnlinkModule(__in HMODULE hTargetModule) {
    std::lock_guard<std::mutex> mtxLock(g_LdrLinkMutex);

    if (hTargetModule == nullptr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Get PEB
    PMY_PEB pMyPeb = GetCurrentPeb();
    if (pMyPeb == nullptr || pMyPeb->Ldr == nullptr) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    PMY_PEB_LDR_DATA pMyLdrData = pMyPeb->Ldr;

    // Traverse InMemoryOrderModuleList to find target module
    PLIST_ENTRY pListHead = &pMyLdrData->InMemoryOrderModuleList;
    PLIST_ENTRY pCurrentNode = pListHead->Flink;
    PMY_LDR_DATA_TABLE_ENTRY pTargetLdrEntry = nullptr;

    while (pCurrentNode != pListHead) {
        pTargetLdrEntry = CONTAINING_RECORD(
            pCurrentNode,
            MY_LDR_DATA_TABLE_ENTRY,
            InMemoryOrderLinks
        );

        if (pTargetLdrEntry->DllBase == hTargetModule) {
            DebugPrint(LEVEL_INFO, "Found target module at 0x%p", hTargetModule);
            break;
        }

        pCurrentNode = pCurrentNode->Flink;
        pTargetLdrEntry = nullptr;
    }

    if (pTargetLdrEntry == nullptr) {
        DebugPrint(LEVEL_ERROR, "Module 0x%p not found in LDR", hTargetModule);
        SetLastError(ERROR_MOD_NOT_FOUND);
        return FALSE;
    }

    // Get the three linked list node pointers
    PLIST_ENTRY pLoadNode = &pTargetLdrEntry->InLoadOrderLinks;
    PLIST_ENTRY pMemNode = &pTargetLdrEntry->InMemoryOrderLinks;
    PLIST_ENTRY pInitNode = &pTargetLdrEntry->InInitializationOrderLinks;


    // Validate node validity
    if (pLoadNode->Flink == nullptr || pLoadNode->Blink == nullptr) {
        DebugPrint(LEVEL_ERROR, "InLoadOrderLinks is invalid");
        return FALSE;
    }
    if (pMemNode->Flink == nullptr || pMemNode->Blink == nullptr) {
        DebugPrint(LEVEL_ERROR, "InMemoryOrderLinks is invalid");
        return FALSE;
    }

    // ============ 1. Unlink InLoadOrderLinks ============
    DebugPrint(LEVEL_INFO, "Unlinking InLoadOrderLinks...");

    // Safety check: cannot be self-referencing and not the list head
    if (pLoadNode->Flink != pLoadNode &&
        pLoadNode != &pMyLdrData->InLoadOrderModuleList) {

        // Predecessor's Flink points to successor
        pLoadNode->Blink->Flink = pLoadNode->Flink;
        // Successor's Blink points to predecessor
        pLoadNode->Flink->Blink = pLoadNode->Blink;

        // Clear current node (prevent dangling pointers)
        pLoadNode->Flink = pLoadNode;
        pLoadNode->Blink = pLoadNode;

        DebugPrint(LEVEL_INFO, "InLoadOrderLinks unlinked successfully");
    }
    else {
        DebugPrint(LEVEL_WARN, "InLoadOrderLinks skipped (head or self-ref)");
    }

    // ============ 2. Unlink InMemoryOrderLinks ============
    DebugPrint(LEVEL_INFO, "Unlinking InMemoryOrderLinks...");

    if (pMemNode->Flink != pMemNode &&
        pMemNode != &pMyLdrData->InMemoryOrderModuleList) {

        pMemNode->Blink->Flink = pMemNode->Flink;
        pMemNode->Flink->Blink = pMemNode->Blink;
        pMemNode->Flink = pMemNode;
        pMemNode->Blink = pMemNode;

        DebugPrint(LEVEL_INFO, "InMemoryOrderLinks unlinked successfully");
    }
    else {
        DebugPrint(LEVEL_WARN, "InMemoryOrderLinks skipped (head or self-ref)");
    }

    DebugPrint(LEVEL_INFO, "Unlinking InInitializationOrderLinks...");
    if (pInitNode->Flink != nullptr && pInitNode->Blink != nullptr &&
        pInitNode->Flink != pInitNode && pInitNode->Blink != pInitNode &&
        pInitNode != &pMyLdrData->InInitializationOrderModuleList) {

        pInitNode->Blink->Flink = pInitNode->Flink;
        pInitNode->Flink->Blink = pInitNode->Blink;
        pInitNode->Flink = pInitNode;
        pInitNode->Blink = pInitNode;

        DebugPrint(LEVEL_INFO, "InInitializationOrderLinks unlinked successfully");
    }
    else {
        DebugPrint(LEVEL_WARN, "InInitializationOrderLinks not initialized or is head, skipping");
    }
    
    // Free memory allocated via HeapAlloc in AddLdrLink
    if (pTargetLdrEntry->BaseDllName.Buffer)
        HeapFree(GetProcessHeap(), 0, pTargetLdrEntry->BaseDllName.Buffer);
    if (pTargetLdrEntry->FullDllName.Buffer)
        HeapFree(GetProcessHeap(), 0, pTargetLdrEntry->FullDllName.Buffer);
    HeapFree(GetProcessHeap(), 0, pTargetLdrEntry);

    DebugPrint(LEVEL_INFO, "Module 0x%p successfully unlinked from LDR", hTargetModule);
    SetLastError(ERROR_SUCCESS);
    RtlInvalidateProcCache(hTargetModule);
    return TRUE;
}

