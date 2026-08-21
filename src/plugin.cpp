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
// P3: 26.08.13 DSP 核心移植完成（P2, docs/04/05/06）后的模块接线:
//   22 输出口（Dry/B1..B10 × L/R, 约束.md §5.1）、17 DSP 参数 +
//   Dry 开关 + 10 band 开关（configParam/configSwitch）、每频带活动灯。
// D5-A 逐样本摊分调度: 每 sample 推 1 样本进 Analysis，帧工作切成
// 92 个子任务摊分到多次 process()（src/Analysis.hpp / DspEngine.hpp）;
// 模块输出流 = golden 输出流整体延迟 91 样本（docs/06 §1.2）。
// 信号 ↔ 电压 1:1 映射（Rack ±5V 音频惯例可直接接）。
#include "plugin.hpp"
#include "DspEngine.hpp"
#include "panel.hpp"

#include <algorithm>
#include <cmath>


Plugin* pluginInstance;

// 右声道 breakout 扩展器（Spectral Dissector R）模型指针。
// 定义在本文件尾部；主模块 process() 仅用其做“右扩展器输入钩子”
// （无扩展器/未连线时行为与 2.0.0 完全一致）。
extern Model* modelSpectralDissectorR;


struct SpectralDissectorModule : Module {
	enum ParamIds {
		PARAM_BLUR,
		PARAM_PERC,
		PARAM_GATE,
		PARAM_THRESHOLD,
		PARAM_SPACING,
		PARAM_FOCUS,
		PARAM_TILT,
		PARAM_RISE_MS,
		PARAM_FALL_MS,
		PARAM_DETAIL,
		PARAM_OFF1,
		PARAM_OFF2,
		PARAM_OFF3,
		PARAM_OFF4,
		PARAM_OFF5,
		PARAM_OFF6,
		PARAM_OFF7,
		PARAM_DRY,          // Dry 开关 0/1（默认 1, 2026-08-18 R2 用户定稿）
		PARAM_BAND1,        // Band 1..10 开关 0/1（默认全开）
		PARAM_BAND2,
		PARAM_BAND3,
		PARAM_BAND4,
		PARAM_BAND5,
		PARAM_BAND6,
		PARAM_BAND7,
		PARAM_BAND8,
		PARAM_BAND9,
		PARAM_BAND10,
		// P5.1 (D9): per-band 音量推子 0..1.5 默认 1.0（Dry 不加）
		PARAM_GAIN1,
		PARAM_GAIN2,
		PARAM_GAIN3,
		PARAM_GAIN4,
		PARAM_GAIN5,
		PARAM_GAIN6,
		PARAM_GAIN7,
		PARAM_GAIN8,
		PARAM_GAIN9,
		PARAM_GAIN10,
		// P5.1 (D10): 双极 attenuator −1..1 默认 0
		// （2026-08-18 R2: TILT 补 CV attenuator ⇒ 8 个）
		PARAM_CVATT_THRESHOLD,
		PARAM_CVATT_SPACING,
		PARAM_CVATT_FOCUS,
		PARAM_CVATT_GATE,
		PARAM_CVATT_BLUR,
		PARAM_CVATT_PERC,
		PARAM_CVATT_DETAIL,
		PARAM_CVATT_TILT,
		NUM_PARAMS
	};
	enum InputIds {
		// D19 (2026-08-17 用户定稿): 音频输入 = 1 个 poly 口
		// （ch0=L, ch1=R; mono/单通道时 R=L, 约束.md §5.5 语义保留）
		INPUT_AUDIO,
		// P5.1 (D10): CV 输入（±10V, 各带双极 attenuator）
		// （2026-08-18 R2: TILT 补 CV 输入 ⇒ 8 个）
		INPUT_CV_THRESHOLD,
		INPUT_CV_SPACING,
		INPUT_CV_FOCUS,
		INPUT_CV_GATE,
		INPUT_CV_BLUR,
		INPUT_CV_PERC,
		INPUT_CV_DETAIL,
		INPUT_CV_TILT,
		NUM_INPUTS
	};
	enum OutputIds {
		// D19: 22 个 mono 口 → 11 个 poly 口（每口 2ch = L/R）。
		// Rack 2 poly 线单根上限 16ch, 22 通道装不进单口; 每 band 1 口
		// 与原版 22 声道一一对应（ch0=L, ch1=R; 端口序 Dry=ch1,2 /
		// BandN=ch(2N+1),(2N+2) 语义不变）。
		OUTPUT_DRY,
		OUTPUT_B1,
		OUTPUT_B2,
		OUTPUT_B3,
		OUTPUT_B4,
		OUTPUT_B5,
		OUTPUT_B6,
		OUTPUT_B7,
		OUTPUT_B8,
		OUTPUT_B9,
		OUTPUT_B10,
		// D20 (2026-08-17 用户定稿): MIX 口 = 各 band 推子后（OutputGate
		// × bandGain 之后）全 band 求和, poly 2ch L/R; Dry 不含（D9）;
		// odd/even 开关经用户斟酌后不要。
		OUTPUT_MIX,
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT_BAND1,
		LIGHT_BAND2,
		LIGHT_BAND3,
		LIGHT_BAND4,
		LIGHT_BAND5,
		LIGHT_BAND6,
		LIGHT_BAND7,
		LIGHT_BAND8,
		LIGHT_BAND9,
		LIGHT_BAND10,
		NUM_LIGHTS
	};

