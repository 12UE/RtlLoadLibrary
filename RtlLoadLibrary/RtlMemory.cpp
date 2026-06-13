#include "pch.h"
#include "RtlMemory.h"
#include "LdrLink.h"
#include "GlobalObject.h"
#include "RtlLoadLibrary.h"
#include "RtlGetProcAddress.h"
#include <random>
#include <chrono>


std::wstring RtlMemory::GenerateRandomFileName()
{
	const wchar_t* wszCharset = L"abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
	const size_t ulCharsetLen = wcslen(wszCharset);

	const size_t ulFileNameBodyLen = 8;
	static std::mt19937_64 tagRNG = [] {
		std::uint64_t ulSeed = std::chrono::steady_clock::now().time_since_epoch().count();
		ulSeed ^= std::random_device{}();
		std::mt19937_64 rng;
		rng.seed(ulSeed);
		return rng;
	}();
	static std::mutex mtxRNG;

	std::wstring wstrFilenameBody;
	wstrFilenameBody.reserve(ulFileNameBodyLen);
	std::uniform_int_distribution<size_t> tagDist(0, ulCharsetLen - 1);

	{
		std::lock_guard<std::mutex> lock(mtxRNG);
		for (size_t i = 0; i < ulFileNameBodyLen; ++i)
		{
			wstrFilenameBody += wszCharset[tagDist(tagRNG)];
		}
	}

	return wstrFilenameBody + L".dll";
}

SIZE_T RtlMemory::GetAlignedSize(__in SIZE_T nOriginalData, __in SIZE_T nAlignment)
{
	if (nAlignment == 0 || (nAlignment & (nAlignment - 1)) != 0) {
		DebugPrint(LEVEL_ERROR, "Invalid alignment value: 0x%IX", nAlignment);
		return 0;
	}
	SIZE_T nMaxValue = (SIZE_T)-1;
	if (nOriginalData > nMaxValue - nAlignment + 1) {
		DebugPrint(LEVEL_ERROR, "Alignment would overflow: data=0x%IX, align=0x%IX",
			nOriginalData, nAlignment);
		return 0;
	}

	return (nOriginalData + nAlignment - 1) & ~(nAlignment - 1);
}
bool RtlMemory::CheckHeader(__in DWORD dwIndex)
{
	PIMAGE_DATA_DIRECTORY pExportDir = GetHeader(dwIndex);
	if (!pExportDir) {
		DebugPrint(LEVEL_ERROR, "Invalid PIMAGE_DATA_DIRECTORY");
		return false;
	}
	return !(pExportDir->Size == 0 || pExportDir->VirtualAddress == 0);
}

const char* GetDataDirectoryName(__in DWORD dwIndex)
{
	switch (dwIndex) {
	case IMAGE_DIRECTORY_ENTRY_EXPORT:
		return "EXPORT";
	case IMAGE_DIRECTORY_ENTRY_IMPORT:
		return "IMPORT";
	case IMAGE_DIRECTORY_ENTRY_RESOURCE:
		return "RESOURCE";
	case IMAGE_DIRECTORY_ENTRY_EXCEPTION:
		return "EXCEPTION";
	case IMAGE_DIRECTORY_ENTRY_SECURITY:
		return "SECURITY";
	case IMAGE_DIRECTORY_ENTRY_BASERELOC:
		return "BASERELOC";
	case IMAGE_DIRECTORY_ENTRY_DEBUG:
		return "DEBUG";
	case IMAGE_DIRECTORY_ENTRY_ARCHITECTURE:
		return "ARCHITECTURE";
	case IMAGE_DIRECTORY_ENTRY_GLOBALPTR:
		return "GLOBALPTR";
	case IMAGE_DIRECTORY_ENTRY_TLS:
		return "TLS";
	case IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG:
		return "LOAD_CONFIG";
	case IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT:
		return "BOUND_IMPORT";
	case IMAGE_DIRECTORY_ENTRY_IAT:
		return "IAT";
	case IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT:
		return "DELAY_IMPORT";
	case IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR:
		return "COM_DESCRIPTOR";
	default:
		return "UNKNOWN";
	}
}
PIMAGE_DATA_DIRECTORY RtlMemory::GetHeader(__in DWORD dwIndex)
{
	if (!m_pNtHeader) {
		DebugPrint(LEVEL_ERROR, "Invalid NtHeader");
		return nullptr;
	}
	PIMAGE_DATA_DIRECTORY pDir = &m_pNtHeader->OptionalHeader.DataDirectory[dwIndex];
	if (!pDir) {
		DebugPrint(LEVEL_WARN, "Invalid PIMAGE_DATA_DIRECTORY");
		return nullptr;
	}
	DebugPrint(LEVEL_INFO, "Getting data directory: %s (Index: %d, RVA: 0x%08X, Size: 0x%08X)",
		GetDataDirectoryName(dwIndex), dwIndex, pDir->VirtualAddress, pDir->Size);
	return pDir;
}

bool RtlMemory::IsExecutableImage()
{
	if ((m_pNtHeader->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0) {
		DebugPrint(LEVEL_ERROR, "IsExecutableImage:Not a executable file! ");
		return false;
	}
	return true;
}


DWORD RtlMemory::ConvertSectionProtect(__in DWORD dwCharacteristics)
{
	bool bExecute = (dwCharacteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
	bool bRead = (dwCharacteristics & IMAGE_SCN_MEM_READ) != 0;
	bool bWrite = (dwCharacteristics & IMAGE_SCN_MEM_WRITE) != 0;
	DWORD dwProtect = PAGE_NOACCESS;
	if (bExecute) {
		if (!bRead) {
			bRead = true;
		}
		if (bWrite) {
			dwProtect = PAGE_EXECUTE_READWRITE;
		}else {
			dwProtect = PAGE_EXECUTE_READ;
		}
	}else {
		if (bRead && bWrite) {
			dwProtect = PAGE_READWRITE;
		}
		else if (bRead) {
			dwProtect = PAGE_READONLY;
		}
		else if (bWrite) {
			dwProtect = PAGE_READWRITE;
		}
	}
	if (dwCharacteristics & IMAGE_SCN_MEM_NOT_CACHED) {
		dwProtect |= PAGE_NOCACHE;
	}
	return dwProtect;
}

PVOID RtlMemory::RunImage(__in PVOID lpBytes, __in DWORD dwSize, __in DWORD dwFlags)
{
    UNREFERENCED_PARAMETER(dwSize);
	BOOL bOnlyResolve = (dwFlags & DONT_RESOLVE_DLL_REFERENCES) != 0;
	PVOID lpModuleBase = nullptr;
	bool bLdrLinkAdded = false;
	UNICODE_STRING ustrFullName{};
	UNICODE_STRING ustrBaseName{};

	m_pFileImage = lpBytes;
	m_pDosHeader = (PIMAGE_DOS_HEADER)m_pFileImage;
	if (m_pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::Invalid DOS header!");
		return nullptr;
	}
	m_pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)m_pFileImage + m_pDosHeader->e_lfanew);
	if (!m_pNtHeader) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::Invalid NT header!");
		return nullptr;
	}
	if (m_pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::Invalid NT signature!");
		return nullptr;
	}
	m_pSectionHeader = (PIMAGE_SECTION_HEADER)((PBYTE)m_pNtHeader + sizeof(IMAGE_NT_HEADERS));
	m_DefaultImageBase = m_pNtHeader->OptionalHeader.ImageBase;
	if (!m_DefaultImageBase) {
		DebugPrint(LEVEL_ERROR, "Invalid default image base");
		return nullptr;
	}
	DebugPrint(LEVEL_INFO, "RtlMemory::Default Image Base at 0x%x", m_DefaultImageBase);
	BOOL bASLR = m_pNtHeader->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
	if (!bASLR) {
		DebugPrint(LEVEL_WARN, ".reloc information was not enabled!");
	}
	lpModuleBase = ReserveVirtualMemory(m_DefaultImageBase, bASLR);
	if (!lpModuleBase) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Reserve virtual memory failed");
		goto cleanup_fail;
	}
	{
		std::vector <PE_SECTION_INFO> vecSections{};
		if (!CopyDataSections(vecSections)) {
			DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Failed to copy sections data");
			goto cleanup_fail;
		}
		if (vecSections.empty()) {
			DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Copy data section failed");
			goto cleanup_fail;
		}
		if (lpModuleBase == (LPVOID)m_DefaultImageBase) {
			DebugPrint(LEVEL_INFO, "RtlMemory::Image at default base don't need do relocation");
		}
		if (!FixImportTable()) {
			DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Fix import table failed");
			goto cleanup_fail;
		}
		if (lpModuleBase != (LPVOID)m_DefaultImageBase && bASLR && !FixRelocationTable()) {
			DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Fix relocation table failed");
			goto cleanup_fail;
		}

		PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)&((const unsigned char*)(m_VirtualBase))[m_pDosHeader->e_lfanew];
		pNtHeader->OptionalHeader.ImageBase = (ULONGLONG)m_VirtualBase;
		FixExportTable(
			(PIMAGE_DOS_HEADER)m_VirtualBase,
			reinterpret_cast<PIMAGE_NT_HEADERS>(
				reinterpret_cast<BYTE*>(m_VirtualBase) +
				reinterpret_cast<PIMAGE_DOS_HEADER>(m_VirtualBase)->e_lfanew));

		if (!bOnlyResolve && !FixExceptionTable()) {
			DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Fix exception table failed");
			goto cleanup_fail;
		}
		if (!bOnlyResolve && !ProtectSections(vecSections)) {
			DebugPrint(LEVEL_WARN, "RtlMemory::Run():ProtectSections Failed");
		}
		if (!bOnlyResolve) {
			if (!FixTLSTable()) {
				DebugPrint(LEVEL_ERROR, "RtlMemory::Run():TLS table fix failed");
				goto cleanup_fail;
			}
		}
		if (!bOnlyResolve && !Flush()) {
			DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Refresh instruction cache failed");
			goto cleanup_fail;
		}

		std::wstring wstrRandName = GenerateRandomFileName();
		if (m_wstrFullPath.empty()) {
			m_wstrFullPath = L"unknown/" + wstrRandName;
		}
		RtlInitUnicodeString(&ustrFullName, m_wstrFullPath.c_str());
		if (m_wstrFileName.empty()) {
			m_wstrFileName = wstrRandName;
		}
		RtlInitUnicodeString(&ustrBaseName, m_wstrFileName.c_str());
		if (!AddLdrLink(_MY_LDR_DATA_TABLE_ENTRY{ LIST_ENTRY{},LIST_ENTRY{},LIST_ENTRY{},GetModuleInstance(),GetEntryPoint(),(ULONG)GetVirtualSize(),ustrFullName,ustrBaseName }, !bOnlyResolve)) {
			DebugPrint(LEVEL_ERROR, "RtlMemory::Run():%s add Ldr link failed", m_wstrFileName.c_str());
			goto cleanup_fail;
		}
		bLdrLinkAdded = true;

		{
			std::wstring wstrKeyLower = m_wstrFileName;
			for (auto& ch : wstrKeyLower) ch = towlower(ch);
			GlobalObject::GetInstance().LockSelfLoadModulesExclusive();
			GlobalObject::GetInstance().SelfLoadModules.insert(std::make_pair(wstrKeyLower, m_VirtualBase));
			GlobalObject::GetInstance().UnlockSelfLoadModulesExclusive();
		}
		if (!bOnlyResolve && !ExecuteMain()) {
			DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Execute entry point failed");
			goto cleanup_fail;
		}
		if (!bOnlyResolve) {
			for (const auto& sec : vecSections) {
				BOOL bDiscardable = (sec.dwCharacteristics & IMAGE_SCN_MEM_DISCARDABLE) != 0;
				if (bDiscardable && !VirtualFree((LPVOID)(sec.VirtualAddress + (uintptr_t)lpModuleBase), sec.VirtualSize, MEM_DECOMMIT)) {
					DebugPrint(LEVEL_ERROR, "Free memory failed for %s", GetLastErrorAsString().c_str());
				}
			}
		}
	}
	return lpModuleBase;

