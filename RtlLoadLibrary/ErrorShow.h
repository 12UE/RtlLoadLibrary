#pragma once
#include"pch.h"
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
std::string GetLastErrorAsString(__in DWORD dwErrorCode = GetLastError());
NTSTATUS RtlGetLastNtStatus();
std::string GetNtLastErrorAsString(__in NTSTATUS lErrorCode = RtlGetLastNtStatus());

