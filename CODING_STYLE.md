# RtlLoadLibrary 编码风格指南

## 1. 变量命名 (匈牙利命名法 Hungarian Notation)

### 1.1 基本类型前缀

| 前缀 | 类型 | 示例 |
|------|------|------|
| `dw` | `DWORD` | `dwLen`, `dwFlags`, `dwSize`, `dwOldProtect`, `dwReadByte`, `dwExceptionCode` |
| `h` / `hMod` | `HANDLE` / `HMODULE` | `hFile`, `hFileReal`, `hNt`, `hMod`, `hResult`, `hTargetModule` |
| `p` | 指针 (pointer) | `pBase`, `pDos`, `pNt`, `pSec`, `pDir`, `pEntryPoint`, `pFileImage` |
| `lp` | 长指针 | `lpBytes`, `lpLibFileName`, `lpSecName`, `lpFullPath` |
| `lpc` | const 长指针 | `lpcLibFileName` |
| `lpcwsz` | `LPCWSTR` | `lpcwszSearchPath` |
| `n` | 整数 / 计数 | `nLen`, `nCount`, `nSize`, `nLastSlash`, `nInitProcess`, `nModuleCount` |
| `ul` | `ULONG` / `unsigned long` | `ulSeed`, `ulFileNameBodyLen`, `ulCharsetLen` |
| `ull` | `unsigned long long` | `ullTotalSize` |
| `b` | `BOOL` / `bool` | `bRNGInitialized`, `bHooked`, `bOwner`, `bValid`, `bPrefix`, `bSuffix`, `bLoadState` |
| `f` | `float` | `fSim` |
| `ch` | 字符 | `c1`, `c2` |
| `c` | char 数组/字符 | `cAryModuleName`, `cAryFullPath` |
| wszAry | 宽字符数组 | `wszAryProgramFiles` |

### 1.2 STL 容器前缀

| 前缀 | 类型 | 示例 |
|------|------|------|
| `vec` | `std::vector` | `vecResults`, `vecPattens`, `vecSafeDirs`, `vecstrResults`, `vecwstrResults` |
| `wstr` | `std::wstring` | `wstrLibFileName`, `wstrBaseName`, `wstrFileName`, `wstrTargetName` |
| `str` | `std::string` | `strLibNameA` |
| `ref` | 引用 | `refSelfLoad` |

### 1.3 结构/类实例前缀

| 前缀 | 含义 | 示例 |
|------|------|------|
| `tag` | 结构体实例 | `tagFileMap`, `tagModInfo`, `tagFileSize`, `tagExpParam`, `tagLocalTime`, `tagPromise`, `tagFuture` |

### 1.4 数组前缀

| 前缀 | 含义 | 示例 |
|------|------|------|
| `wszAry` | 宽字符数组 | `wszAryCurrentDllName`, `wszAryFullPath`, `wszArySystemRoot` |
| `wAry` | 宽字符数组（简写） | `wAryCurrentDllName`, `wAryFileName` |
| `cAry` | char 数组 | `cAryModuleName`, `cAryFullPath` |

---

## 2. 作用域前缀

### 2.1 成员变量：`m_`

所有类的成员变量必须带 `m_` 前缀：

```cpp
// RtlMemory 类
LPVOID                m_pFileImage;
DWORD                 m_dwFileSize;
PIMAGE_DOS_HEADER     m_pDosHeader;
std::wstring          m_wstrFileName;
bool                  m_bHooked;

// ThreadObject 类
HANDLE    m_hThread;
DWORD     m_ThreadId;
uintptr_t m_ulStartAddr;
bool      m_bValid;

// FileMap 类
_THANDLE    m_hFile;
LPVOID      m_lpFileBase;

// GenericHandle 类
T m_handle;
bool m_bOwner;

// ApisetQuery 类
THANDLE m_hMapping;
THANDLE m_hFile;
bool m_bLoaded;
```

### 2.2 全局/静态变量：`g_`

```cpp
static std::function<HMODULE(const wchar_t*)> g_GetModuleHandleCallback;  // 回调函数指针
static std::mutex                              g_LdrLinkMutex;            // 全局互斥锁
static std::mutex                              g_CallHistoryMutex;        // 调用历史锁
static bool                                    g_bInit;                   // 全局初始化标志
constexpr auto                                 gxorTime;                  // XOR 字符串种子
```

### 2.3 无前缀

| 场景 | 示例 |
|------|------|
| 循环局部变量 | `i`, `j`, `len`, `idx` |
| lambda 参数 | `ch`, `c` |
| 模板参数 | `T`, `F`, `Args`, `Tx`, `Ty`, `N`, `K` |
| 枚举值 | `LEVEL_ERROR`, `LEVEL_INFO`, `Running`, `Waiting` |

