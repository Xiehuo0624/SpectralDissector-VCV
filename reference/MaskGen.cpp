// MaskGen.cpp — gen~[main] 逐 bin 实现（26.08.13）
#include "MaskGen.h"
#include <cmath>
#include <algorithm>

namespace sd {

namespace {
    inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
    inline float smoothstep(float v) { v = clamp01(v); return v * v * (3.f - 2.f * v); }
    inline float dbtoa(float db) { return std::pow(10.0f, db / 20.0f); }
    // gen~ slide: 每 bin 一阶平滑 (上升用 rise, 下降用 fall, 样本口径)
    inline float slide(float prev, float x, float rise, float fall) {
        float c = (x > prev) ? (1.0f / std::max(1.0f, rise)) : (1.0f / std::max(1.0f, fall));
        return prev + (x - prev) * c;
    }
}

void MaskGen::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    states.assign(kNumBins, MaskState{});
}

void MaskGen::reset()
{
    std::fill(states.begin(), states.end(), MaskState{});
}

void MaskGen::processBin(float magHarmRaw, float envRaw, int binIdx, const Params& p,
                         float outMasks[9])
{
    MaskState& st = states[binIdx];

    // 幅值归一化
    float mag_harm = magHarmRaw / kMagNormDenom;
    float env_main = envRaw / kMagNormDenom;

    // 直接使用倒谱包络作为背景底噪的基准线
    float norm_mag = mag_harm / (std::max(env_main, 0.00001f) + 0.00001f);

    // Stage 1: 倒谱谐波去噪门
    float clean_conf = (norm_mag - p.gate) * p.focus;
    float v_gate = clamp01(clean_conf);
    // Δ3: deno_th == 0 → gate = 1 短路（在 smoothstep 之前）
    float denoise_gate_raw = (p.gate == 0.0f) ? 1.0f : smoothstep(v_gate);

    float denoise_gate_smooth = slide(st.gateSmooth, denoise_gate_raw,
                                      p.slideRise, p.slideFall);
    st.gateSmooth = denoise_gate_smooth;
    float mag_harmonic = mag_harm * denoise_gate_smooth;

    // 全局倾斜补偿
    float f_i = std::max((float)binIdx * (float)(sampleRate_ / (double)kFFTSize), 1.0f);
    float tilt_comp_db = p.tilt * std::log2(f_i / 1000.0f);

    // ========================================================
    // Stage 2: 绝对幅值级联分拨（Δ4: 输入为 mag_env = slide(mag_harmonic)）
    // ========================================================
    float mag_env = slide(st.magEnv, mag_harmonic, p.slideRise, p.slideFall);
    st.magEnv = mag_env;
    float accum = 0.0f;
    float out_b[8];

    for (int b = 0; b < 7; ++b) {   // Band 1..7
        float t_db = p.threshold - (float)b * p.spacing + p.off[b];
        float t_mult = dbtoa(-(t_db + tilt_comp_db));
        float v = clamp01((mag_env * t_mult - 0.5f) * p.focus + 0.5f);
        float m = smoothstep(v);
        out_b[b] = std::max(m - accum, 0.0f);
        accum = accum + out_b[b];
    }
    // Band 8 (Zero-Threshold Catcher)
    out_b[7] = std::max(1.0f - accum, 0.0f);
    accum = accum + out_b[7];

    // 掩膜输出合成（26.08.13 无 per-band slide）
    for (int b = 0; b < 8; ++b)
        outMasks[b] = out_b[b] * denoise_gate_smooth;

    // Out 9: 纯粹的降噪拦截杂声
    outMasks[8] = 1.0f - denoise_gate_smooth;
}

} // namespace sd
