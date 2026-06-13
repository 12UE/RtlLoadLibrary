#pragma once
#include"pch.h"
#define ANYPROCESS (-1)
enum class ThreadState {
	Running,
	Waiting,
	Terminated,
	Unknown
};
using EnumCallback =std::function<bool(const THREADENTRY32&)>;
bool EnumThread(__in EnumCallback fnCallback, __in ULONG ulProcessID);

class ThreadObject {
public:
	ThreadObject() = default;
	ThreadObject(__in const ThreadObject& tagOthers);
	ThreadObject(__in ThreadObject&& tagOther);
	ThreadObject(__in DWORD dwTid);
	~ThreadObject();
	operator bool();
	BOOL GetContext(__inout CONTEXT& tagCtx);
	BOOL SetContext(__in const CONTEXT& tagCtx);
	DWORD Resume();
	DWORD Suspend();
	HANDLE NativeHandle();
	DWORD ThreadId();
	DWORD Prority();
	uintptr_t StartAddr();
	uintptr_t TebAddr();
	FILETIME CreationTime();
	FILETIME ExitTime();
	FILETIME KernalTime();
	FILETIME UserTime();
	ThreadState GetThreadState();
private:
	int FileTimeToMs(__in const FILETIME& tagFt);
	bool	  m_bValid = false;
	DWORD     m_ThreadId;
	HANDLE    m_hThread;
	uintptr_t m_ulStartAddr;
	uintptr_t m_ulTebAddr;
	FILETIME  m_tagCreationTime;
	DWORD     m_dwPriority;
};
std::vector<ThreadObject> GetCurrentThreads();
