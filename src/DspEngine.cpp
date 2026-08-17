// Spectral Dissector — VCV Rack 2 plugin
// Copyright (C) 2026 Xiehuo
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
#include "DspEngine.hpp"

#include <algorithm>

namespace sdrack {

// ---- Fifo: 逐字对齐 golden SpectralSeparator::Fifo ----
void DspEngine::Fifo::push(const float* src, int n)
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

float DspEngine::Fifo::pop()
{
    if (count <= 0) return 0.0f;
    int size = (int)buf.size();
    float v = buf[readIdx];
    readIdx = (readIdx + 1) % size;
    --count;
    return v;
}

void DspEngine::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    int requested = pendingFftSize_.load(std::memory_order_relaxed);
    if (!isValidFftSize(requested))
        requested = kDefaultFFTSize;

    // 各级按全部可选 FFT size 预分配 pffft plan / 窗, 缓冲按最大 N 分配。
    // 之后 configureFftSize() 与音频线程的切换路径都不再堆分配（约束.md §4.5）。
    analysis_.prepare(sampleRate, requested);
    hpss_.prepare(requested);
    cepstrum_.prepare(sampleRate, requested);
    maskGen_.prepare(sampleRate, requested);
    for (int b = 0; b < kNumBands; ++b)
        synths_[b].prepare(sampleRate, requested);

    harmRaw_.assign(kMaxBins, 0.0f);
    percMask_.assign(kMaxBins, 0.0f);
    for (int b = 0; b < 9; ++b)
        masks_[b].assign(kMaxBins, 0.0f);
    for (int b = 0; b < kNumBands; ++b) {
        fifoL_[b].resize(kMaxFFTSize * 2);
        fifoR_[b].resize(kMaxFFTSize * 2);
    }
    tmpL_.assign(kMaxHop, 0.0f);
    tmpR_.assign(kMaxHop, 0.0f);
    dryDelayL_.assign(kMaxFFTSize + kMaxScheduleLatency, 0.0f);
    dryDelayR_.assign(kMaxFFTSize + kMaxScheduleLatency, 0.0f);

    configureFftSize(requested);
    prepared_ = true;
}

void DspEngine::requestFftSize(int fftSize)
{
    if (!isValidFftSize(fftSize))
        return;
    pendingFftSize_.store(fftSize, std::memory_order_release);
}

// 无分配地切换当前 FFT size 并复位全部流水线状态。仅音频线程调用
// （prepare 在 process 开始前调用, 同样安全）。
void DspEngine::configureFftSize(int fftSize)
{
    if (!isValidFftSize(fftSize))
        fftSize = kDefaultFFTSize;
    fftSize_   = fftSize;
    numBins_   = numBinsForFftSize(fftSize_);
    binSlices_ = binSlicesForFftSize(fftSize_);
    hop_       = hopForFftSize(fftSize_);
    frameSteps_ = frameStepsForFftSize(fftSize_);
    latency_    = scheduleLatencyForFftSize(fftSize_);

    analysis_.setFftSize(fftSize_);
    analysis_.reset();
    hpss_.setFftSize(fftSize_);
    hpss_.reset();
    cepstrum_.setFftSize(fftSize_);
    cepstrum_.reset();
    maskGen_.setFftSize(fftSize_);
    maskGen_.reset();
    for (int b = 0; b < kNumBands; ++b) {
        synths_[b].setFftSize(fftSize_);
        synths_[b].reset();
        gates_[b].prepare(sampleRate_);   // 100ms 淡入从 0 重爬（无分配）
    }

    std::fill(harmRaw_.begin(), harmRaw_.end(), 0.0f);
    std::fill(percMask_.begin(), percMask_.end(), 0.0f);
    for (int b = 0; b < 9; ++b)
        std::fill(masks_[b].begin(), masks_[b].end(), 0.0f);
    for (int b = 0; b < kNumBands; ++b) {
        fifoL_[b].reset();
        fifoR_[b].reset();
    }
    dryDelayIdx_ = 0;
    std::fill(dryDelayL_.begin(), dryDelayL_.end(), 0.0f);
    std::fill(dryDelayR_.begin(), dryDelayR_.end(), 0.0f);

    // gate 增益延迟线播种 0（golden 增益 t<0 恒 0）; dry 增益延迟线
    // 按当前初始态播种（t<0 即当前参数, docs/06 §6）
    for (int b = 0; b < kNumBands; ++b)
        for (int i = 0; i < latency_; ++i)
            gateGainDelay_[b][i] = 0.0f;
    gateGainDelayIdx_ = 0;
    for (int i = 0; i < latency_; ++i)
        dryGainDelay_[i] = p_.dry;
    dryGainDelayIdx_ = 0;

    // P5.2 分析仪缓冲复位（显示层）
    for (int b = 0; b < kNumBands; ++b)
        for (int i = 0; i < 2; ++i)
            std::fill(specDisp_[i][b], specDisp_[i][b] + kMaxBins, 0.0f);
    specFront_.store(0, std::memory_order_release);
    specBack_ = 1;

    jobStep_    = -1;
    frameCount_ = 0;
    frameDone_  = false;
    pendingFftSize_.store(fftSize_, std::memory_order_relaxed);
    currentFftSize_.store(fftSize_, std::memory_order_release);
}

