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
// DspEngine.hpp — P2 阶段 DSP 编排（逐样本摊分调度, D5-A）
// ------------------------------------------------------------
// process() 每次调用只推进一帧任务的一个子任务:
//   step 0..12 : Analysis 13 子任务（加窗拷贝 + rfft + 幅值/相位提取）
//   step 13..21: HpssCore 逐 bin（mag_raw = 0.5*(magL+magR)）
//   step 22..41: Cepstrum 20 子任务（建谱/irfft/scale+lifter/rfft/env）
//   step 42..50: MaskGen 逐 bin → 掩膜 out1..9
//   step 51..90: Synth×10, 每 band 4 子任务（buildSpectrum L+R /
//                irfft L / irfft R / windowAccum L+R）
//   step 91    : 每 band pull 1024 样本 → 2N fifo（帧收尾）
// 共 92 个子任务; 帧完成于触发后第 91 个样本（D5-A 调度延迟, docs/06 §1.2）。
//
// 逐样本输出（与 golden SpectralSeparator::process 同构 + 统一调度延迟）:
//   模块输出流 = golden 输出流整体延迟 kScheduleLatency=91 样本
//   （含 gate 淡入淡出斜率、band 开关响应、dry 通路 —— 全通道统一,
//   对拍以 --shift 91 逐样本对齐, docs/06 §6）:
//   dry: 延迟 kDryDelay+kScheduleLatency 样本 × dry 增益（增益同样延迟 91）
//   band: fifo 出样 × OutputGate（100ms 线性淡入淡出, 目标延迟 91 样本）
// ============================================================
#pragma once
#include <array>
#include <atomic>
#include <vector>

#include "Analysis.hpp"
#include "HpssCore.hpp"
#include "Cepstrum.hpp"
#include "MaskGen.hpp"
#include "Synth.hpp"
#include "OutputGate.hpp"

namespace sdrack {

class DspEngine
{
public:
    // 默认 4096 口径的调度常量（供既有测试/文档使用; 当前实际值随
    // 右键选择的 FFT size 变化, 见 fftSize()/numBins() 等）。
    static constexpr int kNumHpssSteps = binSlicesForFftSize(kDefaultFFTSize);  // 9
    static constexpr int kNumMaskSteps = binSlicesForFftSize(kDefaultFFTSize);  // 9
    static constexpr int kNumSynthSteps = kNumBands * Synth::kNumSteps;         // 40
    static constexpr int kNumFrameSteps = frameStepsForFftSize(kDefaultFFTSize); // 92
    static constexpr int kScheduleLatency = scheduleLatencyForFftSize(kDefaultFFTSize); // 91

    void prepare(double sampleRate);

    // 右键菜单/JSON 在 UI 线程请求切换。音频线程下一次 process() 检测到
    // 变化后执行无分配状态重置（全部 pffft plan 与缓冲已在 prepare 预分配,
    // 约束.md §4.5）。无效值忽略。
    void requestFftSize(int fftSize);
    int fftSize() const { return currentFftSize_.load(std::memory_order_acquire); }
    int hop()     const { return hopForFftSize(fftSize()); }
    int numBins() const { return numBinsForFftSize(fftSize()); }
    int frameSteps()   const { return frameStepsForFftSize(fftSize()); }
    int scheduleLatency() const { return scheduleLatencyForFftSize(fftSize()); }

    bool isPrepared() const { return prepared_; }
    double sampleRate() const { return sampleRate_; }

    // 逐样本处理（无堆分配/无锁/无 IO —— 约束.md §4.5）。
    // 输出指针可为 null（帧级对拍驱动不读音频输出时）。
    void process(float inL, float inR,
                 float* outDryL, float* outDryR,
                 float* outBandL[10], float* outBandR[10]);

    // 帧完成标志（对拍驱动读取后 clearFrameFlag()）
    bool frameJustCompleted() const { return frameDone_; }
    void clearFrameFlag() { frameDone_ = false; }
    int frameCount() const { return frameCount_; }

    // 参数集（26.08.13 运行时默认, docs/00 §7）
    DspParams& params() { return p_; }

    // 最近完成帧的导出访问器（对拍）
    const float* getMagL()    const { return analysis_.getMagL(); }
    const float* getMagR()    const { return analysis_.getMagR(); }
    const float* getHarmRaw() const { return harmRaw_.data(); }
    const float* getPercMask() const { return percMask_.data(); }
    const float* getEnv()     const { return cepstrum_.getEnv(); }
    const float* getBandMask(int b) const { return masks_[b].data(); }   // b=0..8 → Band1..9

