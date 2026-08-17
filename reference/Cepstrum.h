// ============================================================
// Cepstrum.h — 倒谱包络（26.08.13 语义）
// ------------------------------------------------------------
// 输入 = HPSS 谐波幅度（原始量级，来自 HpssCore out1）—— Δ2
// 链路: log|mag| → IFFT → ×lifter(26.08.13 带切, Δ6) → FFT → exp → env
// lifter 闭式见 docs/00 §6（U1 定稿）。
// ============================================================
#pragma once
#include <complex>
#include <vector>
#include "FFT.h"
#include "Params.h"

namespace sd {

class Cepstrum
{
public:
    void prepare(FFT* fft, double sampleRate);
    // mag: kNumBins 谐波幅度·原始量级 (入); env: kNumBins 倒谱包络 (出, 同尺度)
    // detail: 0..1; spesize: 谱帧尺寸 (fftinfo~ 出口2 = 2049)
    void compute(const float* mag, float* env, float detail, float spesize = kSpesize);

private:
    FFT* fft_ = nullptr;
    std::vector<std::complex<float>> fftIn, fftOut;
    std::vector<float> cepstrum;
};

} // namespace sd
