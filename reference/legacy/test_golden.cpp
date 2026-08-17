// test_golden.cpp — 参考实现自测 + 生成 golden 输出 (供 FPGA 仿真对比)
// 用法:
//   ./test_golden              # 自测: 验证各 band 有输出
//   ./test_golden dump <bin>   # 导出指定 band 的样本到 stdout (CSV), 供对比
#include "FFT.h"
#include "Params.h"
#include "SpectralSeparator.h"
#include <cstdio>
#include <cmath>
#include <cstring>

using namespace sd_legacy;

static float rms(const float* p, int n) {
    double s = 0; for (int i = 0; i < n; ++i) s += (double)p[i]*p[i];
    return (float)std::sqrt(s/n);
}
static bool hasNaN(const float* p, int n) {
    for (int i = 0; i < n; ++i) if (std::isnan(p[i]) || std::isinf(p[i])) return true;
    return false;
}

int main(int argc, char** argv)
{
    const double sr = 44100.0;
    const int block = 1024;
    const int total = (int)sr * 2;  // 2 秒

    // FFT 实例 (时分复用: 分析/倒谱/合成共用, 参考实现线程安全单线程)
    Radix2FFT fftA(kFFTSize), fftC(kFFTSize), fftS(kFFTSize);

    SpectralSeparator engine;
    engine.prepare(&fftA, &fftC, &fftS, sr, block);

    Params p = defaultParams();
    p.dry = 0.5f;

    // 测试信号: 220Hz 正弦 + 噪声 + 周期 kick
    std::vector<float> inL(total), inR(total);
    for (int i = 0; i < total; ++i) {
        double t = (double)i / sr;
        float sine  = 0.4f * std::sin(2.0 * M_PI * 220.0 * t);
        float sine2 = 0.2f * std::sin(2.0 * M_PI * 440.0 * t);
        float noise = 0.05f * ((float)std::rand() / (float)RAND_MAX * 2.f - 1.f);
        float kick = 0.f;
        int phase = i % (int)(sr * 0.5);
        if (phase < 200) kick = 0.6f * std::exp(-(float)phase/60.f) * std::sin(2.0f*M_PI*60.f*t);
        inL[i] = inR[i] = sine + sine2 + noise + kick;
    }

    std::vector<float> dryL(block,0), dryR(block,0);
    std::vector<float> bandL[10], bandR[10];
    for (int b = 0; b < 10; ++b) { bandL[b].assign(block,0); bandR[b].assign(block,0); }

    double rmsAcc[11] = {};  // dry + 10 bands
    int blocks = 0;

    // dump 模式: 收集指定 band 全部样本
    int dumpBand = -1;
    bool dump = (argc >= 3 && std::strcmp(argv[1], "dump") == 0);
    if (dump) dumpBand = std::atoi(argv[2]);
    std::vector<float> dumped;

    for (int off = 0; off + block <= total; off += block) {
        float* bL[10]; float* bR[10];
        for (int b = 0; b < 10; ++b) { bL[b] = bandL[b].data(); bR[b] = bandR[b].data(); }
        engine.process(inL.data()+off, inR.data()+off, block, p,
                       dryL.data(), dryR.data(), bL, bR);

        if (hasNaN(dryL.data(), block)) { std::printf("NaN in dry!\n"); return 1; }
        for (int b = 0; b < 10; ++b) if (hasNaN(bL[b], block)) { std::printf("NaN in band %d!\n",b); return 1; }

        if (off > sr * 0.2) {  // 跳过 warmup
            rmsAcc[0] += rms(dryL.data(), block);
            for (int b = 0; b < 10; ++b) rmsAcc[1+b] += rms(bL[b], block);
            ++blocks;
        }
        if (dump && dumpBand >= 0) {
            const float* srcPtr = (dumpBand == 0) ? dryL.data() : bL[dumpBand-1];
            for (int i = 0; i < block; ++i) dumped.push_back(srcPtr[i]);
        }
    }

    if (dump) {
        // CSV: index,sample  (只导出 L 通道, 跳过前 0.2s warmup)
        int skip = (int)(sr * 0.2 / block) * block;
        for (int i = skip; i < (int)dumped.size(); ++i)
            std::printf("%d,%.8f\n", i - skip, dumped[i]);
        return 0;
    }

    std::printf("=== Spectral Dissector 参考实现 (golden model) ===\n");
    std::printf("FFT=%d hop=%d bins=%d bands=%d  采样率=%.0f  信号=2s(220Hz+噪声+kick)\n\n",
                kFFTSize, kHop, kNumBins, kNumBands, sr);
    const char* nm[11] = {"Dry","Band 1","Band 2","Band 3","Band 4","Band 5",
                          "Band 6","Band 7","Band 8","Residual","Perc"};
    int active = 0;
    for (int b = 0; b < 11; ++b) {
        float r = (float)(rmsAcc[b] / blocks);
        std::printf("  %-10s RMS=%.6f  %s\n", nm[b], r, r > 1e-6f ? "[有输出]" : "[静音]");
        if (b > 0 && r > 1e-6f) ++active;
    }
    std::printf("\n%s: %d/10 band 有输出\n",
        active >= 5 ? "OK" : "WARN", active);
    return active >= 5 ? 0 : 1;
}
