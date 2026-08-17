// GenDsp.cpp — 逐 bin 掩膜算法 (与 VST3 版一致)
// 信号流见 PORTING_GUIDE "GenDsp 逐 bin 算法" 一节。
#include "GenDsp.h"
#include <cmath>
#include <algorithm>

namespace sd_legacy {

namespace {
    inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
    inline float smoothstep(float v) { v = clamp01(v); return v * v * (3.f - 2.f * v); }
    inline float dbtoa(float db) { return std::pow(10.0f, db / 20.0f); }
    inline float mix(float a, float b, float f) { return a * (1.f - f) + b * f; }
    // gen~ slide: per-frame 一阶平滑 (上升用 rise, 下降用 fall 系数)
    inline float slide(float prev, float x, float rise, float fall) {
        float c = (x > prev) ? (1.0f / std::max(1.0f, rise)) : (1.0f / std::max(1.0f, fall));
        return prev + (x - prev) * c;
    }
}

void GenDsp::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    states.assign(kNumBins, BinState{});
}

void GenDsp::reset()
{
    std::fill(states.begin(), states.end(), BinState{});
}

void GenDsp::processBin(float mag, float env, int binIdx, const Params& p, float outMasks[10])
{
    BinState& st = states[binIdx];
    float mag_main = mag;
    float env_main = env;

    // ============ Stage 0: 零延迟 HPSS ============
    float mag_harm = mix(st.hpssHist, mag_main, p.blur);  // 缓慢平滑, 留稳定泛音
    st.hpssHist = mag_harm;
    float mag_perc = std::max(0.0f, mag_main - mag_harm);
    outMasks[9] = std::pow(mag_perc / (mag_main + 1e-5f), p.perc);  // Perc

    // ============ Stage 1: 倒谱去噪门 ============
    float norm_mag = mag_harm / (std::max(env_main, 1e-5f) + 1e-5f);
    float v_gate = clamp01((norm_mag - p.gate) * p.focus);
    float denoise_gate_raw = smoothstep(v_gate);
    st.gateSmooth = slide(st.gateSmooth, denoise_gate_raw, p.slideRise, p.slideFall);
    float denoise_gate = st.gateSmooth;
    float mag_harmonic = mag_harm * denoise_gate;

    // 全局倾斜补偿
    float f_i = std::max((float)binIdx * ((float)sampleRate_ / (float)kFFTSize), 1.0f);
    float tilt_comp_db = p.tilt * std::log2(f_i / 1000.0f);

    // ============ Stage 2: 级联幅度分拨 ============
    float accum = 0.0f;
    float bandRaw[8];
    for (int b = 0; b < 7; ++b) {   // Band 1..7
        float t_db = p.threshold - (float)b * p.spacing + p.off[b];
        float t_mult = dbtoa(-(t_db + tilt_comp_db));
        float v = clamp01((mag_harmonic * t_mult - 0.5f) * p.focus + 0.5f);
        float m = smoothstep(v);
        float raw = std::max(m - accum, 0.0f);
        bandRaw[b] = raw;
        accum += raw;
    }
    bandRaw[7] = std::max(1.0f - accum, 0.0f);  // Band 8 兜底
    accum += bandRaw[7];

    // 掩膜靶向平滑 + 合成
    for (int b = 0; b < 8; ++b) {
        st.bandSmooth[b] = slide(st.bandSmooth[b], bandRaw[b], p.slideRise, p.slideFall);
        outMasks[b] = st.bandSmooth[b] * denoise_gate;   // band1..8
    }
    // Residual: 门拦下的部分
    outMasks[8] = 1.0f - denoise_gate;
    // Perc (outMasks[9]) 已在 Stage 0 算好
}

} // namespace sd_legacy