	// band b(0..9 → Band1..10) 的输出口 id（D19: 每 band 1 个 poly 口）
	static int bandOutput(int b) { return OUTPUT_B1 + b; }

	sdrack::DspEngine engine;
	float lightEnv_[10] = {};   // 每频带活动灯峰值包络（音频线程内推进）
	float lastSampleRate_ = 44100.0f;
	// P5.2 (D11): 分析仪显示刻度（右键菜单切换; 默认对数 dB, 参照
	// scope.maxpat spectroscope~ 观感）。纯显示层状态。
	bool spectrumLogScale_ = true;
	// 2026-08-19 用户指示: 分析仪图例 chip 点击只切换该 band 的显示层
	// 可见性（仅图像开关），不再写 PARAM_BAND*，音频通路保持全开。
	bool spectrumBandVisible_[10] = {true, true, true, true, true,
	                                 true, true, true, true, true};
	// 2026-08-18 用户指示: 深浅主题改为每模块独立设置,
	// 不再切换 Rack 全局 preferDarkPanels（原生模块不受影响）。
	bool panelDark_ = true;
	// 2026-08-19 用户指示: FFT window size 入右键菜单。
	// 模块保存用户选择（随 patch 持久化）; engine.requestFftSize() 把
	// 请求交给音频线程下一采样无分配切换（pffft plan 已在 prepare 预分配）。
	int fftSize_ = sdrack::kDefaultFFTSize;

	SpectralDissectorModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		panelDark_ = settings::preferDarkPanels;
		sdpanel::setModuleDark(this, panelDark_);
		// D19: 音频输入 poly 2ch（L/R）; mono 时 R=L 在 process 兜底
		configInput(INPUT_AUDIO, "Audio (L/R poly)");
		// 输出口 = 原版 22 声道映射（约束.md §5.1, D19 poly 化）:
		//   Dry=ch1,2; Band N=ch(2N+1),(2N+2)  N=1..10（Band9=噪声, Band10=打击）
		configOutput(OUTPUT_DRY, "Dry (L/R poly)");
		configOutput(OUTPUT_MIX, "Mix (all bands, post-fader, L/R poly)");
		const char* bandName[10] = {"Band 1", "Band 2", "Band 3", "Band 4", "Band 5",
		                            "Band 6", "Band 7", "Band 8", "Band 9 (Noise)", "Band 10 (Perc)"};
		for (int b = 0; b < 10; ++b)
			configOutput(bandOutput(b), std::string(bandName[b]) + " (L/R poly)");
		// 参数表 = docs/00 §7（UI 范围/运行时默认; gen~ Param 钳位在 process 兜底）
		configParam(PARAM_BLUR, 0.001f, 0.5f, 0.05f, "Blur", "");
		configParam(PARAM_PERC, 1.0f, 10.0f, 1.5f, "Perc", "");
		configParam(PARAM_GATE, 0.0f, 3.0f, 1.0f, "Gate", "");
		configParam(PARAM_THRESHOLD, -70.0f, 12.0f, 0.0f, "Threshold", " dB");
		configParam(PARAM_SPACING, 0.0f, 24.0f, 5.0f, "Spacing", " dB");
		configParam(PARAM_FOCUS, 1.0f, 10.0f, 1.0f, "Focus", "");
		configParam(PARAM_TILT, -6.0f, 6.0f, 0.0f, "Tilt", " dB");
		configParam(PARAM_RISE_MS, 0.0f, 5.0f, 0.0f, "Rise", " ms");
		configParam(PARAM_FALL_MS, 0.0f, 10.0f, 0.0f, "Fall", " ms");
		configParam(PARAM_DETAIL, 0.0f, 1.0f, 1.0f, "Detail", "");
		for (int i = 0; i < 7; ++i) {
			std::string name = "Band " + std::to_string(i + 1) + " Offset";
			configParam(PARAM_OFF1 + i, -24.0f, 24.0f, 0.0f, name, " dB");
		}
		// Dry 开关（0/1, 默认关）与 band 开关（0/1, 默认全开, docs/00 §7 U4）
		configSwitch(PARAM_DRY, 0.0f, 1.0f, 1.0f, "Dry");
		for (int b = 0; b < 10; ++b) {
			std::string name = std::string(bandName[b]) + " On";
			configSwitch(PARAM_BAND1 + b, 0.0f, 1.0f, 1.0f, name);
		}
		// P5.1 (D9): per-band 音量推子（0..1.5, 默认 1.0, OutputGate 之后, Dry 不加）
		for (int b = 0; b < 10; ++b) {
			std::string name = std::string(bandName[b]) + " Gain";
			configParam(PARAM_GAIN1 + b, 0.0f, 1.5f, 1.0f, name, "");
		}
		// P5.1 (D10): 双极 attenuator（−1..1, 默认 0 → CV 不生效）
		configParam(PARAM_CVATT_THRESHOLD, -1.0f, 1.0f, 0.0f, "Threshold CV Att", "%");
		configParam(PARAM_CVATT_SPACING,   -1.0f, 1.0f, 0.0f, "Spacing CV Att", "%");
		configParam(PARAM_CVATT_FOCUS,     -1.0f, 1.0f, 0.0f, "Focus CV Att", "%");
		configParam(PARAM_CVATT_GATE,      -1.0f, 1.0f, 0.0f, "Gate CV Att", "%");
		configParam(PARAM_CVATT_BLUR,      -1.0f, 1.0f, 0.0f, "Blur CV Att", "%");
		configParam(PARAM_CVATT_PERC,      -1.0f, 1.0f, 0.0f, "Perc CV Att", "%");
		configParam(PARAM_CVATT_DETAIL,    -1.0f, 1.0f, 0.0f, "Detail CV Att", "%");
		configParam(PARAM_CVATT_TILT,      -1.0f, 1.0f, 0.0f, "Tilt CV Att", "%");
		// P5.1 (D10): 7 个 CV 输入
		configInput(INPUT_CV_THRESHOLD, "Threshold CV");
		configInput(INPUT_CV_SPACING,   "Spacing CV");
		configInput(INPUT_CV_FOCUS,     "Focus CV");
		configInput(INPUT_CV_GATE,      "Gate CV");
		configInput(INPUT_CV_BLUR,      "Blur CV");
		configInput(INPUT_CV_PERC,      "Perc CV");
		configInput(INPUT_CV_DETAIL,    "Detail CV");
		configInput(INPUT_CV_TILT,      "Tilt CV");
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		lastSampleRate_ = e.sampleRate;
		engine.requestFftSize(fftSize_);
		engine.prepare(e.sampleRate);
	}

	void onReset() override {
		// 引擎状态清零（参数值由 Rack 复位; FFT size 选择保留）
		engine.requestFftSize(fftSize_);
		engine.prepare(lastSampleRate_);
	}

	void setPanelDark(bool dark) {
		panelDark_ = dark;
		sdpanel::setModuleDark(this, dark);
	}

	void setFftSize(int n) {
		if (!sdrack::isValidFftSize(n) || n == fftSize_)
			return;
		fftSize_ = n;
		engine.requestFftSize(n);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "panelDark", json_boolean(panelDark_));
		json_object_set_new(rootJ, "fftSize", json_integer(fftSize_));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* j = json_object_get(rootJ, "panelDark");
		if (j)
			setPanelDark(json_boolean_value(j));
		j = json_object_get(rootJ, "fftSize");
		if (j && json_is_integer(j))
			setFftSize((int)json_integer_value(j));
	}

	void process(const ProcessArgs& args) override {
		// onSampleRateChange 尚未到达的兜底（Rack 保证其先于 process 调用）
		if (!engine.isPrepared())
			return;

		sdrack::DspParams& p = engine.params();
		// P5.1 (D10) CV 语义: param = clamp(旋钮 + cv×att×range/20, min, max);
		// range = UI 范围（Threshold 82 / Spacing 24 / Focus 9 / Gate 3 /
		// Blur 0.499 / Perc 9 / Detail 1 / Tilt 12）。att=0 默认 → 与直读逐位一致。
		p.blur = sdrack::cvModulate(params[PARAM_BLUR].getValue(),
		                            inputs[INPUT_CV_BLUR].getVoltage(),
		                            params[PARAM_CVATT_BLUR].getValue(),
		                            0.001f, 0.5f);
		p.perc = sdrack::cvModulate(params[PARAM_PERC].getValue(),
		                            inputs[INPUT_CV_PERC].getVoltage(),
		                            params[PARAM_CVATT_PERC].getValue(),
		                            1.0f, 10.0f);
		p.gate = sdrack::cvModulate(params[PARAM_GATE].getValue(),
		                            inputs[INPUT_CV_GATE].getVoltage(),
		                            params[PARAM_CVATT_GATE].getValue(),
		                            0.0f, 3.0f);
		p.threshold = sdrack::cvModulate(params[PARAM_THRESHOLD].getValue(),
		                                 inputs[INPUT_CV_THRESHOLD].getVoltage(),
		                                 params[PARAM_CVATT_THRESHOLD].getValue(),
		                                 -70.0f, 12.0f);
		// Spacing: CV 作用于 UI 范围 0..24, 再走既有 gen~ [1,12] 钳位
		p.spacing = math::clamp(sdrack::cvModulate(params[PARAM_SPACING].getValue(),
		                                           inputs[INPUT_CV_SPACING].getVoltage(),
		                                           params[PARAM_CVATT_SPACING].getValue(),
		                                           0.0f, 24.0f),
		                        1.0f, 12.0f);
		p.focus = sdrack::cvModulate(params[PARAM_FOCUS].getValue(),
		                             inputs[INPUT_CV_FOCUS].getVoltage(),
		                             params[PARAM_CVATT_FOCUS].getValue(),
		                             1.0f, 10.0f);
		p.tilt = sdrack::cvModulate(params[PARAM_TILT].getValue(),
		                            inputs[INPUT_CV_TILT].getVoltage(),
		                            params[PARAM_CVATT_TILT].getValue(),
		                            -6.0f, 6.0f);
		// U3: UI ms → DSP 样本（×sr/1000; slide 内 1/max(1,x) 再兜底）
		p.slideRise = params[PARAM_RISE_MS].getValue() * args.sampleRate / 1000.0f;
		p.slideFall = params[PARAM_FALL_MS].getValue() * args.sampleRate / 1000.0f;
		p.detail = sdrack::cvModulate(params[PARAM_DETAIL].getValue(),
		                              inputs[INPUT_CV_DETAIL].getVoltage(),
		                              params[PARAM_CVATT_DETAIL].getValue(),
		                              0.0f, 1.0f);
		for (int i = 0; i < 7; ++i)
			p.off[i] = params[PARAM_OFF1 + i].getValue();
		p.dry = params[PARAM_DRY].getValue();
		for (int b = 0; b < 10; ++b) {
			p.bandOn[b] = params[PARAM_BAND1 + b].getValue() >= 0.5f;
			p.bandGain[b] = params[PARAM_GAIN1 + b].getValue();
		}
		// attenuator 已在上面的 cvModulate 并入生效值; 原值留档（结构完整）
		for (int c = 0; c < sdrack::kNumCv; ++c)
			p.cvAtt[c] = params[PARAM_CVATT_THRESHOLD + c].getValue();

		// D19: poly 输入 —— ch0=L, ch1=R; 单通道/mono 时 R=L（约束.md §5.5 语义）
		int inCh = inputs[INPUT_AUDIO].getChannels();
		float inL = inputs[INPUT_AUDIO].getPolyVoltage(0);
		float inR = inCh >= 2 ? inputs[INPUT_AUDIO].getPolyVoltage(1) : inL;

		// 2026-08-21 右声道扩展器输入钩子（最小修改）:
		// 仅当右侧是本插件 SpectralDissectorR 且其 IN R 已连线时,
		// 用扩展器输入覆盖 poly 输入 ch1。无扩展器/未连线 → 不进入,
		// 行为与 2.0.0 逐位一致。主模块面板/端口/参数/DSP 设计不变。
		if (Module* re = rightExpander.module) {
			if (re->model == modelSpectralDissectorR && re->inputs[0].isConnected())
				inR = re->inputs[0].getVoltage();
		}

		// 逐样本输出（engine 通路 = docs/06 §1.2 口径, 直接透传到端口）
		float dryL = 0.0f, dryR = 0.0f;
		float bandL[10], bandR[10];
		float* bL[10];
		float* bR[10];
		for (int i = 0; i < 10; ++i) {
			bL[i] = &bandL[i];
			bR[i] = &bandR[i];
		}
		engine.process(inL, inR, &dryL, &dryR, bL, bR);

		// D19: poly 输出 —— 每口 2ch（ch0=L, ch1=R）
		outputs[OUTPUT_DRY].setChannels(2);
		outputs[OUTPUT_DRY].setVoltage(dryL, 0);
		outputs[OUTPUT_DRY].setVoltage(dryR, 1);
		// D20: MIX = 各 band 推子后全求和（engine 输出已是
		// OutputGate × bandGain 之后; Dry 不含）
		float mixL = 0.0f, mixR = 0.0f;
		for (int b = 0; b < 10; ++b) {
			outputs[bandOutput(b)].setChannels(2);
			outputs[bandOutput(b)].setVoltage(bandL[b], 0);
			outputs[bandOutput(b)].setVoltage(bandR[b], 1);
			mixL += bandL[b];
			mixR += bandR[b];
			// 活动灯: 瞬时攻击 + 指数释放的峰值包络（免锁, 音频线程内）
			float level = std::max(std::fabs(bandL[b]), std::fabs(bandR[b]));
			float& e = lightEnv_[b];
			if (level > e)
				e = level;
			else
				e = std::max(0.0f, e - e * 0.0004f);   // ~57ms 释放时间常数 @44.1k
			lights[LIGHT_BAND1 + b].setBrightness(math::clamp(e * 0.25f, 0.0f, 1.0f));
		}
		outputs[OUTPUT_MIX].setChannels(2);
		outputs[OUTPUT_MIX].setVoltage(mixL, 0);
		outputs[OUTPUT_MIX].setVoltage(mixR, 1);
	}
};


