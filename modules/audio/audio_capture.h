#pragma once
#include <windows.h>

#include <atomic>
#include <functional>
#include <thread>

namespace audio {

class AudioAnalyzer; // forward declaration

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    // Start capture thread. analyzer will receive PCM float mono samples.
    bool Start(AudioAnalyzer* analyzer);

    // Signal stop and wait for thread to exit
    void Stop();

    bool IsRunning() const { return running_; }

private:
    void CaptureThread();

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> deviceError_{false};
    AudioAnalyzer* analyzer_ = nullptr;

    // COM pointers (opaque)
    void* deviceEnumerator_ = nullptr; // IMMDeviceEnumerator*
    void* audioClient_ = nullptr;      // IAudioClient*
    void* captureClient_ = nullptr;    // IAudioCaptureClient*
    void* device_ = nullptr;           // IMMDevice*
};

} // namespace audio
