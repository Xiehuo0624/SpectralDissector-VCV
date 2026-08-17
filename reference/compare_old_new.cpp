// compare_old_new.cpp — 旧版 vs 26.08.13 golden 的 A/B 量化证据
// ------------------------------------------------------------
// 把 reference/legacy（旧版算法, namespace sd_legacy）与
// reference/（26.08.13 golden, namespace sd）跑同一激励、同一参数，
// 输出每 band 的 RMS、最大绝对差、相对差；另含 Δ6 lifter env 对拍与
// Δ3 deno_th==0 特例对照。结果写入 docs/02_golden_model_升级证据.md。
// 参数映射: 新旧字段同名同义（rise/fall 均为样本口径）。
#include "FFT.h"
#include "Params.h"
#include "HpssCore.h"
#include "MaskGen.h"
#include "Cepstrum.h"
#include "SpectralSeparator.h"
#include "legacy/FFT.h"
#include "legacy/Params.h"
#include "legacy/GenDsp.h"
#include "legacy/Cepstrum.h"
#include "legacy/SpectralSeparator.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

using namespace sd;

namespace {

float rmsOf(const std::vector<float>& v, int skip, int n) {
    double s = 0;
    for (int i = skip; i < skip + n; ++i) s += (double)v[i] * v[i];
    return (float)std::sqrt(s / n);
}
float maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b, int skip, int n) {
    float m = 0.f;
    for (int i = skip; i < skip + n; ++i) m = std::max(m, std::abs(a[i] - b[i]));
    return m;
}
bool anyNaN(const std::vector<float>& v) {
    for (float x : v) if (std::isnan(x) || std::isinf(x)) return true;
    return false;
}

struct Stim {
    std::string name;
    std::vector<float> L, R;
};

Stim makeToneNoiseKick(double sr, int total) {
    Stim s{"220+440Hz正弦+噪声+kick", std::vector<float>(total), std::vector<float>(total)};
    for (int i = 0; i < total; ++i) {
        double t = (double)i / sr;
        float sine  = 0.4f * std::sin(2.0 * M_PI * 220.0 * t);
        float sine2 = 0.2f * std::sin(2.0 * M_PI * 440.0 * t);
        float noise = 0.05f * ((float)std::rand() / (float)RAND_MAX * 2.f - 1.f);
        float kick = 0.f;
        int phase = i % (int)(sr * 0.5);
        if (phase < 200) kick = 0.6f * std::exp(-(float)phase/60.f) * std::sin(2.0*M_PI*60.f*t);
        s.L[i] = s.R[i] = sine + sine2 + noise + kick;
    }
    return s;
}

Stim makeSweep(double sr, int total) {
    Stim s{"对数扫频 20Hz-20kHz", std::vector<float>(total), std::vector<float>(total)};
    double f0 = 20.0, f1 = 20000.0;
    double phase = 0.0;
    for (int i = 0; i < total; ++i) {
        double t = (double)i / sr;
        // 对数扫频瞬时相位: 2π * f0 * (exp(t ln(f1/f0)/T) - 1) / ln(f1/f0)
        double T = (double)total / sr;
        double k = std::log(f1 / f0) / T;
        double ph = 2.0 * M_PI * f0 * (std::exp(k * t) - 1.0) / k;
        s.L[i] = s.R[i] = 0.3f * (float)std::sin(ph);
        (void)phase;
    }
    return s;
}

Stim makeNoise(int total) {
    Stim s{"白噪声", std::vector<float>(total), std::vector<float>(total)};
    for (int i = 0; i < total; ++i)
        s.L[i] = s.R[i] = 0.2f * ((float)std::rand() / (float)RAND_MAX * 2.f - 1.f);
    return s;
}

Stim makeImpulses(double sr, int total) {
    Stim s{"短冲击串 5Hz", std::vector<float>(total), std::vector<float>(total)};
    for (int i = 0; i < total; ++i) {
        int phase = i % (int)(sr / 5.0);
        s.L[i] = s.R[i] = (phase < 8) ? (0.9f * std::exp(-(float)phase / 4.f)) : 0.0f;
    }
    return s;
}

Stim makeMicro(double sr, int total) {
    Stim s{"微电平 1kHz 正弦 (-80dB)", std::vector<float>(total), std::vector<float>(total)};
    for (int i = 0; i < total; ++i)
        s.L[i] = s.R[i] = 1e-4f * std::sin(2.0 * M_PI * 1000.0 * (double)i / sr);
    return s;
}

