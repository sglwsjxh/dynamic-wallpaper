#include "audio/audio_capture.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <avrt.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <vector>

#include "audio/audio_analyzer.h"
#include "logs/log.h"

namespace audio {
namespace {

constexpr GUID kCLSID_MMDeviceEnumerator = {
    0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}
};

constexpr GUID kIID_IMMDeviceEnumerator = {
    0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}
};

constexpr GUID kIID_IAudioClient = {
    0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}
};

constexpr GUID kIID_IAudioCaptureClient = {
    0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}
};

constexpr GUID kSubFormatPcm = {
    0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

constexpr GUID kSubFormatIeeeFloat = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

void SafeRelease(IUnknown*& ptr) {
    if (!ptr) return;
    ptr->Release();
    ptr = nullptr;
}

bool GetAudioFormat(const WAVEFORMATEX* format, bool& isFloat, bool& isPcm, WORD& bitsPerSample) {
    isFloat = false;
    isPcm = false;
    bitsPerSample = format->wBitsPerSample;

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        isFloat = true;
        return true;
    }

    if (format->wFormatTag == WAVE_FORMAT_PCM) {
        isPcm = true;
        return true;
    }

    if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE) return false;

    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    bitsPerSample = extensible->Samples.wValidBitsPerSample ? extensible->Samples.wValidBitsPerSample : format->wBitsPerSample;
    isFloat = IsEqualGUID(extensible->SubFormat, kSubFormatIeeeFloat) != 0;
    isPcm = IsEqualGUID(extensible->SubFormat, kSubFormatPcm) != 0;
    return isFloat || isPcm;
}

float ReadPcmSample(const BYTE* sample, WORD bitsPerSample) {
    if (bitsPerSample <= 8) return (static_cast<int>(*sample) - 128) / 128.0f;

    if (bitsPerSample <= 16) {
        const auto value = static_cast<int16_t>(sample[0] | (sample[1] << 8));
        return static_cast<float>(value) / 32768.0f;
    }

    if (bitsPerSample <= 24) {
        int32_t value = sample[0] | (sample[1] << 8) | (sample[2] << 16);
        if (value & 0x00800000) value |= static_cast<int32_t>(0xff000000);
        return static_cast<float>(value) / 8388608.0f;
    }

    const auto value = static_cast<int32_t>(
        sample[0] | (sample[1] << 8) | (sample[2] << 16) | (sample[3] << 24));
    return static_cast<float>(value) / 2147483648.0f;
}

bool ConvertToMonoFloat(const BYTE* data, UINT32 numFrames, const WAVEFORMATEX* format, std::vector<float>& mono) {
    if (!data || !format || format->nChannels == 0 || format->nBlockAlign == 0) return false;

    bool isFloat = false;
    bool isPcm = false;
    WORD bitsPerSample = 0;
    if (!GetAudioFormat(format, isFloat, isPcm, bitsPerSample)) return false;

    const auto channels = static_cast<UINT32>(format->nChannels);
    const auto bytesPerSample = static_cast<UINT32>((format->wBitsPerSample + 7) / 8);
    if (bytesPerSample == 0 || bytesPerSample * channels > format->nBlockAlign) return false;

    mono.assign(numFrames, 0.0f);

    for (UINT32 frame = 0; frame < numFrames; ++frame) {
        const BYTE* frameData = data + static_cast<size_t>(frame) * format->nBlockAlign;
        float sum = 0.0f;

        for (UINT32 channel = 0; channel < channels; ++channel) {
            const BYTE* sample = frameData + static_cast<size_t>(channel) * bytesPerSample;
            if (isFloat && format->wBitsPerSample == 32) {
                sum += *reinterpret_cast<const float*>(sample);
            } else if (isPcm) {
                sum += ReadPcmSample(sample, bitsPerSample);
            }
        }

        mono[frame] = sum / static_cast<float>(channels);
    }

    return true;
}

void BackoffWhileRunning(const std::atomic<bool>& running) {
    for (int i = 0; i < 63 && running; ++i) Sleep(16);
}

} // namespace

AudioCapture::AudioCapture() = default;

