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
#include "Analysis.hpp"

#include <cmath>
#include <algorithm>

#include <dsp/fft.hpp>   // rack::dsp::RealFFT (pffft, 约束.md §4.2)

namespace sdrack {

// Hann 窗: 与 golden reference/STFT.cpp 的 makeHannWindow 完全一致
static std::vector<float> makeHannWindow(int N)
{
    std::vector<float> w(N);
    for (int n = 0; n < N; ++n)
        w[n] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265358979323846f
                                      * (float)n / (float)N);
    return w;
}

Analysis::Analysis() = default;

Analysis::~Analysis() = default;

void Analysis::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    fft_.reset(new rack::dsp::RealFFT(kFFTSize));
    window_ = makeHannWindow(kFFTSize);
    bufL_.assign(kFFTSize, 0.0f);
    bufR_.assign(kFFTSize, 0.0f);
    fftInL_.assign(kFFTSize, 0.0f);
    fftInR_.assign(kFFTSize, 0.0f);
    fftOutL_.assign(kFFTSize * 2, 0.0f);
    fftOutR_.assign(kFFTSize * 2, 0.0f);
    magL_.assign(kNumBins, 0.0f);
    magR_.assign(kNumBins, 0.0f);
    phaseL_.assign(kNumBins, 0.0f);
    phaseR_.assign(kNumBins, 0.0f);
    writePos_ = 0;
    samplesUntilFrame_ = kHop;
    frameStart_ = 0;
}

bool Analysis::pushSample(float inL, float inR)
{
    bufL_[writePos_] = inL;
    bufR_[writePos_] = inR;
    writePos_ = (writePos_ + 1) & (kFFTSize - 1);
    if (--samplesUntilFrame_ <= 0) {
        samplesUntilFrame_ = kHop;
        frameStart_ = writePos_;   // == golden: int start = writePos
        return true;
    }
    return false;
}

void Analysis::windowCopy(const std::vector<float>& buf, std::vector<float>& dst)
{
    const int mask = kFFTSize - 1;
    const int start = frameStart_;
    for (int n = 0; n < kFFTSize; ++n)
        dst[n] = buf[(start + n) & mask] * window_[n];
}

void Analysis::extractBin(int k)
{
    // canonical 序 (rack::dsp::RealFFT::rfft):
    //   out[0] = F(0), out[1] = F(N/2), out[2k]=Re F(k), out[2k+1]=Im F(k)
    float reL, imL, reR, imR;
    if (k == 0) {
        reL = fftOutL_[0];  imL = 0.0f;
        reR = fftOutR_[0];  imR = 0.0f;
    }
    else if (k == kNumBins - 1) {
        reL = fftOutL_[1];  imL = 0.0f;
        reR = fftOutR_[1];  imR = 0.0f;
    }
    else {
        reL = fftOutL_[2 * k];      imL = fftOutL_[2 * k + 1];
        reR = fftOutR_[2 * k];      imR = fftOutR_[2 * k + 1];
    }
    // 原始 FFT 幅值口径（不做 1/N、不做 4/N；归一化在 HpssCore/MaskGen）
    magL_[k]   = std::hypot(reL, imL);
    magR_[k]   = std::hypot(reR, imR);
    phaseL_[k] = std::atan2(imL, reL);
    phaseR_[k] = std::atan2(imR, reR);
}

void Analysis::runFrameStep(int step)
{
    switch (step) {
        case 0:  windowCopy(bufL_, fftInL_); break;
        case 1:  windowCopy(bufR_, fftInR_); break;
        case 2:  fft_->rfft(fftInL_.data(), fftOutL_.data()); break;
        case 3:  fft_->rfft(fftInR_.data(), fftOutR_.data()); break;
        default: {
            int s = step - 4;
            int k0 = s * kBinSlice;
            int k1 = std::min(k0 + kBinSlice, kNumBins);
            for (int k = k0; k < k1; ++k)
                extractBin(k);
            // 最后一个切片覆盖 k=2048: Nyquist bin 置零 (Max pfft~ 行为)
            if (s == kNumBinSlices - 1) {
                magL_[kNumBins - 1]   = 0.0f;
                magR_[kNumBins - 1]   = 0.0f;
                phaseL_[kNumBins - 1] = 0.0f;
                phaseR_[kNumBins - 1] = 0.0f;
            }
        } break;
    }
}

} // namespace sdrack