cleanup_fail:
	if (bLdrLinkAdded) {
		RemoveLdrLink(&ustrBaseName, nullptr);
		RtlInvalidateProcCache(GetModuleInstance());
	}
	if (m_VirtualBase) {
		uintptr_t ulBase = reinterpret_cast<uintptr_t>(m_VirtualBase);
		GlobalObject::GetInstance().LockSEHTablesExclusive();
		auto& refSEHTables = GlobalObject::GetInstance().SEHTables;
		refSEHTables.erase(
			std::remove_if(refSEHTables.begin(), refSEHTables.end(),
				[ulBase](const _SEHInfo& info) { return info.m_pStartAddr == ulBase; }),
			refSEHTables.end());
		GlobalObject::GetInstance().UnlockSEHTablesExclusive();
	}
	if (lpModuleBase) {
		VirtualFree(lpModuleBase, 0, MEM_RELEASE);
	}
	{
		std::wstring wstrKeyLower = m_wstrFileName;
		for (auto& ch : wstrKeyLower) ch = towlower(ch);
		GlobalObject::GetInstance().LockSelfLoadModulesExclusive();
		auto& refSelfLoadMoudle = GlobalObject::GetInstance().SelfLoadModules;
		auto iter = refSelfLoadMoudle.find(wstrKeyLower);
		if (iter != refSelfLoadMoudle.end()) {
			refSelfLoadMoudle.erase(iter);
		}
		GlobalObject::GetInstance().UnlockSelfLoadModulesExclusive();
	}
	return nullptr;
}

RtlMemory::RtlMemory(__in HANDLE _hFile, __in const std::wstring& wstFullPath)
{
	m_wstrFullPath = wstFullPath;
	m_wstrFileName = GetFileName(wstFullPath);
	LARGE_INTEGER tagFileSize = { 0 };
	m_hFile = _hFile;
	if (!GetFileSizeEx(static_cast<HANDLE>(m_hFile), &tagFileSize)) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::RtlMemory:Get file size failed");
		return;
	}
	m_dwFileSize = static_cast<DWORD>(tagFileSize.QuadPart);
}

RtlMemory::RtlMemory(__in DWORD dwSize)
{
	m_dwFileSize = dwSize;
}

SIZE_T RtlMemory::GetVirtualSize()
{
	SIZE_T nMemoryAlign = m_pNtHeader->OptionalHeader.SectionAlignment;   //                             
	SIZE_T nSectionSum = GetAlignedSize(m_pNtHeader->OptionalHeader.SizeOfHeaders, nMemoryAlign);
	PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)((PBYTE)m_pNtHeader + sizeof(IMAGE_NT_HEADERS));
	if (!pSectionHeader) {
		DebugPrint(LEVEL_ERROR, "Invalid SectionHeader");
		return 0;
	}
	for (int i = 0; i < m_pNtHeader->FileHeader.NumberOfSections; ++i) {
		SIZE_T nTrueCodeSize = pSectionHeader[i].Misc.VirtualSize;
		SIZE_T nFileAlignCodeSize = pSectionHeader[i].SizeOfRawData;
		SIZE_T nMaxSize = (nFileAlignCodeSize > nTrueCodeSize) ? (nFileAlignCodeSize) : (nTrueCodeSize);
		SIZE_T nSectionSize = GetAlignedSize(pSectionHeader[i].VirtualAddress + nMaxSize, nMemoryAlign);
		if (nSectionSum < nSectionSize) {
			nSectionSum = nSectionSize;                                   
		}
	}
	return nSectionSum;
}

