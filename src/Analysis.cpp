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

#include <algorithm>
#include <cmath>

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

void Analysis::prepare(double sampleRate, int fftSize)
{
    sampleRate_ = sampleRate;
    if (!isValidFftSize(fftSize))
        fftSize = kDefaultFFTSize;
    // 预分配全部可选尺寸的 pffft plan 与 Hann 窗; 缓冲按最大 N 一次性分配。
    // 此后 setFftSize()/reset() 只做索引切换与 std::fill, 无堆分配。
    for (int i = 0; i < kNumFftSizes; ++i) {
        fft_[i].reset(new rack::dsp::RealFFT(kFftSizes[i]));
        window_[i] = makeHannWindow(kFftSizes[i]);
    }
    bufL_.assign(kMaxFFTSize, 0.0f);
    bufR_.assign(kMaxFFTSize, 0.0f);
    fftInL_.assign(kMaxFFTSize, 0.0f);
    fftInR_.assign(kMaxFFTSize, 0.0f);
    fftOutL_.assign(kMaxFFTSize * 2, 0.0f);
    fftOutR_.assign(kMaxFFTSize * 2, 0.0f);
    magL_.assign(kMaxBins, 0.0f);
    magR_.assign(kMaxBins, 0.0f);
    phaseL_.assign(kMaxBins, 0.0f);
    phaseR_.assign(kMaxBins, 0.0f);
    setFftSize(fftSize);
    reset();
}

void Analysis::setFftSize(int fftSize)
{
    if (!isValidFftSize(fftSize))
        fftSize = kDefaultFFTSize;
    fftSize_    = fftSize;
    fftSizeIdx_ = fftSizeIndex(fftSize);
    numBins_    = numBinsForFftSize(fftSize);
    binSlices_  = binSlicesForFftSize(fftSize);
    hop_        = hopForFftSize(fftSize);
}

void Analysis::reset()
{
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    std::fill(fftInL_.begin(), fftInL_.end(), 0.0f);
    std::fill(fftInR_.begin(), fftInR_.end(), 0.0f);
    std::fill(fftOutL_.begin(), fftOutL_.end(), 0.0f);
    std::fill(fftOutR_.begin(), fftOutR_.end(), 0.0f);
    std::fill(magL_.begin(), magL_.end(), 0.0f);
    std::fill(magR_.begin(), magR_.end(), 0.0f);
    std::fill(phaseL_.begin(), phaseL_.end(), 0.0f);
    std::fill(phaseR_.begin(), phaseR_.end(), 0.0f);
    writePos_ = 0;
    samplesUntilFrame_ = hop_;
    frameStart_ = 0;
}

bool Analysis::pushSample(float inL, float inR)
{
    bufL_[writePos_] = inL;
    bufR_[writePos_] = inR;
    writePos_ = (writePos_ + 1) & (fftSize_ - 1);
    if (--samplesUntilFrame_ <= 0) {
        samplesUntilFrame_ = hop_;
        frameStart_ = writePos_;   // == golden: int start = writePos
        return true;
    }
    return false;
}

void Analysis::windowCopy(const std::vector<float>& buf, std::vector<float>& dst)
{
    const int mask = fftSize_ - 1;
    const int start = frameStart_;
    const std::vector<float>& win = window_[fftSizeIdx_];
    for (int n = 0; n < fftSize_; ++n)
        dst[n] = buf[(start + n) & mask] * win[n];
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
    else if (k == numBins_ - 1) {
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
    auto& fft = *fft_[fftSizeIdx_];
    switch (step) {
        case 0:  windowCopy(bufL_, fftInL_); break;
        case 1:  windowCopy(bufR_, fftInR_); break;
        case 2:  fft.rfft(fftInL_.data(), fftOutL_.data()); break;
        case 3:  fft.rfft(fftInR_.data(), fftOutR_.data()); break;
        default: {
            int s = step - 4;
            int k0 = s * kBinSlice;
            int k1 = std::min(k0 + kBinSlice, numBins_);
            for (int k = k0; k < k1; ++k)
                extractBin(k);
            // 最后一个切片覆盖 Nyquist bin: 置零 (Max pfft~ 行为)
            if (s == binSlices_ - 1) {
                magL_[numBins_ - 1]   = 0.0f;
                magR_[numBins_ - 1]   = 0.0f;
                phaseL_[numBins_ - 1] = 0.0f;
                phaseR_[numBins_ - 1] = 0.0f;
            }
        } break;
    }
}

} // namespace sdrack
