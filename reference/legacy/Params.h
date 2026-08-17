// ============================================================
// Params.h — 纯 C++ 参数定义 (去 JUCE 依赖)
// ------------------------------------------------------------
// 仅保留算法所需: 常量 + 参数结构。VST3 的 APVTS/ParameterID
// 绑定属于 host 层, 移植到 Tiliqua 时参数来自 SoC 寄存器/CV 输入。
// ============================================================
#pragma once
#include <cstdint>

namespace sd_legacy {

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

// ---------- 参数集 (算法直接消费的结构) ----------
// 范围/默认值见下方 kParamDefaults。Tiliqua 侧每个参数对应一个
// 寄存器或 CV 量化值, 进入此结构后送入 DSP 核心。
struct Params {
    float threshold;   // mast_th   dB
    float spacing;     // dB
    float focus;       // 1..10
    float tilt;        // dB
    float gate;        // deno_th   0..3
    float blur;        // hpss_coeff 0.001..0.5
    float perc;        // perc_focus 1..10
    float slideRise;   // STFT 帧数
    float slideFall;   // STFT 帧数
    float detail;      // 倒谱 lifter 比例 0..1
    float off[7];      // Band 1..7 offset (dB)
    float dry;         // 0..1
    bool  bandOn[10];  // 各 band 开关 (Band1..8, Residual, Perc)
};

// 默认参数 (对应 VST3 makeParameterLayout 的默认值)
inline Params defaultParams() {
    Params p;
    p.threshold = 0.f;   p.spacing = 6.f;  p.focus = 1.5f; p.tilt = 0.f;
    p.gate = 1.0f;       p.blur = 0.05f;   p.perc = 1.5f;
    p.slideRise = 5.f;   p.slideFall = 25.f; p.detail = 0.3f;
    for (int i = 0; i < 7; ++i) p.off[i] = 0.f;
    p.dry = 0.0f;
    for (int i = 0; i < 10; ++i) p.bandOn[i] = (i < 8);  // Band1..8 默认开
    return p;
}

} // namespace sd_legacy
