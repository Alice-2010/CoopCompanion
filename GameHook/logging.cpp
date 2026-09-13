#include "logging.h"

#include <windows.h>
#include <stdio.h>
#include <cstdarg>

void DebugLog(const char* msg)
{
    OutputDebugStringA(msg);
}

void DebugLogF(const char* fmt, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(buffer, _TRUNCATE, fmt, args);
    va_end(args);
    DebugLog(buffer);
}
