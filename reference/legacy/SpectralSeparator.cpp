// SpectralSeparator.cpp — 顶层串联实现 (与 VST3 版一致)
#include "SpectralSeparator.h"
#include <cmath>

namespace sd_legacy {

void SpectralSeparator::Fifo::push(const float* src, int n)
{
    int size = (int)buf.size();
    for (int i = 0; i < n; ++i) {
        buf[writeIdx] = src[i];
        writeIdx = (writeIdx + 1) % size;
    }
    count += n;
    if (count > size) {
        int over = count - size;
        readIdx = (readIdx + over) % size;
        count = size;
    }
}

float SpectralSeparator::Fifo::pop()
{
    if (count <= 0) return 0.0f;
    int size = (int)buf.size();
    float v = buf[readIdx];
    readIdx = (readIdx + 1) % size;
    --count;
    return v;
}

void SpectralSeparator::prepare(FFT* fftA, FFT* fftC, FFT* fftS,
                                double sampleRate, int /*maxBlock*/)
{
    sampleRate_ = sampleRate;
    analysis.prepare(fftA, sampleRate);
    cepstrum.prepare(fftC, sampleRate);
    genDsp.prepare(sampleRate);
    for (auto& s : synths) s.prepare(fftS, sampleRate);
    for (auto& g : gates) g.prepare(sampleRate);

    int N = kNumBins;
    specL.assign(N, {});  specR.assign(N, {});
    envL.assign(N, 0.f);  envR.assign(N, 0.f);
    for (int b = 0; b < kNumBands; ++b) {
        specBandL[b].assign(N, {});
        specBandR[b].assign(N, {});
    }
    for (int b = 0; b < kNumBands; ++b) {
        fifoL[b].resize(kFFTSize * 2);
        fifoR[b].resize(kFFTSize * 2);
    }
    dryDelayL.assign(kDryDelay, 0.f);
    dryDelayR.assign(kDryDelay, 0.f);
    dryDelayIdx = 0;
    tmpL.assign(kHop, 0.f);
    tmpR.assign(kHop, 0.f);
}

void SpectralSeparator::reset()
{
    genDsp.reset();
    for (int b = 0; b < kNumBands; ++b) {
        fifoL[b].resize(kFFTSize * 2);
        fifoR[b].resize(kFFTSize * 2);
    }
    std::fill(dryDelayL.begin(), dryDelayL.end(), 0.f);
    std::fill(dryDelayR.begin(), dryDelayR.end(), 0.f);
    dryDelayIdx = 0;
}

void SpectralSeparator::processFrame(const Params& p)
{
    const float* magL   = analysis.getMagL();
    const float* magR   = analysis.getMagR();
    const float* phaseL = analysis.getPhaseL();
    const float* phaseR = analysis.getPhaseR();

    cepstrum.compute(magL, envL.data(), p.detail);
    cepstrum.compute(magR, envR.data(), p.detail);

    for (int k = 0; k < kNumBins; ++k) {
        float magAvg = 0.5f * (magL[k] + magR[k]);
        float envAvg = 0.5f * (envL[k] + envR[k]);

        float masks[10];
        genDsp.processBin(magAvg, envAvg, k, p, masks);

        std::complex<float> pL = std::polar(magL[k], phaseL[k]);
        std::complex<float> pR = std::polar(magR[k], phaseR[k]);
        for (int b = 0; b < kNumBands; ++b) {
            specBandL[b][k] = masks[b] * pL;
            specBandR[b][k] = masks[b] * pR;
        }
    }

    for (int b = 0; b < kNumBands; ++b) {
        synths[b].processFrame(specBandL[b].data(), specBandR[b].data());
        synths[b].pullSamples(tmpL.data(), tmpR.data(), kHop);
        fifoL[b].push(tmpL.data(), kHop);
        fifoR[b].push(tmpR.data(), kHop);
    }
}

void SpectralSeparator::process(const float* inL, const float* inR, int numSamples,
                                const Params& p,
                                float* dryL, float* dryR,
                                float* bandL[10], float* bandR[10])
{
    for (int b = 0; b < kNumBands; ++b)
        gates[b].setTarget(p.bandOn[b]);

    for (int i = 0; i < numSamples; ++i) {
        bool newFrame = analysis.pushSample(inL[i], inR[i]);
        if (newFrame)
            processFrame(p);

        float dl = dryDelayL[dryDelayIdx];
        float dr = dryDelayR[dryDelayIdx];
        dryDelayL[dryDelayIdx] = inL[i];
        dryDelayR[dryDelayIdx] = inR[i];
        dryDelayIdx = (dryDelayIdx + 1) % kDryDelay;
        dryL[i] = dl * p.dry;
        dryR[i] = dr * p.dry;

        for (int b = 0; b < kNumBands; ++b) {
            bandL[b][i] = fifoL[b].pop();
            bandR[b][i] = fifoR[b].pop();
        }
    }

    for (int b = 0; b < kNumBands; ++b)
        gates[b].process(bandL[b], bandR[b], numSamples);
}

} // namespace sd_legacy
