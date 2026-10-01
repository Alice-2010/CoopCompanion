#pragma once

typedef enum LogLevel
{
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} LogLevel;

typedef struct LogEntry
{
    char message[256];
    LogLevel level;
} LogEntry;

void DebugLog(const char* msg);
void DebugLogF(const char* fmt, ...);
void AppendLogEntry(const char* msg, LogLevel level);
char* FormatLog(LogEntry entry);
char* GetLogBuffer();
