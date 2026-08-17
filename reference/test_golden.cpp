// test_golden.cpp — 26.08.13 golden model 自测 + 导出
// 用法:
//   ./test_golden              # 自测: 验证各 band 有输出 + 能量守恒 + deno_th==0 特例
//   ./test_golden dump <bin>   # 导出指定 band 的样本到 stdout (CSV), 供对拍
//      <bin>: 0=Dry, 1..10 = Band1..10
//   ./test_golden frames <stim> [firstFrame] [numFrames] [blur] [perc] [detail]
//                              [gate] [threshold] [spacing] [focus] [tilt] [rise] [fall]
//                              # 逐帧导出 Analysis+HpssCore+Cepstrum+MaskGen 中间量
//                              # 列: frame,k,magL,magR,harmRaw,percMask,env,m1..m9
//      <stim>: 0=220+440+噪声+kick 1=对数扫频 2=白噪 3=冲击串 4=微电平
//              5=立体声白噪 6=1kHz 正弦（mask=1 恒等重构对拍用）
//   ./test_golden stream <stim> <out.csv> [blur] [perc] [detail] [gate] [threshold]
//                              [spacing] [focus] [tilt] [rise] [fall] [dry] [b1..b10]
//                              # 逐样本导出 25 列 (P2 尾声 Synth/OutputGate/Dry 对拍 oracle):
//                              #   i,inL,inR,dryL,dryR,b1L,b1R,...,b10L,b10R
//   ./test_golden switches <out.csv>
//                              # band 开关序列导出 (OutputGate 100ms 淡入淡出对拍;
//                              # 事件脚本与 plugin/test/ab_stream.cpp 完全一致, docs/06 §2.3)
// 默认参数 = 26.08.13 运行时默认 (docs/00 §7); dry 自测时置 0.5 以便观察。
#include "FFT.h"
#include "Params.h"
#include "HpssCore.h"
#include "MaskGen.h"
#include "Cepstrum.h"
#include "SpectralSeparator.h"
#include "Stimuli.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>

using namespace sd;

static float rms(const float* p, int n) {
    double s = 0; for (int i = 0; i < n; ++i) s += (double)p[i]*p[i];
    return (float)std::sqrt(s/n);
}
static bool hasNaN(const float* p, int n) {
    for (int i = 0; i < n; ++i) if (std::isnan(p[i]) || std::isinf(p[i])) return true;
    return false;
}

// 能量守恒 + deno_th==0 特例单元检查（作用于 MaskGen 级）
static int checkInvariants()
{
    Radix2FFT fft(kFFTSize);
    MaskGen mg;
    mg.prepare(44100.0);
    Cepstrum cep;
    cep.prepare(&fft, 44100.0);

    // 合成一个平缓的谐波幅度谱（原始量级）：形状 + 若干峰
    std::vector<float> mag(kNumBins);
    for (int k = 0; k < kNumBins; ++k) {
        float f = (float)k;
        float envBase = 200.0f * std::exp(-f / 300.0f) + 20.0f;
        float peak = (k % 32 == 5) ? 1500.0f : 0.0f;
        mag[k] = envBase + peak;
    }
    std::vector<float> env(kNumBins);
    cep.compute(mag.data(), env.data(), 1.0f);   // 26.08.13 默认 detail=1.0

    Params p = defaultParams();
    p.gate = 0.0f;   // Δ3: deno_th==0 → gate=1

    float maxSumErr = 0.0f;
    bool gateOneOk = true;
    for (int frame = 0; frame < 2000; ++frame) {
        for (int k = 0; k < kNumBins; ++k) {
            float masks[9];
            mg.processBin(mag[k], env[k], k, p, masks);
            float sum18 = 0.0f;
            for (int b = 0; b < 8; ++b) sum18 += masks[b];
            // 能量守恒: Σ Band1..8 = gate; Band9 = 1 - gate  ⇒ Σ 1..9 = 1
            float sum19 = sum18 + masks[8];
            float err = std::abs(sum19 - 1.0f);
            if (err > maxSumErr) maxSumErr = err;
            // deno_th==0: gate_smooth 应收敛到 1 → Band9 → 0, Σ1..8 → 1
            if (frame > 100) {
                if (masks[8] > 1e-3f) gateOneOk = false;
                if (std::abs(sum18 - 1.0f) > 1e-3f) gateOneOk = false;
            }
        }
    }
    std::printf("  [能量守恒] Σ mask1..9 与 1 的最大偏差 = %.2e  %s\n",
                maxSumErr, maxSumErr < 1e-5f ? "[通过]" : "[失败]");
    std::printf("  [deno_th==0 特例] gate=1, Band9→0, ΣBand1..8→1  %s\n",
                gateOneOk ? "[通过]" : "[失败]");
    return (maxSumErr < 1e-5f && gateOneOk) ? 0 : 1;
}

