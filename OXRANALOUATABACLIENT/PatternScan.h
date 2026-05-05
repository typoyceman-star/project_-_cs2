#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pattern_scan
{
    // Parses a signature string like "AA BB ?? CC" into a vector of bytes/wildcards.
    bool ParsePattern(const std::string& signature, std::vector<std::optional<std::uint8_t>>& out);

    // Scans a memory buffer for the pattern. Returns absolute address (ptr) or 0 if not found.
    std::uintptr_t FindPatternInBuffer(const std::uint8_t* buffer, std::size_t bufferSize, const std::vector<std::optional<std::uint8_t>>& pattern);

    // Finds the offset inside a loaded module (current process) that matches the signature.
    // Returns module-relative offset (uint32) if found.
    std::optional<std::uint32_t> FindOffsetInModule(const char* moduleName, const std::string& signature);
}
