#include "content/sha1.hpp"

#include <array>
#include <bit>
#include <iomanip>
#include <sstream>
#include <vector>

namespace tetris::content {
namespace {

std::uint32_t word_at(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

void process(std::array<std::uint32_t, 5>& hash, const std::vector<std::uint8_t>& message,
             std::size_t block) {
    // Expand one 512-bit block into the 80-word SHA-1 schedule.
    std::array<std::uint32_t, 80> words{};
    for (std::size_t index = 0; index < 16; ++index)
        words[index] = word_at(message, block + index * 4);
    for (std::size_t index = 16; index < words.size(); ++index) {
        words[index] = std::rotl(
            words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
    }

    // Apply all four SHA-1 round functions to the working hash.
    auto [a, b, c, d, e] = hash;
    for (std::size_t index = 0; index < words.size(); ++index) {
        std::uint32_t function{};
        std::uint32_t constant{};
        if (index < 20) {
            function = (b & c) | ((~b) & d);
            constant = 0x5A827999U;
        } else if (index < 40) {
            function = b ^ c ^ d;
            constant = 0x6ED9EBA1U;
        } else if (index < 60) {
            function = (b & c) | (b & d) | (c & d);
            constant = 0x8F1BBCDCU;
        } else {
            function = b ^ c ^ d;
            constant = 0xCA62C1D6U;
        }
        const std::uint32_t next = std::rotl(a, 5) + function + e + constant + words[index];
        e = d;
        d = c;
        c = std::rotl(b, 30);
        b = a;
        a = next;
    }

    // Accumulate this block into the digest state.
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
}

} // namespace

std::string sha1(std::span<const std::uint8_t> bytes) {
    // Append SHA-1 padding and the original big-endian bit length.
    std::vector<std::uint8_t> message(bytes.begin(), bytes.end());
    const std::uint64_t bits = static_cast<std::uint64_t>(message.size()) * 8U;
    message.push_back(0x80U);
    while (message.size() % 64 != 56)
        message.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8)
        message.push_back(static_cast<std::uint8_t>(bits >> static_cast<unsigned int>(shift)));

    // Process every complete padded block from the standard initial state.
    std::array<std::uint32_t, 5> hash = {
        0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U,
    };
    for (std::size_t block = 0; block < message.size(); block += 64)
        process(hash, message, block);

    // Format the five digest words as a lowercase hexadecimal identity.
    std::ostringstream text;
    text << std::hex << std::setfill('0');
    for (const std::uint32_t word : hash)
        text << std::setw(8) << word;
    return text.str();
}

} // namespace tetris::content