---

## 3. 类型命名

### 3.1 类/结构体：PascalCase

| 名称 | 类型 |
|------|------|
| `RtlMemory` | PE 内存加载器类 |
| `ApisetQuery` | API Set 解析器（单例） |
| `GlobalObject` | 全局对象（单例） |
| `ThreadObject` | 线程操作封装 |
| `ThreadPool` | 线程池（单例） |
| `FileMap` | 文件映射 RAII 封装 |
| `GenericHandle` | 泛型 HANDLE RAII 封装 |
| `HandleView` | HANDLE 视图策略 |
| `NormalHandle` | 普通 HANDLE 策略 |
| `FileHandle` | 文件查找 HANDLE 策略 |
| `Singleton` | 单例模板基类 |
| `LastError` | 线程最后错误封装（单例） |
| `CallIdentifier` | 调用标识符结构体 |
| `WorkerQueue` | 线程池工作队列 |

### 3.2 NT 风格结构体：`_MY_` 前缀 + 全大写

Windows 内部结构的自定义镜像，以示区分：

```cpp
typedef struct _MY_LDR_DATA_TABLE_ENTRY { ... } MY_LDR_DATA_TABLE_ENTRY, *PMY_LDR_DATA_TABLE_ENTRY;
typedef struct _MY_PEB_LDR_DATA { ... } MY_PEB_LDR_DATA, *PMY_PEB_LDR_DATA;
typedef struct _MY_PEB { ... } MY_PEB, *PMY_PEB;
typedef struct MYTHREAD_BASIC_INFORMATION { ... } MYTHREAD_BASIC_INFORMATION;
typedef struct MYSYSTEM_PROCESS_INFORMATION { ... } MYSYSTEM_PROCESS_INFORMATION;
```

### 3.3 内部结构体：`_` 前缀

```cpp
typedef struct _SEHInfo { ... } SEHInfo, *PSEHInfo;
typedef struct _PE_SECTION_INFO { ... } PE_SECTION_INFO, *PPE_SECTION_INFO;
typedef struct _IMAGE_SECTION_HEADER64 { ... } IMAGE_SECTION_HEADER64;
```

### 3.4 API Set 结构体：PascalCase + 全大写缩写

```cpp
typedef struct _API_SET_NAMESPACE { ... } API_SET_NAMESPACE, *PAPI_SET_NAMESPACE;
typedef struct _API_SET_NAMESPACE_ENTRY { ... } API_SET_NAMESPACE_ENTRY, *PAPI_SET_NAMESPACE_ENTRY;
typedef struct _API_SET_VALUE_ENTRY { ... } API_SET_VALUE_ENTRY, *PAPI_SET_VALUE_ENTRY;
```

### 3.5 函数指针类型：全称 + `_t` 后缀

```cpp
typedef VOID(NTAPI* RtlInitUnicodeString_t)(PUNICODE_STRING, PCWSTR);
typedef NTSTATUS(NTAPI* NtCreateFile_t)(PHANDLE, ACCESS_MASK, ...);
typedef NTSTATUS(NTAPI* NtWriteFile_t)(HANDLE, HANDLE, ...);
typedef NTSTATUS(NTAPI* NtClose_t)(HANDLE);
```

### 3.6 枚举

| 枚举名 | 值风格 |
|--------|--------|
| `DebugLevel` (enum) | `LEVEL_INFO`, `LEVEL_WARN`, `LEVEL_ERROR`, `LEVEL_DEBUG`, `LEVEL_ALL` |
| `ThreadState` (enum class) | `Running`, `Waiting`, `Terminated`, `Unknown` |
| `_SECTION_INHERIT` (typedef enum) | `ViewShare`, `ViewUnmap` |

### 3.7 `using` 类型别名

```cpp
using THANDLE  = GenericHandle<HANDLE, NormalHandle>;          // 拥有所有权的 HANDLE
using FHANDLE  = GenericHandle<HANDLE, FileHandle>;            // 文件查找 HANDLE
using _THANDLE = GenericHandle<HANDLE, HandleView<NormalHandle>>; // 仅视图的 HANDLE（不负责关闭）
using EnumModuleCallback = const std::function<bool(const MODULEENTRY32W&)>&;
using EnumCallback = std::function<bool(const THREADENTRY32&)>;
```

---

## 4. 函数命名

### 4.1 导出函数：`Rtl` + PascalCase + 后缀

导出函数模仿 ntdll 命名风格（`Rtl` 前缀代替 `Ldr`），ANSI/W 后缀区分字符集：

