#ifndef TEST_MT_AUDIO_ENGINE_H_
#define TEST_MT_AUDIO_ENGINE_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

#define AUDIO_BLOCK_SAMPLES 4
#define MAX_16BIT           65535

class AudioRecordQueue
{
  public:
    using Block = std::array<int16_t, AUDIO_BLOCK_SAMPLES>;

    void begin()
    {
        clear();
        enabled = true;
    }

    void end() { enabled = false; }

    int available() const { return static_cast<int>(blocks.size()); }

    int16_t* readBuffer()
    {
        if (userBlock || blocks.empty())
        {
            return nullptr;
        }

        userBlock = blocks.front();
        blocks.pop_front();
        readCount++;
        return userBlock->data();
    }

    void freeBuffer()
    {
        if (!userBlock)
        {
            return;
        }

        userBlock.reset();
        releasedCount++;
    }

    void clear()
    {
        releasedCount += static_cast<int>(blocks.size());
        blocks.clear();
        if (userBlock)
        {
            userBlock.reset();
            releasedCount++;
        }
    }

    void enqueue(const Block& block) { blocks.push_back(block); }

    void resetStats()
    {
        readCount = 0;
        releasedCount = 0;
    }

    int readCount = 0;
    int releasedCount = 0;
    bool enabled = false;

  private:
    std::deque<Block> blocks;
    std::optional<Block> userBlock;
};

class AudioStereoToMono
{
  public:
    enum class OutSignalType
    {
        stereo,
        monoL,
        monoR
    };

    void setOutSignalType(OutSignalType type)
    {
        outputType = type;
        setCount++;
    }

    OutSignalType outputType = OutSignalType::stereo;
    int setCount = 0;
};

class AudioInput
{
  public:
    enum InputChannels
    {
        kLeft,
        kRight,
        kChannelsCount
    };

    AudioRecordQueue* getRecordingQueueUsb(InputChannels channel) { return &usbQueues[channel]; }
    AudioRecordQueue* getRecordingQueueI2s(InputChannels channel) { return &i2sQueues[channel]; }
    AudioStereoToMono* getStereoToMono() { return &stereoToMono; }

    AudioRecordQueue usbQueues[kChannelsCount];
    AudioRecordQueue i2sQueues[kChannelsCount];
    AudioStereoToMono stereoToMono;
};

class audioEngine
{
  public:
    AudioInput* getAudioInput() { return &audioInput; }

    AudioInput audioInput;
};

struct strMtConfig
{
    struct
    {
        uint8_t lineInOutType = static_cast<uint8_t>(AudioStereoToMono::OutSignalType::stereo);
    } general;
};

inline audioEngine engine;
inline strMtConfig mtConfig;

#endif
