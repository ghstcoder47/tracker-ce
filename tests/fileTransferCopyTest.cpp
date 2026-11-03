#include "fileTransferCopy.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{
class FakeFileSystem : public FileSystem
{
  public:
    struct Handle
    {
        std::string path;
        size_t position = 0;
        bool open = false;
    };

    void setSource(const std::string& path, const std::vector<uint8_t>& data)
    {
        files[path] = data;
    }

    const std::vector<uint8_t>& file(const std::string& path) const { return files.at(path); }

    bool isSdCardInserted() override { return true; }
    void setMountEventCallback(CardStateChangeCallback) override {}
    bool exists(const char* path) override { return files.contains(path); }
    bool rename(const char*, const char*) override { return false; }
    bool mkdir(uint8_t, const char*, bool) override { return false; }
    bool remove(const char* path) override
    {
        removedPaths.emplace_back(path);
        return files.erase(path) > 0;
    }
    bool removeDirWithFiles(const char*) override { return false; }
    bool removeOnlyDirFiles(const char*) override { return false; }
    size_t getFileSize(const char* path) override { return files.at(path).size(); }
    size_t getFileSize(FileHandle fileHandle) override
    {
        auto* handle = static_cast<Handle*>(fileHandle);
        return reportedSourceSize == kUseActualSize ? files.at(handle->path).size()
                                                    : reportedSourceSize;
    }
    size_t getFileAvailable(FileHandle fileHandle) override
    {
        auto* handle = static_cast<Handle*>(fileHandle);
        return files.at(handle->path).size() - handle->position;
    }

    std::tuple<bool, FileHandle> openFile(const char* filepath,
                                          FileOpenMode openMode,
                                          FileHandle* fileHandle) override
    {
        Handle* handle = nullptr;
        if (fileHandle != nullptr && *fileHandle != nullptr)
        {
            handle = static_cast<Handle*>(*fileHandle);
        }
        else
        {
            handles.push_back(std::make_unique<Handle>());
            handle = handles.back().get();
            if (fileHandle != nullptr)
            {
                *fileHandle = handle;
            }
        }

        handle->path = filepath;
        handle->position = 0;
        handle->open = false;

        if (openMode == kRead)
        {
            handle->open = files.contains(filepath);
        }
        else if (openMode == kWriteCreateNew)
        {
            files[filepath].clear();
            handle->open = true;
        }

        return {handle->open, handle};
    }

    size_t readFile(void* destination, size_t bufferLength, FileHandle fileHandle) override
    {
        readRequests.push_back(bufferLength);
        auto* handle = static_cast<Handle*>(fileHandle);

        if (readCallCount < readResults.size())
        {
            const size_t scriptedResult = readResults[readCallCount++];
            if (scriptedResult == std::numeric_limits<size_t>::max())
            {
                return scriptedResult;
            }
            bufferLength = std::min(bufferLength, scriptedResult);
        }
        else
        {
            readCallCount++;
        }

        const auto& source = files.at(handle->path);
        const size_t available = source.size() - handle->position;
        const size_t bytesRead = std::min(bufferLength, available);
        std::memcpy(destination, source.data() + handle->position, bytesRead);
        handle->position += bytesRead;
        return bytesRead;
    }

    size_t writeFile(const void* data, size_t dataSize, FileHandle fileHandle) override
    {
        writeRequests.push_back(dataSize);
        auto* handle = static_cast<Handle*>(fileHandle);
        size_t writeResult = dataSize;

        if (writeCallCount < writeResults.size())
        {
            writeResult = writeResults[writeCallCount];
        }
        writeCallCount++;

        if (writeResult == std::numeric_limits<size_t>::max() || writeResult > dataSize)
        {
            return writeResult;
        }

        auto& destination = files.at(handle->path);
        const auto* source = static_cast<const uint8_t*>(data);
        destination.insert(destination.end(), source, source + writeResult);
        handle->position += writeResult;
        return writeResult;
    }

    bool seekFileCur(int, FileHandle) override { return false; }
    bool seekFile(uint32_t, FileHandle) override { return false; }
    void closeFile(FileHandle fileHandle, bool) override
    {
        auto* handle = static_cast<Handle*>(fileHandle);
        handle->open = false;
        closeCount++;
    }
    void endFile(FileHandle) override {}
    bool isFileOpen(FileHandle fileHandle) override
    {
        return static_cast<Handle*>(fileHandle)->open;
    }
    bool enableFastSeek(FileHandle, uint32_t*, uint32_t, uint32_t*, uint32_t*) override
    {
        return false;
    }
    bool readSectorsAsync(uint8_t*, uint32_t, uint32_t) override { return false; }
    bool pollAsyncRead(bool*) override { return false; }
    bool waitAsyncRead() override { return false; }
    bool isAsyncReadBusy() override { return false; }
    int createInstrumentsList(const char*, char**, uint8_t, uint32_t*) override { return 0; }
    int countContainedFiles(const char*) override { return 0; }
    int createFilesList(const char*,
                        uint8_t,
                        char**,
                        uint8_t,
                        uint16_t,
                        enListFileFilter,
                        uint16_t,
                        uint16_t,
                        enListFileFilter) override
    {
        return 0;
    }
    void listDirContents(const char*,
                         DirContentType,
                         std::function<bool(const char*)>,
                         char*,
                         int,
                         int,
                         int&,
                         char**) override
    {}
    int readNbFiles(const char*) override { return 0; }
    bool nesDir_open(const char*) override { return false; }
    bool nesDir_readItem(char*, uint8_t*, uint8_t*, uint32_t*) override { return false; }
    void nesDir_close() override {}