```cpp
RtlLoadLibraryW          RtlGetModuleHandleA
RtlLoadLibraryA          RtlGetModuleHandleW
RtlLoadLibraryExW        RtlGetProcAddress
RtlLoadLibraryExA        RtlGetSectionDataA
RtlLoadLibraryInMemory   RtlGetSectionDataW
RtlLoadLibraryInMemoryEx RtlGetEntryPoint
RtlLockLoaderLock        RtlInvalidateProcCache
RtlUnlockLoaderLock      
```

### 4.2 类公有方法：PascalCase

```cpp
RtlMemory::RunImage()         ThreadObject::GetContext()
RtlMemory::FixImportTable()   ThreadObject::NativeHandle()
RtlMemory::GetEntryPoint()    FileMap::GetMap()
RtlMemory::FixRelocationTable() FileMap::Initialize()
```

### 4.3 内部/辅助函数

| 命名方式 | 示例 |
|----------|------|
| `_` 前缀静态函数 | `_mywcsicmp()`, `_ucsicmp()` |
| camelCase 静态函数 | `mywcslen()`, `mywcsncpy_s()` |
| 单例访问器 | `GetInstance()`, `DestroyInstance()` |
| 判断函数 `Is*` | `IsApiSetName()`, `IsMemoryReadable()`, `IsExecutableImage()`, `IsDosHeaderValid()`, `IsNtHeaderValid()` |
| 获取函数 `Get*` | `GetAlignedSize()`, `GetHeader()`, `GetFileName()`, `GetContinuousReadableMemorySize()`, `GetCurrentPeb()`, `GetLastNtStatus()` |
| 设置函数 `Set*` | `SetGlobalLogLevel()`, `SetGetModuleHandleCallback()`, `SetGetProcAddressCallback()` |
| 清除函数 `Clear*` | `ClearGetModuleHandleCallback()`, `ClearGetProcAddressCallback()` |
| 操作函数 `Do*` | `AddLdrLink()`, `RemoveLdrLink()`, `RemoveLdr()`, `Flush()` |
| 验证函数 `Check*` | `CheckHeader()`, `CheckMask()`, `CheckAddrMask()`, `CheckPtrReadable()` |

---

## 5. 宏命名：UPPER_SNAKE_CASE

```cpp
#define NT_SUCCESS(Status)                       // 函数式宏
#define CONTAINING_RECORD(address, type, field)  // 系统宏（Windows）
#define XORSTRA(s)                              // 编译期字符串加密
#define XORSTRW(s)                              // 宽字符版本
#define XORSTR(s)                               // 自动选择字符集
#define MAX_FORWARD_DEPTH     32                // 常量
#define STRICT_MAX_SECTIONS    96
#define LEGACY_MAX_SECTIONS    16
#define HOST_MACHINE                           // 架构标识
```

---

## 6. 头文件保护

### 双保险：`#pragma once` + GUID 后缀的传统 include guard

```cpp
// 典型模式
#pragma once
#ifndef SINGLETON_H5C8860F94B8D49198321BF157F1E99E
#define SINGLETON_H5C8860F94B8D49198321BF157F1E99E
// ...
#endif SINGLETON_H5C8860F94B8D49198321BF157F1E99E

// 另一种 GUID 风格
#ifndef GENERICHANDLE7D17956C_1D7C_4257_9843_134CBEB4C900
#define GENERICHANDLE7D17956C_1D7C_4257_9843_134CBEB4C900

// 仅 #pragma once（无辅助宏）
#pragma once   // RtlLoadLibrary.h, RtlMemory.h, EnumModule.h, LdrLink.h 等

// 仅传统 guard
#ifndef RTLGETPROCADDRESS_H
#define RTLGETPROCADDRESS_H
```

---

## 7. 格式与缩进

### 7.1 缩进：Tab

所有缩进使用 Tab 字符，不使用空格缩进。

### 7.2 花括号风格（K&R 变体）

**函数：花括号与函数名同行**

```cpp
HMODULE WINAPI RtlLoadLibraryW(LPCWSTR lpLibFileName)
{
    return RtlLoadLibraryExW(lpLibFileName, NULL, 0);
}

bool RtlMemory::IsExecutableImage()
{
    if ((m_pNtHeader->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0) {
        DebugPrint(LEVEL_ERROR, "Not a executable file!");
        return false;
    }
    return true;
}
```

**类/结构体：花括号换行**

```cpp
class RtlMemory {
    LPVOID m_pFileImage;
    // ...
public:
    PVOID RunImage(PVOID lpBytes, DWORD dwSize, DWORD dwFlags);
};

struct CallIdentifier {
    HMODULE hModule;
    std::string functionName;
};
```

