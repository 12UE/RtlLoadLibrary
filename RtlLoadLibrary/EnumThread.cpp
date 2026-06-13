#include "pch.h"
#include "EnumThread.h"
#include "KernelStruct.h"
bool EnumThread(__in EnumCallback pfnCallback, __in ULONG ulProcessID)
{
	if (!pfnCallback) {
		DebugPrint(LEVEL_ERROR, "EnumThread: callback is null");
		return false;
	}
	auto pBuffer = std::make_unique<CHAR[]>(0x1);

	if (!pBuffer) {
		DebugPrint(LEVEL_ERROR, "EnumThread: Memory allocation failed");

		return false;
	}
	ULONG ulReturnLength = 0;
	if (!NT_SUCCESS(NtQuerySystemInformationEx(SystemProcessInformation, pBuffer, &ulReturnLength))) {
		DebugPrint(LEVEL_ERROR, "EnumThread: NtQuerySystemInformationEx failed");

		return false;
	}

	PMYSYSTEM_PROCESS_INFORMATION pCurrent = (PMYSYSTEM_PROCESS_INFORMATION)pBuffer.get();
	if (!pCurrent) {
		DebugPrint(LEVEL_ERROR, "EnumThread: PMYSYSTEM_PROCESS_INFORMATION is invalid");
		return false;
	}
	bool bStop = false;

	while (TRUE) {
		for (ULONG i = 0; i < pCurrent->NumberOfThreads; i++) {
			PSYSTEM_THREAD_INFORMATION pThreadInfo = (PSYSTEM_THREAD_INFORMATION)((ULONG_PTR)pCurrent + sizeof(SYSTEM_PROCESS_INFORMATION) + i * sizeof(SYSTEM_THREAD_INFORMATION));

			if (!pThreadInfo) {
				//           pThreadInfo                                       
				DebugPrint(LEVEL_ERROR, "EnumThread: pThreadInfo is null");

				continue;
			}

			if (HandleToULong(pCurrent->UniqueProcessId) == ulProcessID || ulProcessID == ANYPROCESS) {
				THREADENTRY32 tagThreadEntry32{0};
				tagThreadEntry32.dwSize = sizeof(THREADENTRY32);
				tagThreadEntry32.cntUsage = 0; //                                                          Windows      NT                            0
				tagThreadEntry32.th32ThreadID = HandleToULong(pThreadInfo->ClientId.UniqueThread);
				tagThreadEntry32.th32OwnerProcessID = HandleToULong(pCurrent->UniqueProcessId);
				tagThreadEntry32.tpBasePri = pThreadInfo->BasePriority;
				tagThreadEntry32.tpDeltaPri = pThreadInfo->Priority;
				tagThreadEntry32.dwFlags = 0;//                                                 0

				bStop = pfnCallback(tagThreadEntry32);

				if (!bStop) {
					DebugPrint(LEVEL_INFO, "EnumThread: Callback returned false, stopping enumeration");

					break;
				}
			}
		}

		ULONG ulNextOffset = pCurrent->NextEntryOffset;

		if (ulNextOffset == 0) {
			DebugPrint(LEVEL_INFO, "EnumThread: No more entries to process");

			break;
		}

		pCurrent = (PMYSYSTEM_PROCESS_INFORMATION)((ULONG_PTR)pCurrent + ulNextOffset);
	}
	return true;
}

std::vector<ThreadObject> GetCurrentThreads()
{
	std::vector<ThreadObject> vecThreads;
	EnumThread([&](const THREADENTRY32& tagThreadEntry)->bool {
		vecThreads.emplace_back(tagThreadEntry.th32ThreadID);
		return true;
		}, GetCurrentProcessId());
	return vecThreads;
}

ThreadObject::ThreadObject(__in const ThreadObject& other) {
	m_bValid = other.m_bValid;
	m_ThreadId = other.m_ThreadId;
	m_hThread = other.m_hThread;
	m_ulStartAddr = other.m_ulStartAddr;
	m_ulTebAddr = other.m_ulTebAddr;
	m_tagCreationTime = other.m_tagCreationTime;
	m_dwPriority = other.m_dwPriority;
}

