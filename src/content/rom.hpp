#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace tetris::content {

constexpr std::string_view supported_sha1 = "74591cc9501af93873f9a5d3eb12da12c0723bbc";
constexpr std::size_t supported_size = 32U * 1024U;

struct Rom {
    std::filesystem::path path;
    std::vector<std::uint8_t> bytes;
    std::string digest;
    std::string title;
    std::uint8_t revision{};
    bool header_checksum_valid{};
    bool global_checksum_valid{};

    std::span<const std::uint8_t> range(std::size_t begin, std::size_t end) const;
};

bool load_rom(const std::filesystem::path& path, Rom& result, std::string& error);
bool load_rom(std::span<const std::uint8_t> bytes, Rom& result, std::string& error);
bool validate_supported(const Rom& rom, std::string& error);
bool is_supported(const Rom& rom);

} // namespace tetris::content