INT RtlMemory::RVAToFOA(__in PIMAGE_NT_HEADERS pImageNtHeader, __in DWORD dwTargetRVA)
{
	PIMAGE_SECTION_HEADER pImageSectionHeader = nullptr;
	INT nTargetFOA = -1;

	// PE32/PE32+                      
	if (pImageNtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		pImageSectionHeader =
		(PIMAGE_SECTION_HEADER)((LPBYTE)pImageNtHeader + sizeof(IMAGE_NT_HEADERS32));
	else
		pImageSectionHeader =
		(PIMAGE_SECTION_HEADER)((LPBYTE)pImageNtHeader + sizeof(IMAGE_NT_HEADERS64));

	//                     
	for (int i = 0; i < pImageNtHeader->FileHeader.NumberOfSections; i++)
	{
		if ((dwTargetRVA >= pImageSectionHeader->VirtualAddress) &&
			(dwTargetRVA < (pImageSectionHeader->VirtualAddress + pImageSectionHeader->SizeOfRawData)))
		{
			nTargetFOA = dwTargetRVA - pImageSectionHeader->VirtualAddress;
			nTargetFOA += pImageSectionHeader->PointerToRawData;
			break;  
		}

		pImageSectionHeader++;
	}

	return nTargetFOA;
}
LPVOID RtlMemory::GetEntryPoint()
{
	if (!m_pNtHeader || !m_VirtualBase) return nullptr;
	return (LPVOID)((LPBYTE)m_VirtualBase + m_pNtHeader->OptionalHeader.AddressOfEntryPoint);
}
#ifndef _WIN64

typedef struct _RTL_INVERTED_FUNCTION_TABLE_ENTRY {
	DWORD ExceptionDirectory;       // RtlEncodeSystemPointer encoded
	DWORD ImageBase;
	DWORD ImageSize;
	DWORD ExceptionDirectorySize;
} RTL_INVERTED_FUNCTION_TABLE_ENTRY;

typedef struct _RTL_INVERTED_FUNCTION_TABLE {
	DWORD Count;
	DWORD MaxCount;
	DWORD Overflow;
	DWORD NextEntrySEHandlerTableEncoded;
	RTL_INVERTED_FUNCTION_TABLE_ENTRY Entries[512];
} RTL_INVERTED_FUNCTION_TABLE;

static RTL_INVERTED_FUNCTION_TABLE* g_pInvertedTable = nullptr;
static PVOID g_pMrdataBase = nullptr;
static SIZE_T g_MrdataSize = 0;

void InitInvertedFunctionTable()
{
	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	if (!hNtdll || g_pInvertedTable) return;

	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hNtdll;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((PBYTE)hNtdll + pDos->e_lfanew);

	PIMAGE_SECTION_HEADER pSec = IMAGE_FIRST_SECTION(pNt);
	for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++, pSec++) {
		if (memcmp(pSec->Name, ".mrdata", 7) == 0 || memcmp(pSec->Name, ".data", 5) == 0) {
			PBYTE pData = (PBYTE)hNtdll + pSec->VirtualAddress;
			PBYTE pDataEnd = pData + pSec->Misc.VirtualSize;

			for (PBYTE p = pData; p < pDataEnd - 8; p++) {
				DWORD dwCount = *(DWORD*)p;
				DWORD dwMaxCount = *(DWORD*)(p + 4);
				if (dwCount <= dwMaxCount && dwMaxCount >= 0x100 && dwMaxCount <= 0x400) {
					MEMORY_BASIC_INFORMATION mbiCheck{};
				if (!VirtualQuery((void*)(p + 8), &mbiCheck, sizeof(mbiCheck)) ||
					mbiCheck.State != MEM_COMMIT ||
					(mbiCheck.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) continue;
					DWORD dwOverflow = *(DWORD*)(p + 8);
					if ((dwOverflow & ~1) == 0) {
						g_pInvertedTable = (RTL_INVERTED_FUNCTION_TABLE*)p;
						DebugPrint(LEVEL_INFO, "InitInvertedFunctionTable: found at 0x%p, count=%d, max=%d",
							p, dwCount, dwMaxCount);
						goto found;
					}
				}
			}
		}
	}
	return;

found:
	typedef NTSTATUS(NTAPI* fnNtQueryVirtualMemory)(HANDLE, PVOID, int, PVOID, SIZE_T, PSIZE_T);
	fnNtQueryVirtualMemory NtQueryVirtualMemory = (fnNtQueryVirtualMemory)
		GetProcAddress(hNtdll, "NtQueryVirtualMemory");
	if (NtQueryVirtualMemory) {
		MEMORY_BASIC_INFORMATION mbi{};
		NTSTATUS st = NtQueryVirtualMemory(GetCurrentProcess(), g_pInvertedTable,
			0 /*MemoryBasicInformation*/, &mbi, sizeof(mbi), nullptr);
		if (st == 0) {
			g_pMrdataBase = mbi.BaseAddress;
			g_MrdataSize = mbi.RegionSize;
			DebugPrint(LEVEL_INFO, "InitInvertedFunctionTable: mrdata base=0x%p, size=0x%zX",
				g_pMrdataBase, g_MrdataSize);
		}
	}
}

static bool InsertInvertedFunctionTable(PVOID ImageBase, ULONG ImageSize)
{
	if (!g_pInvertedTable) return false;
	if (g_pInvertedTable->Count >= g_pInvertedTable->MaxCount) return false;

	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)ImageBase;
	if (pDos->e_magic != IMAGE_DOS_SIGNATURE) return false;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((PBYTE)ImageBase + pDos->e_lfanew);
	if (pNt->Signature != IMAGE_NT_SIGNATURE) return false;

	PIMAGE_DATA_DIRECTORY pExceptDir = &pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
	DWORD dwExceptRVA = (pExceptDir && pExceptDir->Size) ? pExceptDir->VirtualAddress : 0;
	DWORD dwExceptCount = (pExceptDir && pExceptDir->Size) ? pExceptDir->Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY) : 0;

	typedef PVOID(NTAPI* fnRtlEncodeSystemPointer)(PVOID);
	fnRtlEncodeSystemPointer RtlEncodeSystemPointer = (fnRtlEncodeSystemPointer)
		GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlEncodeSystemPointer");

	DWORD dwEncoded = dwExceptRVA ? (DWORD)(DWORD_PTR)RtlEncodeSystemPointer(
		(PVOID)((DWORD_PTR)ImageBase + dwExceptRVA)) : 0;

	typedef NTSTATUS(NTAPI* fnNtProtectVirtualMemory)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
	fnNtProtectVirtualMemory NtProtectVirtualMemory = (fnNtProtectVirtualMemory)
		GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtProtectVirtualMemory");

	DWORD dwOldProtect = 0;
	if (NtProtectVirtualMemory && g_pMrdataBase && g_MrdataSize) {
		PVOID pBase = g_pMrdataBase;
		SIZE_T nSize = g_MrdataSize;
		NtProtectVirtualMemory(GetCurrentProcess(), &pBase, &nSize, PAGE_READWRITE, &dwOldProtect);
	}

	ULONG Index = 0;
	while (Index < g_pInvertedTable->Count) {
		if ((DWORD_PTR)ImageBase < (DWORD_PTR)g_pInvertedTable->Entries[Index].ImageBase)
			break;
		Index++;
	}

	if (Index < g_pInvertedTable->Count) {
		RtlMoveMemory(&g_pInvertedTable->Entries[Index + 1],
			&g_pInvertedTable->Entries[Index],
			(g_pInvertedTable->Count - Index) * sizeof(RTL_INVERTED_FUNCTION_TABLE_ENTRY));
	}

	g_pInvertedTable->Entries[Index].ImageBase = (DWORD)(DWORD_PTR)ImageBase;
	g_pInvertedTable->Entries[Index].ImageSize = ImageSize;
	g_pInvertedTable->Entries[Index].ExceptionDirectory = dwEncoded;
	g_pInvertedTable->Entries[Index].ExceptionDirectorySize = dwExceptCount;
	g_pInvertedTable->Count++;

	if (NtProtectVirtualMemory && g_pMrdataBase && g_MrdataSize && dwOldProtect) {
		PVOID pBase = g_pMrdataBase;
		SIZE_T nSize = g_MrdataSize;
		DWORD dwDummy;
		NtProtectVirtualMemory(GetCurrentProcess(), &pBase, &nSize, dwOldProtect, &dwDummy);
	}

	return true;
}

static EXCEPTION_DISPOSITION NTAPI TopLevelSEHandler(
	PEXCEPTION_RECORD  ExceptionRecord,
	PVOID              EstablisherFrame,
	PCONTEXT           ContextRecord,
	PVOID              DispatcherContext)
{
	UNREFERENCED_PARAMETER(DispatcherContext);
	if (!ExceptionRecord || !ContextRecord || !EstablisherFrame) {
		DebugPrint(LEVEL_ERROR, "TopLevelSEHandler: Invalid parameters");
		return ExceptionContinueSearch;
	}

	const PVOID pExceptionAddr = ExceptionRecord->ExceptionAddress;
	const DWORD dwEsp = ContextRecord->Esp;

	PEXCEPTION_REGISTRATION_RECORD pSEH;
	__asm {
		mov eax, fs:[0]
		mov pSEH, eax
	}

	const uintptr_t lo = static_cast<uintptr_t>(dwEsp) - 0x10000;
	const uintptr_t hi = static_cast<uintptr_t>(dwEsp) + 0x10000;

	while (pSEH && pSEH != (PEXCEPTION_REGISTRATION_RECORD)0xFFFFFFFF) {
		uintptr_t pRecAddr = reinterpret_cast<uintptr_t>(pSEH);
		if (pRecAddr < lo || pRecAddr > hi) {
			break;
		}

		if (pSEH->Handler) {
			PVOID pHandler = pSEH->Handler;

			PVOID pFoundStart = nullptr;
			PVOID pFoundEnd = nullptr;
			BOOL bLocked = TryAcquireSRWLockShared(&GlobalObject::GetInstance().m_SEHTablesLock);
			for (const auto& info : GlobalObject::GetInstance().SEHTables) {
				PVOID pImageStart = (PVOID)info.m_pStartAddr;
				PVOID pImageEnd = (PVOID)info.m_pEndAddr;
				if (!pImageStart || !pImageEnd) continue;
				if (pExceptionAddr >= pImageStart && pExceptionAddr <= pImageEnd) {
					pFoundStart = pImageStart;
					pFoundEnd = pImageEnd;
					break;
				}
			}
			if (bLocked) ReleaseSRWLockShared(&GlobalObject::GetInstance().m_SEHTablesLock);

			if (!pFoundStart) {
				pSEH = pSEH->Next;
				continue;
			}

			if (pHandler > pFoundStart && pHandler < pFoundEnd) {
				DebugPrint(LEVEL_ALL,
					"TopLevelSEHandler: dispatch to handler=%p at SEH rec=%p (ESP=0x%08X)",
					pHandler, pSEH, dwEsp);

				EXCEPTION_REGISTRATION_RECORD* pDispCtx = pSEH;
				EXCEPTION_DISPOSITION tagDisp = ((EXCEPTION_ROUTINE*)pHandler)(
					ExceptionRecord, pSEH, ContextRecord, &pDispCtx);

				if (tagDisp == ExceptionContinueExecution) {
					DebugPrint(LEVEL_ALL,
						"TopLevelSEHandler: Handler returned ExceptionContinueExecution");
					return ExceptionContinueExecution;
				}
			}
		}
		pSEH = pSEH->Next;
	}

	DebugPrint(LEVEL_ALL,
		"TopLevelSEHandler: No handler handled exception at %p (ESP=0x%08X)",
		pExceptionAddr, dwEsp);
	return ExceptionContinueSearch;
}

static LONG CALLBACK VehWrapper(PEXCEPTION_POINTERS ExceptionInfo)
{


	EXCEPTION_DISPOSITION tagDisp = TopLevelSEHandler(
		ExceptionInfo->ExceptionRecord, (PVOID)__readfsdword(0),
		ExceptionInfo->ContextRecord, nullptr);

	return (tagDisp == ExceptionContinueExecution)
		? EXCEPTION_CONTINUE_EXECUTION
		: EXCEPTION_CONTINUE_SEARCH;
}
#endif

bool RtlMemory::FixExceptionTable()
{	
	SIZE_T nModuleSize = GetVirtualSize();
	if (nModuleSize <= 0) {
		DebugPrint(LEVEL_ERROR, "FixExceptionTable: Invalid module size %d", nModuleSize);
		return false;
	}
#ifndef _WIN64  
	{
		static std::once_flag s_vehOnce;
		std::call_once(s_vehOnce, []() {
			AddVectoredExceptionHandler(1, VehWrapper);
			DebugPrint(LEVEL_INFO, "VEH handler registered (TopLevelSEHandler via VehWrapper)");
		});
	}

	_SEHInfo tagSEHInfo(
		m_wstrFullPath.c_str(),
		m_wstrFullPath.size(),
		reinterpret_cast<uintptr_t>(m_VirtualBase),
		reinterpret_cast<uintptr_t>(m_VirtualBase) + nModuleSize
	);
	GlobalObject::GetInstance().LockSEHTablesExclusive();
	GlobalObject::GetInstance().SEHTables.push_back(tagSEHInfo);
	GlobalObject::GetInstance().UnlockSEHTablesExclusive();
	DebugPrint(LEVEL_INFO, "FixExceptionTable: Registered [0x%p - 0x%p] for SEH dispatch",
		m_VirtualBase,
		(PVOID)(reinterpret_cast<uintptr_t>(m_VirtualBase) + nModuleSize));

	//if (InsertInvertedFunctionTable(m_VirtualBase, (ULONG)nModuleSize)) {
	//	DebugPrint(LEVEL_INFO, "FixExceptionTable: Inserted into InvertedFunctionTable for 0x%p",
	//		m_VirtualBase);
	//}
#else
	UNREFERENCED_PARAMETER(nModuleSize);
	DebugPrint(LEVEL_INFO, "x64 platform: table-based exception handling via OS");
#endif
	return true;
}

