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
// Synth.hpp — 26.08.13 重建端（p output ×10 等价实现, P2.8）
// ------------------------------------------------------------
// 逐条对齐 reference/STFT.cpp Synth (golden):
//   spec[k] = mask[k] × X[k]（X = 原始复 FFT 值; 与 golden 的
//   mask × polar(mag,phase) 数学等价, 相位不变 —— docs/00 §9）
//   → 共轭对称填满 4096 → IFFT → ×Hann 窗 → overlap-add ×(2/3)
//   → 2N 环形缓冲 → 每跳读出 1024 样本并清零。
//
// FFT 缩放（计划书 §6.4 落实）:
//   golden Radix2FFT inverse 含 1/N + normGain = 2/3（恒等重构,
//   Σw²=1.5, docs/02 §1）; rack::dsp::RealFFT::irfft 含 N 倍 ⇒
//   irfft 后先 ×(1/N)（4096=2¹², float 除法精确）恢复 golden 口径,
//   再 ×窗 ×(2/3) —— 与 golden 逐位同式。
//
// D5-A 增量调度（计划书 §6.3）:
//   每 band 4 个子任务（buildSpectrum L+R / irfft L / irfft R /
//   windowAccum L+R）, 任一 process() 只执行 1 个子任务
//   （≤1 次 4096 点 irfft）; 10 band 独立实例, 由 DspEngine 编排。
// ============================================================
#pragma once
#include <array>
#include <memory>
#include <vector>

#include "DspParams.hpp"

namespace rack { namespace dsp { struct RealFFT; } }

namespace sdrack {

class Synth
{
public:
    // buildSpectrum(L+R) + irfft(L) + irfft(R) + windowAccum(L+R)
    static constexpr int kNumSteps = 4;

    Synth();
    ~Synth();

    void prepare(double sampleRate, int fftSize = kDefaultFFTSize);
    void setFftSize(int fftSize);
    void reset();

    int fftSize() const { return fftSize_; }
    int hop() const { return hop_; }

    // band: 0..8 → Band1..9（masks[band]）; 9 → Band10（percMask）
    // fftOutL/R: Analysis 的原始复 FFT 输出（canonical 序, 2N float）
    void runStep(int sub, int band,
                 const float* masks[9], const float* percMask,
                 const float* fftOutL, const float* fftOutR);

    // 读出最近一跳的 hop 样本并清零（对齐 golden Synth::pullSamples）
    void pullSamples(float* dstL, float* dstR, int numSamples);

private:
    void buildSpectrum(int band, const float* masks[9], const float* percMask,
                       const float* fftOutL, const float* fftOutR);
    void windowAccum();

    std::array<std::unique_ptr<rack::dsp::RealFFT>, kNumFftSizes> fft_;
    std::array<std::vector<float>, kNumFftSizes> window_;  // 每 size 合成 Hann
    std::vector<float> specInL_, specInR_;   // irfft 输入（最大 2N, canonical 序）
    std::vector<float> irfftOutL_, irfftOutR_;  // irfft 输出（最大 N, 实）
    std::vector<float> outBufL_, outBufR_;   // 输出环形缓冲（最大 2N, overlap-add）

    int fftSize_    = kDefaultFFTSize;
    int fftSizeIdx_ = fftSizeIndex(kDefaultFFTSize);
    int numBins_    = numBinsForFftSize(kDefaultFFTSize);
    int hop_        = hopForFftSize(kDefaultFFTSize);
    float invN_     = 1.0f / (float)kDefaultFFTSize;
    int readPos_ = 0;
};

} // namespace sdrack