// 每模块独立主题面板: 背景只跟随本模块 panelDark_,
// 不再跟随 settings::preferDarkPanels（2026-08-18 用户指示）。
struct SpectralThemedSvgPanel : ThemedSvgPanel {
	SpectralDissectorModule* module = nullptr;

	void step() override {
		if (module)
			SvgPanel::setBackground(module->panelDark_ ? darkSvg : lightSvg);
		SvgPanel::step();
	}
};


// ============================================================
// P5.2 (D11): 频谱分析仪 widget —— 多色分层频谱 + band 图例
// ------------------------------------------------------------
// 显示层: 从 engine 的原子发布缓冲读 masks[b][k]×|X[k]|
// （2049 bin, 每帧发布一次 ~23/s）; 按 scope.maxpat 配色把 10 条
// band 分层绘制（非精确复刻, D11）; 图例 = 10 色条 + 开关态
// （关 = 变暗 + 序号仍在）。对数/线性刻度经右键菜单切换
// （模块 spectrumLogScale_）。不进音频路径。
// ============================================================
struct SpectrumAnalyzerWidget : widget::TransparentWidget {
	SpectralDissectorModule* module = nullptr;

	void draw(const DrawArgs& args) override {
		{ // TEMP-DIAG: 只记一次（地面真相: 真实 Rack 里 module 是否非空）
			static int diagN = 0;
			if (diagN < 3) {
				++diagN;
				INFO("analyzer draw #%d: box=(%g,%g) pos=(%g,%g) module=%p",
				     diagN, (double)box.size.x, (double)box.size.y,
				     (double)box.pos.x, (double)box.pos.y, (void*)module);
			}
		}
		const sdpanel::Theme& t = sdpanel::themeFor(module);
		NVGcontext* vg = args.vg;
		float w = box.size.x;
		float h = box.size.y;

		// 底 + 边框（面板 SVG 已画占位, 这里保证运行时与 SVG 一致）
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.0f, 0.0f, w, h, 2.5f);
		nvgFillColor(vg, t.analyzerBg);
		nvgFill(vg);
		nvgStrokeColor(vg, t.panelLine);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		// ---- 布局: 谱区 + 底部图例行（2026-08-17 精致化;
		//      2026-08-18 修订: 谱底与图例之间留 legendGap=11px 空隙
		//      容纳频率刻度文字（100/1k/10k）—— 此前文字 TOP 对齐画在
		//      谱底 +2px, 与图例芯片顶重叠 ~2.5px（用户反馈）） ----
		float legendH = std::fmin(13.0f, h * 0.10f);
		float legendGap = 11.0f;                       // 谱底 → 图例顶
		float specW = w - 4.0f;
		float specH = h - legendH - legendGap;         // 谱底 = specY+specH
		float specX = 2.0f, specY = 2.0f;
		float chipH = legendH - 5.0f;
		float ly = h - legendH + 2.0f;                 // = specY+specH+legendGap（图例顶）

