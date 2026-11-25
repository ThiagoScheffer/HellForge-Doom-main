#include "../platform/monster_eye_damage.h"
#include <cstdio>
#include <cstdlib>

static void check(bool condition, const char* failure) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", failure);
    std::exit(1);
}

int main() {
    constexpr int width = 8, height = 10, count = width * height;
    const doom_eye_damage::PaintColors shades{1,2,3,4,5,6};
    std::vector<uint8_t> original(count * 2, 120), mask(count);
    original[(2 * width + 2) * 2 + 1] = 0;
    const auto savedOriginal = original;
    auto source = original;
    source[(3 * width + 3) * 2 + 1] = 0;
    auto result = source;
    auto paint = [&](int w, int h) { return doom_eye_damage::paint(result, original, mask, w, h, shades); };
    check(!paint(0,height) && !paint(width,-1) && !paint(257,height), "invalid dimensions");
    result.pop_back();
    check(!paint(width,height), "invalid destination size");
    result = source;
    original.pop_back();
    check(!paint(width,height), "invalid original size");
    original = savedOriginal;
    mask.pop_back();
    check(!paint(width,height), "invalid mask size");
    mask.push_back(0);
    check(!paint(width,height) && result == source, "empty mask");

    for (int y = 1; y < 6; ++y)
        for (int x = 1; x < 5; ++x) mask[y * width + x] = 1;
    mask[8 * width + 1] = 1;
    check(paint(width,height) && result != source, "visible palette changes");
    check(original == savedOriginal, "original pixels preserved");
    for (int i = 0; i < count; ++i) {
        check(result[i * 2 + 1] == source[i * 2 + 1], "coverage preserved");
        if (!mask[i] || !original[i * 2 + 1] || !source[i * 2 + 1])
            check(result[i * 2] == source[i * 2], "unmasked and transparent pixels preserved");
    }
    check(result[(3 * width + 1) * 2] == shades.socketDark, "contiguous run has a closure seam");
    check(result[(8 * width + 1) * 2] == shades.bloodDark, "disconnected profile gets a local dark mark");
    check(result[(7 * width + 1) * 2] == source[(7 * width + 1) * 2], "gap stays unpainted");
    auto repeated = source;
    check(doom_eye_damage::paint(repeated, original, mask, width, height, shades) && repeated == result,
          "painting is deterministic");
    check(!paint(width,height), "identical destination does not need another upload");
}
