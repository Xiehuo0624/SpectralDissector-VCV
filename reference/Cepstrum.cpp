// Cepstrum.cpp — 倒谱包络实现（26.08.13 带切 lifter, Δ2/Δ6）
//
// 与旧版 (legacy/Cepstrum.cpp) 差异:
//   1. 输入语义: 谐波幅度·原始量级（HpssCore out1），而非 L/R 平均原始幅度 —— Δ2
//   2. lifter:  L = max(2,(1-detail)*(N/8)+1) 对称低通
//        → 26.08.13 带切: lifter(n) = (n < A) || (n >= B)
//          A = (1-d)*0.25*spesize,  B = (1-(1-d)*0.25)*spesize   —— Δ6
//      n >= 2048 段按直通(=1)处理: 记录在案的假设, 见 docs/00 §6.3 / docs/01 U1。
//   3. 数值口径不变: log(mag+eps) 保护、IFFT 含 1/N、FFT 不归一化、
//      env = exp(fft(lifter(ifft(log(mag))))) 同尺度往返。
#include "Cepstrum.h"
#include <cmath>

namespace sd {

void Cepstrum::prepare(FFT* fft, double /*sampleRate*/)
{
    fft_ = fft;
    fftIn.assign(kFFTSize, std::complex<float>{});
    fftOut.assign(kFFTSize, std::complex<float>{});
    cepstrum.assign(kFFTSize, 0.0f);
}

void Cepstrum::compute(const float* mag, float* env, float detail, float spesize)
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

    // 2. IFFT → 倒谱 (inverse 含 1/N)
    fft_->perform(fftIn.data(), fftOut.data(), true);
    for (int n = 0; n < kFFTSize; ++n)
        cepstrum[n] = fftOut[n].real();

    // 3. lifter (26.08.13): 切中段 [A, B)，保留 [0, A) 与 [B, N)
    //    A = ((1-detail)*0.25)*spesize ;  B = (1-(1-detail)*0.25)*spesize
    //    比较器只由 bin 索引斜坡 (0..N/2-1) 驱动；倒谱偶对称 →
    //    另一侧按对称镜像做高/低通（用户确认的语义, 2026-08-16, docs/00 §6.3）:
    //    lifter(n) = comparator(min(n, N-n))
    //    切带 = [A, B) 及其镜像 (N-B, N-A]
    float A = (1.0f - detail) * 0.25f * spesize;
    float B = (1.0f - (1.0f - detail) * 0.25f) * spesize;
    for (int n = 0; n < kFFTSize; ++n) {
        float q = (n <= kFFTSize / 2) ? (float)n : (float)(kFFTSize - n);
        if (!((q < A) || (q >= B)))
            cepstrum[n] = 0.0f;
    }

    // 4. FFT(cep_lif) → 平滑 log 包络 (forward 不归一化, 与 inverse /N 配合恒等)
    for (int n = 0; n < kFFTSize; ++n)
        fftIn[n] = std::complex<float>(cepstrum[n], 0.0f);
    fft_->perform(fftIn.data(), fftOut.data(), false);

    // 5. env = exp(log_env)
    for (int k = 0; k < kNumBins; ++k)
        env[k] = std::exp(fftOut[k].real());
}

} // namespace sd
