#pragma once

#include <cstdio>
#include <windows.h>


namespace MokoLog
{
    inline void Print(const char* fmt, ...)
    {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        OutputDebugStringA(buffer);
    }
}

#define LOG_INFO(fmt, ...)  MokoLog::Print("[MOKOLOG INFO] "  fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  MokoLog::Print("[MOKOLOG WARN] "  fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) MokoLog::Print("[MOKOLOG ERROR] " fmt "\n", ##__VA_ARGS__)