AudioCapture::~AudioCapture() {
    Stop();
}

bool AudioCapture::Start(AudioAnalyzer* analyzer) {
    if (!analyzer) return false;
    if (running_) return true;

    if (thread_.joinable()) thread_.join();

    analyzer_ = analyzer;
    deviceError_ = false;
    running_ = true;

    try {
        thread_ = std::thread(&AudioCapture::CaptureThread, this);
    } catch (...) {
        running_ = false;
        analyzer_ = nullptr;
        LOG_ERR << "AudioCapture: 捕获线程创建失败";
        return false;
    }

    return true;
}

void AudioCapture::Stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    analyzer_ = nullptr;
}

void AudioCapture::CaptureThread() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInitialized = SUCCEEDED(hr);
    if (!comInitialized) {
        LOG_ERR << "AudioCapture: CoInitializeEx 失败 hr=0x" << std::hex << static_cast<unsigned long>(hr);
        running_ = false;
        return;
    }

    DWORD taskIndex = 0;
    HANDLE mmcssHandle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    if (!mmcssHandle) LOG_WARN << "AudioCapture: 设置 MMCSS 线程优先级失败";

    while (running_) {
        auto* pEnumerator = static_cast<IMMDeviceEnumerator*>(nullptr);
        auto* pDevice = static_cast<IMMDevice*>(nullptr);
        auto* pAudioClient = static_cast<IAudioClient*>(nullptr);
        auto* pCaptureClient = static_cast<IAudioCaptureClient*>(nullptr);
        WAVEFORMATEX* pwfx = nullptr;
        bool audioStarted = false;

        deviceEnumerator_ = nullptr;
        device_ = nullptr;
        audioClient_ = nullptr;
        captureClient_ = nullptr;

        hr = CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, kIID_IMMDeviceEnumerator,
            reinterpret_cast<void**>(&pEnumerator));
        if (FAILED(hr)) {
            LOG_ERR << "AudioCapture: 创建设备枚举器失败 hr=0x" << std::hex << static_cast<unsigned long>(hr);
            deviceError_ = true;
        }

        if (SUCCEEDED(hr)) {
            deviceEnumerator_ = pEnumerator;
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (FAILED(hr)) LOG_ERR << "AudioCapture: 获取默认输出设备失败 hr=0x" << std::hex << static_cast<unsigned long>(hr);
        }

        if (SUCCEEDED(hr)) {
            device_ = pDevice;
            hr = pDevice->Activate(kIID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&pAudioClient));
            if (FAILED(hr)) LOG_ERR << "AudioCapture: 激活 IAudioClient 失败 hr=0x" << std::hex << static_cast<unsigned long>(hr);
        }

        if (SUCCEEDED(hr)) {
            audioClient_ = pAudioClient;
            hr = pAudioClient->GetMixFormat(&pwfx);
            if (FAILED(hr)) LOG_ERR << "AudioCapture: 获取混音格式失败 hr=0x" << std::hex << static_cast<unsigned long>(hr);
        }

        if (SUCCEEDED(hr)) {
            constexpr REFERENCE_TIME bufferDuration = 10000000; // 1 second in 100ns units
            hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                bufferDuration, 0, pwfx, nullptr);
            if (FAILED(hr)) LOG_ERR << "AudioCapture: 初始化 loopback 失败 hr=0x" << std::hex << static_cast<unsigned long>(hr);
        }

        if (SUCCEEDED(hr)) {
            hr = pAudioClient->GetService(kIID_IAudioCaptureClient, reinterpret_cast<void**>(&pCaptureClient));
            if (FAILED(hr)) LOG_ERR << "AudioCapture: 获取 IAudioCaptureClient 失败 hr=0x" << std::hex << static_cast<unsigned long>(hr);
        }

        if (SUCCEEDED(hr)) {
            captureClient_ = pCaptureClient;
            hr = pAudioClient->Start();
            if (FAILED(hr)) LOG_ERR << "AudioCapture: 开始捕获失败 hr=0x" << std::hex << static_cast<unsigned long>(hr);
            else {
                audioStarted = true;
                LOG_INFO << "AudioCapture: 捕获已启动 (format=" << pwfx->wFormatTag
                         << " bits=" << pwfx->wBitsPerSample
                         << " channels=" << pwfx->nChannels
                         << " rate=" << pwfx->nSamplesPerSec << ")";
            }
        }

        std::vector<float> monoBuffer;
        if (SUCCEEDED(hr)) deviceError_ = false;

        int cycleCount = 0;
        int totalPackets = 0;
        int silentPackets = 0;
        int activePackets = 0;
        int idleCycles = 0;

        while (running_ && SUCCEEDED(hr)) {
            UINT32 packetLength = 0;
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;

            bool hadData = false;

            while (running_ && packetLength > 0) {
                BYTE* pData = nullptr;
                UINT32 numFrames = 0;
                DWORD flags = 0;

                hr = pCaptureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr);
                if (FAILED(hr)) break;

                hadData = true;
                totalPackets++;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    silentPackets++;
                    // Feed zeros so FFT naturally decays the bands during silence
                    if (numFrames > 0 && analyzer_) {
                        monoBuffer.assign(numFrames, 0.0f);
                        analyzer_->FeedSamples(monoBuffer.data(), static_cast<int>(monoBuffer.size()));
                        analyzer_->Process();
                    }
                } else {
                    activePackets++;
                    if (numFrames > 0 && analyzer_) {
                        if (ConvertToMonoFloat(pData, numFrames, pwfx, monoBuffer)) {
                            analyzer_->FeedSamples(monoBuffer.data(), static_cast<int>(monoBuffer.size()));
                            analyzer_->Process();
                        } else {
                            LOG_WARN << "AudioCapture: 不支持的音频格式 tag=" << pwfx->wFormatTag
                                     << " bits=" << pwfx->wBitsPerSample
                                     << " channels=" << pwfx->nChannels;
                        }
                    }
                }

                pCaptureClient->ReleaseBuffer(numFrames);
                hr = pCaptureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) break;
            }

            if (hadData) {
                idleCycles = 0;
            } else if (analyzer_) {
                // No packets at all — complete silence, decay gradually
                idleCycles++;
                if (idleCycles >= 10) {
                    analyzer_->Decay(0.85f);
                }
            }

            cycleCount++;
            if (cycleCount % 300 == 0) {
                LOG_INFO << "AudioCapture: cycle=" << cycleCount
                         << " total=" << totalPackets
                         << " silent=" << silentPackets
                         << " active=" << activePackets;
            }

            if (SUCCEEDED(hr) && running_) Sleep(16);
        }

        if (FAILED(hr) && running_) {
            deviceError_ = true;
            LOG_ERR << "AudioCapture: 捕获中断 hr=0x" << std::hex << static_cast<unsigned long>(hr)
                    << " cycles=" << cycleCount
                    << " total=" << totalPackets
                    << " silent=" << silentPackets
                    << " active=" << activePackets;
            if (analyzer_) analyzer_->Decay(0.85f);
        } else if (running_) {
            LOG_INFO << "AudioCapture: 捕获循环退出 (非错误)"
                     << " cycles=" << cycleCount
                     << " total=" << totalPackets
                     << " silent=" << silentPackets
                     << " active=" << activePackets;
        }

        if (audioStarted && pAudioClient) pAudioClient->Stop();
        if (pwfx) CoTaskMemFree(pwfx);

        auto* captureUnknown = reinterpret_cast<IUnknown*>(pCaptureClient);
        auto* audioUnknown = reinterpret_cast<IUnknown*>(pAudioClient);
        auto* deviceUnknown = reinterpret_cast<IUnknown*>(pDevice);
        auto* enumeratorUnknown = reinterpret_cast<IUnknown*>(pEnumerator);
        SafeRelease(captureUnknown);
        SafeRelease(audioUnknown);
        SafeRelease(deviceUnknown);
        SafeRelease(enumeratorUnknown);

        captureClient_ = nullptr;
        audioClient_ = nullptr;
        device_ = nullptr;
        deviceEnumerator_ = nullptr;

        if (running_ && FAILED(hr)) BackoffWhileRunning(running_);
    }

    if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
    CoUninitialize();
}

} // namespace audio
