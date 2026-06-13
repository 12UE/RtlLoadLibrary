#include "pch.h"
#include "RtlTest.h"
#include "RtlLoadLibrary.h"
#include "RtlGetModuleHandle.h"
#include "RtlGetProcAddress.h"
#include "LongestMatch.h"
#include "RtlMemory.h"

// ===================================================================
//  StringUtils Tests
// ===================================================================

TEST_CASE(WideToNarrow_basic)
{
    std::string result = WideToNarrow(L"Hello");
    ASSERT_STREQ(result.c_str(), "Hello");
}

TEST_CASE(NarrowToWide_basic)
{
    std::wstring result = NarrowToWide("World");
    ASSERT_WSTREQ(result.c_str(), L"World");
}

TEST_CASE(StringRoundtrip)
{
    std::string orig = "Test 123 !@#";
    std::wstring wide = NarrowToWide(orig);
    std::string back = WideToNarrow(wide);
    ASSERT_STREQ(back.c_str(), orig.c_str());
}

// ===================================================================
//  IsApiSetName Tests
// ===================================================================

TEST_CASE(IsApiSetName_valid)
{
    ASSERT_TRUE(IsApiSetName(L"api-ms-win-core-file-l1-1-0.dll"));
    ASSERT_TRUE(IsApiSetName(L"ext-ms-win-ntuser-window-l1-1-0.dll"));
}

TEST_CASE(IsApiSetName_invalid)
{
    ASSERT_FALSE(IsApiSetName(L"kernel32.dll"));
    ASSERT_FALSE(IsApiSetName(L""));
}

// ===================================================================
//  GetFileExtension Tests
// ===================================================================

TEST_CASE(GetFileExtension_basic)
{
    ASSERT_EQ(GetFileExtension(L"test.dll"), L"dll");
    ASSERT_EQ(GetFileExtension(L"noext"), L"");
}

TEST_CASE(GetFileExtension_multi_dot)
{
    ASSERT_EQ(GetFileExtension(L"lib.test.dll"), L"dll");
}

// ===================================================================
//  GetFileNameAndPath Tests
// ===================================================================

TEST_CASE(GetFileNameAndPath_basic)
{
    auto [name, path] = GetFileNameAndPath(L"C:\\Windows\\System32\\ntdll.dll");
    ASSERT_EQ(name, L"ntdll.dll");
    ASSERT_EQ(path, L"C:\\Windows\\System32");
}

TEST_CASE(GetFileNameAndPath_no_path)
{
    auto [name, path] = GetFileNameAndPath(L"test.dll");
    ASSERT_EQ(name, L"test.dll");
    ASSERT_EQ(path, L"");
}

// ===================================================================
//  FileExists Tests
// ===================================================================

TEST_CASE(FileExists_nullptr)
{
    ASSERT_FALSE(FileExists(nullptr));
}

TEST_CASE(FileExists_known_system_dll)
{
    ASSERT_TRUE(FileExists(L"C:\\Windows\\System32\\ntdll.dll"));
}

TEST_CASE(FileExists_bad_path)
{
    ASSERT_FALSE(FileExists(L"C:\\nonexistent\\path\\file.xyz"));
}

// ===================================================================
//  CheckMask Tests
// ===================================================================

TEST_CASE(CheckMask_basic)
{
    ASSERT_TRUE(CheckMask(PAGE_READWRITE, PAGE_READWRITE));
    ASSERT_FALSE(CheckMask(PAGE_READONLY, PAGE_EXECUTE_READWRITE));
}

TEST_CASE(CheckMask_zero_mask)
{
    ASSERT_FALSE(CheckMask(PAGE_READONLY, 0));
}

// ===================================================================
//  LongestMatch Tests
// ===================================================================

TEST_CASE(LongestMatch_exact)
{
    std::vector<std::string> vec = { "hello", "world", "test" };
    float sim = 0;
    int idx = LongestMatch(vec, std::string("hello"), sim);
    ASSERT_EQ(idx, 0);
    ASSERT_EQ(sim, 1.0f);
}

