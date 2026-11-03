#include "wavHeaderReader.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <tuple>
#include <utility>
#include <vector>

namespace {
class FakeFileSystem : public FileSystem {
public:
  explicit FakeFileSystem(std::vector<uint8_t> contents)
      : contents(std::move(contents)) {}

  FileHandle handle() { return this; }

  void failReadOnCall(size_t call) { failedReadCall = call; }

  void failSeeks() { seeksFail = true; }

  size_t position() const { return currentPosition; }

  bool isSdCardInserted() override { return true; }

  void setMountEventCallback(CardStateChangeCallback) override {}

  bool exists(const char *) override { return true; }

  bool rename(const char *, const char *) override { return false; }

  bool mkdir(uint8_t, const char *, bool) override { return false; }

  bool remove(const char *) override { return false; }

  bool removeDirWithFiles(const char *) override { return false; }

  bool removeOnlyDirFiles(const char *) override { return false; }

  size_t getFileSize(const char *) override { return contents.size(); }

  size_t getFileSize(FileHandle) override { return contents.size(); }

  size_t getFileAvailable(FileHandle) override {
    return contents.size() - currentPosition;
  }

  std::tuple<bool, FileHandle> openFile(const char *, FileOpenMode,
                                        FileHandle *) override {
    return {true, handle()};
  }

  size_t readFile(void *destination, size_t size, FileHandle file) override {
    if (file != handle()) {
      return 0;
    }

    readCalls++;
    size_t available = contents.size() - currentPosition;
    size_t bytesToRead = std::min(size, available);
    if (readCalls == failedReadCall && bytesToRead > 0) {
      bytesToRead--;
    }

    memcpy(destination, contents.data() + currentPosition, bytesToRead);
    currentPosition += bytesToRead;
    return bytesToRead;
  }

  size_t writeFile(const void *, size_t, FileHandle) override { return 0; }

  bool seekFileCur(int offset, FileHandle file) override {
    if (file != handle() || seeksFail) {
      return false;
    }

    int64_t nextPosition = (int64_t)currentPosition + offset;
    if (nextPosition < 0 || (uint64_t)nextPosition > contents.size()) {
      return false;
    }

    currentPosition = (size_t)nextPosition;
    return true;
  }

  bool seekFile(uint32_t position, FileHandle file) override {
    if (file != handle() || seeksFail || position > contents.size()) {
      return false;
    }

    currentPosition = position;
    return true;
  }

  void closeFile(FileHandle, bool) override {}
  void endFile(FileHandle) override {}

  bool isFileOpen(FileHandle file) override { return file == handle(); }

  bool enableFastSeek(FileHandle, uint32_t *, uint32_t, uint32_t *,
                      uint32_t *) override {
    return false;
  }

  bool readSectorsAsync(uint8_t *, uint32_t, uint32_t) override {
    return false;
  }

  bool pollAsyncRead(bool *) override { return false; }

  bool waitAsyncRead() override { return false; }

  bool isAsyncReadBusy() override { return false; }

  int createInstrumentsList(const char *, char **, uint8_t,
                            uint32_t *) override {
    return 0;
  }

  int countContainedFiles(const char *) override { return 0; }

  int createFilesList(const char *, uint8_t, char **, uint8_t, uint16_t,
                      enListFileFilter, uint16_t, uint16_t,
                      enListFileFilter) override {
    return 0;
  }

  void listDirContents(const char *, DirContentType,
                       std::function<bool(const char *)>, char *, int, int,
                       int &, char **) override {}

  int readNbFiles(const char *) override { return 0; }

  bool nesDir_open(const char *) override { return false; }

  bool nesDir_readItem(char *, uint8_t *, uint8_t *, uint32_t *) override {
    return false;
  }

  void nesDir_close() override {}

private:
  std::vector<uint8_t> contents;
  size_t currentPosition = 0;
  size_t readCalls = 0;
  size_t failedReadCall = 0;
  bool seeksFail = false;
};

void appendId(std::vector<uint8_t> &bytes, const char *id) {
  bytes.insert(bytes.end(), id, id + 4);
}

void appendLe16(std::vector<uint8_t> &bytes, uint16_t value) {
  bytes.push_back(value & 0xFF);
  bytes.push_back(value >> 8);
}

void appendLe32(std::vector<uint8_t> &bytes, uint32_t value) {
  bytes.push_back(value & 0xFF);
  bytes.push_back((value >> 8) & 0xFF);
  bytes.push_back((value >> 16) & 0xFF);
  bytes.push_back(value >> 24);
}

void setLe32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value) {
  bytes[offset] = value & 0xFF;
  bytes[offset + 1] = (value >> 8) & 0xFF;
  bytes[offset + 2] = (value >> 16) & 0xFF;
  bytes[offset + 3] = value >> 24;
}

void appendChunk(std::vector<uint8_t> &bytes, const char *id,
                 const std::vector<uint8_t> &payload) {
  appendId(bytes, id);
  appendLe32(bytes, payload.size());
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  if ((payload.size() & 1U) != 0) {
    bytes.push_back(0);
  }
}

std::vector<uint8_t> fmtPayload(uint16_t format = wavFormatPCM,
                                uint16_t bitsPerSample = 16) {
  std::vector<uint8_t> payload;
  uint16_t channels = 1;
  uint32_t sampleRate = 44100;
  uint16_t blockAlign = channels * bitsPerSample / 8;
  appendLe16(payload, format);
  appendLe16(payload, channels);
  appendLe32(payload, sampleRate);
  appendLe32(payload, sampleRate * blockAlign);
  appendLe16(payload, blockAlign);
  appendLe16(payload, bitsPerSample);
  return payload;
}