ThreadObject::ThreadObject(__in ThreadObject&& other) {
	//                               
	m_bValid = other.m_bValid;
	m_ThreadId = other.m_ThreadId;
	m_hThread = other.m_hThread;
	m_ulStartAddr = other.m_ulStartAddr;
	m_ulTebAddr = other.m_ulTebAddr;
	m_tagCreationTime = other.m_tagCreationTime;
	m_dwPriority = other.m_dwPriority;

	//                                                                            
	other.m_bValid = false;
	other.m_ThreadId = 0;
	other.m_hThread = nullptr;
	other.m_ulStartAddr = 0;
	other.m_ulTebAddr = 0;
	other.m_tagCreationTime = {};
	other.m_dwPriority = 0;
}

ThreadObject::ThreadObject(__in DWORD dwTid)
{
	if (!dwTid) {
		DebugPrint(LEVEL_ERROR, "Invalid ThreadID");
		return;
	}
	m_ThreadId = dwTid;
	m_hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwTid);
	if (!m_hThread) {
		DebugPrint(LEVEL_ERROR, "Thread open failed for %s", GetLastErrorAsString().c_str());
		return;
	}
	THREAD_BASIC_INFORMATION tagThreadBasicInfo = { 0 };
	ULONG ulReturnLen = 0;
	NTSTATUS lStatus = NtQueryInformationThread(
		m_hThread,
		(THREADINFOCLASS)0,
		&tagThreadBasicInfo,
		sizeof(tagThreadBasicInfo),
		&ulReturnLen
	);
	if (!NT_SUCCESS(lStatus)) {
		DebugPrint(LEVEL_WARN, "NtQueryInformationThread query Teb addr failed for %s", GetNtLastErrorAsString().c_str());
	}
	m_ulTebAddr = (uintptr_t)tagThreadBasicInfo.TebBaseAddress;
	m_dwPriority = tagThreadBasicInfo.Priority;
	PVOID pStartAddr = nullptr;
	lStatus = NtQueryInformationThread(
		m_hThread,
		(THREADINFOCLASS)9,
		&pStartAddr,
		sizeof(pStartAddr),
		&ulReturnLen
	);
	if (!NT_SUCCESS(lStatus)) {
		DebugPrint(LEVEL_WARN, "NtQueryInformationThread query start addr failed for %s", GetNtLastErrorAsString().c_str());
	}

	m_ulStartAddr = (uintptr_t)pStartAddr;
	FILETIME tagCreationTime, tagExitTime, tagKernelTime, tagUserTime;
	ZeroMemory(&tagCreationTime, sizeof(FILETIME));
	ZeroMemory(&tagExitTime, sizeof(FILETIME));
	ZeroMemory(&tagKernelTime, sizeof(FILETIME));
	ZeroMemory(&tagUserTime, sizeof(FILETIME));
	// 3.             GetThreadTimes                
	if (!GetThreadTimes(m_hThread, &tagCreationTime, &tagExitTime, &tagKernelTime, &tagUserTime)) {
		DebugPrint(LEVEL_WARN, "GetThreadTimes failed for %s", GetLastErrorAsString().c_str());
	}
	else {
		m_tagCreationTime = tagCreationTime;
	}
	m_bValid = true;
}

ThreadObject::~ThreadObject() {
	if (m_hThread && m_hThread != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hThread);
		m_hThread = nullptr;
	}
}

ThreadObject::operator bool()
{
	return m_bValid;
}

BOOL ThreadObject::GetContext(__inout CONTEXT& tagCtx) {
	tagCtx.ContextFlags = CONTEXT_ALL;
	BOOL bResult = GetThreadContext(m_hThread, &tagCtx);
	if (!bResult) {
		DebugPrint(LEVEL_WARN, "GetThreadContext for %s", GetLastErrorAsString().c_str());
	}
	return bResult;
}

BOOL ThreadObject::SetContext(__in const CONTEXT& tagCtx) {
	BOOL bResult = SetThreadContext(m_hThread, &tagCtx);
	if (!bResult) {
		DebugPrint(LEVEL_WARN, "SetThreadContext failed for %s", GetLastErrorAsString().c_str());
	}
	return bResult;
}

DWORD ThreadObject::Resume() {
	ULONG ulPreviousSuspendCount = 0;
	if (!NtResumeThread(m_hThread, &ulPreviousSuspendCount)) {
		DebugPrint(LEVEL_ERROR, "Resume thread failed for %s", GetLastErrorAsString().c_str());
	}
	return ulPreviousSuspendCount;
}

