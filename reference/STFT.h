// ============================================================
// STFT.h — 分析器 / 合成器（26.08.13 口径 golden model）
// ------------------------------------------------------------
// 分析: 样本流 → 每跳一帧频谱 (mag/phase, L/R), Hann 窗, overlap 4
//   mag 为「未归一化原始 FFT 幅值」（Max pfft~ fftin~ 口径：
//   单频满幅正弦、Hann 窗 → 中心 bin 幅度 = A*N/4）。
//   gen~ 内部再除以 fft_size*0.5 归一化（HpssCore/MaskGen）。
// 合成: 复数谱 → IFFT → 加窗 → overlap-add → 样本流 (双声道)
//
// 与旧版（reference/legacy/STFT.*）差异（见 docs/02）：
//   Analysis normGain  4/N → 1.0（原始幅度口径，对齐 Max pfft~）
//   Synth    normGain  N/4 → 2/3（恒等重构；旧版存在 1.5x 增益偏差）
//   Analysis 对 Nyquist bin(k=N/2) 置零（Max pfft~ 行为）
// ============================================================
#pragma once
#include <complex>
#include <vector>
#include <cmath>
#include "FFT.h"
#include "Params.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sd {

class Analysis
{
public:
    void prepare(FFT* fft, double sampleRate);
    bool pushSample(float inL, float inR);  // true = 新一帧可用
    const float* getMagL()   const { return magL.data(); }
    const float* getMagR()   const { return magR.data(); }
    const float* getPhaseL() const { return phaseL.data(); }
    const float* getPhaseR() const { return phaseR.data(); }
    int getHop() const { return kHop; }

private:
    void processFrame();
    FFT* fft_ = nullptr;
    std::vector<float> window;                  // Hann, size N
    std::vector<float> bufL, bufR;              // 输入环形缓冲 (size N)
    int writePos = 0;
    int samplesUntilFrame = kHop;
    std::vector<std::complex<float>> fftIn, fftOut;
    std::vector<float> magL, magR, phaseL, phaseR;
    double sampleRate_ = 44100.0;
};

class Synth
{
public:
    void prepare(FFT* fft, double sampleRate);
    void processFrame(const std::complex<float>* specL, const std::complex<float>* specR);
    void pullSamples(float* dstL, float* dstR, int numSamples);

private:
    void reconstructChannel(const std::complex<float>* spec, std::vector<float>& outBuf);
    FFT* fft_ = nullptr;
    std::vector<float> window;                  // 合成 Hann
    std::vector<std::complex<float>> fftIn, fftOut;
    std::vector<float> outBufL, outBufR;        // 输出环形缓冲 (size 2N, overlap-add)
    int readPos = 0;
    double sampleRate_ = 44100.0;
    float normGain = 2.0f / 3.0f;               // 恒等重构: 1/sum(w^2) = 2/3 (Hann, 75% overlap)
};

} // namespace sd
