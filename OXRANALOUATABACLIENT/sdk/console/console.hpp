#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef CONSOLE_ENABLED

namespace logger
{
    inline void initialize()
    {
        AllocConsole();
        SetConsoleTitleA(xorstr_("7th angel"));
        freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
        freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);
        freopen_s(reinterpret_cast<FILE**>(stdin), "CONIN$", "r", stdin);
    }

    inline void shutdown()
    {
        PostMessageA(GetConsoleWindow(), WM_CLOSE, 0, 0);
        FreeConsole();
    }

    inline void log(const char* file, int line, WORD color, const char* fmt, ...)
    {
        SYSTEMTIME time;
        GetLocalTime(&time);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
        WORD originalAttrs = consoleInfo.wAttributes;

        printf(xorstr_("[%02d:%02d:%02d] "), time.wHour, time.wMinute, time.wSecond);

        const char* file_name = strrchr(file, '\\') + 1;

        SetConsoleTextAttribute(hConsole, color);
        printf(xorstr_("%s:%d:"), file_name, line);

        SetConsoleTextAttribute(hConsole, originalAttrs);
        printf(" ");

        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);

        printf("\n");
    }
}

#define LOG(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_RED, fmt, __VA_ARGS__)
#define LOG_WARNING(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN, fmt, __VA_ARGS__)
#define LOG_SUCCESS(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_GREEN, fmt, __VA_ARGS__)
#define LOG_INFO(fmt, ...) logger::log(__FILE__, __LINE__, FOREGROUND_INTENSITY | FOREGROUND_BLUE, fmt, __VA_ARGS__)

#else

namespace logger
{
    inline void initialize() {  }
    inline void shutdown() {  }
}

#define LOG(fmt, ...) ((void)0)
#define LOG_ERROR(fmt, ...) ((void)0)
#define LOG_WARNING(fmt, ...) ((void)0)
#define LOG_SUCCESS(fmt, ...) ((void)0)
#define LOG_INFO(fmt, ...) ((void)0)

#endif
