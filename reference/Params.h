// ============================================================
// Params.h — 纯 C++ 参数定义（26.08.13 语义 golden model）
// ------------------------------------------------------------
// 默认值 = 26.08.13 运行时默认（prams.maxpat live.* parameter_initial
// 在设备加载时下发，经 gen~ Param 钳位后的有效值），详见
// docs/00_DSP_规格_26_08_13.md §7 与 docs/01_待核实项关闭记录.md。
//
// 与旧版（reference/legacy/Params.h）差异：
//   spacing 6.0 → 5.0（UI 初始）
//   focus   1.5 → 1.0（UI 初始）
//   slideRise/slideFall 5/25 → 1/1（UI 0 ms → ×sr/1000 = 0 样本 → gen min=1 钳位）
//   detail  0.3 → 1.0（UI 初始）
//   bandOn  Band1..8 开 → 全部 10 开（band1route toggles 初始 1）
// ============================================================
#pragma once
#include <cstdint>

namespace sd {

// ---------- FFT / STFT 配置 ----------
constexpr int kFFTSize   = 4096;
constexpr int kFFTOrder  = 12;               // log2(kFFTSize), 2^12 = 4096
constexpr int kOverlap   = 4;                // 4x overlap (75%)
constexpr int kHop       = kFFTSize / kOverlap;  // 1024
constexpr int kNumBins   = kFFTSize / 2 + 1;     // 2049 (单边谱)

constexpr int kNumBands    = 10;             // Band1..8 + Residual + Perc
constexpr int kNumOutBuses = 1 + kNumBands;  // Dry + 10 bands

// output_gate 淡入淡出时长 (ms)
constexpr double kGateFadeMs = 100.0;

// 26.08.13 gen~ 内部归一化分母 (mag_main = mag_raw / (fft_size*0.5))
constexpr float kMagNormDenom = 0.5f * (float)kFFTSize;   // = 2048

// p cepstrum 谱帧尺寸 (fftinfo~ 出口 2 = N/2+1)
constexpr float kSpesize = (float)kNumBins;               // = 2049

// ---------- 参数集 (算法直接消费的结构) ----------
// rise/fall 在 DSP 层为「样本」口径（UI 为 ms，宿主层换算，见 docs/00 §7）。
struct Params {
    float threshold = 0.0f;  // mast_th   dB
    float spacing   = 5.0f;  // dB（UI 初始 5.0）
    float focus     = 1.0f;  // 1..10（UI 初始 1.0）
    float tilt      = 0.0f;  // dB
    float gate      = 1.0f;  // deno_th   0..3（deno_th==0 → gate=1 特例）
    float blur      = 0.05f; // hpss_coeff 0.001..0.5
    float perc      = 1.5f;  // perc_focus 1..10
    float slideRise = 1.0f;  // 样本（UI 0 ms → gen min=1 钳位）
    float slideFall = 1.0f;  // 样本
    float detail    = 1.0f;  // 倒谱 lifter 0..1（UI 初始 1.0）
    float off[7];            // Band 1..7 offset (dB)
    float dry       = 0.0f;  // 0..1（默认关）
    bool  bandOn[10];        // 各 band 开关（26.08.13 全部默认开）
};

inline Params defaultParams() {
    Params p;
    for (int i = 0; i < 7; ++i) p.off[i] = 0.0f;
    for (int i = 0; i < 10; ++i) p.bandOn[i] = true;   // band1..9 + bandP 全开
    return p;
}

} // namespace sd
