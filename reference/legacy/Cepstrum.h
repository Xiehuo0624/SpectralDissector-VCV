// ============================================================
// Cepstrum.h — 倒谱包络 (移植自 Max p cepstrum)
// ------------------------------------------------------------
// log|mag| → IFFT → lifter → FFT → exp → 平滑包络 env
// 用作底噪基准线 (gen~ raw_env)。
// detail [0..1]: 越大 lifter 越短 → 包络越粗。
//
// Tiliqua 映射: 需自定义。复用 dsp.fft.FFT (时分复用正反向),
// log/exp 用 WaveShaper LUT 实现。详见 PORTING_GUIDE。
// ============================================================
#pragma once
#include <complex>
#include <vector>
#include "FFT.h"
#include "Params.h"

namespace sd_legacy {

class Cepstrum
{
public:
    void prepare(FFT* fft, double sampleRate);
    // mag: kNumBins 真实幅度 (入), env: kNumBins 倒谱包络 (出, 与 mag 同尺度)
    void compute(const float* mag, float* env, float detail);

private:
    FFT* fft_ = nullptr;
    std::vector<std::complex<float>> fftIn, fftOut;
    std::vector<float> cepstrum;
};

} // namespace sd_legacy
