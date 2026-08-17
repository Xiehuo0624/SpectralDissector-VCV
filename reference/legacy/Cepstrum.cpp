// Cepstrum.cpp — 倒谱包络实现 (已修复归一化: 不再多余 *1/N)
#include "Cepstrum.h"
#include <cmath>

namespace sd_legacy {

void Cepstrum::prepare(FFT* fft, double /*sampleRate*/)
{
    fft_ = fft;
    fftIn.assign(kFFTSize, std::complex<float>{});
    fftOut.assign(kFFTSize, std::complex<float>{});
    cepstrum.assign(kFFTSize, 0.0f);
}

void Cepstrum::compute(const float* mag, float* env, float detail)
{
    constexpr float eps = 1e-10f;
    // 1. log|X|, 共轭对称填到 N 点复数谱 (实偶 → IFFT 后为实)
    fftIn[0] = std::complex<float>(std::log(mag[0] + eps), 0.0f);
    for (int k = 1; k < kNumBins - 1; ++k) {
        float lm = std::log(mag[k] + eps);
        fftIn[k]             = std::complex<float>(lm, 0.0f);
        fftIn[kFFTSize - k]  = std::complex<float>(lm, 0.0f);
    }
    fftIn[kNumBins - 1] = std::complex<float>(std::log(mag[kNumBins - 1] + eps), 0.0f);

    // 2. IFFT → 倒谱
    //    JUCE/inverse FFT 已含 1/N 归一化。不能再手动 *invN,
    //    否则多除 N → env = mag^(1/N) ≈ 1 (已修过的 bug)。
    //    Tiliqua FFT inverse 不归一化, 需在这里手动 *1/N 补偿。
    fft_->perform(fftIn.data(), fftOut.data(), true);
    for (int n = 0; n < kFFTSize; ++n)
        cepstrum[n] = fftOut[n].real();

    // 3. lifter: 保留低 quefrency (两端, 实信号倒谱对称)
    int L = std::max(2, (int)((1.0f - detail) * (float)(kFFTSize / 8)) + 1);
    for (int n = L; n < kFFTSize - L; ++n)
        cepstrum[n] = 0.0f;

    // 4. FFT(cep_lif) → 平滑 log 包络 (forward 不归一化, 与 inverse /N 配合恒等)
    for (int n = 0; n < kFFTSize; ++n)
        fftIn[n] = std::complex<float>(cepstrum[n], 0.0f);
    fft_->perform(fftIn.data(), fftOut.data(), false);

    // 5. env = exp(log_env)
    for (int k = 0; k < kNumBins; ++k)
        env[k] = std::exp(fftOut[k].real());
}

} // namespace sd_legacy