int main(int argc, char** argv)
{
    const double sr = 44100.0;
    const int block = 1024;
    const int total = (int)sr * 2;  // 2 秒

    // ============================================================
    // frames 模式: 逐帧导出 Analysis+HpssCore 中间量 (P2 对拍 oracle)
    // ============================================================
    if (argc >= 2 && std::strcmp(argv[1], "frames") == 0) {
        int stimIdx = (argc > 2) ? std::atoi(argv[2]) : 0;
        int firstFrame = (argc > 3) ? std::atoi(argv[3]) : 0;
        int numFrames = (argc > 4) ? std::atoi(argv[4]) : 0;   // 0 = 全部
        float blur = (argc > 5) ? (float)std::atof(argv[5]) : 0.05f;
        float perc = (argc > 6) ? (float)std::atof(argv[6]) : 1.5f;
        float detail = (argc > 7) ? (float)std::atof(argv[7]) : 1.0f;

        Radix2FFT fftA(kFFTSize), fftC(kFFTSize), fftS(kFFTSize);
        SpectralSeparator engine;
        engine.prepare(&fftA, &fftC, &fftS, sr, block);

        Params p = defaultParams();
        p.blur = blur;
        p.perc = perc;
        p.detail = detail;
        p.dry = 0.0f;
        if (argc > 8)  p.gate       = (float)std::atof(argv[8]);
        if (argc > 9)  p.threshold  = (float)std::atof(argv[9]);
        if (argc > 10) p.spacing    = (float)std::atof(argv[10]);
        if (argc > 11) p.focus      = (float)std::atof(argv[11]);
        if (argc > 12) p.tilt       = (float)std::atof(argv[12]);
        if (argc > 13) p.slideRise  = (float)std::atof(argv[13]);
        if (argc > 14) p.slideFall  = (float)std::atof(argv[14]);

        Stim stim = makeStim(stimIdx, sr, total);

        std::vector<float> dryL(block, 0), dryR(block, 0);
        std::vector<float> bandL[10], bandR[10];
        for (int b = 0; b < 10; ++b) { bandL[b].assign(block, 0); bandR[b].assign(block, 0); }

        std::printf("frame,k,magL,magR,harmRaw,percMask,env,m1,m2,m3,m4,m5,m6,m7,m8,m9\n");
        int dumped = 0, frameSeen = -1;
        for (int off = 0; off + block <= total; off += block) {
            float* bL[10]; float* bR[10];
            for (int b = 0; b < 10; ++b) { bL[b] = bandL[b].data(); bR[b] = bandR[b].data(); }
            engine.process(stim.L.data() + off, stim.R.data() + off, block, p,
                           dryL.data(), dryR.data(), bL, bR);
            while (frameSeen + 1 < engine.frameCount()) {   // 帧索引 0..frameCount-1
                ++frameSeen;
                if (frameSeen < firstFrame) continue;
                if (numFrames > 0 && dumped >= numFrames) break;
                const float* magL = engine.getMagL();
                const float* magR = engine.getMagR();
                const float* harm = engine.getHarmRaw();
                const float* pmask = engine.getPercMask();
                const float* env = engine.getEnv();
                const float* m[9];
                for (int b = 0; b < 9; ++b) m[b] = engine.getBandMask(b);
                for (int k = 0; k < kNumBins; ++k)
                    std::printf("%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g"
                                ",%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
                                frameSeen, k, magL[k], magR[k], harm[k], pmask[k],
                                env[k], m[0][k], m[1][k], m[2][k], m[3][k], m[4][k],
                                m[5][k], m[6][k], m[7][k], m[8][k]);
                ++dumped;
            }
        }
        std::fprintf(stderr, "stim=%d (%s) frames=%d dumped=%d\n",
                     stimIdx, stim.name, engine.frameCount(), dumped);
        return 0;
    }

    // ---- stream/switches 共用行写出 (25 列) ----
    struct StreamWriter {
        static void row(FILE* f, int off,
                        const float* inL, const float* inR,
                        const float* dryL, const float* dryR,
                        float* const* bandL, float* const* bandR, int n)
        {
            for (int i = 0; i < n; ++i) {
                std::fprintf(f, "%d,%.8g,%.8g,%.8g,%.8g", off + i, inL[i], inR[i],
                             dryL[i], dryR[i]);
                for (int b = 0; b < kNumBands; ++b)
                    std::fprintf(f, ",%.8g,%.8g", bandL[b][i], bandR[b][i]);
                std::fprintf(f, "\n");
            }
        }
        static void header(FILE* f)
        {
            std::fprintf(f, "i,inL,inR,dryL,dryR,b1L,b1R,b2L,b2R,b3L,b3R,b4L,b4R,"
                            "b5L,b5R,b6L,b6R,b7L,b7R,b8L,b8R,b9L,b9R,b10L,b10R\n");
        }
    };

    // ============================================================
    // stream 模式: 逐样本导出 25 列（P2 尾声 Synth/OutputGate/Dry 对拍 oracle）
    // ============================================================
    if (argc >= 3 && std::strcmp(argv[1], "stream") == 0) {
        int stimIdx = std::atoi(argv[2]);
        const char* outPath = argv[3];

        Params p = defaultParams();
        if (argc > 4)  p.blur       = (float)std::atof(argv[4]);
        if (argc > 5)  p.perc       = (float)std::atof(argv[5]);
        if (argc > 6)  p.detail     = (float)std::atof(argv[6]);
        if (argc > 7)  p.gate       = (float)std::atof(argv[7]);
        if (argc > 8)  p.threshold  = (float)std::atof(argv[8]);
        if (argc > 9)  p.spacing    = (float)std::atof(argv[9]);
        if (argc > 10) p.focus      = (float)std::atof(argv[10]);
        if (argc > 11) p.tilt       = (float)std::atof(argv[11]);
        if (argc > 12) p.slideRise  = (float)std::atof(argv[12]);
        if (argc > 13) p.slideFall  = (float)std::atof(argv[13]);
        if (argc > 14) p.dry        = (float)std::atof(argv[14]);
        for (int b = 0; b < kNumBands && argc > 15 + b; ++b)
            p.bandOn[b] = std::atoi(argv[15 + b]) != 0;

        Radix2FFT fftA(kFFTSize), fftC(kFFTSize), fftS(kFFTSize);
        SpectralSeparator engine;
        engine.prepare(&fftA, &fftC, &fftS, sr, block);

        Stim stim = makeStim(stimIdx, sr, total);

        std::vector<float> dryL(block, 0), dryR(block, 0);
        std::vector<float> bandL[kNumBands], bandR[kNumBands];
        for (int b = 0; b < kNumBands; ++b) {
            bandL[b].assign(block, 0);
            bandR[b].assign(block, 0);
        }

        FILE* f = std::fopen(outPath, "w");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", outPath); return 1; }
        StreamWriter::header(f);
        for (int off = 0; off < total; off += block) {
            int n = std::min(block, total - off);
            float* bL[kNumBands];
            float* bR[kNumBands];
            for (int b = 0; b < kNumBands; ++b) { bL[b] = bandL[b].data(); bR[b] = bandR[b].data(); }
            engine.process(stim.L.data() + off, stim.R.data() + off, n, p,
                           dryL.data(), dryR.data(), bL, bR);
            StreamWriter::row(f, off, stim.L.data() + off, stim.R.data() + off,
                              dryL.data(), dryR.data(), bL, bR, n);
        }
        std::fclose(f);
        std::fprintf(stderr, "stream: stim=%d (%s) samples=%d\n",
                     stimIdx, stim.name, total);
        return 0;
    }

    // ============================================================
    // switches 模式: band 开关序列（OutputGate 100ms 淡入淡出对拍）
    // 事件脚本与 plugin/test/ab_stream.cpp 完全一致（docs/06 §2.3）:
    //   stim 0, 默认参数, dry=0.5; 初始 band1=0 其余 1
    //   t=0.5s (i=22050): band1 0→1
    //   t=1.0s (i=44100): band5 1→0
    //   t=1.5s (i=66150): band1 1→0 且 band5 0→1
    // ============================================================
    if (argc >= 3 && std::strcmp(argv[1], "switches") == 0) {
        const char* outPath = argv[2];
        Params p = defaultParams();
        p.dry = 0.5f;
        p.bandOn[0] = false;   // band1 初始关

        struct Ev { int at; int band; bool on; };
        const Ev evts[] = {
            {22050, 0, true},    // t=0.5s: band1 0→1
            {44100, 4, false},   // t=1.0s: band5 1→0
            {66150, 0, false},   // t=1.5s: band1 1→0
            {66150, 4, true},    // t=1.5s: band5 0→1
        };
        const int nEvts = 4;

        Radix2FFT fftA(kFFTSize), fftC(kFFTSize), fftS(kFFTSize);
        SpectralSeparator engine;
        engine.prepare(&fftA, &fftC, &fftS, sr, block);

        Stim stim = makeStim(0, sr, total);

        std::vector<float> dryL(block, 0), dryR(block, 0);
        std::vector<float> bandL[kNumBands], bandR[kNumBands];
        for (int b = 0; b < kNumBands; ++b) {
            bandL[b].assign(block, 0);
            bandR[b].assign(block, 0);
        }

        FILE* f = std::fopen(outPath, "w");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", outPath); return 1; }
        StreamWriter::header(f);

        int pos = 0, ev = 0;
        while (pos < total) {
            int end = total;
            if (ev < nEvts) end = evts[ev].at;   // 本段 [pos, end) 用当前 bandOn
            while (pos < end) {
                int n = std::min(block, end - pos);
                float* bL[kNumBands];
                float* bR[kNumBands];
                for (int b = 0; b < kNumBands; ++b) { bL[b] = bandL[b].data(); bR[b] = bandR[b].data(); }
                engine.process(stim.L.data() + pos, stim.R.data() + pos, n, p,
                               dryL.data(), dryR.data(), bL, bR);
                StreamWriter::row(f, pos, stim.L.data() + pos, stim.R.data() + pos,
                                  dryL.data(), dryR.data(), bL, bR, n);
                pos += n;
            }
            // 应用恰好发生在 pos 的事件（生效于 pos 样本）
            while (ev < nEvts && evts[ev].at == pos) {
                p.bandOn[evts[ev].band] = evts[ev].on;
                ++ev;
            }
        }
        std::fclose(f);
        std::fprintf(stderr, "switches: samples=%d\n", total);
        return 0;
    }

    Radix2FFT fftA(kFFTSize), fftC(kFFTSize), fftS(kFFTSize);

    SpectralSeparator engine;
    engine.prepare(&fftA, &fftC, &fftS, sr, block);

    Params p = defaultParams();   // 26.08.13 运行时默认 (band 全开, dry 0)
    p.dry = 0.5f;                 // 自测观察用 (非默认)

    // 测试信号: 220Hz+440Hz 正弦 + 噪声 + 周期 kick (与旧 test_golden 相同)
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

    double rmsAcc[11] = {};
    int blocks = 0;

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
        for (int b = 0; b < 10; ++b) if (hasNaN(bL[b], block)) { std::printf("NaN in band %d!\n",b+1); return 1; }

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
        int skip = (int)(sr * 0.2 / block) * block;
        for (int i = skip; i < (int)dumped.size(); ++i)
            std::printf("%d,%.8f\n", i - skip, dumped[i]);
        return 0;
    }

    std::printf("=== Spectral Dissector 26.08.13 golden model ===\n");
    std::printf("FFT=%d hop=%d bins=%d bands=%d  采样率=%.0f  信号=2s(220+440Hz+噪声+kick)\n",
                kFFTSize, kHop, kNumBins, kNumBands, sr);
    std::printf("参数默认: spacing=%.1f focus=%.1f detail=%.1f rise=%.0f fall=%.0f (26.08.13 UI 初始)\n\n",
                p.spacing, p.focus, p.detail, p.slideRise, p.slideFall);

    const char* nm[11] = {"Dry","Band 1","Band 2","Band 3","Band 4","Band 5",
                          "Band 6","Band 7","Band 8","Residual","Perc"};
    int active = 0;
    for (int b = 0; b < 11; ++b) {
        float r = (float)(rmsAcc[b] / blocks);
        std::printf("  %-10s RMS=%.6f  %s\n", nm[b], r, r > 1e-6f ? "[有输出]" : "[静音]");
        if (b > 0 && r > 1e-6f) ++active;
    }
    std::printf("\n");

    int inv = checkInvariants();

    std::printf("\n%s: %d/10 band 有输出 (band 全开默认)\n",
        active >= 5 ? "OK" : "WARN", active);
    int rc = (active >= 5 && inv == 0) ? 0 : 1;
    std::printf("自测结果: %s\n", rc == 0 ? "通过" : "失败");
    return rc;
}
