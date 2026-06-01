#pragma once

#include <array>
#include <memory>

#include "audio/audio_analyzer.h"
#include "audio/audio_capture.h"

namespace audio {

struct Context {
    Context();
    ~Context();

    bool enabled = false;
    std::unique_ptr<AudioCapture> capture;
    std::unique_ptr<AudioAnalyzer> analyzer;
    std::array<float, kMaxBands> bands{};
    int sampleRate = 48000;
    int numBands = 96;
    float smoothing = 0.78f;
};

bool Init(Context& ctx, int sampleRate, int numBands, float smoothing, float sensitivity = 2.0f);
void Tick(Context& ctx);
void Shutdown(Context& ctx);

// Get current band snapshot (thread-safe copy)
const std::array<float, kMaxBands>& GetBands(const Context& ctx);
void UpdateBands(Context& ctx);

} // namespace audio
