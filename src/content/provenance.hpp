#pragma once

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace tetris::content {

struct RomSpan {
    std::size_t begin{};
    std::size_t end{};

    std::size_t size() const { return end - begin; }
};

struct Provenance {
    std::string id;
    std::string decoder;
    std::vector<RomSpan> spans;
    std::size_t expected_count{};
    std::size_t decoded_count{};
    bool validated{};

    std::size_t byte_count() const {
        std::size_t total = 0;
        for (const RomSpan span : spans)
            total += span.size();
        return total;
    }
};

inline Provenance provenance(std::string_view id, RomSpan span, std::string_view decoder,
                             std::size_t expected_count, std::size_t decoded_count) {
    return {
        .id = std::string(id),
        .decoder = std::string(decoder),
        .spans = {span},
        .expected_count = expected_count,
        .decoded_count = decoded_count,
        .validated = expected_count == decoded_count,
    };
}

inline Provenance provenance(std::string_view id, std::initializer_list<RomSpan> spans,
                             std::string_view decoder, std::size_t expected_count,
                             std::size_t decoded_count) {
    return {
        .id = std::string(id),
        .decoder = std::string(decoder),
        .spans = spans,
        .expected_count = expected_count,
        .decoded_count = decoded_count,
        .validated = expected_count == decoded_count,
    };
}

} // namespace tetris::content
