#include "paths.h"
#include "../core/globals.h"

bool FileExistsA(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool GetDllDirA(std::string& outDir)
{
    char dllPath[MAX_PATH];
    DWORD len = GetModuleFileNameA(g_hModule ? g_hModule : GetModuleHandleA(nullptr), dllPath, MAX_PATH);
    if (!len || len >= MAX_PATH)
        return false;
    int lastSlash = -1;
    for (int i = static_cast<int>(len) - 1; i >= 0; --i)
    {
        if (dllPath[i] == '\\' || dllPath[i] == '/')
        {
            lastSlash = i;
            break;
        }
    }
    if (lastSlash < 0)
        return false;
    dllPath[lastSlash] = '\0';
    outDir.assign(dllPath);
    return true;
}

std::string MakePathA(const std::string& dir, const char* file)
{
    if (dir.empty()) return std::string(file);
    std::string path = dir;
    if (path.back() != '\\' && path.back() != '/')
        path.push_back('\\');
    path.append(file);
    return path;
}