void DspEngine::applyPendingFftSize()
{
    int pending = pendingFftSize_.load(std::memory_order_acquire);
    if (pending != fftSize_ && isValidFftSize(pending))
        configureFftSize(pending);
}

void DspEngine::process(float inL, float inR,
                        float* outDryL, float* outDryR,
                        float* outBandL[10], float* outBandR[10])
{
    if (!prepared_)
        return;

    // UI 线程右键请求的 FFT size 在这里生效: 先无分配重置流水线,
    // 再处理当前样本（输出从静音重新开始, 100ms band 淡入重爬）。
    applyPendingFftSize();

    // gate 实时推进（与 golden 同式）, 但**增益值**延迟 latency_ 样本后
    // 应用 —— 内容与斜坡同相位延迟 ⇒ 模块输出流 = golden 输出流整体
    // 延迟 latency_ 样本（含上电淡入与开关响应, docs/06 §1.2/§6）。
    for (int b = 0; b < kNumBands; ++b)
        gates_[b].setTarget(p_.bandOn[b]);
    float dryGain = dryGainDelay_[dryGainDelayIdx_];
    dryGainDelay_[dryGainDelayIdx_] = p_.dry;
    dryGainDelayIdx_ = (dryGainDelayIdx_ + 1) % latency_;

    // 每 hop_ 样本触发一帧；帧任务由后续 frameSteps_ 次 process() 摊分推进
    if (analysis_.pushSample(inL, inR))
        jobStep_ = 0;

    if (jobStep_ >= 0) {
        runFrameStep(jobStep_);
        ++jobStep_;
        if (jobStep_ >= frameSteps_) {
            jobStep_ = -1;
            frameDone_ = true;
            ++frameCount_;
        }
    }

    // ---- 逐样本输出（与 golden SpectralSeparator::process 同构）----
    // Dry: (N+latency) 样本延迟 + dry 增益（默认 1, 2026-08-18 R2 用户定稿;
    // 延迟与 band 通路一致 ⇒ Dry 与 MIX 同相位; 增益同延迟 latency_）
    float dl = dryDelayL_[dryDelayIdx_];
    float dr = dryDelayR_[dryDelayIdx_];
    dryDelayL_[dryDelayIdx_] = inL;
    dryDelayR_[dryDelayIdx_] = inR;
    dryDelayIdx_ = (dryDelayIdx_ + 1) % (fftSize_ + latency_);
    if (outDryL) *outDryL = dl * dryGain;
    if (outDryR) *outDryR = dr * dryGain;

    // band: fifo 出样 × OutputGate（增益延迟 latency_ 样本后应用）
    // P5.1 (D9): 再 × bandGain（per-band 音量, 默认 1.0 → 逐位无影响;
    // 无 golden 基线, 不随延迟线, 作用于 OutputGate 之后; Dry 不加）
    for (int b = 0; b < kNumBands; ++b) {
        float l = fifoL_[b].pop();
        float r = fifoR_[b].pop();
        float g = gateGainDelay_[b][gateGainDelayIdx_];
        gateGainDelay_[b][gateGainDelayIdx_] = gates_[b].advance();
        l *= g;
        r *= g;
        l *= p_.bandGain[b];
        r *= p_.bandGain[b];
        if (outBandL && outBandL[b]) *outBandL[b] = l;
        if (outBandR && outBandR[b]) *outBandR[b] = r;
    }
    gateGainDelayIdx_ = (gateGainDelayIdx_ + 1) % latency_;
}