TEST_CASE(LongestMatch_partial)
{
    std::vector<std::string> vec = { "kernel32.dll", "user32.dll", "ntdll.dll" };
    float sim = 0;
    int idx = LongestMatch(vec, std::string("kernel"), sim);
    UNREFERENCED_PARAMETER(idx);
    ASSERT_GT(sim, 0.0f);
}

TEST_CASE(NormalizeEqualStr_basic)
{
    ASSERT_TRUE(NormalizeEqualStr(
        std::string("HelloWorld"),
        std::string("helloworld")));
}

// ===================================================================
//  RtlGetModuleHandle Tests
// ===================================================================

TEST_CASE(RtlGetModuleHandleA_kernel32)
{
    HMODULE hMod = RtlGetModuleHandleA("kernel32.dll");
    ASSERT_NOT_NULL(hMod);
}

TEST_CASE(RtlGetModuleHandleA_bad)
{
    HMODULE hMod = RtlGetModuleHandleA("nonexistent_xyz.dll");
    ASSERT_TRUE(hMod == nullptr);
}

// ===================================================================
//  RtlGetProcAddress Tests
// ===================================================================

TEST_CASE(RtlGetProcAddress_kernel32)
{
    HMODULE hKernel32 = RtlGetModuleHandleA("kernel32.dll");
    ASSERT_NOT_NULL(hKernel32);

    PVOID pfn = RtlGetProcAddress(hKernel32, "GetModuleHandleW");
    ASSERT_NOT_NULL(pfn);
}

TEST_CASE(RtlGetProcAddress_nonexistent)
{
    HMODULE hKernel32 = RtlGetModuleHandleA("kernel32.dll");
    ASSERT_NOT_NULL(hKernel32);

    PVOID pfn = RtlGetProcAddress(hKernel32, "NonExistentFunctionXYZ123");
    ASSERT_TRUE(pfn == nullptr);
}

// ===================================================================
//  GetSafeDirectories Tests
// ===================================================================

TEST_CASE(GetSafeDirectories_not_empty)
{
    auto dirs = GetSafeDirectories();
    ASSERT_GT(dirs.size(), (size_t)0);
}

// ===================================================================
//  GetContinuousReadableMemorySize Tests
// ===================================================================

TEST_CASE(GetContinuousReadableMemorySize_min)
{
    SIZE_T sz = GetContinuousReadableMemorySize(nullptr);
    ASSERT_EQ(sz, (SIZE_T)0);
}

// ===================================================================
//  RtlLoadLibraryA Tests
// ===================================================================

#ifdef _M_IX86
TEST_CASE(RtlLoadLibraryA_api_x_module)
{
    HMODULE hMod = RtlLoadLibraryA("api-x-module.dll");
    ASSERT_NOT_NULL(hMod);
}

TEST_CASE(RtlLoadLibraryA_Dll32EXCEPTION)
{
    HMODULE hMod = RtlLoadLibraryA("Dll32EXCEPTION.dll");
    ASSERT_NOT_NULL(hMod);
}
#endif



TEST_CASE(RtlLoadLibraryA_nonexistent)
{
    HMODULE hMod = RtlLoadLibraryA("nonexistent_library_xyz.dll");
    ASSERT_TRUE(hMod == nullptr);
}




// ===================================================================
//  RtlMemory::GetAlignedSize Tests
// ===================================================================

TEST_CASE(RtlMemory_GetAlignedSize_zero)
{
	RtlMemory mem;
	ASSERT_EQ(mem.GetAlignedSize(0, 0x1000), (SIZE_T)0);
	ASSERT_EQ(mem.GetAlignedSize(0, 8), (SIZE_T)0);
}

TEST_CASE(RtlMemory_GetAlignedSize_already_aligned)
{
	RtlMemory mem;
	ASSERT_EQ(mem.GetAlignedSize(0x1000, 0x1000), (SIZE_T)0x1000);
	ASSERT_EQ(mem.GetAlignedSize(0x8000, 0x1000), (SIZE_T)0x8000);
	ASSERT_EQ(mem.GetAlignedSize(16, 4), (SIZE_T)16);
}

