#include "CrcCalculatorImpl.h"
#include "polyassert.h"

uint32_t CrcCalculatorImpl::calculate32bitCrc(const uint8_t* data, const uint16_t datalen)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint16_t byte = 0; byte < datalen; byte++)
    {
        crc ^= data[byte];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1U));
        }
    }

    return ~crc;
}
