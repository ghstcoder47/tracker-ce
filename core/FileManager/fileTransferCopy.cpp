#include "fileTransferCopy.h"

namespace FileTransferInternal
{
bool copyFile(FileSystem* fileSystem,
              const char* srcFilepath,
              const char* dstFilepath,
              uint8_t* buffer,
              size_t bufferSize,
              FileSystem::FileHandle* sourceHandleStorage,
              FileSystem::FileHandle* destinationHandleStorage)
{
    if (!fileSystem->exists(srcFilepath) || bufferSize == 0)
    {
        return false;
    }

    if (fileSystem->exists(dstFilepath))
    {
        fileSystem->remove(dstFilepath);
    }

    bool result = false;
    bool destinationOpened = false;
    FileSystem::FileHandle sourceHandle = nullptr;
    FileSystem::FileHandle destinationHandle = nullptr;

    auto [isSourceOpen, openedSourceHandle] =
      fileSystem->openFile(srcFilepath, FileSystem::FileOpenMode::kRead, sourceHandleStorage);
    sourceHandle = openedSourceHandle;

    if (isSourceOpen)
    {
        auto [isDestinationOpen, openedDestinationHandle] = fileSystem->openFile(
          dstFilepath,
          FileSystem::FileOpenMode::kWriteCreateNew,
          destinationHandleStorage);
        destinationHandle = openedDestinationHandle;
        destinationOpened = isDestinationOpen;

        if (isDestinationOpen)
        {
            const size_t sourceSize = fileSystem->getFileSize(sourceHandle);
            size_t bytesCopied = 0;
            result = true;

            while (bytesCopied < sourceSize)
            {
                const size_t bytesRemaining = sourceSize - bytesCopied;
                const size_t bytesToRead =
                  bytesRemaining < bufferSize ? bytesRemaining : bufferSize;
                const auto readResult =
                  fileSystem->readFile(buffer, bytesToRead, sourceHandle);

                if (readResult <= 0 || static_cast<size_t>(readResult) > bytesToRead)
                {
                    result = false;
                    break;
                }

                const size_t bytesRead = static_cast<size_t>(readResult);
                size_t bytesWritten = 0;
                while (bytesWritten < bytesRead)
                {
                    const size_t bytesToWrite = bytesRead - bytesWritten;
                    const auto writeResult = fileSystem->writeFile(
                      buffer + bytesWritten, bytesToWrite, destinationHandle);

                    if (writeResult <= 0 || static_cast<size_t>(writeResult) > bytesToWrite)
                    {
                        result = false;
                        break;
                    }

                    bytesWritten += static_cast<size_t>(writeResult);
                }

                if (!result)
                {
                    break;
                }

                bytesCopied += bytesRead;
            }
        }
    }

    if (sourceHandle != nullptr)
    {
        fileSystem->closeFile(sourceHandle, false);
    }
    if (destinationHandle != nullptr)
    {
        fileSystem->closeFile(destinationHandle, false);
    }
    if (destinationOpened && !result)
    {
        fileSystem->remove(dstFilepath);
    }

    return result;
}
}
