// ============================================================
// SpectralSeparator.h — 顶层 DSP 串联 (平台无关)
// ------------------------------------------------------------
// Analysis → Cepstrum → GenDsp → 10×Synth → OutputGate
// 流式: 每 kHop 输入样本触发一帧频谱处理, 产 kHop 输出样本。
//
// Tiliqua 映射: 整体对应一个自定义 top-level core, 内部用
// STFTAnalyzer/STFTSynthesizer + 自定义频域处理。见 PORTING_GUIDE。
// ============================================================
#pragma once
#include <complex>
#include <vector>
#include "FFT.h"
#include "Params.h"
#include "STFT.h"
#include "Cepstrum.h"
#include "GenDsp.h"
#include "OutputGate.h"

namespace sd_legacy {

class SpectralSeparator
{
public:
    // fftA: 分析 FFT; fftC: 倒谱 FFT (可与分析同一实例时分复用);
    // fftS[10]: 每条 band 合成 FFT (或单实例时分复用)
    void prepare(FFT* fftA, FFT* fftC, FFT* fftS, double sampleRate, int maxBlock);
    void reset();
    void process(const float* inL, const float* inR, int numSamples,
                 const Params& p,
                 float* dryL, float* dryR,
                 float* bandL[10], float* bandR[10]);

private:
    void processFrame(const Params& p);

    Analysis analysis;
    Cepstrum cepstrum;
    GenDsp   genDsp;
    Synth    synths[kNumBands];
    OutputGate gates[kNumBands];

    std::vector<std::complex<float>> specL, specR;
    std::vector<std::complex<float>> specBandL[kNumBands];
    std::vector<std::complex<float>> specBandR[kNumBands];
    std::vector<float> envL, envR;

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
};

} // namespace sd_legacy
