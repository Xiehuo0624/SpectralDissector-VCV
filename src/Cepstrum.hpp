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
// Cepstrum.hpp — 26.08.13 p cepstrum 等价实现（P2.5）
// ------------------------------------------------------------
// 对齐 reference/Cepstrum.cpp (golden) 与 docs/00 §6:
//   in:  HPSS 谐波幅度·原始量级（HpssCore out1）—— Δ2
//   1. log|H|（log(mag+1e-10) 保护, 实偶共轭对称谱）
//   2. IFFT → 倒谱
//   3. 对称带切 lifter: q=min(n,N−n); 保留 (q<A)||(q>=B), 切 [A,B) 及镜像
//      A=(1−d)·0.25·spesize, B=(1−(1−d)·0.25)·spesize, spesize=2049 —— Δ6
//   4. FFT → 5. env = exp(Re F(k))（与输入同尺度）
//
// FFT 缩放（rack::dsp::RealFFT vs golden 复 Radix2FFT）:
//   golden: inverse 含 1/N、forward 无归一化 → 往返恒等。
//   RealFFT: irfft 含 N 倍增益、rfft 无归一化。
//   ⇒ irfft 输出 = N·(golden 倒谱); 步骤 3 前先 ×(1/N)（4096 为 2 的
//   幂, float 除法精确）恢复 golden 口径; 之后 rfft 无归一化 ⇒ env
//   尺度与 golden 完全一致（对拍验证, docs/05）。
// ============================================================
#pragma once
#include <array>
#include <vector>
#include <memory>

#include "DspParams.hpp"

namespace rack { namespace dsp { struct RealFFT; } }

namespace sdrack {

class Cepstrum
{
public:
    // 默认 4096 口径: 1 建谱 + 1 irfft + 8 scale/lifter + 1 rfft + 9 env 提取 = 20
    static constexpr int kNumFrameSteps = cepstrumStepsForFftSize(kDefaultFFTSize);

    Cepstrum();
    ~Cepstrum();

    void prepare(double sampleRate, int fftSize = kDefaultFFTSize);
    void setFftSize(int fftSize);
    void reset();

    int fftSize() const { return fftSize_; }
    int numBins() const { return numBins_; }

    // harmRaw: HpssCore out1（原始量级, numBins 有效前缀）; detail: lifter 0..1
    void runFrameStep(int step, const float* harmRaw, float detail);

    const float* getEnv() const { return env_.data(); }

private:
    void buildSpectrum(const float* harmRaw);
    void scaleAndLifter(int slice);
    void extractEnv(int slice);

    std::array<std::unique_ptr<rack::dsp::RealFFT>, kNumFftSizes> fft_;
    std::vector<float> specIn_;    // irfft 输入（最大 2N, canonical 序）
    std::vector<float> cep_;       // 倒谱（最大 N, 实）
    std::vector<float> fftOut_;    // rfft 输出（最大 2N, canonical 序）
    std::vector<float> env_;       // 最大 bin 数（原始量级, 与输入同尺度）

    int fftSize_    = kDefaultFFTSize;
    int fftSizeIdx_ = fftSizeIndex(kDefaultFFTSize);
    int numBins_    = numBinsForFftSize(kDefaultFFTSize);
    int binSlices_  = binSlicesForFftSize(kDefaultFFTSize);
    int cepSlices_  = cepSlicesForFftSize(kDefaultFFTSize);
    float invN_     = 1.0f / (float)kDefaultFFTSize;

    float detail_ = 1.0f;
    double sampleRate_ = 44100.0;
};

} // namespace sdrack
