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
#include "HpssCore.hpp"

#include <cmath>
#include <algorithm>

namespace sdrack {

void HpssCore::prepare(int fftSize)
{
    hist_.assign(kMaxBins, 0.0f);
    setFftSize(fftSize);
    reset();
}

void HpssCore::setFftSize(int fftSize)
{
    if (!isValidFftSize(fftSize))
        fftSize = kDefaultFFTSize;
    fftSize_ = fftSize;
    magNormDenom_ = 0.5f * (float)fftSize_;
}

void HpssCore::reset()
{
    std::fill(hist_.begin(), hist_.end(), 0.0f);
}

void HpssCore::processBin(float magRaw, int binIdx, const DspParams& p,
                          float& outHarmRaw, float& outPercMask)
{
    float& hist = hist_[binIdx];

    // 幅值归一化: mag_main = mag_raw / (fft_size * 0.5)
    float mag_main = magRaw / magNormDenom_;

    // 零延迟内置 HPSS: mix(hist, mag, coeff) = hist*(1-c) + mag*c
    float mag_harm_norm = hist * (1.0f - p.blur) + mag_main * p.blur;
    hist = mag_harm_norm;

    float mag_perc_norm = std::max(0.0f, mag_main - mag_harm_norm);

    // 输出 1: 恢复原始量级的 Harmonic 幅值（供 Cepstrum/MaskGen）
    outHarmRaw = mag_harm_norm * magNormDenom_;

    // 输出 2: 打击乐掩膜（Band 10）
    outPercMask = std::pow(mag_perc_norm / (mag_main + 0.00001f), p.perc);
}

} // namespace sdrack
