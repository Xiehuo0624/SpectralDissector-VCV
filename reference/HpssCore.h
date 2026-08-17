// ============================================================
// HpssCore.h — 26.08.13 gen~[hpss] 等价实现（Δ1 拆分后的 HPSS 核）
// ------------------------------------------------------------
// 每 bin: mag_raw/(fft_size*0.5) 归一化 → 零延迟历史 mix（hpss_coeff）
// → poke → 打击残差 → 输出:
//   outHarmRaw   = mag_harm_norm * (fft_size*0.5)   (原始量级, 供 Cepstrum/MaskGen)
//   outPercMask  = pow(perc/(mag_main+1e-5), perc_focus)  (Band 10 掩膜)
// 权威源: patchs/fft2.maxpat obj-19 codebox（docs/00 §4 原文）。
// ============================================================
#pragma once
#include "Params.h"
#include <vector>

namespace sd {

struct HpssState {
    float hist = 0.0f;   // 归一化域 hpss_data (Data hpss_data(4096), 初始 0)
};

class HpssCore
{
public:
    void prepare(double sampleRate);
    void reset();
    // magRaw: 未归一化原始 FFT 幅值 (|X|, Max pfft~ 口径)
    void processBin(float magRaw, int binIdx, const Params& p,
                    float& outHarmRaw, float& outPercMask);

private:
    std::vector<HpssState> states;   // size = kNumBins
    double sampleRate_ = 44100.0;
};

} // namespace sd
