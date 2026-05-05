#pragma once
// Помощники для работы с путями относительно DLL.

#include <string>

bool FileExistsA(const std::string& path);
bool GetDllDirA(std::string& outDir);
std::string MakePathA(const std::string& dir, const char* file);
