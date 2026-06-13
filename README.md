# RtlLoadLibrary

Windows PE 手动内存加载库，支持从内存中直接加载 DLL 而无需落盘，绕过标准 `LoadLibrary` 机制。

## 特性

- **内存加载**: 从内存缓冲区加载 PE 镜像，无需文件落盘
- **API Set 解析**: 自动解析 `api-ms-win-*` / `ext-ms-win-*` 等虚拟 DLL 名到真实系统 DLL
- **完整 PE 修复管线**:
  - 导入表 (IAT) 修复
  - 延迟导入表 (Delay-Load) 修复
  - 重定位表 (Relocation) 修复
  - TLS 回调执行
  - 导出表修复
  - 异常处理表 (SEH/CFG) 修复
- **LdrLink 管理**: 将手动加载的模块链接到 PEB 的模块列表中
- **Loader Lock 封装**: 提供 `RtlLockLoaderLock` / `RtlUnlockLoaderLock` 保护并发加载
- **ANSI/Wide 双接口**: 所有核心 API 提供 `A` / `W` 两个版本
- **异步加载**: 通过 `std::future` 支持异步 DLL 加载
- **RAII 资源管理**: `GenericHandle` / `FileMap` 自动管理 Windows 内核句柄
- **字符串加密**: 编译期 XOR 加密敏感字符串，防静态分析

## 构建

### 环境要求

- Visual Studio 2019 或更高版本
- Windows SDK 10.0+
- C++17 标准

### 编译

1. 打开 `RtlLoadLibrary.sln`
2. 选择目标平台 (x86 / x64) 和配置 (Debug / Release)
3. 生成解决方案

输出为 `RtlLoadLibrary.dll`。

## API 参考

### 核心加载函数

```cpp
// 宽字符版本 — 按文件名加载 DLL
HMODULE WINAPI RtlLoadLibraryW(LPCWSTR lpLibFileName);

// ANSI 版本
HMODULE WINAPI RtlLoadLibraryA(LPCSTR lpLibFileName);

// 扩展版本 — 支持文件句柄和标志
HMODULE WINAPI RtlLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
HMODULE WINAPI RtlLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

// 从内存缓冲区加载
HMODULE WINAPI RtlLoadLibraryInMemory(PVOID lpBytes, DWORD dwSize);

// 从内存缓冲区加载（扩展版，支持文件名和完整路径）
HMODULE WINAPI RtlLoadLibraryInMemoryEx(
    PVOID  lpBytes,
    DWORD  dwSize,
    DWORD  dwFlags,
    LPWSTR lpFileName,
    LPWSTR lpFullName
);
```

### 模块查询

```cpp
HMODULE WINAPI RtlGetModuleHandleA(LPCSTR  lpModuleName);
HMODULE WINAPI RtlGetModuleHandleW(LPCWSTR lpModuleName);
FARPROC WINAPI RtlGetProcAddress(HMODULE hModule, LPCSTR lpProcName);
```

### 区段/入口点

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

### LDR 链表管理

```cpp
bool WINAPI AddLdrLink(const MY_LDR_DATA_TABLE_ENTRY& Entry, BOOL bInitialize);
bool WINAPI RemoveLdrLink(const UNICODE_STRING* BaseName, const UNICODE_STRING* FullPath);
```

### 工具函数

```cpp
// 异步加载
std::future<HMODULE> AsyncRtlLoadLibraryA(LPCSTR lpLibFileName);

// API Set 名称检测
bool IsApiSetName(const std::wstring& name);

```

## 使用示例

### 基础文件加载

```cpp
#include "RtlLoadLibrary.h"

// 加载 user32.dll（等价于 LoadLibraryW）
HMODULE hUser32 = RtlLoadLibraryW(L"user32.dll");

// 获取函数地址
auto pMessageBox = (int(WINAPI*)(HWND, LPCWSTR, LPCWSTR, UINT))
    RtlGetProcAddress(hUser32, "MessageBoxW");

pMessageBox(NULL, L"Hello", L"RtlLoadLibrary", MB_OK);
```

### 从内存加载

```cpp
// 读取 DLL 到内存
HANDLE hFile = CreateFileW(L"mylib.dll", GENERIC_READ, FILE_SHARE_READ,
    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

DWORD dwFileSize = GetFileSize(hFile, NULL);
std::vector<BYTE> buf(dwFileSize);
ReadFile(hFile, buf.data(), dwFileSize, NULL, NULL);
CloseHandle(hFile);

// 从内存加载
HMODULE hMod = RtlLoadLibraryInMemory(buf.data(), dwFileSize);

// 获取导出函数
auto pFunc = (void(*)())RtlGetProcAddress(hMod, "MyExportedFunc");
pFunc();
```

### 异步加载

```cpp
auto future = AsyncRtlLoadLibraryA("kernel32.dll");
// ... 做其他事情 ...
HMODULE hMod = future.get();  // 阻塞等待加载完成
```

### 线程安全加载

```cpp
RtlLockLoaderLock();
HMODULE hMod1 = RtlLoadLibraryInMemory(buf1, size1);
HMODULE hMod2 = RtlLoadLibraryInMemory(buf2, size2);
RtlUnlockLoaderLock();
```

## 架构概览

```
RtlLoadLibrary/
├── RtlLoadLibrary.h/cpp    # 导出 API 实现入口
├── RtlMemory.h/cpp          # PE 核心加载引擎 (1453行)
├── ApisetQuery.h/cpp        # API Set Schema 解析器 (单例)
├── RtlGetModuleHandle.h/cpp # 自定义 GetModuleHandle 实现
├── RtlGetProcAddress.h/cpp  # 自定义 GetProcAddress 实现
├── LdrLink.h/cpp            # PEB LDR 链表操作
├── GlobalObject.h/cpp       # 全局状态管理 (单例)
├── KernelStruct.h           # NT 内部结构定义
├── Singleton.h              # 单例模板基类
├── GenericHandle.h          # RAII 句柄封装
├── FileMap.h/cpp            # 文件映射 RAII 封装
├── XorStr.h                 # 编译期字符串加密
├── DebugPrint.h/cpp         # 日志系统
├── StringUtils.h/cpp        # 宽窄字符转换
├── ThreadObject.h/cpp       # 线程操作封装
├── EnumModule.h/cpp         # 模块枚举
├── EnumThread.h/cpp         # 线程枚举
├── ValidAddress.h/cpp       # 地址有效性验证
├── Exports.def              # 导出符号定义
├── pch.h/cpp                # 预编译头
└── main.cpp                 # 测试入口 (非库代码)
```

### 加载流程

1. 校验 PE 头部 (DOS/NT Header)
2. 计算镜像总大小
3. 申请虚拟内存 (优先使用 ImageBase，失败则 ASLR)
4. 逐区段拷贝数据到虚拟内存
5. 修复导入表 (IAT) — 递归加载依赖 DLL
6. 修复延迟导入表
7. 修复重定位表 (若基址改变)
8. 修复导出表
9. 修复异常处理表
10. 执行 TLS 回调
11. 链接到 PEB LDR 链表
12. 调用 DllMain(DLL_PROCESS_ATTACH)
13. 释放可丢弃区段内存

## 注意事项

- 本项目仅供学习研究 Windows PE 加载机制使用
- 从内存加载的 DLL 无法被 `GetModuleHandle` 等标准 API 找到，需使用本项目提供的 `RtlGetModuleHandle`
- 加载 64 位 DLL 需使用 x64 编译版本，加载 32 位 DLL 需使用 x86 编译版本
- 不适用于 `.NET` 托管程序集

## 许可

仅供学习研究使用。
