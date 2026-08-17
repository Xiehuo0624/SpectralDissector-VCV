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
    analysis_.prepare(sampleRate);
    hpss_.prepare();
    cepstrum_.prepare(sampleRate);
    maskGen_.prepare(sampleRate);
    for (int b = 0; b < kNumBands; ++b) {
        synths_[b].prepare(sampleRate);
        gates_[b].prepare(sampleRate);
    }
    harmRaw_.assign(kNumBins, 0.0f);
    percMask_.assign(kNumBins, 0.0f);
    for (int b = 0; b < 9; ++b)
        masks_[b].assign(kNumBins, 0.0f);
    for (int b = 0; b < kNumBands; ++b) {
        fifoL_[b].resize(kFFTSize * 2);
        fifoR_[b].resize(kFFTSize * 2);
    }
    tmpL_.assign(kHop, 0.0f);
    tmpR_.assign(kHop, 0.0f);
    dryDelayL_.assign(kDryDelay + kScheduleLatency, 0.0f);
    dryDelayR_.assign(kDryDelay + kScheduleLatency, 0.0f);
    dryDelayIdx_ = 0;
    // gate 增益延迟线播种 0（golden 增益 t<0 恒 0）; dry 增益延迟线
    // 按上电初始态播种（t<0 即初始参数, docs/06 §6）
    for (int b = 0; b < kNumBands; ++b)
        for (int i = 0; i < kScheduleLatency; ++i)
            gateGainDelay_[b][i] = 0.0f;
    gateGainDelayIdx_ = 0;
    for (int i = 0; i < kScheduleLatency; ++i)
        dryGainDelay_[i] = p_.dry;
    dryGainDelayIdx_ = 0;
    jobStep_   = -1;
    frameCount_ = 0;
    frameDone_  = false;
    // P5.2 分析仪缓冲复位（显示层）
    for (int b = 0; b < kNumBands; ++b)
        for (int i = 0; i < 2; ++i)
            std::fill(specDisp_[i][b], specDisp_[i][b] + kNumBins, 0.0f);
    specFront_.store(0, std::memory_order_release);
    specBack_ = 1;
    prepared_   = true;
}

void DspEngine::process(float inL, float inR,
                        float* outDryL, float* outDryR,
                        float* outBandL[10], float* outBandR[10])
{
    if (!prepared_)
        return;

    // gate 实时推进（与 golden 同式）, 但**增益值**延迟 kScheduleLatency
    // 样本后应用 —— 内容与斜坡同相位延迟 ⇒ 模块输出流 = golden 输出流
    // 整体延迟 91 样本（含上电淡入与开关响应, docs/06 §1.2/§6）。
    for (int b = 0; b < kNumBands; ++b)
        gates_[b].setTarget(p_.bandOn[b]);
    float dryGain = dryGainDelay_[dryGainDelayIdx_];
    dryGainDelay_[dryGainDelayIdx_] = p_.dry;
    dryGainDelayIdx_ = (dryGainDelayIdx_ + 1) % kScheduleLatency;

    // 每 1024 样本触发一帧；帧任务由后续 kNumFrameSteps 次 process() 摊分推进
    if (analysis_.pushSample(inL, inR))
        jobStep_ = 0;

    if (jobStep_ >= 0) {
        runFrameStep(jobStep_);
        ++jobStep_;
        if (jobStep_ >= kNumFrameSteps) {
            jobStep_ = -1;
            frameDone_ = true;
            ++frameCount_;
        }
    }

    // ---- 逐样本输出（与 golden SpectralSeparator::process 同构）----
    // Dry: (4096+91) 样本延迟 + dry 增益（默认 1, 2026-08-18 R2 用户定稿;
    // 延迟与 band 通路一致 ⇒ Dry 与 MIX 同相位; 增益同延迟 91）
    float dl = dryDelayL_[dryDelayIdx_];
    float dr = dryDelayR_[dryDelayIdx_];
    dryDelayL_[dryDelayIdx_] = inL;
    dryDelayR_[dryDelayIdx_] = inR;
    dryDelayIdx_ = (dryDelayIdx_ + 1) % (kDryDelay + kScheduleLatency);
    if (outDryL) *outDryL = dl * dryGain;
    if (outDryR) *outDryR = dr * dryGain;

    // band: fifo 出样 × OutputGate（增益延迟 91 样本后应用）
    // P5.1 (D9): 再 × bandGain（per-band 音量, 默认 1.0 → 逐位无影响;
    // 无 golden 基线, 不随 91 样本延迟线, 作用于 OutputGate 之后; Dry 不加）
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
    gateGainDelayIdx_ = (gateGainDelayIdx_ + 1) % kScheduleLatency;
}

void DspEngine::pullFrameOutputs()
{
    // 与 golden processFrame 尾部一致: 每 band pull 1024 → fifo
    for (int b = 0; b < kNumBands; ++b) {
        synths_[b].pullSamples(tmpL_.data(), tmpR_.data(), kHop);
        fifoL_[b].push(tmpL_.data(), kHop);
        fifoR_[b].push(tmpR_.data(), kHop);
    }
    // P5.2 (D11): 帧收尾发布分析仪显示帧（release store; 显示层 acquire 读）
    specFront_.store(specBack_, std::memory_order_release);
    specBack_ ^= 1;
}

void DspEngine::runFrameStep(int step)
{
    const int cep0   = Analysis::kNumFrameSteps + kNumHpssSteps;    // 22
    const int mask0  = cep0 + Cepstrum::kNumFrameSteps;             // 42
    const int synth0 = mask0 + kNumMaskSteps;                       // 51
    const int pull0  = synth0 + kNumSynthSteps;                     // 91

    if (step < Analysis::kNumFrameSteps) {
        // Analysis 的帧子任务（加窗拷贝 / rfft / 幅值相位提取）
        analysis_.runFrameStep(step);
        return;
    }
    if (step < cep0) {
        // HpssCore 逐 bin（每调用 ≤ kBinSlice 个 bin）
        // mag_raw = 0.5*(magL+magR) —— fft2.maxpat 的 *~ 0.5
        int s  = step - Analysis::kNumFrameSteps;
        int k0 = s * kBinSlice;
        int k1 = std::min(k0 + kBinSlice, kNumBins);
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
        int k1 = std::min(k0 + kBinSlice, kNumBins);
        const float* env = cepstrum_.getEnv();
        // P5.2 (D11): 同切片顺带写分析仪显示缓冲
        //   specDisp_[specBack_][b][k] = masks[b][k] × |X[k]|（共享谱
        //   mag_raw 口径; b=0..8 → Band1..9, b=9 → Perc 掩膜）。
        // 逐 bin 各 10 次乘法, 摊分在既有 9 个切片步内 —— 不新增
        // 帧子任务 ⇒ kScheduleLatency 不变（对拍基线不受影响）。
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
    // step == pull0: 帧收尾 —— 每 band 读出 1024 样本入 fifo
    pullFrameOutputs();
}

} // namespace sdrack
