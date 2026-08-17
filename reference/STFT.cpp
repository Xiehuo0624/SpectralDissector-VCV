// STFT.cpp — 分析/合成实现（26.08.13 口径，见 STFT.h 头注）
#include "STFT.h"

namespace sd {

static std::vector<float> makeHannWindow(int N)
{
    std::vector<float> w(N);
    for (int n = 0; n < N; ++n)
        w[n] = 0.5f - 0.5f * std::cos(2.0f * (float)M_PI * (float)n / (float)N);
    return w;
}

// ============================================================
// Analysis
// ============================================================
void Analysis::prepare(FFT* fft, double sampleRate)
{
    fft_ = fft;
    sampleRate_ = sampleRate;
    window = makeHannWindow(kFFTSize);
    bufL.assign(kFFTSize, 0.0f);
    bufR.assign(kFFTSize, 0.0f);
    fftIn.assign(kFFTSize, std::complex<float>{});
    fftOut.assign(kFFTSize, std::complex<float>{});
    magL.assign(kNumBins, 0.0f);
    magR.assign(kNumBins, 0.0f);
    phaseL.assign(kNumBins, 0.0f);
    phaseR.assign(kNumBins, 0.0f);
    writePos = 0;
    samplesUntilFrame = kHop;
    // 26.08.13: mag 保持「未归一化原始 FFT 幅值」(= Max pfft~ fftin~ 口径)，
    // 归一化 (/(fft_size*0.5)) 由 HpssCore/MaskGen 内部完成。
}

bool Analysis::pushSample(float inL, float inR)
{
    bufL[writePos] = inL;
    bufR[writePos] = inR;
    writePos = (writePos + 1) % kFFTSize;
    if (--samplesUntilFrame <= 0) {
        samplesUntilFrame = kHop;
        processFrame();
        return true;
    }
    return false;
}

void Analysis::processFrame()
{
    int start = writePos;  // 指向最老样本
    for (int n = 0; n < kFFTSize; ++n) {
        int idx = (start + n) % kFFTSize;
        fftIn[n] = std::complex<float>(bufL[idx] * window[n], 0.0f);
    }
    fft_->perform(fftIn.data(), fftOut.data(), false);  // forward, 无归一化
    for (int k = 0; k < kNumBins; ++k) {
        std::complex<float> c = fftOut[k];
        magL[k]   = std::abs(c);
        phaseL[k] = std::arg(c);
    }
    for (int n = 0; n < kFFTSize; ++n) {
        int idx = (start + n) % kFFTSize;
        fftIn[n] = std::complex<float>(bufR[idx] * window[n], 0.0f);
    }
    fft_->perform(fftIn.data(), fftOut.data(), false);
    for (int k = 0; k < kNumBins; ++k) {
        std::complex<float> c = fftOut[k];
        magR[k]   = std::abs(c);
        phaseR[k] = std::arg(c);
    }
    // Max pfft~ 对 Nyquist bin 输入/输出置零（golden 同步）。
    magL[kNumBins - 1] = 0.0f;
    magR[kNumBins - 1] = 0.0f;
    phaseL[kNumBins - 1] = 0.0f;
    phaseR[kNumBins - 1] = 0.0f;
}

// ============================================================
// Synth
// ============================================================
void Synth::prepare(FFT* fft, double sampleRate)
{
    fft_ = fft;
    sampleRate_ = sampleRate;
    window = makeHannWindow(kFFTSize);
    fftIn.assign(kFFTSize, std::complex<float>{});
    fftOut.assign(kFFTSize, std::complex<float>{});
    outBufL.assign(kFFTSize * 2, 0.0f);
    outBufR.assign(kFFTSize * 2, 0.0f);
    readPos = 0;

    // 恒等重构推导（spec = mask × X_raw，mask=1 → 输出 = 输入）：
    //   ifft(1/N) → x·w；再 ×w → x·w²；4 帧 overlap-add Σw² = 1.5；
    //   故 normGain = 1/1.5 = 2/3（与 Max pfft~ fftout~ 净效果一致）。
    //   旧版用 (4/N 幅度口径, N/4 增益) 组合存在 1.5x 增益偏差，已修正。
    double sumW2 = 0.0;
    for (int n = 0; n < kHop; ++n) {
        double acc = 0.0;
        for (int f = 0; f < kOverlap; ++f) {
            int idx = (n + f * kHop) % kFFTSize;
            acc += (double)window[idx] * (double)window[idx];
        }
        sumW2 += acc;
    }
    sumW2 /= (double)kHop;              // = 1.5
    normGain = (float)(1.0 / sumW2);    // = 2/3
}

void Synth::reconstructChannel(const std::complex<float>* spec, std::vector<float>& outBuf)
{
    fftIn[0] = spec[0];
    for (int k = 1; k < kNumBins; ++k) {
        fftIn[k] = spec[k];
        fftIn[kFFTSize - k] = std::conj(spec[k]);
    }
    fft_->perform(fftIn.data(), fftOut.data(), true);  // inverse, 含 1/N

    int w = readPos;
    for (int n = 0; n < kFFTSize; ++n) {
        float s = fftOut[n].real() * window[n] * normGain;
        outBuf[(w + n) % (kFFTSize * 2)] += s;
    }
}

void Synth::processFrame(const std::complex<float>* specL, const std::complex<float>* specR)
{
    reconstructChannel(specL, outBufL);
    reconstructChannel(specR, outBufR);
    readPos = (readPos + kHop) % (kFFTSize * 2);
}

void Synth::pullSamples(float* dstL, float* dstR, int numSamples)
{
    int start = (readPos - kHop + kFFTSize * 2) % (kFFTSize * 2);
    for (int i = 0; i < numSamples; ++i) {
        int idx = (start + i) % (kFFTSize * 2);
        dstL[i] = outBufL[idx];
        dstR[i] = outBufR[idx];
        outBufL[idx] = 0.0f;
        outBufR[idx] = 0.0f;
    }
}

} // namespace sd