TEST_CASE(RtlMemory_GetAlignedSize_needs_alignment)
{
	RtlMemory mem;
	ASSERT_EQ(mem.GetAlignedSize(1, 0x1000), (SIZE_T)0x1000);
	ASSERT_EQ(mem.GetAlignedSize(0x1001, 0x1000), (SIZE_T)0x2000);
	ASSERT_EQ(mem.GetAlignedSize(0xFFF, 0x1000), (SIZE_T)0x1000);
	ASSERT_EQ(mem.GetAlignedSize(5, 4), (SIZE_T)8);
	ASSERT_EQ(mem.GetAlignedSize(10, 8), (SIZE_T)16);
	ASSERT_EQ(mem.GetAlignedSize(31, 32), (SIZE_T)32);
}

TEST_CASE(RtlMemory_GetAlignedSize_invalid_alignment)
{
	RtlMemory mem;
	ASSERT_EQ(mem.GetAlignedSize(100, 0), (SIZE_T)0);
	ASSERT_EQ(mem.GetAlignedSize(100, 3), (SIZE_T)0);
	ASSERT_EQ(mem.GetAlignedSize(256, 7), (SIZE_T)0);
}

// ===================================================================
//  RtlMemory::ConvertSectionProtect Tests
// ===================================================================

TEST_CASE(RtlMemory_ConvertSectionProtect_execute_read)
{
	RtlMemory mem;
	ASSERT_EQ(mem.ConvertSectionProtect(IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ),
		PAGE_EXECUTE_READ);
}

TEST_CASE(RtlMemory_ConvertSectionProtect_execute_readwrite)
{
	RtlMemory mem;
	ASSERT_EQ(mem.ConvertSectionProtect(
		IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE),
		PAGE_EXECUTE_READWRITE);
}

TEST_CASE(RtlMemory_ConvertSectionProtect_readonly)
{
	RtlMemory mem;
	ASSERT_EQ(mem.ConvertSectionProtect(IMAGE_SCN_MEM_READ), PAGE_READONLY);
}

TEST_CASE(RtlMemory_ConvertSectionProtect_readwrite)
{
	RtlMemory mem;
	ASSERT_EQ(mem.ConvertSectionProtect(IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE),
		PAGE_READWRITE);
}

TEST_CASE(RtlMemory_ConvertSectionProtect_write_only_falls_back)
{
	RtlMemory mem;
	ASSERT_EQ(mem.ConvertSectionProtect(IMAGE_SCN_MEM_WRITE), PAGE_READWRITE);
}

TEST_CASE(RtlMemory_ConvertSectionProtect_no_access)
{
	RtlMemory mem;
	ASSERT_EQ(mem.ConvertSectionProtect(0), PAGE_NOACCESS);
}

TEST_CASE(RtlMemory_ConvertSectionProtect_execute_only_adds_read)
{
	RtlMemory mem;
	ASSERT_EQ(mem.ConvertSectionProtect(IMAGE_SCN_MEM_EXECUTE), PAGE_EXECUTE_READ);
}

TEST_CASE(RtlMemory_ConvertSectionProtect_nocache_flag)
{
	RtlMemory mem;
	DWORD dwProtect = mem.ConvertSectionProtect(
		IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_NOT_CACHED);
	ASSERT_EQ(dwProtect, PAGE_READONLY | PAGE_NOCACHE);
}

// ===================================================================
//  RtlMemory::RVAToFOA Tests
// ===================================================================