		// ---- 网格: 电平线 4 条 + 频率竖线（100Hz/1k/10k）----
		nvgBeginPath(vg);
		for (int g = 1; g <= 4; ++g) {
			float gy = specY + specH * g / 5.0f;
			nvgMoveTo(vg, specX, gy);
			nvgLineTo(vg, specX + specW, gy);
		}
		double sr = module ? module->engine.sampleRate() : 0.0;
		// 2026-08-19: 谱区几何随右键选择的 FFT size 变化（bin 数/满幅参照
		// 均取当前 size; 默认 4096 与旧显示逐像素一致）。
		int fftN = module ? module->engine.fftSize() : sdrack::kDefaultFFTSize;
		int bins = sdrack::numBinsForFftSize(fftN);
		float fftRef = (float)(fftN / 4);
		if (sr > 0.0) {
			for (float f : {100.0f, 1000.0f, 10000.0f}) {
				float k = (float)(f * fftN / sr);
				if (k >= bins)
					continue;
				float gx = specX + specW * k / (float)(bins - 1);
				nvgMoveTo(vg, gx, specY);
				nvgLineTo(vg, gx, specY + specH);
			}
		}
		nvgStrokeColor(vg, t.analyzerGrid);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);

		// ---- 轴刻度文字（不依赖 module） ----
		nvgFontSize(vg, 6.5f);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, t.textDim);
		for (int g = 0; g <= 4; ++g) {
			float gy = specY + specH * g / 5.0f;
			nvgText(vg, specX - 3.0f, gy, (g == 0 ? "0" : g == 4 ? "-80dB" : ""), nullptr);
			if (g == 2) nvgText(vg, specX - 3.0f, gy, "-40dB", nullptr);
		}
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		if (sr > 0.0) {
			for (float f : {100.0f, 1000.0f, 10000.0f}) {
				float k = (float)(f * fftN / sr);
				if (k >= bins)
					continue;
				float gx = specX + specW * k / (float)(bins - 1);
				const char* lab = (f == 100.0f) ? "100" : (f == 1000.0f) ? "1k" : "10k";
				// 文字底 = 图例顶 − 2.5px ⇒ 整行落在谱底→图例之间的 11px 空隙内,
				// 与芯片零重叠（此前 TOP@谱底+2 与芯片顶重叠 ~2.5px）
				nvgText(vg, gx, ly - 2.5f, lab, nullptr);
			}
		}

		// ---- 10 条 band 分层谱（填色 + 顶部辉光描边） ----
		bool log = module ? module->spectrumLogScale_ : true;
		int front = module ? module->engine.spectrumFront() : 0;
		if (module) {
			int cols = std::max(1, (int)specW);
			for (int b = 0; b < 10; ++b) {
				const float* data = module->engine.spectrumData(front, b);
				bool on = module->spectrumBandVisible_[b];
				NVGcolor col = sdpanel::bandColor(b);
				col.a = on ? 0.85f : 0.13f;

				nvgBeginPath(vg);
				nvgMoveTo(vg, specX, specY + specH);
				for (int x = 0; x < cols; ++x) {
					int k0 = x * bins / cols;
					int k1 = (x + 1) * bins / cols;
					float v = sdpanel::spectrumColumnMax(data, k0, k1);
					float y = specY + specH * (1.0f - sdpanel::spectrumLevel(v, log, fftRef));
					nvgLineTo(vg, specX + (float)x + 0.5f, y);
				}
				nvgLineTo(vg, specX + specW, specY + specH);
				nvgClosePath(vg);
				nvgFillColor(vg, col);
				nvgFill(vg);
				// 辉光描边（顶边更亮, 层次感）
				NVGcolor glow = col;
				glow.a = on ? 0.55f : 0.10f;
				nvgStrokeColor(vg, glow);
				nvgStrokeWidth(vg, 1.2f);
				nvgStroke(vg);
			}
		}

		// ---- 底部图例: 10 色条横排（开关态; module 判空 → 全亮） ----
		float gap = 2.0f;
		float chipW = (w - 4.0f - gap * 9.0f) / 10.0f;
		nvgFontSize(vg, std::fmin(7.0f, chipH - 1.0f));
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		for (int b = 0; b < 10; ++b) {
			float x0 = 2.0f + b * (chipW + gap);
			bool on = !module || module->spectrumBandVisible_[b];
			NVGcolor col = sdpanel::bandColor(b);
			if (!on)
				col.a = 0.22f;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x0, ly, chipW, chipH, 1.5f);
			nvgFillColor(vg, col);
			nvgFill(vg);
			float lum = 0.299f * col.r + 0.587f * col.g + 0.114f * col.b;
			NVGcolor txt = lum > 0.6f ? nvgRGBA(0x20, 0x20, 0x26, 0xcc) : nvgRGBA(0xf2, 0xf2, 0xf8, 0xcc);
			nvgFillColor(vg, txt);
			nvgText(vg, x0 + chipW * 0.5f, ly + chipH * 0.5f,
			        b == 9 ? "P" : (std::to_string(b + 1)).c_str(), nullptr);
		}
	}

	// 交互: 左键点底部图例芯片 = band mute/开关（2026-08-17 用户定稿,
	// 取代 band 条里的 mute 开关）; 右键 = 对数/线性刻度菜单。
	void onButton(const ButtonEvent& e) override {
		if (e.action != GLFW_PRESS)
			return;
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && module) {
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Spectrum analyzer"));
			menu->addChild(createMenuItem("Log scale (dB)", CHECKMARK(module->spectrumLogScale_),
				[=]() { module->spectrumLogScale_ = !module->spectrumLogScale_; }));
			menu->addChild(createMenuItem("Use dark panels", CHECKMARK(module->panelDark_),
				[=]() { module->setPanelDark(!module->panelDark_); }));
			menu->addChild(createMenuLabel("FFT window size"));
			for (int n : sdrack::kFftSizes) {
				menu->addChild(createMenuItem(std::to_string(n), CHECKMARK(module->fftSize_ == n),
					[=]() { module->setFftSize(n); }));
			}
			menu->addChild(createMenuLabel("Legend chip click = analyzer layer show/hide"));
			e.consume(this);
			return;
		}
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			// 图例芯片命中测试（与 draw 同几何）
			float legendH = std::fmin(13.0f, box.size.y * 0.10f);
			float ly = box.size.y - legendH + 2.0f;
			float chipH = legendH - 5.0f;
			float gap = 2.0f;
			float chipW = (box.size.x - 4.0f - gap * 9.0f) / 10.0f;
			if (e.pos.y >= ly && e.pos.y <= ly + chipH) {
				int b = (int)((e.pos.x - 2.0f) / (chipW + gap));
				if (b >= 0 && b < 10) {
					// 2026-08-19 用户指示: 图例 chip 只切换分析仪显示层,
					// 不写 PARAM_BAND*, DSP 通路完全不变。
					module->spectrumBandVisible_[b] = !module->spectrumBandVisible_[b];
					e.consume(this);
				}
			}
		}
	}
};