bool RtlMemory::FixLoadConfigTable()
{
	PIMAGE_DATA_DIRECTORY pLoadConfigDir = GetHeader(IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG);
	if (!pLoadConfigDir || pLoadConfigDir->VirtualAddress == 0 || pLoadConfigDir->Size == 0) {
		DebugPrint(LEVEL_INFO, "No Load Configuration Directory found");
		return true;
	}

	PIMAGE_LOAD_CONFIG_DIRECTORY pLoadConfig = reinterpret_cast<PIMAGE_LOAD_CONFIG_DIRECTORY>(
		reinterpret_cast<BYTE*>(m_VirtualBase) + pLoadConfigDir->VirtualAddress);

	if (!pLoadConfig) {
		DebugPrint(LEVEL_ERROR, "Invalid Load Configuration Directory address");
		return false;
	}

	DebugPrint(LEVEL_INFO, "Fixing Load Configuration Directory for %s", m_wstrFileName.c_str());
	if (pLoadConfig->SecurityCookie != 0) {
		DWORD_PTR dwptrCookieRVA = pLoadConfig->SecurityCookie - m_pNtHeader->OptionalHeader.ImageBase;
		PVOID pCookieAddr = reinterpret_cast<PVOID>(
			reinterpret_cast<BYTE*>(m_VirtualBase) + dwptrCookieRVA);
		SIZE_T nModuleSize = GetVirtualSize();
		if (pCookieAddr >= m_VirtualBase &&
			pCookieAddr < reinterpret_cast<BYTE*>(m_VirtualBase) + nModuleSize) {

			if (!InitializeSecurityCookie(pCookieAddr)) {
				DebugPrint(LEVEL_WARN, "Failed to initialize security cookie");
			}
			else {
				DebugPrint(LEVEL_INFO, "Security cookie initialized at 0x%p", pCookieAddr);
				pLoadConfig->SecurityCookie = reinterpret_cast<DWORD_PTR>(pCookieAddr);
			}
		}
		else {
			DebugPrint(LEVEL_ERROR, "Security cookie address 0x%p out of module range (0x%p - 0x%p)",
				pCookieAddr, m_VirtualBase, reinterpret_cast<BYTE*>(m_VirtualBase) + nModuleSize);
		}
	}

#ifndef _WIN64
	if (pLoadConfig->SEHandlerTable != 0 && pLoadConfig->SEHandlerCount > 0) {
		DebugPrint(LEVEL_INFO, "Processing SafeSEH handlers...");
		DWORD dwOriginalTableRVA = static_cast<DWORD>(
			pLoadConfig->SEHandlerTable - m_pNtHeader->OptionalHeader.ImageBase);
		DWORD* pSEHandlerTable = reinterpret_cast<DWORD*>(
			reinterpret_cast<BYTE*>(m_VirtualBase) + dwOriginalTableRVA);

		DWORD dwHandlerCount = static_cast<DWORD>(pLoadConfig->SEHandlerCount);
		SIZE_T nModuleSize = GetVirtualSize();
		if (reinterpret_cast<BYTE*>(pSEHandlerTable) >= m_VirtualBase &&
			reinterpret_cast<BYTE*>(pSEHandlerTable + dwHandlerCount) <=
			reinterpret_cast<BYTE*>(m_VirtualBase) + nModuleSize) {
			DWORD_PTR dwRelocDelta = reinterpret_cast<DWORD_PTR>(m_VirtualBase) -
				m_pNtHeader->OptionalHeader.ImageBase;
			UNREFERENCED_PARAMETER(dwRelocDelta);

			DebugPrint(LEVEL_INFO, "Relocating %d SafeSEH handlers (delta: 0x%p)",
				dwHandlerCount, (PVOID)dwRelocDelta);

			for (DWORD i = 0; i < dwHandlerCount; i++) {
				DWORD dwOriginalHandlerVA = pSEHandlerTable[i];
				if (dwOriginalHandlerVA > m_pNtHeader->OptionalHeader.ImageBase) {
					DWORD dwHandlerRVA = dwOriginalHandlerVA - m_pNtHeader->OptionalHeader.ImageBase;
					DWORD_PTR newHandlerVA = reinterpret_cast<DWORD_PTR>(m_VirtualBase) + dwHandlerRVA;

					pSEHandlerTable[i] = static_cast<DWORD>(newHandlerVA);

					DebugPrint(LEVEL_DEBUG, "  Handler[%d]: 0x%08X -> 0x%p (RVA: 0x%08X)",
						i, dwOriginalHandlerVA, (PVOID)newHandlerVA, dwHandlerRVA);
				}
				else {
					DWORD_PTR dwNewHandlerVA = reinterpret_cast<DWORD_PTR>(m_VirtualBase) + dwOriginalHandlerVA;
					pSEHandlerTable[i] = static_cast<DWORD>(dwNewHandlerVA);

					DebugPrint(LEVEL_DEBUG, "  Handler[%d]: RVA 0x%08X -> VA 0x%p",
						i, dwOriginalHandlerVA, (PVOID)dwNewHandlerVA);
				}
			}
			pLoadConfig->SEHandlerTable = reinterpret_cast<DWORD>(pSEHandlerTable);

			DebugPrint(LEVEL_INFO, "SafeSEH table fixed: %d handlers at 0x%p",
				dwHandlerCount, pSEHandlerTable);
			if (!ValidateSEHandlers(pSEHandlerTable, dwHandlerCount)) {
				DebugPrint(LEVEL_WARN, "SafeSEH handler validation failed");
			}

		}
		else {
			DebugPrint(LEVEL_ERROR, "SafeSEH table address 0x%p out of module range",
				pSEHandlerTable);
			return false;
		}
	}
	else {
		DebugPrint(LEVEL_INFO, "No SafeSEH handlers to process");
	}
#else
	// x64                SafeSEH                                    
	DebugPrint(LEVEL_INFO, "x64 platform: SafeSEH not applicable");
#endif


	if (pLoadConfig->GuardCFCheckFunctionPointer != 0) {
		ULONGLONG ullOriginalPointer = pLoadConfig->GuardCFCheckFunctionPointer;
		ULONGLONG ullPointerRVA = ullOriginalPointer - m_pNtHeader->OptionalHeader.ImageBase;
		ULONGLONG ullNewPointer = reinterpret_cast<ULONGLONG>(m_VirtualBase) + ullPointerRVA;
		pLoadConfig->GuardCFCheckFunctionPointer = (DWORD)ullNewPointer;
		DebugPrint(LEVEL_INFO, "CFG check function pointer fixed: 0x%llX -> 0x%llX",
			ullOriginalPointer, ullNewPointer);
	}

	if (pLoadConfig->GuardCFFunctionTable != 0 && pLoadConfig->GuardCFFunctionCount > 0) {
		ULONGLONG ullOriginalTable = pLoadConfig->GuardCFFunctionTable;
		ULONGLONG ullTableRVA = ullOriginalTable - m_pNtHeader->OptionalHeader.ImageBase;
		ULONGLONG ullNewTable = reinterpret_cast<ULONGLONG>(m_VirtualBase) + ullTableRVA;
		pLoadConfig->GuardCFFunctionTable = (DWORD)ullNewTable;

		DebugPrint(LEVEL_INFO, "CFG function table fixed: %llu entries at 0x%llX -> 0x%llX",
			pLoadConfig->GuardCFFunctionCount, ullOriginalTable, ullNewTable);
		if (!FixCFGFunctionTable(reinterpret_cast<PVOID>(ullNewTable),
			static_cast<DWORD>(pLoadConfig->GuardCFFunctionCount))) {
			DebugPrint(LEVEL_WARN, "Failed to fix CFG function table entries");
		}
	}
	if (pLoadConfig->Size >= sizeof(IMAGE_LOAD_CONFIG_DIRECTORY)) {
		FixExtendedLoadConfigFields(pLoadConfig);
	}

	DebugPrint(LEVEL_INFO, "Load Configuration Directory fixed successfully");
	return true;
}
bool RtlMemory::ValidateSEHandlers(__in DWORD* pHandlerTable, __in DWORD dwCount)
{
#ifndef _WIN64
	SIZE_T nModuleSize = GetVirtualSize();
	BYTE* pModuleStart = reinterpret_cast<BYTE*>(m_VirtualBase);
	BYTE* pModuleEnd = pModuleStart + nModuleSize;

	for (DWORD i = 0; i < dwCount; i++) {
		PVOID pHandlerAddr = reinterpret_cast<PVOID>(pHandlerTable[i]);
		if (pHandlerAddr < pModuleStart || pHandlerAddr >= pModuleEnd) {
			DebugPrint(LEVEL_ERROR, "Invalid handler address at index %d: 0x%p (out of range)",
				i, pHandlerAddr);
			return false;
		}
		if (!IsAddressInExecutableSection(pHandlerAddr)) {
			DebugPrint(LEVEL_WARN, "Handler at index %d (0x%p) not in executable section",
				i, pHandlerAddr);
		}
	}

	DebugPrint(LEVEL_INFO, "All %d SafeSEH handlers validated successfully", dwCount);
	return true;
#else
	return true;
#endif
}


