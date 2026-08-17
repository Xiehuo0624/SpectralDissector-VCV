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
// DspParams.hpp — 26.08.13 DSP 常量与参数（P2）
// ------------------------------------------------------------
// 常量与 reference/Params.h 一致（docs/00 §9 / §7）:
//   STFT 4096 / hop 1024 / 4x overlap / bins 2049
//   gen~ 内部归一化分母 = fft_size*0.5 = 2048
//   p cepstrum 谱帧尺寸 spesize = N/2+1 = 2049
// 参数: 26.08.13 运行时默认（docs/00 §7 参数表）:
//   P2.2 已接 blur/perc; P2.6 接入 gen~[main] 全量 + detail。
// ============================================================
#pragma once
#include <cmath>

namespace sdrack {

// ---------- STFT 配置 ----------
// 默认保持 26.08.13 黄金口径: 4096 / hop 1024 / 4x overlap。
// 2026-08-19 用户指示: 右键菜单可切换 FFT window size
// （1024/2048/4096/8192, 均为 4x overlap, 切换即重置分析状态）。
// 默认值 4096 下与 26.08.13 逐位一致（run_ab / run_ab_stream 回归）。
constexpr int   kFFTSize      = 4096;                 // 默认 FFT size（黄金口径）
constexpr int   kHop          = kFFTSize / 4;        // 1024 (4x overlap)
constexpr int   kOverlap      = 4;
constexpr int   kNumBins      = kFFTSize / 2 + 1;    // 2049 (单边谱, 含 Nyquist)
constexpr float kMagNormDenom = 0.5f * (float)kFFTSize;  // 2048 (mag_main = mag_raw / 2048)
constexpr float kSpesize      = (float)kNumBins;     // 2049 (fftinfo~ 出口 2)

// ---------- 输出端配置（P2 尾声, reference/Params.h）----------
constexpr int   kNumBands     = 10;          // Band1..8 + Band9(噪声) + Band10(打击)
constexpr double kGateFadeMs  = 100.0;       // output_gate 淡入淡出时长 (ms)
constexpr int   kDryDelay     = kFFTSize;    // Dry 延迟对齐 (SpectralSeparator.h kDryDelay)

// ---------- P5.1 增量（D9/D10）----------
constexpr int   kNumCv        = 8;           // Threshold/Spacing/Focus/Gate/Blur/Perc/Detail/Tilt

// ---------- D5-A 逐样本摊分 ----------
// 一帧 = 若干"子任务"; process() 每次只推进一个子任务。
// bin 切片: 每调用最多处理 kBinSlice 个 bin 的幅值/相位提取或 HPSS。
constexpr int kBinSlice     = 256;
constexpr int kCepSlice     = 512;

// ---------- 运行时可选 FFT size（2026-08-19 用户指示）----------
// 全部为 32 的倍数（rack::dsp::RealFFT / pffft 约束.md §4.2）且
// 为 2 的幂 ⇒ 环形缓冲掩码 / 1/N 缩放与 4096 黄金路径同型。
constexpr int kDefaultFFTSize = kFFTSize;
constexpr int kMinFFTSize     = 1024;
constexpr int kMaxFFTSize     = 8192;
constexpr int kNumFftSizes    = 4;
constexpr int kFftSizes[kNumFftSizes] = {1024, 2048, 4096, 8192};
constexpr int kMaxBins        = kMaxFFTSize / 2 + 1;                 // 4097
constexpr int kMaxBinSlices   = (kMaxBins + kBinSlice - 1) / kBinSlice;  // 17
constexpr int kMaxCepSlices   = kMaxFFTSize / kCepSlice;             // 16
constexpr int kMaxHop         = kMaxFFTSize / 4;                     // 2048
constexpr int kSynthFrameSteps = 4;                                  // Synth::kNumSteps

constexpr bool isValidFftSize(int n) {
    return n == 1024 || n == 2048 || n == 4096 || n == 8192;
}
constexpr int fftSizeIndex(int n) {
    return (n == 1024) ? 0 : (n == 2048) ? 1 : (n == 4096) ? 2 : 3;
}
constexpr int hopForFftSize(int n)              { return n / 4; }
constexpr int numBinsForFftSize(int n)          { return n / 2 + 1; }
constexpr int binSlicesForFftSize(int n)        { return (numBinsForFftSize(n) + kBinSlice - 1) / kBinSlice; }
constexpr int cepSlicesForFftSize(int n)        { return n / kCepSlice; }
constexpr int analysisStepsForFftSize(int n)    { return 2 + 2 + binSlicesForFftSize(n); }
constexpr int cepstrumStepsForFftSize(int n)    { return 1 + 1 + cepSlicesForFftSize(n) + 1 + binSlicesForFftSize(n); }
constexpr int frameStepsForFftSize(int n) {
    return analysisStepsForFftSize(n)
         + binSlicesForFftSize(n)                // HpssCore
         + cepstrumStepsForFftSize(n)
         + binSlicesForFftSize(n)                // MaskGen
         + kNumBands * kSynthFrameSteps
         + 1;                                    // pull
}
constexpr int scheduleLatencyForFftSize(int n) { return frameStepsForFftSize(n) - 1; }
constexpr int kMaxFrameSteps      = frameStepsForFftSize(kMaxFFTSize);       // 132
constexpr int kMaxScheduleLatency = scheduleLatencyForFftSize(kMaxFFTSize);  // 131

constexpr int kNumBinSlices = (kNumBins + kBinSlice - 1) / kBinSlice;  // 9
// 倒谱 lifter 切片: 每调用最多处理 kCepSlice 个倒谱点。
constexpr int kNumCepSlices = kFFTSize / kCepSlice;                    // 8

// ---------- 参数集（算法直接消费） ----------
// 26.08.13 运行时默认（docs/00 §7）。rise/fall 为样本口径
// （UI ms → 宿主层 ×sr/1000，U3 定稿）。
// P5.1 增量（D9/D10）: bandGain[10] 默认 1.0（unity, 默认值下与
// 26.08.13 逐位一致）; cvAtt[8] 默认 0（CV 不生效）。二者默认值
// 保证既有对拍基线（docs/04–07）不受影响。
struct DspParams {
    // gen~[hpss]
    float blur = 0.05f;   // hpss_coeff, UI 显示名 Blur,  0.001..0.5
    float perc = 1.5f;    // perc_focus, UI 显示名 Perc,  1..10
    // gen~[main]
    float gate      = 1.0f;   // deno_th, Gate, 0..3（==0 → gate=1 特例）
    float threshold = 0.0f;   // mast_th, Threshold, dB
    float spacing   = 5.0f;   // Spacing, dB（UI 初始 5.0）
    float focus     = 1.0f;   // Focus, 1..10（UI 初始 1.0）
    float tilt      = 0.0f;   // Tilt, dB
    float slideRise = 1.0f;   // Rise, 样本（UI 0ms → 钳位 1）
    float slideFall = 1.0f;   // Fall, 样本
    float off[7]    = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // Band 1..7 Offset, dB
    // p cepstrum
    float detail = 1.0f;      // Detail, 0..1（UI 初始 1.0）
    // 主设备层（output_gate / dry, docs/00 §2/§7）
    float dry = 0.0f;         // Dry 增益, 0/1（默认关）
    bool bandOn[kNumBands];   // 各 band 开关（26.08.13 全部默认开）
    // P5.1 增量（D9/D10）:
    //   bandGain: per-band 音量, 0..1.5 默认 1.0, 作用于 OutputGate 之后
    //   （D9; Dry 不加）。cvAtt: 双极 attenuator −1..1, 默认 0（D10）;
    //   CV 语义 param = clamp(旋钮 + cv×att×range/20) 在模块层计算
    //   （plugin.cpp cvModulate），此处仅存 attenuator 值。
    float bandGain[kNumBands];
    float cvAtt[kNumCv];      // Threshold/Spacing/Focus/Gate/Blur/Perc/Detail/Tilt
    DspParams() {
        for (int b = 0; b < kNumBands; ++b) {
            bandOn[b] = true;
            bandGain[b] = 1.0f;
        }
        for (int c = 0; c < kNumCv; ++c)
            cvAtt[c] = 0.0f;
    }
};

// ---------- P5.1 (D10) CV 语义 ----------
// param = clamp(旋钮值 + cvVoltage × att × range/20, min, max)
//   range = UI 范围（min..max）⇒ CV ±10V 在旋钮位于中位时恰好
//   扫到参数极值。非有限 CV（NaN/Inf）按 0 处理（防 NaN 传播,
//   文档化偏差 —— Rack 正常端口不会输出非有限电压）。
// 默认路径逐位回归: att=0 时 v = knob + 0.0f = knob（IEEE 精确）,
//   clamp 界内为恒等 ⇒ 与 P3 的直读逐位一致。
inline float cvModulate(float knob, float cvVoltage, float att, float min, float max)
{
    if (!std::isfinite(cvVoltage))
        cvVoltage = 0.0f;
    float range = max - min;
    float v = knob + cvVoltage * att * range / 20.0f;
    return std::fmax(std::fmin(v, max), min);
}

} // namespace sdrack
