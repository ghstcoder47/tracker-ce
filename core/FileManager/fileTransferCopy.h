#ifndef FILEMANAGER_FILETRANSFERCOPY_H_
#define FILEMANAGER_FILETRANSFERCOPY_H_

#include "Platform.h"

#include <stddef.h>
#include <stdint.h>

namespace FileTransferInternal
{
bool copyFile(FileSystem* fileSystem,
              const char* srcFilepath,
              const char* dstFilepath,
              uint8_t* buffer,
              size_t bufferSize,
              FileSystem::FileHandle* sourceHandleStorage,
              FileSystem::FileHandle* destinationHandleStorage);
}

#endif /* FILEMANAGER_FILETRANSFERCOPY_H_ */
