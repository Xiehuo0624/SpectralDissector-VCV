// SpectralSeparator.cpp — 顶层串联实现（26.08.13 语义）
#include "SpectralSeparator.h"
#include <cmath>

namespace sd {

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
    hpss.prepare(sampleRate);
    cepstrum.prepare(fftC, sampleRate);
    maskGen.prepare(sampleRate);
    for (auto& s : synths) s.prepare(fftS, sampleRate);
    for (auto& g : gates) g.prepare(sampleRate);

    harmRaw.assign(kNumBins, 0.f);
    percMask.assign(kNumBins, 0.f);
    env.assign(kNumBins, 0.f);
    for (int b = 0; b < kNumBands - 1; ++b)
        bandMask[b].assign(kNumBins, 0.f);
    for (int b = 0; b < kNumBands; ++b) {
        specBandL[b].assign(kNumBins, {});
        specBandR[b].assign(kNumBins, {});
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
    frameCount_ = 0;
}

void SpectralSeparator::reset()
{
    hpss.reset();
    maskGen.reset();
    for (int b = 0; b < kNumBands; ++b) {
        fifoL[b].resize(kFFTSize * 2);
        fifoR[b].resize(kFFTSize * 2);
    }
    std::fill(dryDelayL.begin(), dryDelayL.end(), 0.f);
    std::fill(dryDelayR.begin(), dryDelayR.end(), 0.f);
    dryDelayIdx = 0;
    frameCount_ = 0;
}

void SpectralSeparator::processFrame(const Params& p)
{
    const float* magL   = analysis.getMagL();
    const float* magR   = analysis.getMagR();
    const float* phaseL = analysis.getPhaseL();
    const float* phaseR = analysis.getPhaseR();

    // ============ Δ1/Δ2: HpssCore → 谐波幅度 + 打击掩膜 ============
    for (int k = 0; k < kNumBins; ++k) {
        float magRaw = 0.5f * (magL[k] + magR[k]);   // *~ 0.5 (mag_raw)
        hpss.processBin(magRaw, k, p, harmRaw[k], percMask[k]);
    }

    // 倒谱包络基于 HPSS 谐波幅度（原始量级, 单声道）—— Δ2
    cepstrum.compute(harmRaw.data(), env.data(), p.detail);

    // ============ MaskGen (Band1..8 + 噪声) + 重建 ============
    for (int k = 0; k < kNumBins; ++k) {
        float masks[9];
        maskGen.processBin(harmRaw[k], env[k], k, p, masks);
        for (int b = 0; b < 9; ++b)              // 帧级导出 (P2 第二阶段对拍)
            bandMask[b][k] = masks[b];

        std::complex<float> pL = std::polar(magL[k], phaseL[k]);
        std::complex<float> pR = std::polar(magR[k], phaseR[k]);
        for (int b = 0; b < 9; ++b) {          // Band1..8 + Band9(噪声)
            specBandL[b][k] = masks[b] * pL;
            specBandR[b][k] = masks[b] * pR;
        }
        // Band 10 (打击) 掩膜来自 HpssCore.out2 —— Δ5
        specBandL[9][k] = percMask[k] * pL;
        specBandR[9][k] = percMask[k] * pR;
    }

    for (int b = 0; b < kNumBands; ++b) {
        synths[b].processFrame(specBandL[b].data(), specBandR[b].data());
        synths[b].pullSamples(tmpL.data(), tmpR.data(), kHop);
        fifoL[b].push(tmpL.data(), kHop);
        fifoR[b].push(tmpR.data(), kHop);
    }

    ++frameCount_;   // 帧级导出对拍用计数
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

} // namespace sd
