#include "pch.h"
#include "RtlGetProcAddress.h"
#ifndef RELOADMODULE_H
#define RELOADMODULE_H

HMODULE CopySystemDllA(__in const char* lpszDllName);
HMODULE CopySystemDllW(__in const wchar_t* lpwszDllName);
namespace Rtl {
	class Invoker {
		PVOID m_pFunc;
	public:
		Invoker(__in PVOID pFunc) :m_pFunc(pFunc) {}
		template<typename Ret, typename ...Args>
		Ret StdCall(__in Args&&...args) {
			if (!m_pFunc) return Ret{};
			using FuncType = Ret(__stdcall*)(Args...);
			return ((FuncType)m_pFunc)(args...);
		}
	};
	class ModuleBasicInfo {
		HMODULE m_hod;
	public:
		ModuleBasicInfo(__in HMODULE hmod) :m_hod(hmod) {}
		Invoker operator[](__in const char* lpszFuncName) {
			return RtlGetProcAddress(m_hod, lpszFuncName);
		}
	};
	class Reloader :public Singleton<Reloader> {
		std::unordered_map<std::wstring, HMODULE> m_ReloadModules;
	public:
		Reloader() = default;
		ModuleBasicInfo operator [](__in const char* lpszModuleName) {
			return operator[](NarrowToWide(lpszModuleName).c_str());
		}
		ModuleBasicInfo operator [](__in const wchar_t* lpwszModuleName) {
			HMODULE hMod = nullptr;
			auto iter = m_ReloadModules.find(lpwszModuleName);
			if (iter != m_ReloadModules.end()) {
				hMod = iter->second;
			}
			else {
				hMod = CopySystemDllW(lpwszModuleName);
				if (hMod) {
					m_ReloadModules.insert(std::make_pair(lpwszModuleName, hMod));
				}
				else {
					DebugPrint(LEVEL_ERROR, "CopySystemDllW failed!");
					throw std::runtime_error("invalid CopySystemDllw");
				}
			}
			return hMod;
		}

	};
}
#endif
