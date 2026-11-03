#include "instrumentFileValidation.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <vector>

namespace
{
constexpr uint16_t kCurrentInstrumentSize = 372;
constexpr uint8_t kCurrentVersion = 9;
constexpr uint32_t kCurrentMetadataSize = 16 + kCurrentInstrumentSize;

void writeUint16(std::vector<uint8_t>& data, size_t offset, uint16_t value)
{
    data[offset] = value & 0xFF;
    data[offset + 1] = value >> 8;
}

void writeUint32(std::vector<uint8_t>& data, size_t offset, uint32_t value)
{
    data[offset] = value & 0xFF;
    data[offset + 1] = (value >> 8) & 0xFF;
    data[offset + 2] = (value >> 16) & 0xFF;
    data[offset + 3] = value >> 24;
}

std::vector<uint8_t> makeFile(uint8_t version,
                              uint16_t instrumentSize,
                              uint32_t payloadSize,
                              bool includeCrc)
{
    const uint32_t metadataSize = 16 + instrumentSize;
    std::vector<uint8_t> data(metadataSize + (includeCrc ? 4 : 0) + payloadSize);
    data[0] = 'T';
    data[1] = 'I';
    writeUint16(data, 2, 1);
    memset(data.data() + 8, version, 4);
    writeUint16(data, 12, instrumentSize);
    for (uint32_t i = 16; i < metadataSize; i++)
    {
        data[i] = (i * 37) & 0xFF;
    }
    if (includeCrc)
    {
        writeUint32(data,
                    metadataSize,
                    InstrumentFileValidation::calculateCrc32(data.data(), metadataSize));
    }
    return data;
}

bool validate(const std::vector<uint8_t>& data,
              uint32_t loadedSize,
              InstrumentFileValidation::Result& result)
{
    return InstrumentFileValidation::validate(data.data(),
                                              loadedSize,
                                              data.size(),
                                              kCurrentInstrumentSize,
                                              kCurrentVersion,
                                              result);
}
}

int main()
{
    InstrumentFileValidation::Result result;
    const uint8_t crcTest[] = "123456789";
    assert(InstrumentFileValidation::calculateCrc32(crcTest, sizeof(crcTest) - 1) == 0xCBF43926);

    auto current = makeFile(kCurrentVersion, kCurrentInstrumentSize, 64, true);
    assert(validate(current, 392, result));
    assert(result.dataOffset == 392 && result.hasCrc);

    writeUint32(current, kCurrentMetadataSize, 0);
    assert(validate(current, 392, result));

    auto legacy = makeFile(7, kCurrentInstrumentSize - 4, 64, true);
    assert(validate(legacy, 392, result));
    assert(result.dataOffset == 388 && result.hasCrc);

    auto crcMissing = makeFile(7, kCurrentInstrumentSize - 4, 0, false);
    assert(validate(crcMissing, crcMissing.size(), result));
    assert(result.dataOffset == crcMissing.size() && !result.hasCrc);

    auto corrupted = makeFile(kCurrentVersion, kCurrentInstrumentSize, 64, true);
    corrupted[100] ^= 0x80;
    assert(!validate(corrupted, 392, result));

    auto future = makeFile(kCurrentVersion + 1, kCurrentInstrumentSize, 64, true);
    assert(!validate(future, 392, result));

    auto oversized = makeFile(kCurrentVersion, kCurrentInstrumentSize + 4, 64, true);
    assert(!validate(oversized, 396, result));

    auto truncatedMetadata = makeFile(kCurrentVersion, kCurrentInstrumentSize, 0, true);
    assert(!validate(truncatedMetadata, kCurrentMetadataSize - 1, result));

    auto truncatedCrc = makeFile(kCurrentVersion, kCurrentInstrumentSize, 0, true);
    truncatedCrc.resize(kCurrentMetadataSize + 2);
    assert(!validate(truncatedCrc, truncatedCrc.size(), result));

    auto inconsistentVersion = makeFile(kCurrentVersion, kCurrentInstrumentSize, 0, true);
    inconsistentVersion[9] = kCurrentVersion - 1;
    assert(!validate(inconsistentVersion, inconsistentVersion.size(), result));

    return 0;
}