bool RtlMemory::IsAddressInExecutableSection(__in PVOID pAddress)
{
	PIMAGE_SECTION_HEADER pSectionHeaders = IMAGE_FIRST_SECTION(m_pNtHeader);
	for (WORD i = 0; i < m_pNtHeader->FileHeader.NumberOfSections; i++, pSectionHeaders++) {
		if (pSectionHeaders->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
			BYTE* pSectionStart = reinterpret_cast<BYTE*>(m_VirtualBase) + pSectionHeaders->VirtualAddress;
			BYTE* pSectionEnd = pSectionStart + pSectionHeaders->Misc.VirtualSize;

			if (pAddress >= pSectionStart && pAddress < pSectionEnd) {
				return true;
			}
		}
	}

	return false;
}

bool RtlMemory::FixCFGFunctionTable(__in PVOID pTable, __in DWORD dwCount)
{


#ifdef _WIN64
	typedef struct _CFG_ENTRY {
		DWORD RVA;
		BYTE  Flags; //                                     
	} CFG_ENTRY;
#else
	typedef DWORD CFG_ENTRY;
#endif

	CFG_ENTRY* pEntries = reinterpret_cast<CFG_ENTRY*>(pTable);
	SIZE_T nModuleSize = GetVirtualSize();

	for (DWORD i = 0; i < dwCount; i++) {
#ifdef _WIN64
		DWORD dwRva = pEntries[i].RVA;
#else
		DWORD dwRva = pEntries[i];
#endif
		if (dwRva >= nModuleSize) {
			DebugPrint(LEVEL_ERROR, "Invalid CFG entry RVA at index %d: 0x%08X", i, dwRva);
			return false;
		}
	}

	DebugPrint(LEVEL_INFO, "CFG function table entries validated");
	return true;
}
void RtlMemory::FixExtendedLoadConfigFields(__inout PIMAGE_LOAD_CONFIG_DIRECTORY pLoadConfig)
{
	DWORD_PTR dwRelocDelta = reinterpret_cast<DWORD_PTR>(m_VirtualBase) -
		m_pNtHeader->OptionalHeader.ImageBase;
	if (pLoadConfig->Size >= offsetof(IMAGE_LOAD_CONFIG_DIRECTORY, GuardAddressTakenIatEntryTable) + sizeof(ULONGLONG)) {
		if (pLoadConfig->GuardAddressTakenIatEntryTable != 0) {
			pLoadConfig->GuardAddressTakenIatEntryTable += dwRelocDelta;
			DebugPrint(LEVEL_DEBUG, "Fixed GuardAddressTakenIatEntryTable");
		}
	}

	if (pLoadConfig->Size >= offsetof(IMAGE_LOAD_CONFIG_DIRECTORY, GuardLongJumpTargetTable) + sizeof(ULONGLONG)) {
		if (pLoadConfig->GuardLongJumpTargetTable != 0) {
			pLoadConfig->GuardLongJumpTargetTable += dwRelocDelta;
			DebugPrint(LEVEL_DEBUG, "Fixed GuardLongJumpTargetTable");
		}
	}
}
bool RtlMemory::InitializeSecurityCookie(__out PVOID pCookieAddr)
{
	DWORD_PTR dwptrCookie = 0;

	LARGE_INTEGER perfCounter;
	QueryPerformanceCounter(&perfCounter);

	dwptrCookie = (DWORD_PTR)perfCounter.QuadPart;
	dwptrCookie ^= GetCurrentProcessId();
	dwptrCookie ^= GetCurrentThreadId();
	dwptrCookie ^= reinterpret_cast<DWORD_PTR>(&dwptrCookie);
	if (dwptrCookie == 0) {
		dwptrCookie = 0xBB40E64E;
	}

	//          cookie
#ifdef _WIN64
	* reinterpret_cast<ULONGLONG*>(pCookieAddr) = dwptrCookie;
#else
	* reinterpret_cast<DWORD*>(pCookieAddr) = static_cast<DWORD>(dwptrCookie);
#endif

	DebugPrint(LEVEL_DEBUG, "Security cookie set to 0x%p at address 0x%p",
		(PVOID)dwptrCookie, pCookieAddr);

	return true;
}

bool RtlMemory::RunFile(__in DWORD dwFlags)
{
	if (m_wstrFileName.empty() || m_wstrFullPath.empty()) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::Run():Empty file name or fullpath.");
		return false;
	}
	FileMap tagFileMap(static_cast<HANDLE>(m_hFile));
	if (!tagFileMap) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::Run():File map failed");
		return false;
	}
	m_pFileImage = tagFileMap.GetMap();
	if (!m_pFileImage) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::Run():File map get address failed");
		return false;
	}
	return RunImage(m_pFileImage, (DWORD)tagFileMap.GetFileSize(), dwFlags);
}

bool RtlMemory::IsDosHeaderValid()
{
	if (m_pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {//                                     pe        
		DebugPrint(LEVEL_ERROR, "Invalid dos header for %ls", m_wstrFullPath.c_str());
		return false;
	}
	if (m_pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS) == 0) {
		DebugPrint(LEVEL_ERROR, "Invalid Nt header for %ls", m_wstrFullPath.c_str());
		return false;
	}
	return true;
}

bool RtlMemory::IsNtHeaderValid()
{
	if (m_pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
		DebugPrint(LEVEL_ERROR, "Not a valid value of IMAGE_NT_SIGNATURE %d", m_pNtHeader->Signature);
		return false;
	}
	if (m_pNtHeader->FileHeader.Machine != HOST_MACHINE) {
		DebugPrint(LEVEL_ERROR,"RtlMemory::IsNtHeaderValid():Platform error,you could be obstacle the x86 and x64");
		return false;
	}
	if ((m_pNtHeader->OptionalHeader.SectionAlignment & 1) != 0) {
		DebugPrint(LEVEL_ERROR, "Invalid OptionalHeader.SectionAlignment");
		return false;
	}
	return true;
}

bool RtlMemory::IsOptionalHeaderSizeCorrect()
{
	if (m_pNtHeader->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER))return false;
	return true;
}
void* RtlMemory::ReserveVirtualMemory(__in DWORD_PTR _defaultImageBase, __in BOOL bASLR)
{
	SIZE_T nImageLength = GetVirtualSize();
	if (nImageLength == 0) {
		DebugPrint(LEVEL_ERROR, "ReserveVirtualMemory: Invalid image length");
		return nullptr;
	}
	SYSTEM_INFO tagSysInfo={0};
	GetSystemInfo(&tagSysInfo);
	DWORD_PTR dwptrAllocationGranularity = tagSysInfo.dwAllocationGranularity;
	DWORD_PTR dwptrAlignedBase = (_defaultImageBase / dwptrAllocationGranularity) * dwptrAllocationGranularity;
	if (dwptrAlignedBase != _defaultImageBase) {
		DebugPrint(LEVEL_WARN, "Default base address 0x%p aligned to 0x%p",
			(LPVOID)_defaultImageBase, (LPVOID)dwptrAlignedBase);
	}
	m_VirtualBase = VirtualAlloc((LPVOID)dwptrAlignedBase, nImageLength, MEM_RESERVE, PAGE_NOACCESS);
	if (m_VirtualBase) {
		DebugPrint(LEVEL_INFO, "Memory reserved at preferred address: 0x%p", m_VirtualBase);
		return m_VirtualBase;
	}
	DWORD dwError = GetLastError();
	DebugPrint(LEVEL_INFO, "Default address alloc failed! Error: %d (0x%08X)", dwError, dwError);
	if (bASLR) {
		m_VirtualBase = VirtualAlloc(nullptr, nImageLength, MEM_RESERVE, PAGE_NOACCESS);
	}
	if (!m_VirtualBase) {
		dwError = GetLastError();
		DebugPrint(LEVEL_ERROR, "Address alloc failed! Error: %d (0x%08X)", dwError, dwError);
		return nullptr;
	}

	DebugPrint(LEVEL_INFO, "Memory reserved at system-chosen address: 0x%p", m_VirtualBase);
	return m_VirtualBase;
}
void* RtlMemory::CommitMemory(__in void* lpAddr, __in SIZE_T nRegionSize)
{
	if (!lpAddr || nRegionSize == 0) {
		DebugPrint(LEVEL_ERROR, "Invalid parameters: addr=0x%p, size=0x%X", lpAddr, nRegionSize);
		return nullptr;
	}
	static const ULONG_PTR dwPageSize = [] {
		SYSTEM_INFO si{};
		GetSystemInfo(&si);
		return (ULONG_PTR)si.dwPageSize;
	}();

	ULONG_PTR ulptrAlignedStart = (ULONG_PTR)lpAddr & ~(dwPageSize - 1);
	ULONG_PTR ulptrAlignedEnd = ((ULONG_PTR)lpAddr + nRegionSize + dwPageSize - 1) & ~(dwPageSize - 1);
	SIZE_T nAlignedSize = ulptrAlignedEnd - ulptrAlignedStart;
	MEMORY_BASIC_INFORMATION tagMbi{};
	if (!VirtualQuery((LPVOID)ulptrAlignedStart, &tagMbi, sizeof(tagMbi))) {
		DebugPrint(LEVEL_ERROR, "VirtualQuery failed at 0x%p! Error: %s",
			(void*)ulptrAlignedStart, GetLastErrorAsString().c_str());
		return nullptr;
	}
	if (tagMbi.State != MEM_RESERVE && tagMbi.State != MEM_COMMIT) {
		DebugPrint(LEVEL_ERROR, "Invalid memory state 0x%X at 0x%p", tagMbi.State, lpAddr);
		return nullptr;
	}
	if (tagMbi.State == MEM_COMMIT && tagMbi.RegionSize >= nAlignedSize) {
		return lpAddr;
	}
	void* pResult = VirtualAlloc((LPVOID)ulptrAlignedStart, nAlignedSize, MEM_COMMIT, PAGE_READWRITE);

	if (!pResult) {
		DebugPrint(LEVEL_ERROR,
			"VirtualAlloc(COMMIT) failed! Addr: 0x%p, Size: 0x%zX, Error: %s",
			(void*)ulptrAlignedStart, nAlignedSize, GetLastErrorAsString().c_str());
		return nullptr;
	}

	DebugPrint(LEVEL_INFO, "Successfully committed memory: 0x%p - 0x%p (size: 0x%zX)",
		(void*)ulptrAlignedStart, (void*)ulptrAlignedEnd, nAlignedSize);

	return lpAddr;
}

