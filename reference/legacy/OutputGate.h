// ============================================================
// OutputGate.h — 频带输出门 (100ms 线性淡入淡出)
// ------------------------------------------------------------
// 移植自 Max output_gate (sel 1/sel 0 + line~ 100ms)。
// Tiliqua 映射: 用 dsp.Ramp 或自定义线性斜坡; rampInc 按
// sampleRate * (kGateFadeMs/1000) 计算。
// ============================================================
#pragma once
#include "Params.h"
#include <algorithm>
#include <cmath>

namespace sd_legacy {

class OutputGate
{
public:
    void prepare(double sampleRate)
    {
        rampInc = 1.0f / (float)(sampleRate * (kGateFadeMs / 1000.0));
    }
    void setTarget(bool on) { target = on ? 1.0f : 0.0f; }
    void process(float* L, float* R, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i) {
            if (gain < target)      gain = std::min(target, gain + rampInc);
            else if (gain > target) gain = std::max(target, gain - rampInc);
            L[i] *= gain;
            R[i] *= gain;
        }
    }
    float getGain() const { return gain; }

private:
    float gain = 0.0f;
    float target = 0.0f;
    float rampInc = 0.001f;
};

} // namespace sd_legacy
