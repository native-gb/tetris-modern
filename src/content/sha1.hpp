#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace tetris::content {

std::string sha1(std::span<const std::uint8_t> bytes);

} // namespace tetris::content
