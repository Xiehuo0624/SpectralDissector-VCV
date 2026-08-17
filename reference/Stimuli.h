// ============================================================
// Stimuli.h — golden 对拍共用激励集（26.08.13 P2 起）
// ------------------------------------------------------------
// 用途: test_golden frames 模式与 plugin/test/ab_analysis_hpss 驱动
// 共享同一激励生成代码, 保证两侧输入逐样本一致 (对比才有意义)。
// 激励 = compare_old_new.cpp 内 5 组现成生成逻辑 (docs/02 §3),
// 另加 1 组立体声去相关白噪声 (L≠R, 验证双声道路径)。
// 随机数: 自包含 LCG (不依赖 std::rand 的跨二进制一致性)。
// ============================================================
#pragma once
#include <cstdint>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sd {

struct Stim {
    const char* name;
    std::vector<float> L, R;
};

// 确定性 LCG, 输出 [-1, 1)
struct Rng {
    uint32_t s = 0x9E3779B9u;
    float next() {
        s = s * 1664525u + 1013904223u;
        return ((float)(s >> 8) / 8388608.0f) - 1.0f;   // 24-bit / 2^23 → [0,2) → [-1,1)
    }
};

// 0: 220+440Hz 正弦+噪声+kick (test_golden 原激励)
// 1: 对数扫频 20Hz-20kHz
// 2: 白噪声
// 3: 短冲击串 5Hz (HPSS 打击分离)
// 4: 微电平 1kHz 正弦 (-80dB)
// 5: 立体声去相关白噪声 (L≠R)
// 6: 1kHz 满幅正弦 (无 Nyquist 能量; mask=1 恒等重构对拍用, docs/06)
inline Stim makeStim(int index, double sr, int total)
{
    switch (index) {
    case 6: {
        Stim s{"1kHz 正弦", std::vector<float>(total), std::vector<float>(total)};
        for (int i = 0; i < total; ++i)
            s.L[i] = s.R[i] = 0.3f * (float)std::sin(2.0 * M_PI * 1000.0 * (double)i / sr);
        return s;
    }
    case 1: {
        Stim s{"对数扫频 20Hz-20kHz", std::vector<float>(total), std::vector<float>(total)};
        double f0 = 20.0, f1 = 20000.0;
        double T = (double)total / sr;
        double k = std::log(f1 / f0) / T;
        for (int i = 0; i < total; ++i) {
            double t = (double)i / sr;
            double ph = 2.0 * M_PI * f0 * (std::exp(k * t) - 1.0) / k;
            s.L[i] = s.R[i] = 0.3f * (float)std::sin(ph);
        }
        return s;
    }
    case 2: {
        Stim s{"白噪声", std::vector<float>(total), std::vector<float>(total)};
        Rng rng;
        for (int i = 0; i < total; ++i)
            s.L[i] = s.R[i] = 0.2f * rng.next();
        return s;
    }
    case 3: {
        Stim s{"短冲击串 5Hz", std::vector<float>(total), std::vector<float>(total)};
        int period = (int)(sr / 5.0);
        for (int i = 0; i < total; ++i) {
            int phase = i % period;
            s.L[i] = s.R[i] = (phase < 8) ? (0.9f * std::exp(-(float)phase / 4.f)) : 0.0f;
        }
        return s;
    }
    case 4: {
        Stim s{"微电平 1kHz 正弦 (-80dB)", std::vector<float>(total), std::vector<float>(total)};
        for (int i = 0; i < total; ++i)
            s.L[i] = s.R[i] = 1e-4f * (float)std::sin(2.0 * M_PI * 1000.0 * (double)i / sr);
        return s;
    }
    case 5: {
        Stim s{"立体声去相关白噪声", std::vector<float>(total), std::vector<float>(total)};
        Rng rng;
        for (int i = 0; i < total; ++i) {
            s.L[i] = 0.2f * rng.next();
            s.R[i] = 0.2f * rng.next();
        }
        return s;
    }
    case 0:
    default: {
        Stim s{"220+440Hz正弦+噪声+kick", std::vector<float>(total), std::vector<float>(total)};
        Rng rng;
        for (int i = 0; i < total; ++i) {
            double t = (double)i / sr;
            float sine  = 0.4f * (float)std::sin(2.0 * M_PI * 220.0 * t);
            float sine2 = 0.2f * (float)std::sin(2.0 * M_PI * 440.0 * t);
            float noise = 0.05f * rng.next();
            float kick = 0.f;
            int phase = i % (int)(sr * 0.5);
            if (phase < 200)
                kick = 0.6f * std::exp(-(float)phase / 60.f) * (float)std::sin(2.0 * M_PI * 60.0 * t);
            s.L[i] = s.R[i] = sine + sine2 + noise + kick;
        }
        return s;
    }
    }
}

} // namespace sd