**控制流 if/for/while：花括号同行**

```cpp
if (!lpLibFileName) {
    return NULL;
}

for (size_t i = 0; i < ulFileNameBodyLen; ++i) {
    wstrFilenameBody += wszCharset[tagDist(tagRNG)];
}

while (pCurrentEntry != &pLdr->InLoadOrderModuleList) {
    // ...
}
```

### 7.3 include 风格

```cpp
// 预编译头（无空格，双引号）
#include"pch.h"

// 系统头文件（尖括号）
#include <windows.h>
#include <stdio.h>
#include <future>
#include <string>
#include <vector>

// 项目头文件（双引号）
#include "RtlLoadLibrary.h"
#include "ApisetQuery.h"
#include "RtlGetModuleHandle.h"
```

注意：`#include"pch.h"` 是 `#include` 和 `"pch.h"` 之间没有空格的紧凑写法。

### 7.4 空行

- 函数体之间通常有一个或多个空行分隔
- 逻辑块之间用空行分隔
- 部分文件头尾有不一致的空白

---

## 8. 代码模式

### 8.1 单例模式：`Singleton<T>`

通过 CRTP 继承使用：

```cpp
class ApisetQuery   : public Singleton<ApisetQuery>   { ... };
class GlobalObject  : public Singleton<GlobalObject>  { ... };
class ThreadPool    : public Singleton<ThreadPool>    { ... };
class LastError     : public Singleton<LastError>     { ... };
class Rtl::Reloader : public Singleton<Rtl::Reloader> { ... };
```

访问单例：`ClassName::GetInstance()`

### 8.2 静态局部变量初始化

利用静态局部变量的 lazy init 特性进行初始化，常用于 Lambda：

```cpp
static const std::wstring& SystemDirectory()
{
    static std::wstring wstrDir = [] {
        wchar_t waryBuf[MAX_PATH] = {0};
        DWORD dwLen = ::GetSystemDirectoryW(waryBuf, _countof(waryBuf));
        return std::wstring(waryBuf, dwLen);
    }();
    return wstrDir;
}
```

### 8.3 全局构造期自动初始化

```cpp
// BreakDump.h
static int nInitProcess = InitHandleException();

// GlobalObject.h
static bool g_bInit = Init();
```

### 8.4 extern "C" 导出块

```cpp
extern "C" {
    HMODULE WINAPI RtlLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
    HMODULE WINAPI RtlLoadLibraryW(LPCWSTR lpLibFileName);
    HMODULE WINAPI RtlLoadLibraryInMemory(__in PVOID lpBytes, __in_opt DWORD dwSize);
    bool WINAPI AddLdrLink(const _MY_LDR_DATA_TABLE_ENTRY& Entry, BOOL bInitialize);
    bool WINAPI RemoveLdrLink(const UNICODE_STRING* BaseName, const UNICODE_STRING* FullPath);
    NTSYSAPI NTSTATUS NTAPI NtMapViewOfSection(...);
    NTSYSAPI NTSTATUS NTAPI NtUnmapViewOfSection(...);
}
```

### 8.5 调用约定

| 宏 | 含义 | 使用场景 |
|----|------|----------|
| `WINAPI` | `__stdcall` | 大部分导出函数 |
| `NTAPI` | `__stdcall` | NT 原生 API 函数声明 |
| `NTSYSAPI` | 导出属性 | NT 系统 API 导出 |
| `XORSTR_INLINE` | `__forceinline` | XOR 字符串解密 |

### 8.6 SAL 注解

```cpp
__in PVOID lpBytes
__in_opt DWORD dwSize
__in LPCWSTR src
__out UNICODE_STRING* dest
_In_ HMODULE hMod
_In_ const char* lpszProcName
_Out_ std::unique_ptr<CHAR[]>& SystemInfo
_Out_opt_ PULONG nSize
```

### 8.7 宽窄字符转换

```cpp
std::string  WideToNarrow(const std::wstring& wideStr);
std::wstring NarrowToWide(const std::string& narrowStr);
```

### 8.8 日志系统

```cpp
enum DebugLevel {
    LEVEL_NONE  = 0,
    LEVEL_INFO  = 1 << 0,
    LEVEL_WARN  = 1 << 1,
    LEVEL_ERROR = 1 << 2,
    LEVEL_DEBUG = 1 << 3,
    LEVEL_ALL   = LEVEL_INFO | LEVEL_WARN | LEVEL_ERROR | LEVEL_DEBUG
};

// 宏形式调用
DebugPrint(LEVEL_ERROR, "Failed to get SystemRoot");
DebugPrint(LEVEL_INFO,  "Section found: %s", pSec->Name);
DebugPrint(LEVEL_WARN,  "Empty Entry Point!");
```

