#include "content/rom.hpp"

#include "content/sha1.hpp"

#include <fstream>

namespace tetris::content {
namespace {

std::uint8_t header_checksum(std::span<const std::uint8_t> bytes) {
    std::uint8_t result = 0;
    for (std::size_t offset = 0x134; offset < 0x14D; ++offset)
        result = static_cast<std::uint8_t>(result - bytes[offset] - 1U);
    return result;
}

std::uint16_t global_checksum(std::span<const std::uint8_t> bytes) {
    std::uint32_t result = 0;
    for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
        if (offset != 0x14E && offset != 0x14F)
            result += bytes[offset];
    }
    return static_cast<std::uint16_t>(result & 0xFFFFU);
}

std::string read_title(std::span<const std::uint8_t> bytes) {
    std::string title;
    for (std::size_t offset = 0x134; offset <= 0x143; ++offset) {
        const std::uint8_t value = bytes[offset];
        if (value < 0x20 || value > 0x7E)
            break;
        title.push_back(static_cast<char>(value));
    }
    return title;
}

} // namespace

std::span<const std::uint8_t> Rom::range(std::size_t begin, std::size_t end) const {
    if (begin > end || end > bytes.size())
        return {};
    return std::span<const std::uint8_t>(bytes).subspan(begin, end - begin);
}

bool load_rom(const std::filesystem::path& path, Rom& result, std::string& error) {
    // Read the complete cartridge image before interpreting its header.
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "could not open ROM: " + path.string();
        return false;
    }
    const std::streamoff size = file.tellg();
    if (size < 0x150 || size > 16 * 1024 * 1024) {
        error = "file is not a supported Game Boy ROM size";
        return false;
    }

    // Read bytes first. The shared memory loader below owns all decoding and
    // validation metadata so desktop and browser hosts cannot drift.
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!file) {
        error = "could not read ROM: " + path.string();
        return false;
    }
    if (!load_rom(bytes, result, error))
        return false;
    result.path = path;
    return true;
}

bool load_rom(std::span<const std::uint8_t> bytes, Rom& result, std::string& error) {
    error.clear();
    if (bytes.size() < 0x150 || bytes.size() > 16U * 1024U * 1024U) {
        error = "data is not a supported Game Boy ROM size";
        return false;
    }

    // Own the supplied bytes and decode identity fields and checksums exactly
    // as the file loader does.
    Rom loaded;
    loaded.bytes.assign(bytes.begin(), bytes.end());
    loaded.digest = sha1(loaded.bytes);
    loaded.title = read_title(loaded.bytes);
    loaded.revision = loaded.bytes[0x14C];
    loaded.header_checksum_valid = loaded.bytes[0x14D] == header_checksum(loaded.bytes);
    const std::uint16_t stored = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(loaded.bytes[0x14E]) << 8U) | loaded.bytes[0x14F]);
    loaded.global_checksum_valid = stored == global_checksum(loaded.bytes);
    result = std::move(loaded);
    return true;
}

bool is_supported(const Rom& rom) {
    std::string error;
    return validate_supported(rom, error);
}

bool validate_supported(const Rom& rom, std::string& error) {
    // Give actionable metadata failures before the strict revision digest check.
    error.clear();
    if (rom.bytes.size() != supported_size) {
        error = "unsupported ROM size: expected 32768 bytes, received " +
                std::to_string(rom.bytes.size());
        return false;
    }
    if (rom.title != "TETRIS") {
        error = "unsupported ROM title: expected TETRIS, received " + rom.title;
        return false;
    }
    if (rom.revision != 1) {
        error = "unsupported ROM revision: expected 1, received " + std::to_string(rom.revision);
        return false;
    }
    if (rom.bytes[0x143] != 0 || rom.bytes[0x146] != 0 || rom.bytes[0x147] != 0 ||
        rom.bytes[0x148] != 0 || rom.bytes[0x149] != 0 || rom.bytes[0x14A] != 0 ||
        rom.bytes[0x14B] != 1) {
        error = "unsupported Tetris cartridge metadata";
        return false;
    }
    if (!rom.header_checksum_valid) {
        error = "ROM header checksum is invalid";
        return false;
    }
    if (!rom.global_checksum_valid) {
        error = "ROM global checksum is invalid";
        return false;
    }

    // Content offsets are only valid for this exact known revision.
    if (rom.digest != supported_sha1) {
        error = "unsupported ROM SHA-1: expected " + std::string(supported_sha1) + ", received " +
                rom.digest;
        return false;
    }
    return true;
}

} // namespace tetris::content