struct SpectralDissectorWidget : ModuleWidget {
	SpectralDissectorWidget(SpectralDissectorModule* module) {
		setModule(module);
		// D13 + 2026-08-18: 深 + 浅两套 SVG; 主题由本模块 panelDark_
		// 独立控制（不影响 Rack 全局/原生模块）。
		auto* panel = createPanel<SpectralThemedSvgPanel>(
			asset::plugin(pluginInstance, "res/SpectralDissector_light.svg"),
			asset::plugin(pluginInstance, "res/SpectralDissector.svg"));
		panel->module = module;
		setPanel(panel);

		// 2026-08-18 用户指示: 不挂左右上角螺丝（SVG 亦不再绘制）

		// ---- 部件放置（panel_layout.inc, 由 gen_panel.py 从 layout JSON 生成） ----
#include "panel_layout.inc"
	}

	// 2026-08-18 用户指示: 深浅主题切换放入模块右键菜单;
	// 只切换本模块 panelDark_, 不影响 Rack 全局/原生模块。
	// 2026-08-19 用户指示: FFT window size 也入右键菜单。
	void appendContextMenu(ui::Menu* menu) override {
		auto* m = dynamic_cast<SpectralDissectorModule*>(module);
		if (!m)
			return;
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Panel theme"));
		menu->addChild(createMenuItem("Use dark panels", CHECKMARK(m->panelDark_),
			[=]() { m->setPanelDark(!m->panelDark_); }));
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("FFT window size"));
		for (int n : sdrack::kFftSizes) {
			menu->addChild(createMenuItem(std::to_string(n), CHECKMARK(m->fftSize_ == n),
				[=]() { m->setFftSize(n); }));
		}
	}
};


