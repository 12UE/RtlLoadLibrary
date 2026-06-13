#pragma once
#include"pch.h"
#pragma warning(disable: 5039)
#define STATUS_INFO_LENGTH_MISMATCH      ((NTSTATUS)0xC0000004L)
static  WCHAR UpCase(__in WCHAR c);
bool WCharInitUnicodeString(__in LPCWSTR src,__out UNICODE_STRING* dest);

BOOLEAN RtlEqualUnicodeString(
    __in const UNICODE_STRING* String1,
    __in const UNICODE_STRING* String2,
    __in BOOLEAN              CaseInSensitive
);
bool FreeUnicodeString(__in UNICODE_STRING* src);
BOOL CopyUnicodeString(__in const UNICODE_STRING& src,__out UNICODE_STRING& dest);
typedef struct _MY_LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;

    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;

    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG  Flags;
    USHORT ObsoleteLoadCount;
    USHORT TlsIndex;

    union  Reserved {
        LIST_ENTRY HashLinks;
        struct other{
            PVOID SectionPointer;
            ULONG CheckSum;
        };
    };

    union Reserved2{
        ULONG TimeDateStamp;
        PVOID LoadedImports;
    };
    VOID* EntryPointActivationContext;
    VOID* PatchInformation;
    LIST_ENTRY ForwarderLinks;
    LIST_ENTRY ServiceTagLinks;
    LIST_ENTRY StaticLinks;
} MY_LDR_DATA_TABLE_ENTRY, * PMY_LDR_DATA_TABLE_ENTRY;

typedef struct MYTHREAD_BASIC_INFORMATION {
    NTSTATUS                ThreadStatus;
    PVOID                   TebBaseAddress;
    CLIENT_ID               ClientId;
    KAFFINITY               AffinityMask;
    KPRIORITY               Priority;
    KPRIORITY               BasePriority;
} MYTHREAD_BASIC_INFORMATION, * PMYTHREAD_BASIC_INFORMATION;

typedef struct _MY_PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} MY_PEB_LDR_DATA, * PMY_PEB_LDR_DATA;

#if defined(_WIN64)
typedef struct _MY_PEB {
    BYTE Reserved1[0x18];
    PMY_PEB_LDR_DATA Ldr;
} MY_PEB, * PMY_PEB;
#else
typedef struct _MY_PEB {
    BYTE Reserved1[0x0C];
    PMY_PEB_LDR_DATA Ldr;
} MY_PEB, * PMY_PEB;
#endif

PMY_PEB GetCurrentPeb();

PLIST_ENTRY GetInLoadOrderModuleList();

PLIST_ENTRY GetInMemoryOrderModuleList();

PLIST_ENTRY GetInInitializationOrderModuleList();

PMY_PEB_LDR_DATA GetCurrentLdrData();
MY_LDR_DATA_TABLE_ENTRY GetPartialExeLdrEntry();

typedef enum _SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT, * PSECTION_INHERIT;
typedef struct MY_SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER Reserved[3];
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
	SYSTEM_THREAD_INFORMATION Threads[1];
} MYSYSTEM_PROCESS_INFORMATION, * PMYSYSTEM_PROCESS_INFORMATION;
typedef struct _THREAD_BASIC_INFORMATION {
    NTSTATUS                ThreadStatus;
    PVOID                   TebBaseAddress;
    CLIENT_ID               ClientId;
    KAFFINITY               AffinityMask;
    KPRIORITY               Priority;
    KPRIORITY               BasePriority;
} THREAD_BASIC_INFORMATION, * PTHREAD_BASIC_INFORMATION;
typedef struct _PE_SECTION_INFO {
    CHAR        szSectionName[8] = {0};
    DWORD       dwCharacteristics;
    DWORD       dwMemProtect;
    ULONG_PTR   ImageBase;
    ULONG_PTR   VirtualAddress;
    ULONG_PTR   MemoryStartAddr;
    ULONG_PTR   MemoryEndAddr;
    DWORD       VirtualSize;
    DWORD       dwSectionAlignment;
    ULONG_PTR   FileStartOffset;
    ULONG_PTR   FileEndOffset;
    DWORD       SizeOfRawData;
    DWORD       dwFileAlignment;
} PE_SECTION_INFO, * PPE_SECTION_INFO;
extern "C" {
    NTSYSAPI NTSTATUS NTAPI NtMapViewOfSection(
        __in HANDLE SectionHandle,
        __in HANDLE ProcessHandle,
        __out PVOID* BaseAddress,
        __in ULONG_PTR ZeroBits,
        __in SIZE_T CommitSize,
        __in_opt PLARGE_INTEGER SectionOffset,
        __inout PSIZE_T ViewSize,
        __in SECTION_INHERIT InheritDisposition,
        __in ULONG AllocationType,
        __in ULONG Win32Protect
    );

    NTSYSAPI NTSTATUS NTAPI NtCreateSection(
        __out PHANDLE SectionHandle,
        __in ACCESS_MASK DesiredAccess,
        __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
        __in_opt PLARGE_INTEGER MaximumSize,
        __in ULONG SectionPageProtection,
        __in ULONG AllocationAttributes,
        __in HANDLE FileHandle
    );

    NTSYSAPI NTSTATUS NTAPI NtUnmapViewOfSection(
        __in HANDLE ProcessHandle,
        __in PVOID BaseAddress
    );

    NTSYSAPI NTSTATUS NTAPI NtSuspendThread(
        __in HANDLE ThreadHandle,
        __out PULONG PreviousSuspendCount
    );

     NTSYSAPI NTSTATUS NTAPI NtResumeThread(
        __in HANDLE ThreadHandle,
        __out PULONG PreviousSuspendCount
     );

     NTSYSAPI NTSTATUS NTAPI ZwTerminateProcess(
         __in HANDLE hProcess,
         __in NTSTATUS nExitCode
     );


}

NTSTATUS NtQuerySystemInformationEx(
    __in SYSTEM_INFORMATION_CLASS SystemClass,
    __inout std::unique_ptr<CHAR[]>& SystemInfo,
    __out_opt PULONG nSize
);
