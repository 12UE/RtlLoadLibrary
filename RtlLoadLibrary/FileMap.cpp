#include "pch.h"
#include "FileMap.h"
#define STATUS_IMAGE_NOT_AT_BASE 0x40000003
FileMap::FileMap(__in HANDLE hFile, __in DWORD _Protect, __in DWORD dwDesiredAccess, __in bool Raii) {
	Initialize(hFile, _Protect, dwDesiredAccess, Raii);
}

FileMap::FileMap()
{
	m_hFileMap = nullptr;
	m_lpFileBase = nullptr;
	RAII = true;
}

 FileMap& FileMap::Initialize(__in HANDLE hFile, __in DWORD dwProtect, __in DWORD dwDesiredAccess, __in bool Raii) {
	if (!NormalHandle::IsValid(hFile)) return *this;
	m_hFileMap = CreateFileMappingW(hFile, nullptr, dwProtect, 0, 0, nullptr);

	if (!m_hFileMap) {
		DebugBreak();
	}
	m_lpFileBase = MapViewOfFile(static_cast<HANDLE>(m_hFileMap), dwDesiredAccess, 0, 0, 0);

	if (!m_lpFileBase) {
		DebugBreak();
	}

	RAII = Raii;
	return *this;
}

 LONGLONG FileMap::GetFileSize() 
 {
	 if (!m_hFile) {
		 DebugPrint(LEVEL_ERROR, "FileMap::GetFileSize: Invalid file handle");
		 return 0;
	 }
	 LARGE_INTEGER tagFileSize{};
	if (!GetFileSizeEx(static_cast<HANDLE>(m_hFile), &tagFileSize)) {
		 return 0;
	 }
	 return tagFileSize.QuadPart;
 }

FileMap::operator bool() noexcept {
	return (bool)m_lpFileBase;
}

LPVOID FileMap::GetMap() noexcept {
	return m_lpFileBase;
}

FileMap::operator LPVOID() noexcept {
	return m_lpFileBase;
}

void FileMap::NotClose() {
	RAII = false;
}

void FileMap::SholdClose() {
	RAII = true;
}

FileMap::~FileMap() {
	if (m_lpFileBase && RAII) {
		UnmapViewOfFile(m_lpFileBase);
	}
}