TEST_CASE(RtlMemory_RVAToFOA_within_first_section)
{
	RtlMemory mem;
	struct FakePE32 {
		IMAGE_NT_HEADERS32 nt;
		IMAGE_SECTION_HEADER sections[2];
	} fake = {};
	fake.nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	fake.nt.FileHeader.NumberOfSections = 2;
	fake.sections[0].VirtualAddress = 0x1000;
	fake.sections[0].SizeOfRawData = 0x2000;
	fake.sections[0].PointerToRawData = 0x400;
	fake.sections[1].VirtualAddress = 0x3000;
	fake.sections[1].SizeOfRawData = 0x1000;
	fake.sections[1].PointerToRawData = 0x2400;

	INT foa = mem.RVAToFOA(reinterpret_cast<PIMAGE_NT_HEADERS>(&fake.nt), 0x1500);
	ASSERT_EQ(foa, (INT)(0x400 + 0x1500 - 0x1000));
}

TEST_CASE(RtlMemory_RVAToFOA_second_section)
{
	RtlMemory mem;
	struct FakePE32 {
		IMAGE_NT_HEADERS32 nt;
		IMAGE_SECTION_HEADER sections[2];
	} fake = {};
	fake.nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	fake.nt.FileHeader.NumberOfSections = 2;
	fake.sections[0].VirtualAddress = 0x1000;
	fake.sections[0].SizeOfRawData = 0x2000;
	fake.sections[0].PointerToRawData = 0x400;
	fake.sections[1].VirtualAddress = 0x3000;
	fake.sections[1].SizeOfRawData = 0x1000;
	fake.sections[1].PointerToRawData = 0x2400;

	INT foa = mem.RVAToFOA(reinterpret_cast<PIMAGE_NT_HEADERS>(&fake.nt), 0x3500);
	ASSERT_EQ(foa, (INT)(0x2400 + 0x3500 - 0x3000));
}

TEST_CASE(RtlMemory_RVAToFOA_section_boundary)
{
	RtlMemory mem;
	struct FakePE32 {
		IMAGE_NT_HEADERS32 nt;
		IMAGE_SECTION_HEADER sections[1];
	} fake = {};
	fake.nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	fake.nt.FileHeader.NumberOfSections = 1;
	fake.sections[0].VirtualAddress = 0x1000;
	fake.sections[0].SizeOfRawData = 0x1000;
	fake.sections[0].PointerToRawData = 0x400;

	ASSERT_EQ(mem.RVAToFOA(reinterpret_cast<PIMAGE_NT_HEADERS>(&fake.nt), 0x1000), (INT)0x400);
	ASSERT_EQ(mem.RVAToFOA(reinterpret_cast<PIMAGE_NT_HEADERS>(&fake.nt), 0x1000 + 0xFFF),
		(INT)(0x400 + 0xFFF));
}

TEST_CASE(RtlMemory_RVAToFOA_outside_all_sections)
{
	RtlMemory mem;
	struct FakePE32 {
		IMAGE_NT_HEADERS32 nt;
		IMAGE_SECTION_HEADER sections[1];
	} fake = {};
	fake.nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	fake.nt.FileHeader.NumberOfSections = 1;
	fake.sections[0].VirtualAddress = 0x1000;
	fake.sections[0].SizeOfRawData = 0x1000;
	fake.sections[0].PointerToRawData = 0x400;

	ASSERT_EQ(mem.RVAToFOA(reinterpret_cast<PIMAGE_NT_HEADERS>(&fake.nt), 0x5000), (INT)-1);
	ASSERT_EQ(mem.RVAToFOA(reinterpret_cast<PIMAGE_NT_HEADERS>(&fake.nt), 0), (INT)-1);
}

TEST_CASE(RtlMemory_RVAToFOA_empty_section_table)
{
	RtlMemory mem;
	IMAGE_NT_HEADERS32 nt = {};
	nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	nt.FileHeader.NumberOfSections = 0;

	ASSERT_EQ(mem.RVAToFOA(reinterpret_cast<PIMAGE_NT_HEADERS>(&nt), 0x1000), (INT)-1);
}

