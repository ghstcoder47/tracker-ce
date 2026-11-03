#include "patternFileValidation.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

using namespace PatternFileValidation;

static void storeCrc(uint8_t* data, size_t offset, uint32_t crc)
{
    memcpy(data + offset, &crc, sizeof(crc));
}

int main()
{
    uint8_t pattern[k16TracksPatternSize]{};
    Layout layout;

    assert(resolveLayout(1, k8TracksPatternSizeV5, k8TracksPatternSizeV5, layout));
    assert(resolveLayout(5, k12TracksPatternSizeV5, k12TracksPatternSizeV5, layout));
    assert(resolveLayout(5, k16TracksPatternSizeV5, k16TracksPatternSizeV5, layout));
    assert(resolveLayout(6, k8TracksPatternSize, k8TracksPatternSize, layout));
    assert(resolveLayout(6, k12TracksPatternSize, k12TracksPatternSize, layout));
    assert(resolveLayout(6, k16TracksPatternSize, k16TracksPatternSize, layout));
    assert(layout.hasCrc);
    assert(layout.crcDataSize == k16TracksPatternSize - kCrcSize);

    constexpr uint32_t crc = 0x12345678;
    storeCrc(pattern, layout.crcDataSize, crc);
    assert(isCrcValid(pattern, layout, crc));
    assert(!isCrcValid(pattern, layout, crc + 1));

    storeCrc(pattern, layout.crcDataSize, 0);
    assert(!isCrcValid(pattern, layout, crc));

    assert(resolveLayout(5, k12TracksPatternSizeV5, k12TracksPatternSizeV5, layout));
    storeCrc(pattern, layout.crcDataSize, 0);
    assert(isCrcValid(pattern, layout, crc));
    storeCrc(pattern, layout.crcDataSize, crc + 1);
    assert(!isCrcValid(pattern, layout, crc));

    assert(resolveLayout(5, k8TracksPatternSizeV5, k8TracksPatternSizeV5 - kCrcSize, layout));
    assert(!layout.hasCrc);
    assert(isCrcValid(pattern, layout, crc));

    assert(!resolveLayout(6, k8TracksPatternSize, k8TracksPatternSize - kCrcSize, layout));
    assert(!resolveLayout(5, k12TracksPatternSizeV5, k12TracksPatternSizeV5 - 1, layout));
    assert(!resolveLayout(5, k12TracksPatternSize, k12TracksPatternSize, layout));
    assert(!resolveLayout(6, k16TracksPatternSize, k16TracksPatternSize - 8, layout));
}
