#pragma once

static char g_logBuffer[4096] = {};

void DebugLog(const char* msg);
void DebugLogF(const char* fmt, ...);