// ---------- 引擎驱动（新旧同一模板） ----------
template <class Engine, class P>
void drive(Engine& engine, const P& p, const Stim& stim, int block, int total,
           std::vector<std::vector<float>>& outL)
{
    std::vector<float> dryL(block), dryR(block);
    std::vector<float> bL[10], bR[10];
    for (int b = 0; b < 10; ++b) { bL[b].assign(block, 0.f); bR[b].assign(block, 0.f); }
    for (int off = 0; off + block <= total; off += block) {
        float* bpL[10]; float* bpR[10];
        for (int b = 0; b < 10; ++b) { bpL[b] = bL[b].data(); bpR[b] = bR[b].data(); }
        engine.process(stim.L.data() + off, stim.R.data() + off, block, p,
                       dryL.data(), dryR.data(), bpL, bpR);
        outL[0].insert(outL[0].end(), dryL.begin(), dryL.end());
        for (int b = 0; b < 10; ++b)
            outL[1 + b].insert(outL[1 + b].end(), bL[b].begin(), bL[b].end());
    }
}

// 相同参数集：算法差异之外全部一致（默认值差异不参与——此处显式指定）
sd::Params newParams() {
    sd::Params p = sd::defaultParams();
    p.threshold = 0.f; p.spacing = 6.f; p.focus = 1.5f; p.tilt = 0.f;
    p.gate = 1.0f; p.blur = 0.05f; p.perc = 1.5f;
    p.slideRise = 5.f; p.slideFall = 25.f; p.detail = 0.3f;
    for (int i = 0; i < 7; ++i) p.off[i] = 0.f;
    p.dry = 0.5f;
    for (int i = 0; i < 10; ++i) p.bandOn[i] = true;
    return p;
}
sd_legacy::Params legacyParams() {
    sd_legacy::Params p = sd_legacy::defaultParams();
    p.threshold = 0.f; p.spacing = 6.f; p.focus = 1.5f; p.tilt = 0.f;
    p.gate = 1.0f; p.blur = 0.05f; p.perc = 1.5f;
    p.slideRise = 5.f; p.slideFall = 25.f; p.detail = 0.3f;
    for (int i = 0; i < 7; ++i) p.off[i] = 0.f;
    p.dry = 0.5f;
    for (int i = 0; i < 10; ++i) p.bandOn[i] = true;
    return p;
}

void reportStimulus(const Stim& s, const std::vector<std::vector<float>>& oldL,
                    const std::vector<std::vector<float>>& newL,
                    int sr, int block)
{
    int skip = (int)(sr * 0.2 / block) * block;   // 跳过 0.2s warmup
    int n = (int)oldL[0].size() - skip;
    const char* nm[11] = {"Dry","Band 1","Band 2","Band 3","Band 4","Band 5",
                          "Band 6","Band 7","Band 8","Residual","Perc"};
    std::printf("### %s\n", s.name.c_str());
    std::printf("| 输出 | 旧版 RMS | 26.08.13 RMS | max|Δ| | 相对 RMS 差 |\n");
    std::printf("|---|---|---|---|---|---:|\n");
    for (int b = 0; b < 11; ++b) {
        float ro = rmsOf(oldL[b], skip, n);
        float rn = rmsOf(newL[b], skip, n);
        float md = maxAbsDiff(oldL[b], newL[b], skip, n);
        float rel = (ro > 1e-9f) ? (rn - ro) / ro : 0.f;
        std::printf("| %-8s | %.6f | %.6f | %.6f | %+.1f%% |\n",
                    nm[b], ro, rn, md, rel * 100.f);
    }
    std::printf("\n");
}

} // namespace