Model* modelSpectralDissector = createModel<SpectralDissectorModule, SpectralDissectorWidget>("SpectralDissector");


// ============================================================
// Spectral Dissector R —— 右声道 breakout 扩展器（2026-08-21）
// ------------------------------------------------------------
// 1 输入 + 12 输出, 仅音频 I/O, 无参数/CV/灯。
// 通信采用 SDK 允许的“拉取”模式: 扩展器 process() 直接读左侧
// SpectralDissector 12 个 poly 输出口的 ch1(R)。主模块与本模块
// 同插件、加入顺序保证主模块先处理 → 同一采样内复制, 无额外延迟。
// 输入方向: 主模块只加了 4 行右扩展器 R 输入钩子（见上）,
// 主模块设计（面板/端口/参数/DSP）不变。
// ============================================================
struct SpectralDissectorRModule : Module {
	enum InputIds {
		INPUT_R,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_DRY_R,
		OUTPUT_B1_R,
		OUTPUT_B2_R,
		OUTPUT_B3_R,
		OUTPUT_B4_R,
		OUTPUT_B5_R,
		OUTPUT_B6_R,
		OUTPUT_B7_R,
		OUTPUT_B8_R,
		OUTPUT_B9_R,
		OUTPUT_B10_R,
		OUTPUT_MIX_R,
		NUM_OUTPUTS
	};

	SpectralDissectorRModule() {
		config(0, NUM_INPUTS, NUM_OUTPUTS, 0);
		configInput(INPUT_R, "Audio input (R)");
		configOutput(OUTPUT_DRY_R, "Dry (R)");
		const char* bandName[10] = {"Band 1", "Band 2", "Band 3", "Band 4", "Band 5",
		                            "Band 6", "Band 7", "Band 8", "Band 9 (Noise)", "Band 10 (Perc)"};
		for (int b = 0; b < 10; ++b)
			configOutput(OUTPUT_B1_R + b, std::string(bandName[b]) + " (R)");
		configOutput(OUTPUT_MIX_R, "Mix (R, all bands post-fader)");
	}

