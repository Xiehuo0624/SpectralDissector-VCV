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
#include "Cepstrum.hpp"

#include <cmath>

#include <dsp/fft.hpp>   // rack::dsp::RealFFT

namespace sdrack {

namespace {
    constexpr float kEps = 1e-10f;   // log(mag+eps) 保护, 同 golden
    // golden inverse 含 1/N, RealFFT::irfft 含 N 倍 ⇒ ×(1/N) 恢复口径
    constexpr float kInvN = 1.0f / (float)kFFTSize;
}

Cepstrum::Cepstrum() = default;

Cepstrum::~Cepstrum() = default;

void Cepstrum::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    fft_.reset(new rack::dsp::RealFFT(kFFTSize));
    specIn_.assign(kFFTSize * 2, 0.0f);
    cep_.assign(kFFTSize, 0.0f);
    fftOut_.assign(kFFTSize * 2, 0.0f);
    env_.assign(kNumBins, 0.0f);
    detail_ = 1.0f;
}

// 1. log|H| → 实偶共轭对称谱（canonical 序: F(0), F(N/2), Re F(1), Im F(1), ...）
void Cepstrum::buildSpectrum(const float* harmRaw)
{
    specIn_[0] = std::log(harmRaw[0] + kEps);
    specIn_[1] = std::log(harmRaw[kNumBins - 1] + kEps);
    for (int k = 1; k < kNumBins - 1; ++k) {
        float lm = std::log(harmRaw[k] + kEps);
        specIn_[2 * k]     = lm;
        specIn_[2 * k + 1] = 0.0f;
    }
}

// 2+3. irfft 后 ×(1/N) 恢复 golden 口径, 再对称带切 lifter（每切片 512 点）
void Cepstrum::scaleAndLifter(int slice)
{
    float A = (1.0f - detail_) * 0.25f * kSpesize;
    float B = (1.0f - (1.0f - detail_) * 0.25f) * kSpesize;
    int n0 = slice * kCepSlice;
    int n1 = n0 + kCepSlice;
    const int half = kFFTSize / 2;
    for (int n = n0; n < n1; ++n) {
        cep_[n] *= kInvN;
        float q = (n <= half) ? (float)n : (float)(kFFTSize - n);
        if (!((q < A) || (q >= B)))
            cep_[n] = 0.0f;
    }
}

// 5. env = exp(Re F(k))（原始量级）
void Cepstrum::extractEnv(int slice)
{
    int k0 = slice * kBinSlice;
    int k1 = (k0 + kBinSlice < kNumBins) ? (k0 + kBinSlice) : kNumBins;
    for (int k = k0; k < k1; ++k) {
        float re = (k == 0) ? fftOut_[0]
                 : (k == kNumBins - 1) ? fftOut_[1]
                 : fftOut_[2 * k];
        env_[k] = std::exp(re);
    }
}

void Cepstrum::runFrameStep(int step, const float* harmRaw, float detail)
{
    if (step == 0) {
        detail_ = detail;
        buildSpectrum(harmRaw);
    }
    else if (step == 1) {
        fft_->irfft(specIn_.data(), cep_.data());   // 含 N 倍增益
    }
    else if (step < 1 + 1 + kNumCepSlices) {
        scaleAndLifter(step - 2);                    // ×(1/N) + lifter
    }
    else if (step == 1 + 1 + kNumCepSlices) {
        fft_->rfft(cep_.data(), fftOut_.data());     // forward, 无归一化
    }
    else {
        extractEnv(step - (1 + 1 + kNumCepSlices + 1));
    }
}

} // namespace sdrack
