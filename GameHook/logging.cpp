#include "logging.h"

#include <windows.h>
#include <stdio.h>
#include <cstdarg>

static LogEntry g_logBuffer[4096] = {};

void DebugLog(const char* msg)
{
    OutputDebugStringA(msg);
    AppendLogEntry(msg, LOG_LEVEL_DEBUG);
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

void AppendLogEntry(const char* msg, LogLevel level)
{
    constexpr size_t logCapacity = sizeof(g_logBuffer) / sizeof(g_logBuffer[0]);
    size_t index = 0;

    for (; index < logCapacity; ++index)
    {
        if (g_logBuffer[index].message[0] == '\0')
        {
            break;
        }
    }

    if (index == logCapacity)
    {
        memmove(g_logBuffer, g_logBuffer + 1, sizeof(g_logBuffer[0]) * (logCapacity - 1));
        index = logCapacity - 1;
    }

    strncpy_s(g_logBuffer[index].message, msg, _TRUNCATE);
    g_logBuffer[index].level = level;
}

char* FormatLog(LogEntry entry)
{
    static char formatted[270];
    const char* levelStr = "";
    switch (entry.level)
    {
        case LOG_LEVEL_INFO: levelStr = "INFO"; break;
        case LOG_LEVEL_DEBUG: levelStr = "DEBUG"; break;
        case LOG_LEVEL_WARNING: levelStr = "WARNING"; break;
        case LOG_LEVEL_ERROR: levelStr = "ERROR"; break;
    }
    snprintf(formatted, sizeof(formatted), "[%s] %s", levelStr, entry.message);
    return formatted;
}

char* GetLogBuffer()
{
    static char combined[4096 * 270];
    combined[0] = '\0';
    for (int i = 0; i < sizeof(g_logBuffer) / sizeof(g_logBuffer[0]); ++i)
    {
        if (g_logBuffer[i].message[0] != '\0')
        {
            strncat_s(combined, FormatLog(g_logBuffer[i]), _TRUNCATE);
            strncat_s(combined, "\n", _TRUNCATE);
        }
    }
    return combined;
}