BOOL RtlMemory::CopyDataSections(__inout std::vector<PE_SECTION_INFO>& vecSections)
{
	if (!m_pFileImage || !m_pNtHeader || !m_VirtualBase) {
		DebugPrint(LEVEL_ERROR, "Invalid class members!");
		return FALSE;
	}

	static const ULONG_PTR dwPageSize = [] {
		SYSTEM_INFO si{};
		GetSystemInfo(&si);
		return (ULONG_PTR)si.dwPageSize;
	}();

	auto AlignUp = [](ULONG_PTR addr) -> ULONG_PTR {
		return (addr + dwPageSize - 1) & ~(dwPageSize - 1);
	};

	WORD wNumsSections = m_pNtHeader->FileHeader.NumberOfSections;
	vecSections.reserve(wNumsSections);
	DWORD dwImageSize = m_pNtHeader->OptionalHeader.SizeOfImage;
	int nHeaderLength = m_pNtHeader->OptionalHeader.SizeOfHeaders;

	struct CommitRange {
		ULONG_PTR start;
		ULONG_PTR end;
	};

	std::vector<CommitRange> vecRanges;

	vecRanges.push_back({
		(ULONG_PTR)m_VirtualBase,
		AlignUp((ULONG_PTR)m_VirtualBase + nHeaderLength)
	});

	for (WORD i = 0; i < wNumsSections; ++i) {
		IMAGE_SECTION_HEADER& refSection = m_pSectionHeader[i];

		if (refSection.VirtualAddress == 0 ||
			(refSection.SizeOfRawData == 0 && refSection.Misc.VirtualSize == 0))
			continue;

		DWORD dwVirtualSize = refSection.Misc.VirtualSize ?
			refSection.Misc.VirtualSize : refSection.SizeOfRawData;
		if (refSection.VirtualAddress + dwVirtualSize > dwImageSize)
			continue;

		vecRanges.push_back({
			(ULONG_PTR)m_VirtualBase + refSection.VirtualAddress,
			AlignUp((ULONG_PTR)m_VirtualBase + refSection.VirtualAddress + dwVirtualSize)
		});
	}

	std::sort(vecRanges.begin(), vecRanges.end(),
		[](const CommitRange& a, const CommitRange& b) { return a.start < b.start; });

	std::vector<CommitRange> vecMerged;
	for (const auto& r : vecRanges) {
		if (vecMerged.empty() || r.start > vecMerged.back().end) {
			vecMerged.push_back(r);
		} else if (r.end > vecMerged.back().end) {
			vecMerged.back().end = r.end;
		}
	}

	for (const auto& r : vecMerged) {
		MEMORY_BASIC_INFORMATION tagMbi{};
		if (VirtualQuery((LPVOID)r.start, &tagMbi, sizeof(tagMbi))) {
			if (tagMbi.State == MEM_COMMIT) continue;
		}
		void* pResult = VirtualAlloc((LPVOID)r.start,
			r.end - r.start, MEM_COMMIT, PAGE_READWRITE);
		if (!pResult) {
			DebugPrint(LEVEL_ERROR,
				"Batch commit failed at 0x%p, size 0x%zX: %s",
				(LPVOID)r.start, r.end - r.start,
				GetLastErrorAsString().c_str());
			return FALSE;
		}
	}

	if (memcpy_s(m_VirtualBase, nHeaderLength, m_pFileImage, nHeaderLength) != 0) {
		DebugPrint(LEVEL_ERROR, "Failed to copy PE headers!");
		return FALSE;
	}

	for (WORD i = 0; i < wNumsSections; ++i) {
		IMAGE_SECTION_HEADER& refSection = m_pSectionHeader[i];

		if (refSection.VirtualAddress == 0 ||
			(refSection.SizeOfRawData == 0 && refSection.Misc.VirtualSize == 0))
			continue;

		DWORD dwVirtualSize = refSection.Misc.VirtualSize ?
			refSection.Misc.VirtualSize : refSection.SizeOfRawData;
		if (refSection.VirtualAddress + dwVirtualSize > dwImageSize)
			continue;

		PVOID pSectionAddr = (PVOID)((DWORD_PTR)m_VirtualBase + refSection.VirtualAddress);
		DWORD dwCommitSize = (DWORD)AlignUp(dwVirtualSize);

		if (refSection.SizeOfRawData > 0) {
			memcpy_s(pSectionAddr, dwCommitSize,
				(PVOID)((DWORD_PTR)m_pFileImage + refSection.PointerToRawData),
				refSection.SizeOfRawData);
		} else {
			memset(pSectionAddr, 0, dwVirtualSize);
		}

		if (dwVirtualSize > refSection.SizeOfRawData) {
			memset((PBYTE)pSectionAddr + refSection.SizeOfRawData, 0,
				dwVirtualSize - refSection.SizeOfRawData);
		}

		PE_SECTION_INFO tagInfo = {0};
		memcpy_s(tagInfo.szSectionName, sizeof(tagInfo.szSectionName),
			refSection.Name, IMAGE_SIZEOF_SHORT_NAME);
		tagInfo.dwCharacteristics = refSection.Characteristics;
		tagInfo.dwMemProtect = ConvertSectionProtect(refSection.Characteristics);
		tagInfo.ImageBase = (ULONG_PTR)m_VirtualBase;
		tagInfo.VirtualAddress = refSection.VirtualAddress;
		tagInfo.VirtualSize = dwVirtualSize;
		tagInfo.MemoryStartAddr = tagInfo.ImageBase + refSection.VirtualAddress;
		tagInfo.MemoryEndAddr = tagInfo.MemoryStartAddr + dwVirtualSize;

		vecSections.push_back(tagInfo);
	}

	m_pDosHeader = (PIMAGE_DOS_HEADER)m_VirtualBase;
	m_pNtHeader = (PIMAGE_NT_HEADERS)((PVOID)((DWORD_PTR)m_VirtualBase + m_pDosHeader->e_lfanew));
	m_pSectionHeader = (PIMAGE_SECTION_HEADER)((PBYTE)m_pNtHeader + sizeof(IMAGE_NT_HEADERS));

	return TRUE;
}

bool RtlMemory::ProtectSections(__in const std::vector<PE_SECTION_INFO>& vecSections)
{
	if (vecSections.empty()) {
		DebugPrint(LEVEL_WARN, "No region need protect!");
		return true;
	}

	auto sorted = vecSections;
	std::sort(sorted.begin(), sorted.end(),
		[](const PE_SECTION_INFO& a, const PE_SECTION_INFO& b) noexcept {
			return a.MemoryStartAddr < b.MemoryStartAddr;
		});

	struct MergedRange {
		ULONG_PTR addr;
		SIZE_T     size;
		DWORD      protect;
	};
	std::vector<MergedRange> ranges;
	ranges.reserve(sorted.size());

	for (const auto& sec : sorted) {
		if (!sec.MemoryStartAddr || sec.VirtualSize == 0) {
			DebugPrint(LEVEL_ERROR, "Invalid section: addr=0x%p, size=0x%X",
				(void*)sec.MemoryStartAddr, sec.VirtualSize);
			return false;
		}
		if (sec.dwMemProtect == 0 || sec.dwMemProtect > PAGE_TARGETS_INVALID) {
			DebugPrint(LEVEL_ERROR, "Invalid protection flags: 0x%X for addr 0x%p",
				sec.dwMemProtect, (void*)sec.MemoryStartAddr);
			return false;
		}

		ULONG_PTR secEnd = sec.MemoryStartAddr + sec.VirtualSize;

		if (!ranges.empty() &&
			ranges.back().protect == sec.dwMemProtect &&
			ranges.back().addr + ranges.back().size == sec.MemoryStartAddr) {
			ranges.back().size = secEnd - ranges.back().addr;
		} else {
			ranges.push_back({ sec.MemoryStartAddr, sec.VirtualSize, sec.dwMemProtect });
		}
	}

	for (const auto& r : ranges) {
		DWORD dwOld = 0;
		if (!VirtualProtect((LPVOID)r.addr, r.size, r.protect, &dwOld)) {
			DebugPrint(LEVEL_ERROR,
				"VirtualProtect failed for 0x%p (size: 0x%zX, prot: 0x%X): %s",
				(void*)r.addr, r.size, r.protect, GetLastErrorAsString().c_str());
			return false;
		}
	}

	DebugPrint(LEVEL_INFO, "ProtectSections: %zu sections merged into %zu VirtualProtect call(s)",
		sorted.size(), ranges.size());
	return true;
}

WORD RtlMemory::GetPlatformMaxSectionsForPE()
{
	if (!m_pNtHeader) {
		DebugPrint(LEVEL_ERROR, "Invalid NtHeader!");
		return 0;
	}

#ifdef _WIN64
	if (m_pNtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
		return STRICT_MAX_SECTIONS;
	}
#else
	if (m_pNtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
		return REASONABLE_MAX_SECTIONS;
	}
#endif
	return 0;
}

