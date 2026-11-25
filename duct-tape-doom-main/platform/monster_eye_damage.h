#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace doom_eye_damage {

struct PaintColors {
    uint8_t socketDark;
    uint8_t lidDark;
    uint8_t lidMid;
    uint8_t bloodDark;
    uint8_t bloodMid;
    uint8_t bloodBright;
};

inline bool paint(std::vector<uint8_t>& destination, const std::vector<uint8_t>& original,
                  const std::vector<uint8_t>& allowed, int width, int height,
                  const PaintColors& colors)
{
    if (width < 1 || height < 1 || width > 256 || height > 256)
        return false;
    const auto count = std::size_t(width) * height;
    if (destination.size() != count * 2 || original.size() != count * 2 || allowed.size() != count)
        return false;

    auto eligible = [&](int x, int y) {
        const auto at = std::size_t(y) * width + x;
        return allowed[at] && original[at * 2 + 1] && destination[at * 2 + 1];
    };
    bool changed = false;
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height;) {
            if (!eligible(x, y)) {
                ++y;
                continue;
            }
            const int top = y;
            do { ++y; } while (y < height && eligible(x, y));
            // Fit the lid to each contiguous run so separate regions in the
            // same column cannot influence each other's seam or staining.
            const int span = y - top;
            const int seam = top + span / 2;
            for (int row = top; row < y; ++row) {
                uint8_t shade = colors.bloodDark;
                if (span >= 3) {
                    if (row < seam)
                        shade = row == seam - 1 ? colors.lidDark : colors.lidMid;
                    else if (row == seam)
                        shade = colors.socketDark;
                    else if ((x + top) % 5 == 0 && row == y - 1 && span >= 5)
                        shade = colors.bloodBright;
                    else
                        shade = (x * 3 + row * 5) % 7 < 3 ? colors.bloodMid : colors.bloodDark;
                }
                auto& pixel = destination[(std::size_t(row) * width + x) * 2];
                changed |= pixel != shade;
                pixel = shade;
            }
        }
    }
    return changed;
}

} // namespace doom_eye_damage
