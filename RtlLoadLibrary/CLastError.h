#include"pch.h"
struct LastError :public Singleton<LastError> {
	LastError& operator=(__in DWORD value);
	operator DWORD();
	operator bool();
};
