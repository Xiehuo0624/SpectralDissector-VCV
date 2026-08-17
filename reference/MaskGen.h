// ============================================================
// MaskGen.h — 26.08.13 gen~[main] 等价实现（Δ1 拆分后的主掩膜核）
// ------------------------------------------------------------
// 每 bin: 归一化 → 倒谱去噪门 (deno_th==0 → gate=1 特例, Δ3) → slide 门
// → mag_harmonic → 全局 tilt → mag_env = slide(mag_harmonic) (Δ4, 级联前单次)
// → 级联分拨 Band1..7 + Band8 兜底 → ×门 → out1..8；out9 = 1 - 门
// 权威源: patchs/fft2.maxpat obj-40 codebox（docs/00 §5 原文）。
// ============================================================
#pragma once
#include "Params.h"
#include <vector>

namespace sd {

struct MaskState {
    float gateSmooth = 0.0f;   // denoise_gate_smooth (slide)
    float magEnv     = 0.0f;   // mag_env = slide(mag_harmonic) —— 26.08.13 新增状态
};

class MaskGen
{
public:
    void prepare(double sampleRate);
    void reset();
    // magHarmRaw: HpssCore out1 (原始量级); envRaw: 倒谱包络 (原始量级)
    // outMasks[9]: Band1..8 掩膜 + Band9 噪声掩膜
    void processBin(float magHarmRaw, float envRaw, int binIdx, const Params& p,
                    float outMasks[9]);

private:
    std::vector<MaskState> states;   // size = kNumBins
    double sampleRate_ = 44100.0;
};

} // namespace sd
