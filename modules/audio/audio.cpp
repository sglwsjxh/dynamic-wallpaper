#include "audio/audio.h"

#include "audio/audio_analyzer.h"
#include "audio/audio_capture.h"
#include "logs/log.h"

#include <algorithm>

namespace audio {

Context::Context() = default;
Context::~Context() = default;

bool Init(Context& ctx, int sampleRate, int numBands, float smoothing, float sensitivity) {
    ctx.sampleRate = sampleRate;
    ctx.numBands = numBands;
    ctx.smoothing = smoothing;

    ctx.analyzer = std::make_unique<AudioAnalyzer>();
    ctx.analyzer->Init(sampleRate, numBands, sensitivity, smoothing);

    ctx.capture = std::make_unique<AudioCapture>();
    if (!ctx.capture->Start(ctx.analyzer.get())) {
        LOG_ERR << "Audio: WASAPI capture 启动失败";
        ctx.capture.reset();
        ctx.analyzer.reset();
        return false;
    }

    ctx.enabled = true;
    LOG_INFO << "Audio: 初始化完成 (采样率=" << sampleRate << ", bands=" << numBands << ")";
    return true;
}

void Tick(Context& ctx) {
    if (!ctx.enabled) return;

    // Read latest bands from analyzer (thread-safe)
    if (ctx.analyzer) {
        ctx.bands = ctx.analyzer->GetBands();
        static int dbg = 0;
        if (++dbg % 150 == 0) {
            float maxBand = *std::max_element(ctx.bands.begin(), ctx.bands.end());
            LOG_INFO << "Audio: max band=" << maxBand;
        }
    }

    // Restart capture if device was lost
    if (ctx.capture && !ctx.capture->IsRunning()) {
        LOG_WARN << "Audio: 捕获设备断开，尝试重启";
        ctx.capture->Stop();
        if (!ctx.capture->Start(ctx.analyzer.get())) ctx.analyzer->Decay(0.85f);
    }
}

void Shutdown(Context& ctx) {
    if (!ctx.enabled) return;
    ctx.enabled = false;

    LOG_INFO << "Audio: 开始清理";

    if (ctx.capture) ctx.capture->Stop();

    // Zero out bands
    ctx.bands.fill(0.0f);

    ctx.capture.reset();
    ctx.analyzer.reset();

    LOG_INFO << "Audio: 清理完成";
}

const std::array<float, kMaxBands>& GetBands(const Context& ctx) {
    return ctx.bands;
}

void UpdateBands(Context& ctx) {
    if (ctx.enabled && ctx.analyzer) ctx.bands = ctx.analyzer->GetBands();
}

} // namespace audio
