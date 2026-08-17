// ============================================================
// FFT.h — 平台无关 FFT 接口 + 自包含 Radix-2 实现
// ------------------------------------------------------------
// 这是 "算法契约" 的参考实现。移植到 Tiliqua 时, 此处的 FFT
// 对应 tiliqua.dsp.fft.FFT (硬件 Cooley-Tukey Radix-2 核心)。
//
// 归一化约定 (与原 JUCE dsp::FFT 一致, 算法已按此校准):
//   forward (inverse=false): 不归一化,  out = sum x[n] e^{-i 2pi kn/N}
//   inverse (inverse=true ): 含 1/N,    out = (1/N) sum X[k] e^{+i 2pi kn/N}
//
// ⚠️ Tiliqua 的 dsp.fft.FFT 约定相反:
//      forward 含 1/N, inverse 不归一化。
//    移植时需在分析端/合成端相应增减 N 倍增益补偿 (见 PORTING_GUIDE)。
// ============================================================
#pragma once
#include <complex>
#include <vector>
#include <cmath>

namespace sd_legacy {

// FFT 抽象接口 (算法代码只依赖此接口, 便于替换实现)
class FFT
{
public:
    virtual ~FFT() = default;
    // size 必须是 2 的幂。inverse 见上文归一化约定。
    virtual void perform(const std::complex<float>* in,
                         std::complex<float>* out,
                         bool inverse) const = 0;
    virtual int size() const = 0;
};

// 自包含迭代 Radix-2 Cooley-Tukey (DIT)。
// 仅依赖 <complex>/<cmath>, 无外部库。size 必须为 2 的幂。
class Radix2FFT : public FFT
{
public:
    explicit Radix2FFT(int n) : n_(n), log2n_(log2Int(n))
    {
        // 预计算 bit-reversal 表
        rev_.resize(n);
        for (int i = 0; i < n; ++i)
            rev_[i] = bitReverse(i, log2n_);
        // 预计算 twiddle (单位圆上的根)
        twiddles_.resize(n);
        for (int k = 0; k < n; ++k)
            twiddles_[k] = std::polar(1.0f,
                -2.0f * (float)M_PI * (float)k / (float)n);
    }

    int size() const override { return n_; }

    void perform(const std::complex<float>* in,
                 std::complex<float>* out,
                 bool inverse) const override
    {
        // 1. bit-reversal 重排
        for (int i = 0; i < n_; ++i)
            out[i] = in[rev_[i]];
        // 2. 蝶形迭代
        for (int s = 1; s <= log2n_; ++s) {
            int m = 1 << s;          // 蝶形组大小
            int m2 = m >> 1;
            // W_m^k = exp(dir * i * 2pi * k / m) = twiddles_[k * (n/m)]^dir
            int step = n_ / m;
            for (int k = 0; k < n_; k += m) {
                for (int j = 0; j < m2; ++j) {
                    std::complex<float> w = twiddles_[j * step];
                    if (inverse) w = std::conj(w);
                    std::complex<float> t = w * out[k + j + m2];
                    std::complex<float> u = out[k + j];
                    out[k + j]       = u + t;
                    out[k + j + m2]  = u - t;
                }
            }
        }
        // 3. inverse 含 1/N 归一化 (JUCE 约定)
        if (inverse) {
            float invN = 1.0f / (float)n_;
            for (int i = 0; i < n_; ++i)
                out[i] *= invN;
        }
    }

private:
    static int log2Int(int n) {
        int r = 0; while ((1 << r) < n) ++r; return r;
    }
    static int bitReverse(int x, int bits) {
        int r = 0;
        for (int i = 0; i < bits; ++i) { r = (r << 1) | (x & 1); x >>= 1; }
        return r;
    }

    int n_, log2n_;
    std::vector<int> rev_;
    std::vector<std::complex<float>> twiddles_;
};

} // namespace sd_legacy
