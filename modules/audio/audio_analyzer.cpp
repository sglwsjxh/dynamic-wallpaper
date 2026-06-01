#include "audio/audio_analyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "third_party/kissfft/kiss_fftr.h"

extern "C" {
#include "third_party/kissfft/kiss_fft.c"
#include "third_party/kissfft/kiss_fftr.c"
}

namespace audio {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDefaultSensitivity = 20.0f;
constexpr float kDefaultSmoothing = 0.78f;
constexpr float kCompressionScale = 1.7f;

kiss_fftr_cfg AsFftCfg(void* cfg) {
    return reinterpret_cast<kiss_fftr_cfg>(cfg);
}

} // namespace

AudioAnalyzer::AudioAnalyzer() {
    bands_.fill(0.0f);
}

AudioAnalyzer::~AudioAnalyzer() {
    if (fftCfg_f) {
        kiss_fftr_free(AsFftCfg(fftCfg_f));
        fftCfg_f = nullptr;
    }
}

void AudioAnalyzer::Init(int sampleRate, int bands, float sensitivity) {
    std::lock_guard<std::mutex> lock(mutex_);

    sampleRate_ = sampleRate > 0 ? sampleRate : 48000;
    numBands_ = std::clamp(bands, 1, kMaxBands);
    sensitivity_ = sensitivity > 0.0f ? sensitivity : kDefaultSensitivity;
    writePos_ = 0;
    filled_ = false;

    ringBuffer_.assign(kFftSize, 0.0f);
    fftInput_.assign(kFftSize, 0.0f);
    hannWindow_.resize(kFftSize);
    currentBands_.assign(numBands_, 0.0f);
    bands_.fill(0.0f);

    for (int i = 0; i < kFftSize; ++i) {
        float phase = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(kFftSize - 1);
        hannWindow_[i] = 0.5f * (1.0f - std::cos(phase));
    }

    if (fftCfg_f) {
        kiss_fftr_free(AsFftCfg(fftCfg_f));
        fftCfg_f = nullptr;
    }
    fftCfg_f = kiss_fftr_alloc(kFftSize, 0, nullptr, nullptr);
}

void AudioAnalyzer::FeedSamples(const float* samples, int count) {
    if (!samples || count <= 0) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (ringBuffer_.empty()) ringBuffer_.assign(kFftSize, 0.0f);

    for (int i = 0; i < count; ++i) {
        ringBuffer_[writePos_] = samples[i];
        writePos_ = (writePos_ + 1) % kFftSize;
        if (writePos_ == 0) filled_ = true;
    }
}

void AudioAnalyzer::Process() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!filled_ || !fftCfg_f) return;

    if (fftInput_.size() != kFftSize) fftInput_.resize(kFftSize);
    if (currentBands_.size() != static_cast<size_t>(numBands_)) currentBands_.assign(numBands_, 0.0f);

    for (int i = 0; i < kFftSize; ++i) {
        int sourceIndex = (writePos_ + i) % kFftSize;
        fftInput_[i] = ringBuffer_[sourceIndex];
    }

    ApplyHannWindow();

    std::array<kiss_fft_cpx, kFftSize / 2 + 1> fftOutput{};
    kiss_fftr(AsFftCfg(fftCfg_f), reinterpret_cast<const kiss_fft_scalar*>(fftInput_.data()), fftOutput.data());

    constexpr int totalBins = kFftSize / 2 + 1;
    for (int i = 0; i < totalBins; ++i) {
        float real = static_cast<float>(fftOutput[i].r);
        float imag = static_cast<float>(fftOutput[i].i);
        fftInput_[i] = std::sqrt(real * real + imag * imag) / static_cast<float>(kFftSize / 2);
    }

    CompressToBands();
    ApplySmoothing();
}

std::array<float, kMaxBands> AudioAnalyzer::GetBands() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bands_;
}

void AudioAnalyzer::Decay(float factor) {
    std::lock_guard<std::mutex> lock(mutex_);
    factor = std::clamp(factor, 0.0f, 1.0f);
    for (float& band : bands_)
        band *= factor;
}

void AudioAnalyzer::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fill(ringBuffer_.begin(), ringBuffer_.end(), 0.0f);
    std::fill(fftInput_.begin(), fftInput_.end(), 0.0f);
    std::fill(currentBands_.begin(), currentBands_.end(), 0.0f);
    bands_.fill(0.0f);
    writePos_ = 0;
    filled_ = false;
}

void AudioAnalyzer::ApplyHannWindow() {
    if (hannWindow_.size() != kFftSize) return;

    for (int i = 0; i < kFftSize; ++i)
        fftInput_[i] *= hannWindow_[i];
}

void AudioAnalyzer::CompressToBands() {
    std::fill(currentBands_.begin(), currentBands_.end(), 0.0f);
    std::vector<int> counts(numBands_, 0);

    constexpr int totalBins = kFftSize / 2 + 1;
    for (int bin = 1; bin < totalBins; ++bin) {
        int band = static_cast<int>(MapToLogBand(bin, totalBins, numBands_));
        band = std::clamp(band, 0, numBands_ - 1);
        currentBands_[band] += fftInput_[bin];
        counts[band]++;
    }

    for (int band = 0; band < numBands_; ++band) {
        if (counts[band] > 0)
            currentBands_[band] /= static_cast<float>(counts[band]);

        float magnitude = currentBands_[band] * sensitivity_;
        magnitude = std::log10(1.0f + magnitude) * kCompressionScale;
        currentBands_[band] = std::clamp(magnitude, 0.0f, 1.0f);
    }
}

void AudioAnalyzer::ApplySmoothing() {
    for (int i = 0; i < numBands_; ++i) {
        float previous = bands_[i];
        float current = currentBands_[i];
        bands_[i] = previous * kDefaultSmoothing + current * (1.0f - kDefaultSmoothing);
    }

    for (int i = numBands_; i < kMaxBands; ++i)
        bands_[i] = 0.0f;
}

float AudioAnalyzer::MapToLogBand(int fftBin, int totalBins, int numBands) {
    if (totalBins <= 1 || numBands <= 1) return 0.0f;

    float normalized = static_cast<float>(fftBin) / static_cast<float>(totalBins - 1);
    float curved = std::log10(1.0f + normalized * 9.0f);
    return curved * static_cast<float>(numBands - 1);
}

} // namespace audio
