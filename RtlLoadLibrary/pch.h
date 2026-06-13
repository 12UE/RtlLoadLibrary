//stdc++=msvc 17
#pragma once
#ifndef PCH_H9DDC94F1A26E420EB8ECE21E4D13B1E2
#define PCH_H9DDC94F1A26E420EB8ECE21E4D13B1E2
#ifdef _WIN64
#define HOST_MACHINE IMAGE_FILE_MACHINE_AMD64
#else
#define HOST_MACHINE IMAGE_FILE_MACHINE_I386
#endif
#define STRICT_MAX_SECTIONS 96
#define  USECOPYSYSTEMMODULE  0x00008000
#define REASONABLE_MAX_SECTIONS 64
#define LEGACY_MAX_SECTIONS 16

#include <Windows.h>
#include <winnt.h>
#include <delayimp.h>
#include <winternl.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <dbghelp.h>
#include <Shlwapi.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include <utility>
#include <type_traits>

#include "BreakDump.h"
#include "Singleton.h"
#include "CLastError.h"
#include "ErrorShow.h"
#include "StringUtils.h"
#include "GetFullPath.h"
#include "GenericHandle.h"
#include "FileMap.h"
#include "XorStr.h"
#include "DebugPrint.h"
#include "ValidAddress.h"

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "shlwapi.lib")

#endif//!PCH_H9DDC94F1A26E420EB8ECE21E4D13B1E2