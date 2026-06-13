#ifndef RtlMemory_H
#define RtlMemory_H
#pragma once
#include"pch.h"
#include "KernelStruct.h"

// Disable 4733 warning for fs:[0] usage
#pragma warning(disable: 4733)
class RtlMemory {
	LPVOID                m_pFileImage;
	DWORD                 m_dwFileSize;
	PIMAGE_DOS_HEADER     m_pDosHeader;
	PIMAGE_NT_HEADERS     m_pNtHeader;
	PIMAGE_SECTION_HEADER m_pSectionHeader;
	LPVOID                m_VirtualBase;
	bool                  m_bHooked = false;
	_THANDLE              m_hFile;
	DWORD_PTR             m_DefaultImageBase;
	std::wstring GenerateRandomFileName();
	bool CheckHeader(__in DWORD dwIndex);
	PIMAGE_DATA_DIRECTORY GetHeader(__in DWORD dwIndex);
	bool IsExecutableImage();
public:
	SIZE_T GetAlignedSize(__in SIZE_T OriginalData, __in SIZE_T Alignment);
	DWORD ConvertSectionProtect(__in DWORD dwCharacteristics);
	std::wstring          m_wstrFileName;
	std::wstring          m_wstrFullPath;
	PVOID  RunImage(__in PVOID lpBytes, __in DWORD dwSize, __in DWORD dwFlags);
	bool  RunFile(__in DWORD dwFlags);
	RtlMemory() = default;
	RtlMemory(__in HANDLE _hFile, __in const std::wstring& _szFullPath);
	RtlMemory(__in DWORD dwSize);
	~RtlMemory() = default;
	SIZE_T GetVirtualSize();
	INT   RVAToFOA(__in PIMAGE_NT_HEADERS _pImageNtHeader, __in DWORD dwTargetRVA);
	bool ValidateSEHandlers(__in DWORD* pHandlerTable, __in DWORD dwCount);
	bool  IsDosHeaderValid();
	bool  IsNtHeaderValid();
	bool  IsOptionalHeaderSizeCorrect();
	void* ReserveVirtualMemory(__in DWORD_PTR _defaultImageBase, __in BOOL bASLR);
	void* CommitMemory(__in void* lpAddr, __in SIZE_T RegionSize);
	BOOL CopyDataSections(__inout std::vector<PE_SECTION_INFO>& vecSections);
	bool ProtectSections(__in const std::vector<PE_SECTION_INFO>& vecSections);
	WORD  GetPlatformMaxSectionsForPE();
	bool  FixRelocationTable();
	bool  FixImportTable();
	bool  FixDelayImportTable();
	bool  Flush();
	bool  ExecuteMain();
	bool  FixTLSTable();
	HMODULE GetModuleInstance();
	void  FixExportTable(__in PIMAGE_DOS_HEADER pImageDosHeaderMap, __in PIMAGE_NT_HEADERS pImageNtHeaderMap);
	bool FixCFGFunctionTable(__in PVOID pTable, __in DWORD dwCount);
	bool IsAddressInExecutableSection(__in PVOID pAddress);
	void FixExtendedLoadConfigFields(__inout PIMAGE_LOAD_CONFIG_DIRECTORY pLoadConfig);
	LPVOID GetEntryPoint();
	bool  FixExceptionTable();
	bool FixLoadConfigTable();
	bool InitializeSecurityCookie(__out PVOID pCookieAddr);
};
#endif // RtlMemory_H
