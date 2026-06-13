# RtlLoadLibrary

**[English](README.md) | [中文](README_CN.md) | [Español](README_ES.md)**

A Windows PE manual memory loader library that supports loading DLLs directly from memory without writing to disk, bypassing the standard `LoadLibrary` mechanism.

## Features

- **Memory Loading**: Load PE images from memory buffers without file I/O
- **API Set Resolution**: Automatically resolve `api-ms-win-*` / `ext-ms-win-*` virtual DLL names to real system DLLs
- **Complete PE Fix-up Pipeline**:
  - Import Address Table (IAT) fix-up
  - Delay-Load Import fix-up
  - Relocation fix-up
  - TLS callback execution
  - Export table fix-up
  - Exception handling table (SEH/CFG) fix-up
- **LdrLink Management**: Link manually loaded modules into PEB's module list
- **Loader Lock Wrapper**: `RtlLockLoaderLock` / `RtlUnlockLoaderLock` for concurrent loading protection
- **ANSI/Wide Dual Interface**: All core APIs provide both `A` and `W` versions
- **Async Loading**: Asynchronous DLL loading via `std::future`
- **RAII Resource Management**: `GenericHandle` / `FileMap` for automatic Windows kernel handle management
- **String Encryption**: Compile-time XOR encryption of sensitive strings to prevent static analysis

## Build

### Requirements

- Visual Studio 2019 or later
- Windows SDK 10.0+
- C++17 standard

### Compilation

1. Open `RtlLoadLibrary.sln`
2. Select target platform (x86 / x64) and configuration (Debug / Release)
3. Build the solution

Output: `RtlLoadLibrary.dll`

## API Reference

### Core Loading Functions

```cpp
// Wide-char version — load DLL by file name
HMODULE WINAPI RtlLoadLibraryW(LPCWSTR lpLibFileName);

// ANSI version
HMODULE WINAPI RtlLoadLibraryA(LPCSTR lpLibFileName);

// Extended version — supports file handle and flags
HMODULE WINAPI RtlLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
HMODULE WINAPI RtlLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

// Load from memory buffer
HMODULE WINAPI RtlLoadLibraryInMemory(PVOID lpBytes, DWORD dwSize);

// Load from memory buffer (extended, supports file name and full path)
HMODULE WINAPI RtlLoadLibraryInMemoryEx(
    PVOID  lpBytes,
    DWORD  dwSize,
    DWORD  dwFlags,
    LPWSTR lpFileName,
    LPWSTR lpFullName
);
```

### Module Query

```cpp
HMODULE WINAPI RtlGetModuleHandleA(LPCSTR  lpModuleName);
HMODULE WINAPI RtlGetModuleHandleW(LPCWSTR lpModuleName);
FARPROC WINAPI RtlGetProcAddress(HMODULE hModule, LPCSTR lpProcName);
```

### Section / Entry Point

```cpp
PVOID WINAPI RtlGetSectionDataA(HMODULE hMod, LPCSTR  lpSecName);
PVOID WINAPI RtlGetSectionDataW(HMODULE hMod, LPCWSTR lpSecName);
PVOID WINAPI RtlGetEntryPoint(HMODULE hMod);
```

### Loader Lock

```cpp
void WINAPI RtlLockLoaderLock();
void WINAPI RtlUnlockLoaderLock();
```

### LDR List Management

```cpp
bool WINAPI AddLdrLink(const MY_LDR_DATA_TABLE_ENTRY& Entry, BOOL bInitialize);
bool WINAPI RemoveLdrLink(const UNICODE_STRING* BaseName, const UNICODE_STRING* FullPath);
```

### Utility Functions

```cpp
// Async loading
std::future<HMODULE> AsyncRtlLoadLibraryA(LPCSTR lpLibFileName);

// API Set name detection
bool IsApiSetName(const std::wstring& name);

```

## Usage Examples

### Basic File Loading