int main()
{
    const double sr = 44100.0;
    const int block = 1024;
    const int total = (int)sr * 2;

    std::printf("# 旧版 vs 26.08.13 golden A/B（参数一致: thr=0 spacing=6 focus=1.5 "
                "gate=1 blur=0.05 perc=1.5 rise=5 fall=25 detail=0.3, band 全开）\n\n");

    // ---------- 全链路 A/B ----------
    {
        sd::Radix2FFT fftA(sd::kFFTSize), fftC(sd::kFFTSize), fftS(sd::kFFTSize);
        sd::SpectralSeparator engNew;
        engNew.prepare(&fftA, &fftC, &fftS, sr, block);
        sd_legacy::Radix2FFT lA(sd_legacy::kFFTSize), lC(sd_legacy::kFFTSize), lS(sd_legacy::kFFTSize);
        sd_legacy::SpectralSeparator engOld;
        engOld.prepare(&lA, &lC, &lS, sr, block);

        sd::Params pn = newParams();
        sd_legacy::Params po = legacyParams();

        std::vector<Stim> stims;
        stims.push_back(makeToneNoiseKick(sr, total));
        stims.push_back(makeSweep(sr, total));
        stims.push_back(makeNoise(total));
        stims.push_back(makeImpulses(sr, total));
        stims.push_back(makeMicro(sr, total));

        for (const auto& s : stims) {
            std::vector<std::vector<float>> oldL(11), newL(11);
            drive(engOld, po, s, block, total, oldL);
            drive(engNew, pn, s, block, total, newL);
            std::printf("NaN 检查: 旧版 %s, 26.08.13 %s\n",
                        anyNaN(oldL[0]) || anyNaN(oldL[10]) ? "[NaN!]" : "[干净]",
                        anyNaN(newL[0]) || anyNaN(newL[10]) ? "[NaN!]" : "[干净]");
            reportStimulus(s, oldL, newL, (int)sr, block);
            engOld.reset();
            engNew.reset();
        }
    }

    // ---------- Δ6: Cepstrum lifter env 对拍 ----------
    {
        std::printf("## Δ6 倒谱 lifter/env 对拍（同一谐波幅度谱, 不同 detail）\n\n");
        sd::Radix2FFT fftN(sd::kFFTSize);
        sd_legacy::Radix2FFT fftO(sd_legacy::kFFTSize);
        sd::Cepstrum cepN; cepN.prepare(&fftN, sr);
        sd_legacy::Cepstrum cepO; cepO.prepare(&fftO, sr);

        std::vector<float> mag(sd::kNumBins);
        for (int k = 0; k < sd::kNumBins; ++k) {
            float f = (float)k;
            mag[k] = 800.0f * std::exp(-f / 250.0f) + 30.0f
                   + ((k % 30 == 3) ? 2000.0f : 0.0f);
        }
        std::vector<float> envN(sd::kNumBins), envO(sd_legacy::kNumBins);
        const float details[3] = {0.0f, 0.3f, 1.0f};
        for (float d : details) {
            cepN.compute(mag.data(), envN.data(), d);
            cepO.compute(mag.data(), envO.data(), d);
            float maxRel = 0.f, sumRel = 0.f;
            for (int k = 1; k < sd::kNumBins; ++k) {
                float rel = std::abs(envN[k] - envO[k]) / std::max(std::abs(envO[k]), 1e-6f);
                maxRel = std::max(maxRel, rel);
                sumRel += rel;
            }
            std::printf("| detail=%.1f | env max 相对差=%.4f | env 平均相对差=%.4f |\n",
                        d, maxRel, sumRel / (sd::kNumBins - 1));
        }
        std::printf("\n（26.08.13 lifter = 带切 (n<A)||(n>=B)，旧版 = 对称低通 L=max(2,(1-d)*(N/8)+1)，差异随 detail 增大）\n\n");
    }

    // ---------- Δ3: deno_th==0 特例对照 ----------
    {
        std::printf("## Δ3 deno_th==0 → gate=1 特例对照\n\n");
        sd::MaskGen mg; mg.prepare(sr);
        sd_legacy::GenDsp gd; gd.prepare(sr);
        sd::Params pn = newParams(); pn.gate = 0.0f;
        sd_legacy::Params po = legacyParams(); po.gate = 0.0f;

        // 判别性输入: env = 4×mag → norm_mag ≈ 0.25
        // 旧版: v=clamp(0.375) → gate ≈ smoothstep(0.375) ≈ 0.32 → Band9 ≈ 0.68
        // 26.08.13: 特例 gate=1 → Band9 → 0, ΣBand1..8 → 1
        std::vector<float> mag(sd::kNumBins, 1000.0f);
        std::vector<float> env(sd::kNumBins, 4000.0f);
        float newBand9 = -1.f, oldBand9 = -1.f, newSum18 = 0.f;
        for (int frame = 0; frame < 500; ++frame) {
            for (int k = 0; k < sd::kNumBins; ++k) {
                float masksN[9], masksO[10];
                mg.processBin(mag[k], env[k], k, pn, masksN);
                gd.processBin(mag[k], env[k], k, po, masksO);
                if (frame == 499) {
                    if (k == 0) {
                        newBand9 = masksN[8];
                        oldBand9 = masksO[8];
                        for (int b = 0; b < 8; ++b) newSum18 += masksN[b];
                    }
                }
            }
        }
        std::printf("| 实现 | Band9(1-gate) 收敛值 | ΣBand1..8 收敛值 |\n");
        std::printf("|---|---|---|\n");
        std::printf("| 旧版 (无特例) | %.6f | — |\n", oldBand9);
        std::printf("| 26.08.13 (特例) | %.6f | %.6f |\n", newBand9, newSum18);
        std::printf("\n（26.08.13: gate 强制 1 → Band9 → 0；旧版无短路, gate 由 norm_mag 决定, 不为 1）\n");
    }

    return 0;
}
