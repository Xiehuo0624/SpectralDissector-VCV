// HpssCore.cpp — gen~[hpss] 逐 bin 实现（26.08.13）
#include "HpssCore.h"
#include <cmath>
#include <algorithm>

namespace sd {

void HpssCore::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    states.assign(kNumBins, HpssState{});
}

void HpssCore::reset()
{
    std::fill(states.begin(), states.end(), HpssState{});
}

void HpssCore::processBin(float magRaw, int binIdx, const Params& p,
                          float& outHarmRaw, float& outPercMask)
{
    HpssState& st = states[binIdx];

    // 幅值归一化: mag_main = mag_raw / (fft_size * 0.5)
    float mag_main = magRaw / kMagNormDenom;

    // 零延迟内置 HPSS: mix(hist, mag, coeff) = hist*(1-c) + mag*c
    float mag_harm_norm = st.hist * (1.0f - p.blur) + mag_main * p.blur;
    st.hist = mag_harm_norm;

    float mag_perc_norm = std::max(0.0f, mag_main - mag_harm_norm);

    // out1: 恢复原始量级的 Harmonic 幅值
    outHarmRaw = mag_harm_norm * kMagNormDenom;

    // out2: 打击乐掩膜 (Band P)
    outPercMask = std::pow(mag_perc_norm / (mag_main + 0.00001f), p.perc);
}

} // namespace sd