std::vector<uint8_t> riffPrefix() {
  std::vector<uint8_t> bytes;
  appendId(bytes, "RIFF");
  appendLe32(bytes, 0);
  appendId(bytes, "WAVE");
  return bytes;
}

void finishRiff(std::vector<uint8_t> &bytes) {
  setLe32(bytes, 4, bytes.size() - 8);
}

std::vector<uint8_t> canonicalWav(uint16_t format = wavFormatPCM,
                                  uint16_t bitsPerSample = 16) {
  std::vector<uint8_t> bytes = riffPrefix();
  appendChunk(bytes, "fmt ", fmtPayload(format, bitsPerSample));
  appendChunk(bytes, "data", {0x00, 0x00, 0x01, 0x00});
  finishRiff(bytes);
  return bytes;
}

bool isZeroed(const strWavFileHeader &header) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&header);
  return std::all_of(bytes, bytes + sizeof(header),
                     [](uint8_t value) { return value == 0; });
}

bool expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
  }
  return condition;
}

bool expectFailure(std::vector<uint8_t> bytes, const char *name,
                   size_t failedReadCall = 0, bool failedSeek = false) {
  FakeFileSystem fileSystem(std::move(bytes));
  if (failedReadCall != 0) {
    fileSystem.failReadOnCall(failedReadCall);
  }
  if (failedSeek) {
    fileSystem.failSeeks();
  }

  strWavFileHeader header;
  memset(&header, 0xA5, sizeof(header));
  bool result = readHeader(&header, &fileSystem, fileSystem.handle());
  return expect(!result, name) &&
         expect(isZeroed(header), "failure output must be zeroed");
}

bool testCanonicalWav() {
  FakeFileSystem fileSystem(canonicalWav());
  strWavFileHeader header;
  bool result = readHeader(&header, &fileSystem, fileSystem.handle());
  return expect(result, "canonical WAV should parse") &&
         expect(header.AudioFormat == wavFormatPCM, "canonical format") &&
         expect(header.numChannels == 1, "canonical channels") &&
         expect(header.sampleRate == 44100, "canonical sample rate") &&
         expect(header.bitsPerSample == 16, "canonical bit depth") &&
         expect(header.subchunk2Size == 4, "canonical data size") &&
         expect(fileSystem.position() == 44, "canonical data position");
}

bool testFloatWav() {
  FakeFileSystem fileSystem(canonicalWav(wavFormatFloat, 32));
  strWavFileHeader header;
  bool result = readHeader(&header, &fileSystem, fileSystem.handle());
  return expect(result, "float WAV should parse") &&
         expect(header.AudioFormat == wavFormatFloat, "float format") &&
         expect(header.bitsPerSample == 32, "float bit depth");
}

bool testOddUnknownChunks() {
  std::vector<uint8_t> bytes = riffPrefix();
  appendChunk(bytes, "JUNK", {1, 2, 3});
  appendChunk(bytes, "fmt ", fmtPayload());
  appendChunk(bytes, "LIST", {4});
  appendChunk(bytes, "data", {0, 0, 0, 0});
  finishRiff(bytes);

  FakeFileSystem fileSystem(std::move(bytes));
  strWavFileHeader header;
  bool result = readHeader(&header, &fileSystem, fileSystem.handle());
  return expect(result, "odd-sized unknown chunks should parse") &&
         expect(header.subchunk2Size == 4, "odd chunk data size") &&
         expect(fileSystem.position() == 66, "odd chunk data position");
}

bool testTruncatedHeaders() {
  std::vector<uint8_t> riff = canonicalWav();
  riff.resize(10);

  std::vector<uint8_t> fmt = riffPrefix();
  appendId(fmt, "fmt ");
  finishRiff(fmt);

  std::vector<uint8_t> data = riffPrefix();
  appendChunk(data, "fmt ", fmtPayload());
  appendId(data, "data");
  finishRiff(data);

  return expectFailure(std::move(riff), "truncated RIFF header") &&
         expectFailure(std::move(fmt), "truncated fmt header") &&
         expectFailure(std::move(data), "truncated data header");
}

bool testSmallFmtChunk() {
  std::vector<uint8_t> bytes = riffPrefix();
  appendChunk(bytes, "fmt ", std::vector<uint8_t>(14));
  appendChunk(bytes, "data", {0, 0});
  finishRiff(bytes);
  return expectFailure(std::move(bytes), "fmt chunk smaller than 16 bytes");
}

bool testOversizedChunk() {
  std::vector<uint8_t> bytes = riffPrefix();
  appendId(bytes, "JUNK");
  appendLe32(bytes, UINT32_MAX);
  finishRiff(bytes);
  return expectFailure(std::move(bytes), "oversized chunk length");
}

bool testReadAndSeekErrors() {
  std::vector<uint8_t> oddChunk = riffPrefix();
  appendChunk(oddChunk, "JUNK", {1});
  appendChunk(oddChunk, "fmt ", fmtPayload());
  appendChunk(oddChunk, "data", {0, 0});
  finishRiff(oddChunk);

  return expectFailure(canonicalWav(), "short read", 2) &&
         expectFailure(std::move(oddChunk), "seek error", 0, true);
}
} // namespace

int main() {
  bool passed = testCanonicalWav() && testFloatWav() &&
                testOddUnknownChunks() && testTruncatedHeaders() &&
                testSmallFmtChunk() && testOversizedChunk() &&
                testReadAndSeekErrors();
  if (!passed) {
    return 1;
  }

  std::cout << "wavHeaderReader tests passed\n";
  return 0;
}
