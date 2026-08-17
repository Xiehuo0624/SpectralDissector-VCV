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
#include "Synth.hpp"

#include <algorithm>
#include <cmath>

#include <dsp/fft.hpp>   // rack::dsp::RealFFT (pffft, 约束.md §4.2)

namespace sdrack {

namespace {
    // 恒等重构: 4 帧 overlap-add Σw² = 1.5 ⇒ normGain = 2/3 (docs/02 §1)
    constexpr float kNormGain = 2.0f / 3.0f;
}

// Hann 窗: 与 golden reference/STFT.cpp 的 makeHannWindow 完全一致
static std::vector<float> makeHannWindow(int N)
{
    std::vector<float> w(N);
    for (int n = 0; n < N; ++n)
        w[n] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265358979323846f
                                      * (float)n / (float)N);
    return w;
}

Synth::Synth() = default;

Synth::~Synth() = default;

void Synth::prepare(double sampleRate, int fftSize)
{
    (void)sampleRate;   // 合成端不依赖采样率（golden Synth::prepare 仅存快照）
    if (!isValidFftSize(fftSize))
        fftSize = kDefaultFFTSize;
    // 全部可选 size 的 pffft plan 与 Hann 窗预分配; 缓冲按最大 N 一次分配。
    for (int i = 0; i < kNumFftSizes; ++i) {
        fft_[i].reset(new rack::dsp::RealFFT(kFftSizes[i]));
        window_[i] = makeHannWindow(kFftSizes[i]);
    }
    specInL_.assign(kMaxFFTSize * 2, 0.0f);
    specInR_.assign(kMaxFFTSize * 2, 0.0f);
    irfftOutL_.assign(kMaxFFTSize, 0.0f);
    irfftOutR_.assign(kMaxFFTSize, 0.0f);
    outBufL_.assign(kMaxFFTSize * 2, 0.0f);
    outBufR_.assign(kMaxFFTSize * 2, 0.0f);
    setFftSize(fftSize);
    reset();
}

void Synth::setFftSize(int fftSize)
{
    if (!isValidFftSize(fftSize))
        fftSize = kDefaultFFTSize;
    fftSize_    = fftSize;
    fftSizeIdx_ = fftSizeIndex(fftSize);
    numBins_    = numBinsForFftSize(fftSize);
    hop_        = hopForFftSize(fftSize);
    invN_       = 1.0f / (float)fftSize_;
}

void Synth::reset()
{
    std::fill(specInL_.begin(), specInL_.end(), 0.0f);
    std::fill(specInR_.begin(), specInR_.end(), 0.0f);
    std::fill(irfftOutL_.begin(), irfftOutL_.end(), 0.0f);
    std::fill(irfftOutR_.begin(), irfftOutR_.end(), 0.0f);
    std::fill(outBufL_.begin(), outBufL_.end(), 0.0f);
    std::fill(outBufR_.begin(), outBufR_.end(), 0.0f);
    readPos_ = 0;
}

// spec[k] = mask[k] × X[k]（相位不变）→ canonical 序喂给 irfft。
// canonical 序（rack::dsp::RealFFT）:
//   in[0] = F(0), in[1] = F(N/2), in[2k] = Re F(k), in[2k+1] = Im F(k)
void Synth::buildSpectrum(int band, const float* masks[9], const float* percMask,
                          const float* fftOutL, const float* fftOutR)
{
    const float* m = (band < 9) ? masks[band] : percMask;

    specInL_[0] = m[0] * fftOutL[0];
    specInR_[0] = m[0] * fftOutR[0];
    // Nyquist bin: golden Analysis 对 mag/phase[N/2] 置零 ⇒ spec[N/2]=0
    specInL_[1] = 0.0f;
    specInR_[1] = 0.0f;
    for (int k = 1; k < numBins_ - 1; ++k) {
        float mkV = m[k];
        specInL_[2 * k]     = mkV * fftOutL[2 * k];
        specInL_[2 * k + 1] = mkV * fftOutL[2 * k + 1];
        specInR_[2 * k]     = mkV * fftOutR[2 * k];
        specInR_[2 * k + 1] = mkV * fftOutR[2 * k + 1];
    }
}

// golden: outBuf[(readPos+n)%2N] += ifftOut[n](含 1/N) × window[n] × (2/3);
// 本实现: irfftOut[n]（含 N 倍）× (1/N) × window[n] × (2/3), 逐位同式。
void Synth::windowAccum()
{
    const int mask = 2 * fftSize_ - 1;   // 2N 为 2 的幂, 掩码 ≡ 取模
    const std::vector<float>& win = window_[fftSizeIdx_];
    int w = readPos_;
    for (int n = 0; n < fftSize_; ++n) {
        float sL = (irfftOutL_[n] * invN_) * win[n] * kNormGain;
        float sR = (irfftOutR_[n] * invN_) * win[n] * kNormGain;
        outBufL_[(w + n) & mask] += sL;
        outBufR_[(w + n) & mask] += sR;
    }
    readPos_ = (readPos_ + hop_) & mask;
}

void Synth::runStep(int sub, int band,
                    const float* masks[9], const float* percMask,
                    const float* fftOutL, const float* fftOutR)
{
    auto& fft = *fft_[fftSizeIdx_];
    switch (sub) {
        case 0: buildSpectrum(band, masks, percMask, fftOutL, fftOutR); break;
        case 1: fft.irfft(specInL_.data(), irfftOutL_.data()); break;
        case 2: fft.irfft(specInR_.data(), irfftOutR_.data()); break;
        case 3: windowAccum(); break;
        default: break;
    }
}

void Synth::pullSamples(float* dstL, float* dstR, int numSamples)
{
    const int mask = 2 * fftSize_ - 1;
    int start = (readPos_ - hop_ + 2 * fftSize_) & mask;
    for (int i = 0; i < numSamples; ++i) {
        int idx = (start + i) & mask;
        dstL[i] = outBufL_[idx];
        dstR[i] = outBufR_[idx];
        outBufL_[idx] = 0.0f;
        outBufR_[idx] = 0.0f;
    }
}

} // namespace sdrack
