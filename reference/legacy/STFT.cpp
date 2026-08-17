// STFT.cpp — 分析/合成实现 (与 VST3 版算法一致, 已含归一化修复)
#include "STFT.h"

namespace sd_legacy {

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
    // 真实幅度归一化: |X[k]| * 4/N (Hann 窗, forward FFT 无 1/N)
    normGain = 4.0f / (float)kFFTSize;
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
        magL[k]   = std::abs(c) * normGain;
        phaseL[k] = std::arg(c);
    }
    for (int n = 0; n < kFFTSize; ++n) {
        int idx = (start + n) % kFFTSize;
        fftIn[n] = std::complex<float>(bufR[idx] * window[n], 0.0f);
    }
    fft_->perform(fftIn.data(), fftOut.data(), false);
    for (int k = 0; k < kNumBins; ++k) {
        std::complex<float> c = fftOut[k];
        magR[k]   = std::abs(c) * normGain;
        phaseR[k] = std::arg(c);
    }
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

    // 完美重建归一化 (见 STFT.h 注释 + PORTING_GUIDE):
    //   out = mask*mag (mask=1 时). normGain = N / (2 * sumOverlap(w))
    //   Hann 75% overlap: sumOverlap(w) = 2, 故 normGain = N/4
    double sumOverlapW = 0.0;
    for (int n = 0; n < kHop; ++n) {
        double acc = 0.0;
        for (int f = 0; f < kOverlap; ++f) {
            int idx = (n + f * kHop) % kFFTSize;
            acc += (double)window[idx];
        }
        sumOverlapW += acc;
    }
    sumOverlapW /= (double)kHop;  // ≈ 2
    normGain = (float)((double)kFFTSize / (2.0 * sumOverlapW));  // = N/4 ≈ 1024
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

} // namespace sd_legacy
