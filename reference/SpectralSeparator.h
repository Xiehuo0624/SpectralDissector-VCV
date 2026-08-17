// ============================================================
// SpectralSeparator.h — 顶层 DSP 串联（26.08.13 语义）
// ------------------------------------------------------------
// Analysis → HpssCore → Cepstrum → MaskGen → 10×Synth → OutputGate
// 流式: 每 kHop 输入样本触发一帧频谱处理, 产 kHop 输出样本。
//
// 26.08.13 拓扑（fft2.maxpat, docs/00 §3）:
//   mag_raw = (magL+magR)*0.5 → HpssCore
//   HpssCore.out1 (谐波幅度·原始量级) → Cepstrum 与 MaskGen
//   HpssCore.out2 (打击掩膜)          → Band 10
//   Cepstrum.env → MaskGen → Band1..8 掩膜 + Band9 噪声掩膜
// 与旧版 (legacy/SpectralSeparator.*) 差异 = Δ1/Δ2/Δ5 + 幅度口径。
// ============================================================
#pragma once
#include <complex>
#include <vector>
#include "FFT.h"
#include "Params.h"
#include "STFT.h"
#include "Cepstrum.h"
#include "HpssCore.h"
#include "MaskGen.h"
#include "OutputGate.h"

namespace sd {

class SpectralSeparator
{
public:
    // fftA: 分析 FFT; fftC: 倒谱 FFT; fftS: 合成 FFT (10 band 时分复用)
    void prepare(FFT* fftA, FFT* fftC, FFT* fftS, double sampleRate, int maxBlock);
    void reset();
    void process(const float* inL, const float* inR, int numSamples,
                 const Params& p,
                 float* dryL, float* dryR,
                 float* bandL[10], float* bandR[10]);

    // ---- 帧级导出访问器 (P2 对拍用, test_golden frames 模式) ----
    int frameCount() const { return frameCount_; }
    const float* getMagL() const { return analysis.getMagL(); }
    const float* getMagR() const { return analysis.getMagR(); }
    const float* getHarmRaw() const { return harmRaw.data(); }
    const float* getPercMask() const { return percMask.data(); }
    const float* getEnv() const { return env.data(); }
    const float* getBandMask(int b) const { return bandMask[b].data(); }  // b=0..8 → Band1..9

private:
    void processFrame(const Params& p);

    Analysis  analysis;
    HpssCore  hpss;
    Cepstrum  cepstrum;
    MaskGen   maskGen;
    Synth     synths[kNumBands];
    OutputGate gates[kNumBands];

    std::vector<float> harmRaw;      // HpssCore out1 (每帧 kNumBins)
    std::vector<float> percMask;     // HpssCore out2 (每帧 kNumBins)
    std::vector<float> env;          // 倒谱包络 (每帧 kNumBins, 单声道)
    std::vector<float> bandMask[kNumBands - 1];   // MaskGen out1..9 (每帧 kNumBins)

    std::vector<std::complex<float>> specBandL[kNumBands];
    std::vector<std::complex<float>> specBandR[kNumBands];

    struct Fifo {
        std::vector<float> buf;
        int writeIdx = 0, readIdx = 0, count = 0;
        void resize(int n) { buf.assign(n, 0.f); writeIdx = readIdx = count = 0; }
        void push(const float* src, int n);
        float pop();
    };
    Fifo fifoL[kNumBands], fifoR[kNumBands];

    std::vector<float> dryDelayL, dryDelayR;
    int dryDelayIdx = 0;
    static constexpr int kDryDelay = kFFTSize;

    std::vector<float> tmpL, tmpR;
    double sampleRate_ = 44100.0;
    int frameCount_ = 0;   // 已完成的分析帧计数 (prepare/reset 归零)
};

} // namespace sd