	void process(const ProcessArgs& args) override {
		Module* left = leftExpander.module;
		bool ok = left && left->model == modelSpectralDissector;

		auto readR = [&](int mainOutId) -> float {
			return ok ? left->outputs[mainOutId].getVoltage(1) : 0.0f;
		};

		outputs[OUTPUT_DRY_R].setChannels(1);
		outputs[OUTPUT_DRY_R].setVoltage(readR(SpectralDissectorModule::OUTPUT_DRY));
		for (int b = 0; b < 10; ++b) {
			outputs[OUTPUT_B1_R + b].setChannels(1);
			outputs[OUTPUT_B1_R + b].setVoltage(readR(SpectralDissectorModule::bandOutput(b)));
		}
		outputs[OUTPUT_MIX_R].setChannels(1);
		outputs[OUTPUT_MIX_R].setVoltage(readR(SpectralDissectorModule::OUTPUT_MIX));
	}
};


// 面板: 6HP 纯代码绘制（无 SVG 资产）; 主题跟随左侧主模块的
// 独立深/浅设置（无左模块时跟随 Rack 全局）。
struct SpectralDissectorRPanel : widget::Widget {
	SpectralDissectorRModule* module = nullptr;

	void step() override {
		if (module) {
			bool dark = settings::preferDarkPanels;
			Module* left = module->leftExpander.module;
			if (left && left->model == modelSpectralDissector)
				dark = static_cast<SpectralDissectorModule*>(left)->panelDark_;
			sdpanel::setModuleDark(module, dark);
		}
		widget::Widget::step();
	}

	void draw(const DrawArgs& args) override {
		const sdpanel::Theme& t = sdpanel::themeFor(module);
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRect(vg, 0.0f, 0.0f, box.size.x, box.size.y);
		nvgFillColor(vg, t.bg);
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 2.0f, 2.0f, box.size.x - 4.0f, box.size.y - 4.0f, 3.0f);
		nvgStrokeColor(vg, t.panelLine);
		nvgStrokeWidth(vg, 1.5f);
		nvgStroke(vg);
	}
};


struct SpectralDissectorRWidget : ModuleWidget {
	SpectralDissectorRWidget(SpectralDissectorRModule* module) {
		setModule(module);

		auto* panel = new SpectralDissectorRPanel;
		panel->module = module;
		panel->box.size = mm2px(Vec(6.0f * 5.08f, 128.5f));
		setPanel(panel);

		auto addLabel = [&](const std::string& text, float cx, float y, float w, float fontSize) {
			auto* lw = new sdpanel::LabelWidget;
			lw->text = text;
			lw->fontSize = fontSize;
			lw->center = true;
			lw->module = module;
			float h = fontSize * 1.6f;                 // px 高度（与 gen_panel.py 同口径）
			lw->box.pos = mm2px(Vec(cx - w * 0.5f, y));
			lw->box.size = Vec(w * 75.0f / 25.4f, h);
			addChild(lw);
		};

		addLabel("DISSECTOR R", 15.24f, 1.8f, 30.48f, 8.0f);
		addLabel("IN R", 15.24f, 7.6f, 12.0f, 6.5f);
		addInput(createInputCentered<sdpanel::ThemedPort>(
			mm2px(Vec(15.24f, 16.0f)), module, SpectralDissectorRModule::INPUT_R));

		static const char* leftNames[6] = {"DRY", "1", "2", "3", "4", "5"};
		static const char* rightNames[6] = {"6", "7", "8", "N", "P", "MIX"};
		static const int leftIds[6] = {
			SpectralDissectorRModule::OUTPUT_DRY_R,
			SpectralDissectorRModule::OUTPUT_B1_R,
			SpectralDissectorRModule::OUTPUT_B2_R,
			SpectralDissectorRModule::OUTPUT_B3_R,
			SpectralDissectorRModule::OUTPUT_B4_R,
			SpectralDissectorRModule::OUTPUT_B5_R,
		};
		static const int rightIds[6] = {
			SpectralDissectorRModule::OUTPUT_B6_R,
			SpectralDissectorRModule::OUTPUT_B7_R,
			SpectralDissectorRModule::OUTPUT_B8_R,
			SpectralDissectorRModule::OUTPUT_B9_R,
			SpectralDissectorRModule::OUTPUT_B10_R,
			SpectralDissectorRModule::OUTPUT_MIX_R,
		};
		for (int r = 0; r < 6; ++r) {
			float y = 31.0f + 15.0f * r;
			addOutput(createOutputCentered<sdpanel::ThemedPort>(
				mm2px(Vec(8.0f, y)), module, leftIds[r]));
			addOutput(createOutputCentered<sdpanel::ThemedPort>(
				mm2px(Vec(22.5f, y)), module, rightIds[r]));
			addLabel(leftNames[r], 8.0f, y + 5.6f, 8.0f, 6.5f);
			addLabel(rightNames[r], 22.5f, y + 5.6f, 8.0f, 6.5f);
		}
	}
};


Model* modelSpectralDissectorR = createModel<SpectralDissectorRModule, SpectralDissectorRWidget>("SpectralDissectorR");


void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelSpectralDissector);
	p->addModel(modelSpectralDissectorR);
}
