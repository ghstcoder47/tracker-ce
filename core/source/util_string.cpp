#include "util_string.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

bool compareStrings(const char* a, const char* b)
{
    size_t aLength = std::strlen(a);
    size_t bLength = std::strlen(b);

    while ((*a != 0) && (*b != 0))
    {
        bool isDigitA = std::isdigit(*a);
        bool isDigitB = std::isdigit(*b);

        if (isDigitA && isDigitB)
        {
            int intA = std::atoi(a);
            int intB = std::atoi(b);

            if (intA == intB)
            {
                while (std::isdigit(*a))
                {
                    a++;
                    b++;
                }
            }
            else
            {
                return intA < intB;
            }
        }
        else if (isDigitA != isDigitB)
        {
            return isDigitA;
        }
        else
        {
            char tempA = std::tolower(*a);
            char tempB = std::tolower(*b);

            if (tempA == tempB)
            {
                a++;
                b++;
            }
            else
            {
                return tempA < tempB;
            }
        }
    }

    return aLength <= bLength;
}

bool hasFileExtension(const char* filename, const char* extension)
{
    if (filename == nullptr || extension == nullptr)
    {
        return false;
    }

    size_t filenameLength = std::strlen(filename);
    size_t extensionLength = std::strlen(extension);

    if (extensionLength == 0 || filenameLength <= extensionLength ||
        filename[filenameLength - extensionLength - 1] != '.')
    {
        return false;
    }

    const char* filenameExtension = filename + filenameLength - extensionLength;
    for (size_t idx = 0; idx < extensionLength; idx++)
    {
        if (std::tolower(static_cast<unsigned char>(filenameExtension[idx])) !=
            std::tolower(static_cast<unsigned char>(extension[idx])))
        {
            return false;
        }
    }

    return true;
}

void polystrncpy(char* dest, const char* src, int destSize)
{
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = 0;
}
