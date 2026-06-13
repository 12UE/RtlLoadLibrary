#pragma once 
#ifndef FILEMAPA1EEF1A5_C840_400B_A32D_649584D21980
#define FILEMAPA1EEF1A5_C840_400B_A32D_649584D21980
#include"pch.h"

class FileMap {
protected:
	_THANDLE    m_hFile;
    THANDLE     m_hFileMap;
    LPVOID      m_lpFileBase;
    bool RAII;
public:
    FileMap(__in HANDLE hFile, __in DWORD _Protect = PAGE_READONLY, __in DWORD dwDesiredAccess = FILE_MAP_READ, __in bool Raii = true);
    FileMap();
    FileMap& Initialize(__in HANDLE hFile, __in DWORD dwProtect = PAGE_READONLY, __in DWORD dwDesiredAccess = FILE_MAP_READ, __in bool Raii = true);
	LONGLONG GetFileSize();
    operator bool() noexcept;

    LPVOID GetMap() noexcept;


    operator LPVOID() noexcept;

    void NotClose();
	void SholdClose();
    ~FileMap();
};


#else
#endif//FILEMAPA1EEF1A5_C840_400B_A32D_649584D21980
