// ============================================================
// GenDsp.h — gen~ codebox 逐 bin 掩膜生成 (算法核心)
// ------------------------------------------------------------
// 每个 STFT 帧的每个 bin 调用一次 processBin, 生成 10 个掩膜。
// 掩膜基于 L/R 平均幅度计算, 重建时 L/R 共用同一组掩膜。
// 输入 mag/env 已归一化为 "真实幅度" (单频满幅 → 1.0)。
//
// ★ 移植到 Tiliqua 时这是最核心的自定义逻辑:
//   per-bin 状态 (hpssHist / gateSmooth / bandSmooth[8]) 存 BRAM,
//   每帧逐 bin 读-改-写。详见 PORTING_GUIDE。
// ============================================================
#pragma once
#include "Params.h"
#include <vector>
#include <algorithm>

namespace sd_legacy {

struct BinState {
    float hpssHist   = 0.0f;   // HPSS 历史 (Data hpss_data)
    float gateSmooth = 0.0f;   // denoise_gate_smooth (slide)
    float bandSmooth[8] = {};  // out_b1..8 (slide)
};

class GenDsp
{
public:
    void prepare(double sampleRate);
    // 处理一个 bin:
    //   mag/env: L/R 平均真实幅度 / 倒谱包络
    //   binIdx:  0..N/2
    //   outMasks[10]: band1..8, Residual(9), Perc(10)
    void processBin(float mag, float env, int binIdx, const Params& p, float outMasks[10]);
    void reset();

private:
    std::vector<BinState> states;   // size = kNumBins (per-bin 状态)
    double sampleRate_ = 44100.0;
};

} // namespace sd_legacy