```cpp
#include "RtlLoadLibrary.h"

// Load user32.dll (equivalent to LoadLibraryW)
HMODULE hUser32 = RtlLoadLibraryW(L"user32.dll");

// Get function address
auto pMessageBox = (int(WINAPI*)(HWND, LPCWSTR, LPCWSTR, UINT))
    RtlGetProcAddress(hUser32, "MessageBoxW");

pMessageBox(NULL, L"Hello", L"RtlLoadLibrary", MB_OK);
```

### Loading from Memory

```cpp
// Read DLL into memory
HANDLE hFile = CreateFileW(L"mylib.dll", GENERIC_READ, FILE_SHARE_READ,
    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

DWORD dwFileSize = GetFileSize(hFile, NULL);
std::vector<BYTE> buf(dwFileSize);
ReadFile(hFile, buf.data(), dwFileSize, NULL, NULL);
CloseHandle(hFile);

// Load from memory
HMODULE hMod = RtlLoadLibraryInMemory(buf.data(), dwFileSize);

// Get exported function
auto pFunc = (void(*)())RtlGetProcAddress(hMod, "MyExportedFunc");
pFunc();
```

### Async Loading

```cpp
auto future = AsyncRtlLoadLibraryA("kernel32.dll");
// ... do other work ...
HMODULE hMod = future.get();  // Block until loading completes
```

### Thread-Safe Loading

```cpp
RtlLockLoaderLock();
HMODULE hMod1 = RtlLoadLibraryInMemory(buf1, size1);
HMODULE hMod2 = RtlLoadLibraryInMemory(buf2, size2);
RtlUnlockLoaderLock();
```

## Architecture Overview

```
RtlLoadLibrary/
├── RtlLoadLibrary.h/cpp    # Exported API implementation entry
├── RtlMemory.h/cpp          # PE core loading engine (1453 lines)
├── ApisetQuery.h/cpp        # API Set Schema resolver (singleton)
├── RtlGetModuleHandle.h/cpp # Custom GetModuleHandle implementation
├── RtlGetProcAddress.h/cpp  # Custom GetProcAddress implementation
├── LdrLink.h/cpp            # PEB LDR linked list operations
├── GlobalObject.h/cpp       # Global state management (singleton)
├── KernelStruct.h           # NT internal structure definitions
├── Singleton.h              # Singleton template base class
├── GenericHandle.h          # RAII handle wrapper
├── FileMap.h/cpp            # File mapping RAII wrapper
├── XorStr.h                 # Compile-time string encryption
├── DebugPrint.h/cpp         # Logging system
├── StringUtils.h/cpp        # Wide/narrow character conversion
├── ThreadObject.h/cpp       # Thread operation wrapper
├── EnumModule.h/cpp         # Module enumeration
├── EnumThread.h/cpp         # Thread enumeration
├── ValidAddress.h/cpp       # Address validity verification
├── Exports.def              # Export symbol definitions
├── pch.h/cpp                # Precompiled header
└── main.cpp                 # Test entry (non-library code)
```

### Loading Flow

1. Validate PE headers (DOS/NT Header)
2. Calculate total image size
3. Allocate virtual memory (prefer ImageBase, fallback to ASLR)
4. Copy section data to virtual memory
5. Fix-up Import Table (IAT) — recursively load dependent DLLs
6. Fix-up Delay-Load Import Table
7. Fix-up Relocation Table (if base address changed)
8. Fix-up Export Table
9. Fix-up Exception Handling Table
10. Execute TLS callbacks
11. Link into PEB LDR list
12. Call DllMain(DLL_PROCESS_ATTACH)
13. Free discardable section memory

## Notes

- This project is for educational and research purposes on Windows PE loading mechanisms only
- DLLs loaded from memory cannot be found by standard APIs like `GetModuleHandle`; use the provided `RtlGetModuleHandle` instead
- Loading 64-bit DLLs requires the x64 build; loading 32-bit DLLs requires the x86 build
- Not applicable to .NET managed assemblies

## License

For educational and research purposes only.