bool RtlMemory::FixRelocationTable()
{
	if (!CheckHeader(IMAGE_DIRECTORY_ENTRY_BASERELOC)) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::CopyDataSections():%s don't has rolocation table", m_wstrFileName.c_str());
		return true;
	}

	PIMAGE_BASE_RELOCATION pBaseRelocation = (PIMAGE_BASE_RELOCATION)((ULONG_PTR)m_VirtualBase +
		m_pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	if (!pBaseRelocation) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::CopyDataSections():Get BaseRelocation failed [failed]");
		return false;
	}

	DebugPrint(LEVEL_INFO, "RtlMemory::CopyDataSections():Start to fix relocation table, base: 0x%p", m_VirtualBase);

#ifdef _WIN64
	const ULONGLONG ullDelta = (ULONGLONG)m_VirtualBase - m_pNtHeader->OptionalHeader.ImageBase;
#else
	const DWORD dwDelta = (DWORD)m_VirtualBase - m_pNtHeader->OptionalHeader.ImageBase;
#endif

	while ((pBaseRelocation->VirtualAddress + pBaseRelocation->SizeOfBlock) != 0) {
		WORD* pwRelocationData = (WORD*)((PBYTE)pBaseRelocation + sizeof(IMAGE_BASE_RELOCATION));
		int nNumberOfRelocation = (pBaseRelocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);

		for (int i = 0; i < nNumberOfRelocation; i++) {
			WORD wRelocationType = (pwRelocationData[i] & 0xF000) >> 12;
			WORD wOffset = pwRelocationData[i] & 0x0FFF;

			switch (wRelocationType) {
			case IMAGE_REL_BASED_HIGHADJ:

			case IMAGE_REL_BASED_HIGH:
			{
#ifndef _WIN64
				DWORD* pdwAddress = (DWORD*)((PBYTE)m_VirtualBase + pBaseRelocation->VirtualAddress + wOffset);
				DWORD dwOldAddr = *pdwAddress;
				WORD wOldHigh16 = (WORD)((dwOldAddr >> 16) & 0xFFFF);
				WORD wOldLow16  = (WORD)(dwOldAddr & 0xFFFF);
				WORD wDeltaHigh16 = (WORD)(((DWORD)dwDelta >> 16) & 0xFFFF);
				WORD wNewHigh16 = (WORD)(wOldHigh16 + wDeltaHigh16);
				*pdwAddress = ((DWORD)wNewHigh16 << 16) | (DWORD)wOldLow16;
#endif
				break;
			}
			case IMAGE_REL_BASED_LOW:
			{
#ifndef _WIN64
				DWORD* pdwAddress = (DWORD*)((PBYTE)m_VirtualBase + pBaseRelocation->VirtualAddress + wOffset);
				DWORD dwOldAddr = *pdwAddress;
				WORD wOldHigh16 = (WORD)((dwOldAddr >> 16) & 0xFFFF);
				WORD wOldLow16  = (WORD)(dwOldAddr & 0xFFFF);
				WORD wDeltaLow16 = (WORD)(dwDelta & 0xFFFF);
				WORD wNewLow16 = (WORD)(wOldLow16 + wDeltaLow16);
				*pdwAddress = ((DWORD)wOldHigh16 << 16) | (DWORD)wNewLow16;
#endif
				break;
			}
			case IMAGE_REL_BASED_ABSOLUTE:
				break;
			case IMAGE_REL_BASED_HIGHLOW:
			{
#ifndef _WIN64
				DWORD* pdwAddress = (DWORD*)((PBYTE)m_VirtualBase + pBaseRelocation->VirtualAddress + wOffset);
				*pdwAddress += dwDelta;
#endif
				break;
			}
			case IMAGE_REL_BASED_DIR64:
			{
#ifdef _WIN64
				auto pAddress = (ULONGLONG*)((PBYTE)m_VirtualBase + pBaseRelocation->VirtualAddress + wOffset);
				*pAddress += ullDelta;
#endif
				break;
			}
			default:
				DebugPrint(LEVEL_WARN,"Unknown relocation type: 0x%X", wRelocationType);
				break;
			}
		}
		pBaseRelocation = (PIMAGE_BASE_RELOCATION)((PBYTE)pBaseRelocation + pBaseRelocation->SizeOfBlock);
	}
	DebugPrint(LEVEL_INFO, "RtlMemory::CopyDataSections():Relocation table fix successsfully");
	return true;
}


static bool ResolveThunkArray(
	__in HMODULE hModule,
	__in ULONG_PTR* pThunk,
	__in ULONG_PTR* pIAT,
	__in ULONG_PTR ulOrdinalFlag,
	__in ULONG_PTR ulVirtualBase)
{
	int nGuard = 0;
	while (nGuard < 100000 && *pThunk) {
		nGuard++;
		FARPROC pfnFuncAddr = nullptr;

		if (*pThunk & ulOrdinalFlag) {
			DWORD dwOrdinal = static_cast<DWORD>(*pThunk & 0xFFFF);
			pfnFuncAddr = RtlGetProcAddress(hModule,
				reinterpret_cast<LPCSTR>(static_cast<ULONG_PTR>(dwOrdinal)));
		} else {
			PIMAGE_IMPORT_BY_NAME pByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
				ulVirtualBase + *pThunk);
			pfnFuncAddr = RtlGetProcAddress(hModule, pByName->Name);
		}

		*pIAT = pfnFuncAddr ? reinterpret_cast<ULONG_PTR>(pfnFuncAddr) : 0;

		pThunk++;
		pIAT++;
	}
	return true;
}

bool RtlMemory::FixImportTable() {
	if (!m_VirtualBase) {
		DebugPrint(LEVEL_ERROR, "Invalid VirtualBase");
		return false;
	}
	if (!IsNtHeaderValid()) {
		DebugPrint(LEVEL_ERROR, "Invalid NtHeader");
		return false;
	}

	PIMAGE_DATA_DIRECTORY pImportDir = GetHeader(IMAGE_DIRECTORY_ENTRY_IMPORT);
	if (!pImportDir || pImportDir->VirtualAddress == 0 || pImportDir->Size == 0) {
		return true;
	}


	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
		reinterpret_cast<uintptr_t>(m_VirtualBase) + pImportDir->VirtualAddress);
	if(!pImportDesc) {
		return false;
	}

#ifdef _WIN64
	const ULONG_PTR ulOrdinalFlag = IMAGE_ORDINAL_FLAG64;
#else
	const ULONG_PTR ulOrdinalFlag = IMAGE_ORDINAL_FLAG32;
#endif
	const ULONG_PTR ulVirtualBase = reinterpret_cast<uintptr_t>(m_VirtualBase);

	int nMaxLoopCount = 0;
	while (nMaxLoopCount < 10000 && pImportDesc->Name != 0) {
		nMaxLoopCount++;
		const char* pszDllName = reinterpret_cast<const char*>(
			ulVirtualBase + pImportDesc->Name);
		if (!pszDllName || strlen(pszDllName) == 0) {
			pImportDesc++;
			continue;
		}
		RtlUnlockLoaderLock();
		HMODULE hModule = RtlLoadLibraryA(pszDllName);
		RtlLockLoaderLock();
		if (!hModule) {
			pImportDesc++;
			continue;
		}
		ULONG_PTR* pThunk = reinterpret_cast<ULONG_PTR*>(
			ulVirtualBase + pImportDesc->OriginalFirstThunk);
		ULONG_PTR* pIAT = reinterpret_cast<ULONG_PTR*>(
			ulVirtualBase + pImportDesc->FirstThunk);

		ResolveThunkArray(hModule, pThunk, pIAT, ulOrdinalFlag, ulVirtualBase);
		pImportDesc++;
	}
	return true;
}

