#pragma once
#ifndef CLOGGER_H
#define CLOGGER_H

#include"pch.h"

enum DebugLevel {
    LEVEL_NONE = 0,
    LEVEL_INFO = 1 << 0,
    LEVEL_WARN = 1 << 1,
    LEVEL_ERROR = 1 << 2,
    LEVEL_DEBUG = 1 << 3,
    LEVEL_ALL = LEVEL_INFO | LEVEL_WARN | LEVEL_ERROR | LEVEL_DEBUG
};
DebugLevel SetGlobalLogLevel(__in DebugLevel level);

void DebugPrintA(__in int level, __in const char* fmt, ...);

#define DebugPrint(level,fmt,...) DebugPrintA(level, XORSTRA(fmt), __VA_ARGS__)



#endif// CLOGGER_H
