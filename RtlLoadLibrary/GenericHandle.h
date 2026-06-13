#pragma once
#ifndef GENERICHANDLE7D17956C_1D7C_4257_9843_134CBEB4C900
#define GENERICHANDLE7D17956C_1D7C_4257_9843_134CBEB4C900
#include<Windows.h>
class NormalHandle {
public:
	inline  static void Close(__inout HANDLE& handle)noexcept {
		CloseHandle(handle);
		handle = InvalidHandle();
	}
	inline static HANDLE InvalidHandle()noexcept { 
		return INVALID_HANDLE_VALUE; 
	}
	inline static bool   IsValid(__in HANDLE handle)noexcept { 
		return handle != InvalidHandle() && handle;
	}
	inline static DWORD  Wait(__in HANDLE handle, __in DWORD dwMilliseconds) {
		return WaitForSingleObject(handle, dwMilliseconds);
	}
};
class FileHandle {
public:
	inline  static void  Close(__inout HANDLE& handle)noexcept { 
		FindClose(handle);
		handle = InvalidHandle();
	}
	inline static HANDLE InvalidHandle()noexcept { 
		return INVALID_HANDLE_VALUE; 
	}
	inline static bool   IsValid(__in HANDLE handle)noexcept { 
		return handle != InvalidHandle() && handle; 
	}
};
template<typename T>
struct HandleView :public T {
public:
	inline static void Close(__inout HANDLE& handle) noexcept {
		(handle);
	}
};
template<class T, class Traits>
class GenericHandle {
private:
	T m_handle = Traits::InvalidHandle();
	bool m_bOwner = false;
	inline bool IsValid()noexcept { 
		return Traits::IsValid(m_handle); 
	}
public:
	GenericHandle() = default;
	GenericHandle& operator =(__in const T& handle){
		Close();
		m_handle = handle;
		m_bOwner = true;
		if (!IsValid()) {
			m_bOwner = false;
		}
		return *this;
	}
	GenericHandle(__in const T& handle, __in bool bOwner = true) :m_handle(handle), m_bOwner(bOwner) {
		if (!IsValid()) {
			m_bOwner = false;
		}
	}
	GenericHandle& Owned() {
		if(IsValid()) {
			m_bOwner = true;
		}
		return *this;
	}
	T Release(){
		m_bOwner = false;
		T oldHandle = m_handle;
		m_handle = Traits::InvalidHandle();
		return oldHandle;
	}

	~GenericHandle() {
		Close();
	}
	void Close() {
		if (m_bOwner && IsValid()) {
			Traits::Close(m_handle);
			m_bOwner = false;
		}else{
			m_handle = Traits::InvalidHandle();
		}
	}
	GenericHandle(GenericHandle&) = delete;
	GenericHandle& operator =(const GenericHandle&) = delete;
	inline GenericHandle& operator =(__in GenericHandle&& other)noexcept {
		m_handle = other.m_handle;
		m_bOwner = other.m_bOwner;
		other.m_handle = Traits::InvalidHandle();
		other.m_bOwner = false;
		return *this;
	}
	inline GenericHandle(__in GenericHandle&& other)noexcept {
		m_handle = other.m_handle;
		m_bOwner = other.m_bOwner;
		other.m_handle = Traits::InvalidHandle();
		other.m_bOwner = false;
	}
	inline explicit operator T() noexcept {
		return m_handle;
	}
	T& GetHandle() {

		m_bOwner = true;
		return	m_handle;
	}
	inline HANDLE* operator&() noexcept {
		return &m_handle;
	}
	inline operator bool() noexcept {
		return IsValid();
	}
	DWORD Wait(__in DWORD millionSecond = INFINITE) {
		return (IsValid()) ? Traits::Wait(m_handle, millionSecond) : WAIT_FAILED;
	}
};
using THANDLE  = GenericHandle<HANDLE, NormalHandle>;
using FHANDLE  = GenericHandle<HANDLE, FileHandle>;
using _THANDLE = GenericHandle<HANDLE, HandleView<NormalHandle>>;
#else
#endif//GENERICHANDLE7D17956C_1D7C_4257_9843_134CBEB4C900