### 8.9 字符串加密（XorStr）

编译期 XOR 加密字符串，防止静态分析：

```cpp
HMODULE hNt = RtlGetModuleHandleA(XORSTRA("ntdll.dll"));  // ANSI 版本
DebugPrint(LEVEL_ERROR, XORSTRA("can't find xxx"));        // ANSI 版本
#define XORSTRW(s)  // 宽字符版本
#define XORSTR(s)   // 根据 _UNICODE 自动选择
```

### 8.10 RAII 资源管理

```cpp
// GenericHandle<T, Traits> — 自动关闭 HANDLE
using THANDLE  = GenericHandle<HANDLE, NormalHandle>;
using FHANDLE  = GenericHandle<HANDLE, FileHandle>;
using _THANDLE = GenericHandle<HANDLE, HandleView<NormalHandle>>;  // 仅观察，不负责关闭

// FileMap — 自动 UnmapViewOfFile + CloseHandle
// RtlMemory — 构造函数分配内存，自动管理 PE 加载过程
```

### 8.11 使用 `UNREFERENCED_PARAMETER` 宏

```cpp
UNREFERENCED_PARAMETER(DispatcherContext);
```

### 8.12 条件编译

```cpp
#if defined(_WIN64)       // 架构判断
#ifdef _WIN64             // x64 特定代码
#ifndef _WINDOWS_         // 防止重复包含
#ifdef UNICODE            // 字符集自动选择
```

---

## 9. 语言标准与特性

### 9.1 C++17 特性

```cpp
if constexpr (std::is_same_v<TxRaw, wchar_t>)   // 编译期条件
auto [key, val] : refSelfLoad                     // 结构化绑定
```

### 9.2 C++11/14 特性

```cpp
std::future<HMODULE>                              // 异步结果
std::call_once(m_once_flag, ...)                  // 线程安全单次调用
std::make_unique<WorkerQueue>()                   // 智能指针
std::enable_if_t                                   // SFINAE
decltype(auto)                                     // 完美转发返回类型
noexcept                                           // 异常规范
constexpr                                          // 编译期计算
```

### 9.3 C++ STL 使用

| 头文件 | 组件 |
|--------|------|
| `<string>` | `std::string`, `std::wstring` |
| `<vector>` | `std::vector<T>` |
| `<map>` | `std::map<K,V>` |
| `<unordered_map>` | `std::unordered_map<K,V>` |
| `<set>` | `std::set<T>` |
| `<memory>` | `std::unique_ptr`, `std::make_unique` |
| `<functional>` | `std::function<Signature>` |
| `<algorithm>` | `std::find_if_not`, `std::transform` |
| `<utility>` | `std::pair`, `std::make_pair`, `std::forward`, `std::move` |
| `<future>` | `std::future<T>`, `std::promise<T>` |
| `<thread>` | `std::thread` |
| `<mutex>` | `std::mutex`, `std::lock_guard`, `std::call_once`, `std::once_flag` |
| `<atomic>` | `std::atomic<T>` |
| `<deque>` | `std::deque<T>` |
| `<chrono>` | `std::chrono::steady_clock` |
| `<random>` | `std::mt19937_64`, `std::random_device` |
| `<type_traits>` | `std::is_same_v`, `std::remove_const_t` |

---

## 10. 混合风格不一致点

以下为代码中存在的风格不一致之处：

| 问题 | 示例 |
|------|------|
| 有无 `#pragma once` | 部分文件仅有传统 guard，部分仅 `#pragma once`，部分两者兼备 |
| 花括号对齐 | 部分单行 if 未加花括号，部分右括号位置对不齐 |
| 函数接口空格 | `operator bool()` vs `operator bool()`，`operator =(`, `& operator&(` |
| `_THANDLE` vs `THANDLE` | `_` 前缀表示不负责关闭的所有权策略 |

---

## 11. 项目结构约定

- **一个 `.h` 对应一个 `.cpp`**（大部分情况），但存在全 inline/template 的仅头文件（`ThreadPool.h`, `Singleton.h`, `XorStr.h`）
- **预编译头**：`pch.h` 集中包含所有公共系统头文件和项目工具头文件
- **模块文件**：`RtlMemory.cpp`（1453 行）为核心 PE 加载逻辑，为项目中最庞大的实现文件
- **导出**：通过 `Exports.def` 显式枚举所有导出函数符号

---

*本文档基于项目代码自动分析生成，覆盖了 `RtlLoadLibrary` 项目的主要编码约定。*