DWORD ThreadObject::Suspend() {
	ULONG ulPreviousSuspendCount = 0;
	if (!NtSuspendThread(m_hThread, &ulPreviousSuspendCount)) {
		DebugPrint(LEVEL_ERROR, "Suspend thread failed for %s", GetLastErrorAsString().c_str());
	}
	return ulPreviousSuspendCount;
}

HANDLE ThreadObject::NativeHandle() {
	return m_hThread;
}

DWORD ThreadObject::ThreadId() {
	return m_ThreadId;
}

DWORD ThreadObject::Prority() {
	return m_dwPriority;
}

uintptr_t ThreadObject::StartAddr() {
	return m_ulStartAddr;
}

uintptr_t ThreadObject::TebAddr() {
	return m_ulTebAddr;
}

FILETIME ThreadObject::CreationTime()
{
	return m_tagCreationTime;
}

FILETIME ThreadObject::ExitTime()
{
	FILETIME creationTime, exitTime, kernelTime, userTime;
	ZeroMemory(&creationTime, sizeof(FILETIME));
	ZeroMemory(&exitTime, sizeof(FILETIME));
	ZeroMemory(&kernelTime, sizeof(FILETIME));
	ZeroMemory(&userTime, sizeof(FILETIME));
	if (!GetThreadTimes(m_hThread, &creationTime, &exitTime, &kernelTime, &userTime)) {
		DebugPrint(LEVEL_WARN, "GetThreadTimes failed for %s", GetLastErrorAsString().c_str());
	}
	return exitTime;
}

FILETIME ThreadObject::KernalTime()
{
	FILETIME creationTime, exitTime, kernelTime, userTime;
	ZeroMemory(&creationTime, sizeof(FILETIME));
	ZeroMemory(&exitTime, sizeof(FILETIME));
	ZeroMemory(&kernelTime, sizeof(FILETIME));
	ZeroMemory(&userTime, sizeof(FILETIME));
	if (!GetThreadTimes(m_hThread, &creationTime, &exitTime, &kernelTime, &userTime)) {
		DebugPrint(LEVEL_WARN, "GetThreadTimes failed for %s", GetLastErrorAsString().c_str());
	}
	return kernelTime;
}

FILETIME ThreadObject::UserTime()
{
	FILETIME tagCreationTime, tagExitTime, tagKernelTime, tagUserTime;
	ZeroMemory(&tagCreationTime, sizeof(FILETIME));
	ZeroMemory(&tagExitTime, sizeof(FILETIME));
	ZeroMemory(&tagKernelTime, sizeof(FILETIME));
	ZeroMemory(&tagUserTime, sizeof(FILETIME));
	if (!GetThreadTimes(m_hThread, &tagCreationTime, &tagExitTime, &tagKernelTime, &tagUserTime)) {
		DebugPrint(LEVEL_WARN, "GetThreadTimes failed for %s", GetLastErrorAsString().c_str());
	}
	return tagUserTime;
}

ThreadState ThreadObject::GetThreadState() {
	THREAD_BASIC_INFORMATION tagThreadBasicInfo = { 0 };
	NTSTATUS lStatus = NtQueryInformationThread(
		m_hThread,
		(THREADINFOCLASS)0, //                                 
		&tagThreadBasicInfo,
		sizeof(THREAD_BASIC_INFORMATION),
		nullptr
	);
	if (NT_SUCCESS(lStatus)) {
		// ThreadState: 0=Running, 1=Waiting, 2=Transition, 3=Terminated
		if (tagThreadBasicInfo.ThreadStatus == 0) {
			return ThreadState::Running;
		}
		else if (tagThreadBasicInfo.ThreadStatus == 1) {
			return ThreadState::Waiting;
		}
	}

	//                     API                                                      WAIT_TIMEOUT=                    
	return ThreadState::Running;
}

int ThreadObject::FileTimeToMs(__in const FILETIME& tagFt)
{
	// FILETIME      64                32  +      32                          ULONGLONG
	ULONGLONG ullTotal100ns = ((ULONGLONG)tagFt.dwHighDateTime << 32) | tagFt.dwLowDateTime;
	// 100             = 0.1         = 0.0001                             10000                    
	return (int)ullTotal100ns / 10000;
}