#pragma once
#include <array>
#include <mutex>
#include <vector>

namespace audio {

constexpr int kFftSize = 1024;
constexpr int kMaxBands = 96;

class AudioAnalyzer {
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    void Init(int sampleRate, int bands, float sensitivity = 2.0f, float smoothing = 0.78f);

    // Feed PCM float mono samples
    void FeedSamples(const float* samples, int count);

    // Process FFT - call after feeding samples, before GetBands
    void Process();

    // Get current band snapshot (thread-safe)
    std::array<float, kMaxBands> GetBands() const;

    // Force decay when audio is silent
    void Decay(float factor = 0.92f);

    // Reset state
    void Reset();

private:
    void ApplyHannWindow();
    void CompressToBands();
    void ApplySmoothing();
    float MapToLogBand(int fftBin, int totalBins, int numBands);

    int sampleRate_ = 48000;
    int numBands_ = 96;
    float sensitivity_ = 2.0f;
    float smoothing_ = 0.78f;
    int writePos_ = 0;
    bool filled_ = false;

    std::vector<float> ringBuffer_;       // size = kFftSize
    std::vector<float> fftInput_;         // size = kFftSize
    std::vector<float> hannWindow_;       // precomputed
    std::vector<float> currentBands_;     // raw bands
    std::array<float, kMaxBands> bands_;  // smoothed output

    // KISS FFT state (opaque pointers)
    void* fftCfg_ = nullptr;   // kiss_fftr_cfg
    void* fftCfg_f = nullptr;  // kiss_fftr_cfg for forward

    mutable std::mutex mutex_;
};

} // namespace audio
