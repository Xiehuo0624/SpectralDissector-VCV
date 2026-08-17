// ============================================================
// STFT.h — 分析器 / 合成器 (平台无关, 用 FFT 接口)
// ------------------------------------------------------------
// 分析: 样本流 → 每跳一帧频谱 (mag/phase, L/R), Hann 窗, overlap 4
//   mag 归一化为 "真实幅度" (单频满幅 → bin 幅度 ≈ 1.0)
// 合成: 复数谱 → IFFT → 加窗 → overlap-add → 样本流 (双声道)
//
// Tiliqua 映射:
//   Analysis  ≈ dsp.fft.STFTAnalyzer (sz=N, Hann) + RectToPolarCordic
//   Synth     ≈ dsp.fft.STFTSynthesizer (sz=N, Hann)
//   注意 Tiliqua STFT 用 SQRT_HANN 做完美重建; 此处用 Hann + 手算
//   归一化增益, 二者重建条件不同, 详见 PORTING_GUIDE。
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

namespace sd_legacy {

class Analysis
{
public:
    // 注入 FFT 实例 (Tiliqua 侧为硬件 FFT; 参考实现为 Radix2FFT)
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
    float normGain = 1.0f;                      // 真实幅度归一化 = 4/N
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
    float normGain = 1.0f;                      // = N/(2*sumOverlap(w)) = N/4
};

} // namespace sd_legacy