    // ---- P5.2 (D11) 频谱分析仪共享缓冲 ----
    // 音频线程在 MaskGen 步把 masks[b][k]×|X[k]|（|X| = 0.5(magL+magR),
    // 即 mag_raw 共享谱口径）写入后缓冲 specDisp_[specBack_], 帧收尾
    // （pull 步）用 release store 发布前缓冲索引。显示层（UI 线程）:
    //   int front = spectrumFront();                 // acquire 读一次
    //   const float* data = spectrumData(front, b);  // 此后读该帧 10 条 band
    // 音频线程只写"另一"缓冲, 永不覆盖已发布帧 ⇒ 免锁且读数一致。
    // 显示层不进音频路径（约束 D11: 分析仪为显示层）。切换 FFT size
    // 时重置谱缓冲并发布索引 0。
    int spectrumFront() const { return specFront_.load(std::memory_order_acquire); }
    const float* spectrumData(int buf, int band) const { return specDisp_[buf][band]; }

private:
    void applyPendingFftSize();
    void configureFftSize(int fftSize);   // 设当前配置 + 无分配重置状态
    void runFrameStep(int step);
    void pullFrameOutputs();

    // 与 golden SpectralSeparator::Fifo 完全一致（pop 空时返 0）
    struct Fifo {
        std::vector<float> buf;
        int writeIdx = 0, readIdx = 0, count = 0;
        void resize(int n) { buf.assign(n, 0.0f); writeIdx = readIdx = count = 0; }
        void push(const float* src, int n);
        float pop();
        void reset() { writeIdx = readIdx = count = 0; }
    };

    Analysis  analysis_;
    HpssCore  hpss_;
    Cepstrum  cepstrum_;
    MaskGen   maskGen_;
    Synth     synths_[kNumBands];
    OutputGate gates_[kNumBands];        // 100ms 线性淡入淡出（P2.9）
    DspParams p_;

    // 帧中间缓冲（按最大 FFT size 预分配; 有效前缀 = 当前 fftSize_）
    std::vector<float> harmRaw_;       // HpssCore out1
    std::vector<float> percMask_;      // HpssCore out2
    std::vector<float> masks_[9];      // MaskGen out1..9

    // P5.2 (D11): 分析仪显示缓冲 masks[b][k]×|X[k]|（b=0..9 → B1..B10,
    // 双缓冲 2×10×maxBins）。specBack_ 为音频线程私有的写索引,
    // specFront_ 为跨线程发布索引。
    float specDisp_[2][kNumBands][kMaxBins] = {};
    std::atomic<int> specFront_{0};
    int specBack_ = 1;

    Fifo fifoL_[kNumBands], fifoR_[kNumBands];   // 2N 环形 fifo（对齐 golden）
    std::vector<float> tmpL_, tmpR_;             // pull 暂存（最大 hop）

    // 参数时间线与内容时间线统一延迟 latency_（docs/06 §6）:
    // OutputGate 增益与 dry 增益在应用前先延迟 latency_ 样本。数组按
    // 最大 size（8192 → latency 131）预分配, 有效长度随当前 size 变化。
    std::array<std::array<float, kMaxScheduleLatency>, kNumBands> gateGainDelay_{};
    int   gateGainDelayIdx_ = 0;
    std::array<float, kMaxScheduleLatency> dryGainDelay_{};
    int   dryGainDelayIdx_ = 0;

    // Dry 通路: golden kDryDelay=N（对齐其湿路 N−1）; 插件湿路
    // = golden+latency ⇒ dry 延迟 N+latency 保持与湿路相同的 1 样本相对关系
    std::vector<float> dryDelayL_, dryDelayR_;
    int dryDelayIdx_ = 0;

    // 当前 FFT size 配置（音频线程专用; UI 只读原子 currentFftSize_）。
    int fftSize_   = kDefaultFFTSize;
    int numBins_   = kNumBins;
    int binSlices_ = kNumBinSlices;
    int hop_       = kHop;
    int frameSteps_ = kNumFrameSteps;
    int latency_    = kScheduleLatency;
    std::atomic<int> pendingFftSize_{kDefaultFFTSize};
    std::atomic<int> currentFftSize_{kDefaultFFTSize};

    int  jobStep_    = -1;   // -1 = 无帧任务在途
    int  frameCount_ = 0;
    bool frameDone_  = false;
    bool prepared_   = false;
    double sampleRate_ = 44100.0;
};

} // namespace sdrack