bool RtlMemory::FixDelayImportTable()
{
	if (!m_VirtualBase) {
		return false;
	}
	if (!IsNtHeaderValid()) {
		return false;
	}

	PIMAGE_DATA_DIRECTORY pDelayImportDir = GetHeader(IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
	if (!pDelayImportDir || pDelayImportDir->VirtualAddress == 0 || pDelayImportDir->Size == 0) {
		return true;
	}

	PImgDelayDescr pDelayDesc = reinterpret_cast<PImgDelayDescr>(
		reinterpret_cast<uintptr_t>(m_VirtualBase) + pDelayImportDir->VirtualAddress);

	if (!pDelayDesc) {
		return false;
	}

#ifdef _WIN64
	const ULONG_PTR ulOrdinalFlag = IMAGE_ORDINAL_FLAG64;
#else
	const ULONG_PTR ulOrdinalFlag = IMAGE_ORDINAL_FLAG32;
#endif
	const ULONG_PTR ulVirtualBase = reinterpret_cast<uintptr_t>(m_VirtualBase);

	int nLoopGuard = 0;
	while (nLoopGuard < 10000 && pDelayDesc->rvaDLLName != 0) {
		nLoopGuard++;

		const char* pszDllName = reinterpret_cast<const char*>(
			ulVirtualBase + static_cast<uintptr_t>(pDelayDesc->rvaDLLName));

		if (!pszDllName || strlen(pszDllName) == 0) {
			pDelayDesc++;
			continue;
		}

		HMODULE hModule = RtlLoadLibraryA(pszDllName);
		if (!hModule) {
			pDelayDesc++;
			continue;
		}

		ULONG_PTR* pIAT = reinterpret_cast<ULONG_PTR*>(
			ulVirtualBase + static_cast<uintptr_t>(pDelayDesc->rvaIAT));
		ULONG_PTR* pINT = reinterpret_cast<ULONG_PTR*>(
			ulVirtualBase + static_cast<uintptr_t>(pDelayDesc->rvaINT));

		if (!pIAT || !pINT) {
			pDelayDesc++;
			continue;
		}

		ResolveThunkArray(hModule, pINT, pIAT, ulOrdinalFlag, ulVirtualBase);
		pDelayDesc++;
	}

	return true;
}

bool RtlMemory::Flush()
{
	if (!m_VirtualBase) {
		DebugPrint(LEVEL_WARN, "Flush::Invalid address");
		return false;
	}
	if (!FlushInstructionCache(GetCurrentProcess(), m_VirtualBase, GetVirtualSize())) {
		DebugPrint(LEVEL_WARN, "FlushInstructionCache failed!");
	}
	return true;
}

bool RtlMemory::ExecuteMain()
{
	typedef BOOL(WINAPI* DllMainProc)(HINSTANCE, DWORD, LPVOID);

	DllMainProc pfnDllMain = (DllMainProc)GetEntryPoint();
	if (!pfnDllMain || !IsExecutableImage()) {
		DebugPrint(LEVEL_ERROR, "Invalid entry point or non-executable image");
		return false;
	}

	if (!(m_pNtHeader->FileHeader.Characteristics & IMAGE_FILE_DLL)) {
		DebugPrint(LEVEL_WARN, "Not a DLL file, skipping DllMain call");
		return true;
	}
	if (m_VirtualBase) {
		DisableThreadLibraryCalls(static_cast<HINSTANCE>(m_VirtualBase));
	}
	DebugPrint(LEVEL_INFO, "Calling DllMain(DLL_PROCESS_ATTACH) at 0x%p", pfnDllMain);
	if (!pfnDllMain(static_cast<HINSTANCE>(m_VirtualBase), DLL_PROCESS_ATTACH, nullptr)) {
		DebugPrint(LEVEL_ERROR, "DllMain(ATTACH) returned FALSE, cleaning up...");
		pfnDllMain(static_cast<HINSTANCE>(m_VirtualBase), DLL_PROCESS_DETACH, nullptr);
		return false;
	}

	DebugPrint(LEVEL_INFO, "DllMain(ATTACH) succeeded");
	return true;
}

bool RtlMemory::FixTLSTable()
{
	if (!CheckHeader(IMAGE_DIRECTORY_ENTRY_TLS)) {
		DebugPrint(LEVEL_INFO,"RtlMemory::ExecuteMain():FixTLSTable:TLS directionary is empty");
		return true;
	}

	DebugPrint(LEVEL_INFO, "RtlMemory::ExecuteMain():FixTLSTable:Start to fix tls table");

	PIMAGE_DATA_DIRECTORY pTLSDirectory = (PIMAGE_DATA_DIRECTORY) & (m_pNtHeader)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
	DebugPrint(LEVEL_INFO, "RtlMemory::ExecuteMain():FixTLSTable:VirtualAddress=0x%08X, Size=0x%08X,VirualBase=0x%08X",
		pTLSDirectory->VirtualAddress, pTLSDirectory->Size,m_VirtualBase);

	if (pTLSDirectory->VirtualAddress == 0) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::ExecuteMain():FixTLSTable:TLS table address0 [        ]");
		return false;
	}

	PIMAGE_TLS_DIRECTORY pTLSDir = (PIMAGE_TLS_DIRECTORY)((PBYTE)m_VirtualBase + pTLSDirectory->VirtualAddress);
	DebugPrint(LEVEL_INFO, "RtlMemory::ExecuteMain():TLS info:!");
	DebugPrint(LEVEL_INFO, "RtlMemory::ExecuteMain():StartAddressOfRawData: 0x%p!", (void*)pTLSDir->StartAddressOfRawData);
	DebugPrint(LEVEL_INFO, "RtlMemory::ExecuteMain():EndAddressOfRawData: 0x%p!", (void*)pTLSDir->EndAddressOfRawData);
	DebugPrint(LEVEL_INFO, "AddressOfIndex: 0x%p!", (void*)pTLSDir->AddressOfIndex);
	DebugPrint(LEVEL_INFO, "AddressOfCallBacks: 0x%p!", (void*)pTLSDir->AddressOfCallBacks);
	DebugPrint(LEVEL_INFO, "SizeOfZeroFill: 0x%08X!", pTLSDir->SizeOfZeroFill);
	DebugPrint(LEVEL_INFO, "Characteristics: 0x%08X!", pTLSDir->Characteristics);
	PIMAGE_TLS_CALLBACK* pCallBack = (PIMAGE_TLS_CALLBACK*)pTLSDir->AddressOfCallBacks;
	if (pCallBack) {
		DebugPrint(LEVEL_INFO,"RtlMemory::ExecuteMain():FixTLSTable:Start to fix relocation table");
		int nCallbackCount = 0;

		while (*pCallBack) {
			DebugPrint(LEVEL_INFO,"RtlMemory::ExecuteMain():FixTLSTable:Start to running tls function by%d, address: 0x%p", nCallbackCount + 1, *pCallBack);
			__try {
				(*pCallBack)(m_VirtualBase, DLL_PROCESS_ATTACH, nullptr);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				DebugPrint(LEVEL_ERROR, "tls callback execute failed");
				pCallBack++;
				nCallbackCount++;
				continue;
			}
			
			DebugPrint(LEVEL_INFO,"RtlMemory::ExecuteMain():FixTLSTable:callbak function Ok");

			pCallBack++;
			nCallbackCount++;
		}

		DebugPrint(LEVEL_INFO,"RtlMemory::ExecuteMain():FixTLSTable:total:%d", nCallbackCount);
	}
	else {
		DebugPrint(LEVEL_ERROR, "RtlMemory::ExecuteMain():FixTLSTable:Tls callback function is not found");
	}

	DebugPrint(LEVEL_INFO, "RtlMemory::ExecuteMain():TLS table fix successfullly");
	return true;
}

HMODULE RtlMemory::GetModuleInstance() {
	return (HMODULE)m_VirtualBase;
}




void RtlMemory::FixExportTable(__in PIMAGE_DOS_HEADER pImageDosHeaderMap,
	__in PIMAGE_NT_HEADERS pImageNtHeaderMap)
{
	if (pImageNtHeaderMap->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0) {
		DebugPrint(LEVEL_INFO, "RtlMemory::FixExportTable: Export directory is empty");
		return;
	}

	DWORD dwExportRVA = pImageNtHeaderMap->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	PIMAGE_EXPORT_DIRECTORY pExportDir = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(
		reinterpret_cast<BYTE*>(pImageDosHeaderMap) + dwExportRVA);

	if (!pExportDir) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::FixExportTable: Failed to get export directory");
		return;
	}

	ULONGLONG ullDelta = (DWORD_PTR)m_VirtualBase - pImageNtHeaderMap->OptionalHeader.ImageBase;
	DWORD dwExportStart = dwExportRVA;
	DWORD dwExportEnd = dwExportStart + pImageNtHeaderMap->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	DWORD dwOldProtect = 0;
	SIZE_T nSize = pImageNtHeaderMap->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	if (!VirtualProtect(pExportDir, nSize, PAGE_READWRITE, &dwOldProtect)) {
		DebugPrint(LEVEL_ERROR, "RtlMemory::FixExportTable: Failed to change memory protection");
		return;
	}
	DWORD* pdwFunctions = reinterpret_cast<DWORD*>(
		reinterpret_cast<BYTE*>(pImageDosHeaderMap) + pExportDir->AddressOfFunctions);

	for (DWORD i = 0; i < pExportDir->NumberOfFunctions; ++i)
	{
		DWORD dwRva = pdwFunctions[i];
		if (dwRva == 0)
			continue;

		if (dwRva >= dwExportStart && dwRva < dwExportEnd)
		{
			char* pForwardStr = reinterpret_cast<char*>(
				reinterpret_cast<BYTE*>(pImageDosHeaderMap) + dwRva);
			UNREFERENCED_PARAMETER(pForwardStr);

			DebugPrint(LEVEL_INFO, "RtlMemory::FixExportTable: Forwarder detected: %s", pForwardStr);
		}
		else
		{
#ifdef _WIN64
			pdwFunctions[i] = static_cast<DWORD>(dwRva + ullDelta);
#else
			pdwFunctions[i] = dwRva + static_cast<DWORD>(ullDelta);
#endif
		}
	}

	if (pExportDir->AddressOfNames != 0)
	{
		DWORD* pdwNames = reinterpret_cast<DWORD*>(
			reinterpret_cast<BYTE*>(pImageDosHeaderMap) + pExportDir->AddressOfNames);

		for (DWORD i = 0; i < pExportDir->NumberOfNames; ++i)
		{
			if (pdwNames[i] != 0)
				pdwNames[i] = static_cast<DWORD>(pdwNames[i] + ullDelta);
		}
	}

	if (pExportDir->Name != 0)
		pExportDir->Name = static_cast<DWORD>(pExportDir->Name + ullDelta);

	pExportDir->AddressOfFunctions = static_cast<DWORD>(pExportDir->AddressOfFunctions + ullDelta);

	if (pExportDir->AddressOfNames != 0)
		pExportDir->AddressOfNames = static_cast<DWORD>(pExportDir->AddressOfNames + ullDelta);

	if (pExportDir->AddressOfNameOrdinals != 0)
		pExportDir->AddressOfNameOrdinals = static_cast<DWORD>(pExportDir->AddressOfNameOrdinals + ullDelta);

	VirtualProtect(pExportDir, nSize, dwOldProtect, &dwOldProtect);

	DebugPrint(LEVEL_INFO, "RtlMemory::FixExportTable: Export table fixed successfully");
}