#ifdef _WIN64
TEST_CASE(RtlMemory_RVAToFOA_pe64)
{
	RtlMemory mem;
	struct FakePE64 {
		IMAGE_NT_HEADERS64 nt;
		IMAGE_SECTION_HEADER sections[1];
	} fake = {};
	fake.nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
	fake.nt.FileHeader.NumberOfSections = 1;
	fake.sections[0].VirtualAddress = 0x1000;
	fake.sections[0].SizeOfRawData = 0x2000;
	fake.sections[0].PointerToRawData = 0x400;

	INT foa = mem.RVAToFOA(reinterpret_cast<PIMAGE_NT_HEADERS>(&fake.nt), 0x1500);
	ASSERT_EQ(foa, (INT)(0x400 + 0x1500 - 0x1000));
}
#endif

// ===================================================================
//  RtlLoadLibraryInMemory Integration Tests
// ===================================================================

TEST_CASE(RtlLoadLibraryInMemory_kernel32)
{
	WCHAR wszAryPath[MAX_PATH] = {};
	DWORD dwLen = SearchPathW(nullptr, L"kernel32.dll", nullptr,
		MAX_PATH, wszAryPath, nullptr);
	ASSERT_GT(dwLen, (DWORD)0);

	THANDLE hFile = CreateFileW(wszAryPath, GENERIC_READ, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	ASSERT_TRUE(hFile);

	LARGE_INTEGER tagFileSize = {};
	ASSERT_TRUE(GetFileSizeEx(static_cast<HANDLE>(hFile), &tagFileSize));

	std::vector<BYTE> vecBuffer(static_cast<size_t>(tagFileSize.QuadPart));
	DWORD dwRead = 0;
	ASSERT_TRUE(ReadFile(static_cast<HANDLE>(hFile), vecBuffer.data(),
		static_cast<DWORD>(vecBuffer.size()), &dwRead, nullptr));
	ASSERT_EQ(dwRead, (DWORD)vecBuffer.size());

	HMODULE hMod = RtlLoadLibraryInMemory(vecBuffer.data(),
		static_cast<DWORD>(vecBuffer.size()));
	ASSERT_NOT_NULL(hMod);
}

TEST_CASE(RtlLoadLibraryInMemory_null_bytes)
{
	HMODULE hMod = RtlLoadLibraryInMemory(nullptr, 0);
	ASSERT_TRUE(hMod == nullptr);
}

TEST_CASE(RtlLoadLibraryInMemory_bad_dos_signature)
{
	BYTE badPE[1024] = {};
	memset(badPE, 0xCC, sizeof(badPE));
	HMODULE hMod = RtlLoadLibraryInMemory(badPE, sizeof(badPE));
	ASSERT_TRUE(hMod == nullptr);
}

// ===================================================================
//  RtlMemory End-to-End Load + Introspection Tests
// ===================================================================

TEST_CASE(RtlMemory_load_and_get_entry_point)
{
	HMODULE hMod = RtlLoadLibraryW(L"kernel32.dll");
	ASSERT_NOT_NULL(hMod);

	PVOID pEntry = RtlGetEntryPoint(hMod);
	ASSERT_NOT_NULL(pEntry);
}

TEST_CASE(RtlMemory_load_and_get_section_data)
{
	HMODULE hMod = RtlLoadLibraryW(L"kernel32.dll");
	ASSERT_NOT_NULL(hMod);

	PVOID pText = RtlGetSectionDataA(hMod, ".text");
	ASSERT_NOT_NULL(pText);

	PVOID pBadSection = RtlGetSectionDataA(hMod, ".nonexistent_section_xyz");
	ASSERT_TRUE(pBadSection == nullptr);
}

TEST_CASE(RtlMemory_load_and_get_proc_address)
{
	HMODULE hMod = RtlLoadLibraryW(L"kernel32.dll");
	ASSERT_NOT_NULL(hMod);

	PVOID pGetProc = RtlGetProcAddress(hMod, "GetProcAddress");
	ASSERT_NOT_NULL(pGetProc);

	PVOID pLoadLib = RtlGetProcAddress(hMod, "LoadLibraryW");
	ASSERT_NOT_NULL(pLoadLib);
}

