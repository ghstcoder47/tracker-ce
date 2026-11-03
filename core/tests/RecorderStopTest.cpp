#define TRACKERIMPL_H_

#include "../source/Recorder.cpp"
#include "mtAudioEngine.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
using Block = AudioRecordQueue::Block;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void resetAudioInput()
{
    engine.audioInput = AudioInput();
    mtConfig.general.lineInOutType = static_cast<uint8_t>(AudioStereoToMono::OutSignalType::stereo);
}

Recorder makeRecorder(int16_t* destination,
                      uint32_t destinationSize,
                      int destinationChannels,
                      int sourceChannels)
{
    Recorder recorder;
    recorder.setConfig({ destination,
                         destinationSize,
                         destinationChannels,
                         sourceChannels,
                         Recorder::Config::kI2s });
    recorder.record();
    for (auto& queue : engine.audioInput.i2sQueues)
    {
        queue.resetStats();
    }
    return recorder;
}

void stereoDestinationRetainsPairsAndDiscardsUnmatchedTail()
{
    resetAudioInput();
    std::array<int16_t, 18> destination;
    destination.fill(-1);
    Recorder recorder = makeRecorder(destination.data(), 16 * sizeof(int16_t), 2, 2);
    auto& left = engine.audioInput.i2sQueues[AudioInput::kLeft];
    auto& right = engine.audioInput.i2sQueues[AudioInput::kRight];
    left.enqueue({ 1, 2, 3, 4 });
    left.enqueue({ 9, 9, 9, 9 });
    right.enqueue({ 5, 6, 7, 8 });
    int routingChangesBeforeStop = engine.audioInput.stereoToMono.setCount;

    recorder.stop();

    expect(!recorder.isRecordingActive(), "stereo recording stops");
    expect(recorder.getLength() == AUDIO_BLOCK_SAMPLES, "one complete stereo pair is retained");
    expect(std::equal(destination.begin(), destination.begin() + 4, Block{ 1, 2, 3, 4 }.begin()),
           "left paired block is copied");
    expect(
      std::equal(destination.begin() + 8, destination.begin() + 12, Block{ 5, 6, 7, 8 }.begin()),
      "right paired block is copied");
    expect(destination[4] == -1 && destination[12] == -1 && destination[16] == -1 &&
             destination[17] == -1,
           "stereo destination bounds are preserved");
    expect(left.available() == 0 && right.available() == 0, "unmatched stereo tail is cleared");
    expect(left.readCount == 1 && right.readCount == 1, "only complete pairs are read");
    expect(left.releasedCount == 2 && right.releasedCount == 1, "all queued blocks are released");
    expect(engine.audioInput.stereoToMono.setCount == routingChangesBeforeStop + 1,
           "line-in routing is restored exactly once");
}

void monoDestinationMixesPairsAndDiscardsUnmatchedTail()
{
    resetAudioInput();
    std::array<int16_t, 10> destination;
    destination.fill(-1);
    Recorder recorder = makeRecorder(destination.data(), 8 * sizeof(int16_t), 1, 2);
    auto& left = engine.audioInput.i2sQueues[AudioInput::kLeft];
    auto& right = engine.audioInput.i2sQueues[AudioInput::kRight];
    left.enqueue({ 2, 4, 6, 8 });
    right.enqueue({ 4, 8, 12, 16 });
    right.enqueue({ 20, 20, 20, 20 });

    recorder.stop();

    expect(recorder.getLength() == AUDIO_BLOCK_SAMPLES,
           "one complete stereo-to-mono pair is retained");
    expect(std::equal(destination.begin(), destination.begin() + 4, Block{ 3, 6, 9, 12 }.begin()),
           "paired channels are mixed to mono");
    expect(destination[4] == -1 && destination[8] == -1 && destination[9] == -1,
           "mono destination bounds are preserved");
    expect(left.available() == 0 && right.available() == 0,
           "mono destination clears unmatched tail");
    expect(left.releasedCount == 1 && right.releasedCount == 2,
           "mono destination releases paired and unmatched blocks");
}

void monoSourceDrainsAllBlocks()
{
    resetAudioInput();
    std::array<int16_t, 14> destination;
    destination.fill(-1);
    Recorder recorder = makeRecorder(destination.data(), 12 * sizeof(int16_t), 1, 1);
    auto& left = engine.audioInput.i2sQueues[AudioInput::kLeft];
    left.enqueue({ 1, 2, 3, 4 });
    left.enqueue({ 5, 6, 7, 8 });

    recorder.stop();

    expect(recorder.getLength() == 8, "mono source drains every complete block");
    expect(std::equal(destination.begin(),
                      destination.begin() + 8,
                      std::array<int16_t, 8>{ 1, 2, 3, 4, 5, 6, 7, 8 }.begin()),
           "mono blocks are copied in order");
    expect(destination[8] == -1 && destination[12] == -1 && destination[13] == -1,
           "mono source destination bounds are preserved");
    expect(left.available() == 0 && left.releasedCount == 2, "mono source blocks are released");
}

void fullMemoryCallbackAndRoutingRunOnce()
{
    resetAudioInput();
    std::array<int16_t, 10> destination;
    destination.fill(-1);
    Recorder recorder = makeRecorder(destination.data(), 8 * sizeof(int16_t), 1, 1);
    auto& left = engine.audioInput.i2sQueues[AudioInput::kLeft];
    left.enqueue({ 1, 2, 3, 4 });
    left.enqueue({ 5, 6, 7, 8 });
    int callbackCount = 0;
    recorder.setFullMemoryCallback([&callbackCount]() { callbackCount++; });
    int routingChangesBeforeStop = engine.audioInput.stereoToMono.setCount;

    recorder.stop();

    expect(callbackCount == 1, "full-memory callback runs once");
    expect(recorder.getLength() == AUDIO_BLOCK_SAMPLES, "full-memory length remains consistent");
    expect(left.available() == 0 && left.releasedCount == 2,
           "full-memory stop clears blocks that no longer fit");
    expect(engine.audioInput.stereoToMono.setCount == routingChangesBeforeStop + 1,
           "full-memory stop restores line-in routing exactly once");
}
}

int main()
{
    stereoDestinationRetainsPairsAndDiscardsUnmatchedTail();
    monoDestinationMixesPairsAndDiscardsUnmatchedTail();
    monoSourceDrainsAllBlocks();
    fullMemoryCallbackAndRoutingRunOnce();
    std::cout << "Recorder stop regression tests passed\n";
    return 0;
}
