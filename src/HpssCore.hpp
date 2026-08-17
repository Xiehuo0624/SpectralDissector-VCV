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
// HpssCore.hpp — 26.08.13 gen~[hpss] 等价实现（P2.2）
// ------------------------------------------------------------
// 逐字对齐 reference/HpssCore.cpp (golden) 与 docs/00 §4 codebox:
//   mag_main        = mag_raw / (fft_size * 0.5)          (归一化域)
//   mag_harm_norm   = mix(hist, mag_main, hpss_coeff)     (零延迟历史 mix)
//   poke(hist)      = mag_harm_norm
//   mag_perc_norm   = max(0, mag_main - mag_harm_norm)
//   outHarmRaw      = mag_harm_norm * (fft_size * 0.5)    (原始量级)
//   outPercMask     = pow(mag_perc_norm/(mag_main+1e-5), perc_focus)
// 每 bin 状态 hpssHist[2049]（归一化域, 初始 0）。全部 float32。
// ============================================================
#pragma once
#include <vector>

#include "DspParams.hpp"

namespace sdrack {

class HpssCore
{
public:
    void prepare();
    void reset();

    // magRaw: 未归一化原始 FFT 幅值（|X|, Max pfft~ 口径）
    void processBin(float magRaw, int binIdx, const DspParams& p,
                    float& outHarmRaw, float& outPercMask);

private:
    std::vector<float> hist_;   // size kNumBins, 归一化域 hpss_data, 初始 0
};

} // namespace sdrack
