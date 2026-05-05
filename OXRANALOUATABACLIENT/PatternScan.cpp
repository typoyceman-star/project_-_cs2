#include "PatternScan.h"

#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <algorithm>
#include <sstream>
#include <limits>

namespace pattern_scan
{
    bool ParsePattern(const std::string& signature, std::vector<std::optional<std::uint8_t>>& out)
    {
        out.clear();
        std::istringstream iss(signature);
        std::string token;
        while (iss >> token)
        {
            if (token == "??" || token == "?")
            {
                out.emplace_back(std::nullopt);
                continue;
            }
            if (token.size() != 2)
                return false;
            int value = 0;
            std::istringstream hex(token);
            hex >> std::hex >> value;
            if (hex.fail())
                return false;
            out.emplace_back(static_cast<std::uint8_t>(value & 0xFF));
        }
        return !out.empty();
    }

    std::uintptr_t FindPatternInBuffer(const std::uint8_t* buffer, std::size_t bufferSize, const std::vector<std::optional<std::uint8_t>>& pattern)
    {
        if (buffer == nullptr || bufferSize == 0 || pattern.empty())
            return 0;
        const std::size_t patSize = pattern.size();
        if (patSize > bufferSize)
            return 0;
        for (std::size_t i = 0; i <= bufferSize - patSize; ++i)
        {
            bool match = true;
            for (std::size_t j = 0; j < patSize; ++j)
            {
                if (pattern[j].has_value() && buffer[i + j] != pattern[j].value())
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return reinterpret_cast<std::uintptr_t>(buffer + i);
        }
        return 0;
    }

    std::optional<std::uint32_t> FindOffsetInModule(const char* moduleName, const std::string& signature)
    {
        if (!moduleName || *moduleName == '\0')
            return std::nullopt;

        HMODULE hMod = GetModuleHandleA(moduleName);
        if (!hMod)
            return std::nullopt;

        MODULEINFO mi{};
        if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi)))
            return std::nullopt;

        const auto base = reinterpret_cast<const std::uint8_t*>(mi.lpBaseOfDll);
        const std::size_t size = static_cast<std::size_t>(mi.SizeOfImage);

        std::vector<std::optional<std::uint8_t>> pat;
        if (!ParsePattern(signature, pat))
            return std::nullopt;

        const auto absolute = FindPatternInBuffer(base, size, pat);
        if (absolute == 0)
            return std::nullopt;

        const auto offset = absolute - reinterpret_cast<std::uintptr_t>(base);
        if (offset > std::numeric_limits<std::uint32_t>::max())
            return std::nullopt;

        return static_cast<std::uint32_t>(offset);
    }
}