void DspEngine::pullFrameOutputs()
{
    // 与 golden processFrame 尾部一致: 每 band pull hop_ → fifo
    for (int b = 0; b < kNumBands; ++b) {
        synths_[b].pullSamples(tmpL_.data(), tmpR_.data(), hop_);
        fifoL_[b].push(tmpL_.data(), hop_);
        fifoR_[b].push(tmpR_.data(), hop_);
    }
    // P5.2 (D11): 帧收尾发布分析仪显示帧（release store; 显示层 acquire 读）
    specFront_.store(specBack_, std::memory_order_release);
    specBack_ ^= 1;
}

void DspEngine::runFrameStep(int step)
{
    const int analysisSteps = analysisStepsForFftSize(fftSize_);
    const int hpssSteps     = binSlices_;
    const int cepSteps      = cepstrumStepsForFftSize(fftSize_);
    const int maskSteps     = binSlices_;
    const int cep0   = analysisSteps + hpssSteps;
    const int mask0  = cep0 + cepSteps;
    const int synth0 = mask0 + maskSteps;
    const int pull0  = synth0 + kNumSynthSteps;

    if (step < analysisSteps) {
        // Analysis 的帧子任务（加窗拷贝 / rfft / 幅值相位提取）
        analysis_.runFrameStep(step);
        return;
    }
    if (step < cep0) {
        // HpssCore 逐 bin（每调用 ≤ kBinSlice 个 bin）
        // mag_raw = 0.5*(magL+magR) —— fft2.maxpat 的 *~ 0.5
        int s  = step - analysisSteps;
        int k0 = s * kBinSlice;
        int k1 = std::min(k0 + kBinSlice, numBins_);
        const float* magL = analysis_.getMagL();
        const float* magR = analysis_.getMagR();
        for (int k = k0; k < k1; ++k) {
            float magRaw = 0.5f * (magL[k] + magR[k]);
            hpss_.processBin(magRaw, k, p_, harmRaw_[k], percMask_[k]);
        }
        return;
    }
    if (step < mask0) {
        // Cepstrum（基于 HPSS 谐波幅度, Δ2）
        cepstrum_.runFrameStep(step - cep0, harmRaw_.data(), p_.detail);
        return;
    }
    if (step < synth0) {
        // MaskGen 逐 bin → 掩膜 out1..9
        int s  = step - mask0;
        int k0 = s * kBinSlice;
        int k1 = std::min(k0 + kBinSlice, numBins_);
        const float* env = cepstrum_.getEnv();
        // P5.2 (D11): 同切片顺带写分析仪显示缓冲
        //   specDisp_[specBack_][b][k] = masks[b][k] × |X[k]|（共享谱
        //   mag_raw 口径; b=0..8 → Band1..9, b=9 → Perc 掩膜）。
        // 逐 bin 各 10 次乘法, 摊分在既有切片步内 —— 不新增帧子任务。
        const float* magL = analysis_.getMagL();
        const float* magR = analysis_.getMagR();
        for (int k = k0; k < k1; ++k) {
            float m[9];
            maskGen_.processBin(harmRaw_[k], env[k], k, p_, m);
            for (int b = 0; b < 9; ++b)
                masks_[b][k] = m[b];
            float magRaw = 0.5f * (magL[k] + magR[k]);
            for (int b = 0; b < 9; ++b)
                specDisp_[specBack_][b][k] = m[b] * magRaw;
            specDisp_[specBack_][9][k] = percMask_[k] * magRaw;
        }
        return;
    }
    if (step < pull0) {
        // Synth×10: 每 band 4 子任务（buildSpectrum / irfft L / irfft R / windowAccum）
        int s = step - synth0;
        int band = s / Synth::kNumSteps;
        int sub  = s % Synth::kNumSteps;
        const float* mptr[9];
        for (int b = 0; b < 9; ++b)
            mptr[b] = masks_[b].data();
        synths_[band].runStep(sub, band, mptr, percMask_.data(),
                              analysis_.getFftOutL(), analysis_.getFftOutR());
        return;
    }
    // step == pull0: 帧收尾 —— 每 band 读出 hop_ 样本入 fifo
    pullFrameOutputs();
}

} // namespace sdrack
