// Spectral Dissector — VCV Rack 2 plugin
// Copyright (C) 2026 Xiehuo
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// ============================================================
// MaskGen.hpp — 26.08.13 gen~[main] 等价实现（P2.6）
// ------------------------------------------------------------
// 逐字对齐 reference/MaskGen.cpp (golden) 与 docs/00 §5 codebox:
//   归一化 /2048 → norm_mag 双 eps → 去噪门（deno_th==0 → gate=1 特例）
//   → gate slide（rise/fall 样本口径）→ mag_harmonic = mag·gate
//   → tilt 补偿（f_i=max(bin·sr/N,1)）→ mag_env = slide(mag_harmonic) 前置
//   → 级联 7 band + Band8 兜底 max(1−accum,0)
//   → out1..8 = out_b × gate；out9 = 1 − gate
// 逐 bin 状态: gateSmooth[2049] + magEnv[2049]（float32, 初始 0）。
// 能量守恒: Σ out_b1..8 = 1 ⇒ Σ out1..8 = gate, out9 = 1−gate
//   ⇒ Σ out1..9 = 1（自动化测试项, 计划书 §8.2）。
// ============================================================
#pragma once
#include <vector>

#include "DspParams.hpp"

namespace sdrack {

class MaskGen
{
public:
    void prepare(double sampleRate);
    void reset();

    // magHarmRaw: HpssCore out1（原始量级）; envRaw: Cepstrum env（原始量级）
    void processBin(float magHarmRaw, float envRaw, int binIdx,
                    const DspParams& p, float outMasks[9]);

private:
    struct State {
        float gateSmooth = 0.0f;   // 归一化域
        float magEnv     = 0.0f;   // 归一化域
    };
    std::vector<State> states_;    // size kNumBins
    double sampleRate_ = 44100.0;
};

} // namespace sdrack