    static constexpr size_t kUseActualSize = std::numeric_limits<size_t>::max() - 1;
    size_t reportedSourceSize = kUseActualSize;
    std::vector<size_t> readResults;
    std::vector<size_t> writeResults;
    std::vector<size_t> readRequests;
    std::vector<size_t> writeRequests;
    std::vector<std::string> removedPaths;
    size_t readCallCount = 0;
    size_t writeCallCount = 0;
    size_t closeCount = 0;

  private:
    std::map<std::string, std::vector<uint8_t>> files;
    std::vector<std::unique_ptr<Handle>> handles;
};

bool copy(FakeFileSystem& fileSystem, size_t bufferSize = 8)
{
    std::vector<uint8_t> buffer(bufferSize);
    FileSystem::FileHandle sourceHandle = nullptr;
    FileSystem::FileHandle destinationHandle = nullptr;
    return FileTransferInternal::copyFile(&fileSystem,
                                          "source.pts",
                                          "destination.pts",
                                          buffer.data(),
                                          buffer.size(),
                                          &sourceHandle,
                                          &destinationHandle);
}

#define CHECK(condition)                                                                          \
    do                                                                                            \
    {                                                                                             \
        if (!(condition))                                                                         \
        {                                                                                         \
            std::cerr << __func__ << ":" << __LINE__ << ": check failed: " #condition << '\n'; \
            return false;                                                                         \
        }                                                                                         \
    } while (false)

bool successfulMultiChunkCopy()
{
    FakeFileSystem fileSystem;
    const std::vector<uint8_t> source = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
    fileSystem.setSource("source.pts", source);

    CHECK(copy(fileSystem));
    CHECK(fileSystem.file("destination.pts") == source);
    CHECK((fileSystem.readRequests == std::vector<size_t>{8, 8, 3}));
    CHECK(fileSystem.closeCount == 2);
    return true;
}

bool earlyEofFailsAndRemovesDestination()
{
    FakeFileSystem fileSystem;
    fileSystem.setSource("source.pts", {1, 2, 3, 4});
    fileSystem.reportedSourceSize = 10;

    CHECK(!copy(fileSystem));
    CHECK(!fileSystem.exists("destination.pts"));
    CHECK(fileSystem.readCallCount == 2);
    CHECK(fileSystem.closeCount == 2);
    return true;
}

bool readErrorFailsAndRemovesDestination()
{
    FakeFileSystem fileSystem;
    fileSystem.setSource("source.pts", {1, 2, 3, 4, 5, 6, 7, 8, 9});
    fileSystem.readResults = {8, std::numeric_limits<size_t>::max()};

    CHECK(!copy(fileSystem));
    CHECK(!fileSystem.exists("destination.pts"));
    CHECK(fileSystem.readCallCount == 2);
    CHECK(fileSystem.closeCount == 2);
    return true;
}

bool partialWritesAreRetried()
{
    FakeFileSystem fileSystem;
    const std::vector<uint8_t> source = {1, 2, 3, 4, 5, 6};
    fileSystem.setSource("source.pts", source);
    fileSystem.writeResults = {2, 1, 3};

    CHECK(copy(fileSystem));
    CHECK(fileSystem.file("destination.pts") == source);
    CHECK((fileSystem.writeRequests == std::vector<size_t>{6, 4, 3}));
    CHECK(fileSystem.writeCallCount == 3);
    return true;
}

bool zeroWriteFailsAndRemovesDestination()
{
    FakeFileSystem fileSystem;
    fileSystem.setSource("source.pts", {1, 2, 3});
    fileSystem.writeResults = {0};

    CHECK(!copy(fileSystem));
    CHECK(!fileSystem.exists("destination.pts"));
    CHECK(fileSystem.writeCallCount == 1);
    CHECK(fileSystem.closeCount == 2);
    return true;
}

bool writeErrorFailsAndRemovesDestination()
{
    FakeFileSystem fileSystem;
    fileSystem.setSource("source.pts", {1, 2, 3});
    fileSystem.writeResults = {std::numeric_limits<size_t>::max()};

    CHECK(!copy(fileSystem));
    CHECK(!fileSystem.exists("destination.pts"));
    CHECK(fileSystem.writeCallCount == 1);
    CHECK(fileSystem.closeCount == 2);
    return true;
}
}

int main()
{
    const std::vector<std::pair<const char*, bool (*)()>> tests = {
      {"successful multi-chunk copy", successfulMultiChunkCopy},
      {"early EOF", earlyEofFailsAndRemovesDestination},
      {"read error", readErrorFailsAndRemovesDestination},
      {"partial writes", partialWritesAreRetried},
      {"zero write", zeroWriteFailsAndRemovesDestination},
      {"write error", writeErrorFailsAndRemovesDestination},
    };

    for (const auto& [name, test] : tests)
    {
        if (!test())
        {
            return 1;
        }
        std::cout << "[PASS] " << name << '\n';
    }

    return 0;
}
