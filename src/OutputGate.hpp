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
//
// ============================================================
// OutputGate.hpp — 频带输出门（100ms 线性淡入淡出, P2.9）
// ------------------------------------------------------------
// 逐字对齐 reference/OutputGate.h (golden) / docs/00 §9:
//   rampInc = 1/(sampleRate × 0.1)
//   上电 gain=0; setTarget(bandN) → 每样本 gain 向 target 线性爬升
//   （line~ 0. 初始 → 收到 bandN=1 后 100ms 淡入语义）
// 逐样本 process（golden 为块内循环, 逐样本调用逐位等价）。
// ============================================================
#pragma once
#include <algorithm>

#include "DspParams.hpp"

namespace sdrack {

class OutputGate
{
public:
    void prepare(double sampleRate)
    {
        rampInc = 1.0f / (float)(sampleRate * (kGateFadeMs / 1000.0));
        gain = 0.0f;
        target = 0.0f;
    }
    void setTarget(bool on) { target = on ? 1.0f : 0.0f; }
    // 推进一次斜坡并返回新增益（golden 块内循环的逐样本版, 逐位等价）
    float advance()
    {
        if (gain < target)      gain = std::min(target, gain + rampInc);
        else if (gain > target) gain = std::max(target, gain - rampInc);
        return gain;
    }
    void process(float& L, float& R)
    {
        advance();
        L *= gain;
        R *= gain;
    }
    float getGain() const { return gain; }

private:
    float gain = 0.0f;
    float target = 0.0f;
    float rampInc = 0.001f;
};

} // namespace sdrack
