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
// Analysis.hpp — STFT 前端（26.08.13 口径, P2.1）
// ------------------------------------------------------------
// 行为逐条对齐 reference/STFT.cpp (golden):
//   Hann 窗 4096 / hop 1024 / 4x overlap
//   mag/phase = 未归一化原始 FFT 幅值/相位 (Max pfft~ fftin~ 口径,
//   无 1/N、无 4/N; 归一化 /(fft_size*0.5) 在 HpssCore/MaskGen 内)
//   Nyquist bin (k=2048) 置零 (Max pfft~ 行为, docs/00 §9)
// FFT 用 rack::dsp::RealFFT (pffft, 4096=32 倍数 ✓; 约束.md §4.2)
//
// D5-A 增量调度（计划书 §6.3）:
//   pushSample() 只入环形缓冲; 每 1024 样本触发一帧, 但整帧工作
//   严禁在一次 process() 内完成 —— runFrameStep(0..kNumFrameSteps-1)
//   把一帧切成 13 个子任务, 由 DspEngine 逐样本调用推进:
//     step 0: 加窗拷贝 L → FFT 输入（子任务: 帧数据准备）
//     step 1: 加窗拷贝 R
//     step 2: rfft(L)（子任务: 一帧 FFT 的左声道段）
//     step 3: rfft(R)（右声道段 —— 整帧 FFT 因此至少摊分 2 次调用）
//     step 4..12: 幅值/相位提取, 每次 ≤256 bin
//   任一 process() 只执行 1 个子任务（≤1 次 4096 点 rfft ≈ 数十 µs,
//   pffft 无法中途挂起, 故"一段 FFT 子任务"的最小粒度 = 一次 rfft）。
// ============================================================
#pragma once
#include <array>
#include <vector>
#include <memory>

#include "DspParams.hpp"

namespace rack { namespace dsp { struct RealFFT; } }

namespace sdrack {

class Analysis
{
public:
    // 默认 4096 口径: 2 加窗拷贝 + 2 rfft + 9 个 bin 切片 = 13 个子任务
    static constexpr int kNumFrameSteps = analysisStepsForFftSize(kDefaultFFTSize);

    Analysis();
    ~Analysis();

    // 预分配全部可选 FFT size 的窗/pffft plan + 最大尺寸缓冲,
    // 并把当前 size 设为 fftSize（音频线程切换时不再分配, 约束.md §4.5）。
    void prepare(double sampleRate, int fftSize = kDefaultFFTSize);

    // 仅改当前 size 索引（不分配/不重置）; 由 DspEngine 在 prepare 或
    // 音频线程切换时调用, 随后调用 reset() 清状态。
    void setFftSize(int fftSize);
    void reset();

    int fftSize() const { return fftSize_; }
    int numBins() const { return numBins_; }

    // 逐样本推入。返回 true = 新一帧已触发（帧任务从 step 0 开始）。
    bool pushSample(float inL, float inR);

    // 推进一帧任务的一个子任务 (0..analysisStepsForFftSize(fftSize_)-1)。
    // 帧触发时 snapshot 的环形缓冲起点与 golden processFrame 的
    // `int start = writePos` 完全一致。
    void runFrameStep(int step);

    const float* getMagL()   const { return magL_.data(); }
    const float* getMagR()   const { return magR_.data(); }
    const float* getPhaseL() const { return phaseL_.data(); }
    const float* getPhaseR() const { return phaseR_.data(); }
    // 原始复 FFT 输出（canonical 序, 2N; P2.8 Synth 重建用, 相位不变）
    const float* getFftOutL() const { return fftOutL_.data(); }
    const float* getFftOutR() const { return fftOutR_.data(); }

private:
    void windowCopy(const std::vector<float>& buf, std::vector<float>& dst);
    void extractBin(int k);

    std::array<std::unique_ptr<rack::dsp::RealFFT>, kNumFftSizes> fft_;
    std::array<std::vector<float>, kNumFftSizes> window_;  // 每 size 一套 Hann
    std::vector<float> bufL_, bufR_;    // 输入环形缓冲（按最大 N 预分配）
    std::vector<float> fftInL_, fftInR_;    // rfft 实输入
    std::vector<float> fftOutL_, fftOutR_;  // rfft 输出 (2N, canonical 序)
    std::vector<float> magL_, magR_, phaseL_, phaseR_;   // 按最大 bin 数预分配

    int fftSize_    = kDefaultFFTSize;
    int fftSizeIdx_ = fftSizeIndex(kDefaultFFTSize);
    int numBins_    = numBinsForFftSize(kDefaultFFTSize);
    int binSlices_  = binSlicesForFftSize(kDefaultFFTSize);
    int hop_        = hopForFftSize(kDefaultFFTSize);

    int writePos_ = 0;
    int samplesUntilFrame_ = kHop;
    int frameStart_ = 0;                // 帧触发时 snapshot 的环形缓冲起点
    double sampleRate_ = 44100.0;
};

} // namespace sdrack
