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
// panel.hpp — P5 面板部件（主题感知自绘控件, D13 双主题）
// ------------------------------------------------------------
// 深/浅两套配色共用一套 layout（D13）。部件不用 SVG 资产,
// 在 draw() 内按 settings::preferDarkPanels 取色 ⇒ 主题切换
// 即时生效（与 ThemedSvgPanel 的 step() 换背景同步）。
// 配色与 layout.html 设计器 / plugin/tools/gen_panel.py 的调色板
// 一致（docs/08 记录为唯一来源）。
//
// 频谱分析仪 band 色（P5.2, D11）: 取自 26.08.13 patchs/scope.maxpat
// （pak r g b 消息, 非精确复刻 —— 仅视觉参照）:
//   B1 橙 #d97f14 · B2 黄 #d9d914 · B3 绿 #4ad914 · B4 青 #14d9b5
//   B5 浅蓝 #14a3d9 · B6 蓝 #144ad9 · B7 紫 #6e14d9 · B8 品红 #d914d9
//   B9(噪声) 白 #d9d9d9 · B10(打击) 红 #d91414
// ============================================================
#pragma once
#include <cmath>
#include <map>

#include <rack.hpp>

using namespace rack;


namespace sdpanel {

// ---------- 面板调色板（与 gen_panel.py / layout.html 一致） ----------
struct Theme {
	NVGcolor bg;          // 面板底色
	NVGcolor panelLine;   // 面板描边/分隔线
	NVGcolor text;        // 主文字
	NVGcolor textDim;     // 次文字
	NVGcolor knobBody;    // 旋钮体
	NVGcolor knobRim;     // 旋钮边缘
	NVGcolor knobPtr;     // 旋钮指针
	NVGcolor faderTrack;  // 推子槽
	NVGcolor faderCap;    // 推子帽
	NVGcolor switchOn;    // 开关 ON
	NVGcolor switchOff;   // 开关 OFF
	NVGcolor jackRing;    // 插孔外圈
	NVGcolor jackHole;    // 插孔内孔
	NVGcolor analyzerBg;  // 分析仪底
	NVGcolor analyzerGrid;// 分析仪网格
};

inline const Theme& theme(bool dark)
{
	// 现代深色扁平（2026-08-17 用户定稿）: 近黑底 + 高对比 + 细灰描边
	static const Theme darkTheme = {
		nvgRGB(0x10, 0x10, 0x16), nvgRGB(0x2c, 0x2c, 0x36),
		nvgRGB(0xe9, 0xe9, 0xf0), nvgRGB(0x8f, 0x8f, 0x9c),
		nvgRGB(0x1c, 0x1c, 0x24), nvgRGB(0x34, 0x34, 0x3f), nvgRGB(0xf2, 0xf2, 0xf7),
		nvgRGB(0x0a, 0x0a, 0x10), nvgRGB(0xd9, 0xd9, 0xe2),
		nvgRGB(0x5a, 0xa7, 0xff), nvgRGB(0x2c, 0x2c, 0x36),
		nvgRGB(0xd9, 0xd9, 0xe2), nvgRGB(0x08, 0x08, 0x0c),
		nvgRGB(0x08, 0x08, 0x0d), nvgRGB(0x1d, 0x1d, 0x26),
	};
	static const Theme lightTheme = {
		nvgRGB(0xe9, 0xe9, 0xee), nvgRGB(0xc6, 0xc6, 0xd0),
		nvgRGB(0x1d, 0x1d, 0x24), nvgRGB(0x70, 0x70, 0x7e),
		nvgRGB(0xdc, 0xdc, 0xe4), nvgRGB(0xab, 0xab, 0xb8), nvgRGB(0x1d, 0x1d, 0x24),
		nvgRGB(0xc4, 0xc4, 0xce), nvgRGB(0x2a, 0x2a, 0x33),
		nvgRGB(0x2b, 0x7f, 0xd4), nvgRGB(0xc4, 0xc4, 0xce),
		nvgRGB(0xb8, 0xb8, 0xc6), nvgRGB(0xf2, 0xf2, 0xf6),
		nvgRGB(0xf2, 0xf2, 0xf6), nvgRGB(0xd2, 0xd2, 0xdc),
	};
	return dark ? darkTheme : lightTheme;
}

inline const Theme& theme() {
	return theme(settings::preferDarkPanels);
}

// ---------- 每模块独立主题（2026-08-18 用户指示） ----------
// 右键菜单的深/浅切换只改变本模块，不再写 settings::preferDarkPanels。
// 用 Module* 键的 map 存每模块覆盖值（仅 UI 线程读写）。
inline std::map<engine::Module*, bool>& moduleDarkMap() {
	static std::map<engine::Module*, bool> map;
	return map;
}
inline bool moduleDark(engine::Module* m) {
	if (!m)
		return settings::preferDarkPanels;
	auto it = moduleDarkMap().find(m);
	return (it == moduleDarkMap().end()) ? settings::preferDarkPanels : it->second;
}
inline void setModuleDark(engine::Module* m, bool dark) {
	if (m)
		moduleDarkMap()[m] = dark;
}
inline const Theme& themeFor(engine::Module* m) {
	return theme(moduleDark(m));
}

// ---------- 频谱分析仪 band 色（scope.maxpat 视觉参照） ----------
inline const NVGcolor& bandColor(int b)   // b=0..9 → B1..B10
{
	static const NVGcolor c[10] = {
		nvgRGB(0xd9, 0x7f, 0x14), nvgRGB(0xd9, 0xd9, 0x14), nvgRGB(0x4a, 0xd9, 0x14),
		nvgRGB(0x14, 0xd9, 0xb5), nvgRGB(0x14, 0xa3, 0xd9), nvgRGB(0x14, 0x4a, 0xd9),
		nvgRGB(0x6e, 0x14, 0xd9), nvgRGB(0xd9, 0x14, 0xd9), nvgRGB(0xd9, 0xd9, 0xd9),
		nvgRGB(0xd9, 0x14, 0x14),
	};
	return c[b];
}

// ---------- 分析仪显示刻度（plugin.cpp 分析仪 widget 共用, 可单测） ----------
// |X| 满幅正弦 bin 幅度 = A·N/4 = 1024（mag_raw 口径, docs/00 §9）→ 0 dB 参照;
// 线性 0..1 / 对数 −80..0 dB → 0..1, 均钳位且负值/零 → 0。
inline float spectrumLevel(float v, bool log)
{
	const float ref = 1024.0f;
	if (!log)
		return std::fmax(std::fmin(v / ref, 1.0f), 0.0f);
	if (v <= 0.0f)
		return 0.0f;
	float db = 20.0f * std::log10((double)v / ref);
	return std::fmax(std::fmin((db + 80.0f) / 80.0f, 1.0f), 0.0f);
}

// 列抽取: [k0, k1) 的 max（widget 把 2049 bin 映射到 ~specW 列）
inline float spectrumColumnMax(const float* data, int k0, int k1)
{
	float v = 0.0f;
	for (int k = k0; k < k1; ++k)
		v = std::fmax(v, data[k]);
	return v;
}

// ---------- 参数归一化辅助（0..1, 与部件绘制同源） ----------
// 修复 2026-08-17 用户反馈"指针角度/帽位与范围不匹配"：
// 此前部件用 getValue()（显示值域, 如 Threshold −70..12、Gain 0..1.5）
// 直接当 0..1 用 ⇒ 指针角度错位甚至越界。统一改为 (v−min)/(max−min)。
inline float paramNorm(engine::ParamQuantity* pq) {
	if (!pq)
		return 0.0f;
	float range = pq->getMaxValue() - pq->getMinValue();
	if (range <= 0.0f)
		return 0.0f;
	return math::clamp((pq->getValue() - pq->getMinValue()) / range, 0.0f, 1.0f);
}

// ---------- 主题感知标签（运行时 nvgText, 2026-08-18 修订） ----------
// 此前标签由 gen_panel.py 生成 5×7 点阵 SVG path —— Rack 的 nanosvg
// 对 1px 级横笔画的抗锯齿/伪影不稳定, 深主题下用户仍反馈"伪影、不清晰"。
// 改为真实字体 nvgText（Rack 窗口已加载 DejaVuSans 等系统字体）:
// 文字清晰且主题切换即时生效。box.pos = 标签 bbox 左上角,
// box.size = 预估 bbox（由 panel_layout.inc 按文本长度生成）;
// draw 内基线 = box.size.y - 2, 与原 layout JSON 的 y=基线口径一致。
struct LabelWidget : widget::TransparentWidget {
	std::string text;
	float fontSize = 7.0f;
	bool center = false;
	bool dim = false;   // true → textDim（次文字/作者名深灰, 2026-08-18 R9）
	engine::Module* module = nullptr;

	void draw(const DrawArgs& args) override {
		const Theme& t = themeFor(module);
		NVGcontext* vg = args.vg;
		nvgFontSize(vg, fontSize);
		nvgTextAlign(vg, center ? (NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE)
		                        : (NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE));
		nvgFillColor(vg, dim ? t.textDim : t.text);
		float x = center ? box.size.x * 0.5f : 0.0f;
		float y = box.size.y - 2.0f;
		nvgText(vg, x, y, text.c_str(), nullptr);
	}
};

// ---------- 主题感知自绘旋钮（极简细件 + 刻度, 2026-08-17 定稿;
//      2026-08-18 修订: 极值角度与 Rack 大多数模块一致, 不显示数值） ----------
struct ThemedKnob : app::Knob {
	ThemedKnob() {
		box.size = mm2px(Vec(9.5f, 9.5f));
		minAngle = -0.83f * M_PI;
		maxAngle = 0.83f * M_PI;
		// 2026-08-18 用户反馈: 触控板拖拽参数改变过快。此前自实现
		// onDragMove 按"框高 = 全量程"1:1 映射, 灵敏度是 Rack 标准
		// Knob（0.001 × range/角域 × 速度 × 修饰键倍率）的百倍级。
		// 此处不再设置 forceLinear、也不再覆写 onDragMove —— 拖拽
		// 交给 app::Knob 标准实现, 与大多数模块的手感/全局 knobMode
		// 设置完全一致。
	}

	// 视觉角（Rack 大多数模块的 SvgKnob 语义）: 表盘 12 点为基准,
	// 值 0 → minAngle 旋转（7 点钟方向）, 值 1 → maxAngle（5 点钟方向）,
	// 中位 → 12 点。直接画指针时角度 = -90° + rescale(v, minAngle..maxAngle)。
	float visualAngle(float norm) const {
		return -M_PI / 2.0f + math::rescale(norm, 0.0f, 1.0f, minAngle, maxAngle);
	}

	// 归一化 0..1（与指针角度的绘制同源）。修复 2026-08-17 用户反馈
	// "指针角度与范围不匹配"：此前用 getValue()（显示值域, 如 Threshold
	// −70..12、Gain 0..1.5）直接当 0..1 用 ⇒ 指针角度错位甚至越界。
	float norm() {
		return sdpanel::paramNorm(getParamQuantity());
	}

	void draw(const DrawArgs& args) override {
		const Theme& t = themeFor(module);
		NVGcontext* vg = args.vg;
		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;
		float r = std::fmin(cx, cy) - 1.5f;

		// 2026-08-18 用户定稿: 电位器外圈纹路（弧轨 + 刻度）全部删除,
		// 只保留扁平盘 + 细描边 + 指针。
		// 扁平盘 + 细描边
		nvgBeginPath(vg);
		nvgCircle(vg, cx, cy, r - 1.8f);
		nvgFillColor(vg, t.knobBody);
		nvgFill(vg);
		nvgStrokeColor(vg, t.knobRim);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		// 细指针（1.2px, 圆头; 角度 = 归一化值, 与参数范围严格匹配;
		// 2026-08-18: 不绘制参数数值、不绘制外圈纹路）
		float v = norm();
		float ang = visualAngle(v);
		float pr = r - 2.6f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, cx, cy);
		nvgLineTo(vg, cx + pr * std::cos(ang), cy + pr * std::sin(ang));
		nvgStrokeColor(vg, t.knobPtr);
		nvgStrokeWidth(vg, 1.2f);
		nvgLineCap(vg, NVG_ROUND);
		nvgStroke(vg);
	}
};

// ---------- 主题感知自绘推子（细槽 + 刻度 + 细帽, 2026-08-17 定稿） ----------
struct ThemedFader : app::SliderKnob {
	// 帽几何常量（与 draw 同源; 行程两端 3px 内边距 ⇒ 帽严格在槽内）
	static constexpr float kCapW = 7.0f, kCapH = 5.0f, kMargin = 3.0f;

	// 归一化 0..1（同 ThemedKnob::norm —— 修复"推子帽超图形范围":
	// 此前用 getValue()（如 Threshold −70..12）当 0..1 ⇒ 帽位错位/越界）
	float norm() {
		return sdpanel::paramNorm(getParamQuantity());
	}

	// 位置 → 归一化: 帽中心行程 [margin+cap/2, len−margin−cap/2],
	// 与 draw 的 travel 严格同源 ⇒ 点击跳转位置 = 帽显示位置。
	float normAt(float pos) const {
		float len = horizontal ? box.size.x : box.size.y;
		float cap = horizontal ? kCapW : kCapH;
		float travel = len - cap - 2.0f * kMargin;
		if (travel <= 0.0f)
			return 0.0f;
		float p = horizontal ? (pos - kMargin - cap * 0.5f)
		                     : (len - kMargin - cap * 0.5f - pos);
		return math::clamp(p / travel, 0.0f, 1.0f);
	}

	void setNorm(float norm) {
		auto* pq = getParamQuantity();
		if (!pq)
			return;
		pq->setValue(math::rescale(norm, 0.0f, 1.0f, pq->getMinValue(), pq->getMaxValue()));
	}

	// 左键点击 = 帽跳到该位置（SliderKnob 惯例; 几何与 draw 同源）。
	// 调用 Knob::onButton 而非 SliderKnob::onButton —— 后者的轴向
	// 自适应行为与本推子的固定 horizontal 设定冲突。
	// 2026-08-18 用户反馈: 点击槽边缘会漏到模块拖动 —— 这里对框内
	// 任意左键按下显式 consume, 保证整个 fader box 都是推子判定区。
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			setNorm(normAt(horizontal ? e.pos.x : e.pos.y));
			e.consume(this);
		}
		Knob::onButton(e);
	}

	// 拖拽 = 自实现线性（帽位移 = 指针位移, 1:1; 不受 settings::knobMode
	// 影响, 且帽永远不会超出槽的行程范围 —— 与 draw/normAt 同源）
	void onDragMove(const DragMoveEvent& e) override {
		float len = horizontal ? box.size.x : box.size.y;
		float cap = horizontal ? kCapW : kCapH;
		float travel = len - cap - 2.0f * kMargin;
		if (travel <= 0.0f)
			return;
		float dn = (horizontal ? e.mouseDelta.x : -e.mouseDelta.y) / travel;
		setNorm(norm() + dn);
	}

	void draw(const DrawArgs& args) override {
		const Theme& t = themeFor(module);
		NVGcontext* vg = args.vg;
		float w = box.size.x;
		float h = box.size.y;
		bool horiz = horizontal;
		float slot = 4.2f * 2.9528f;   // 凹槽宽 4.2mm

		// 凹槽 + 内轨
		nvgBeginPath(vg);
		if (horiz)
			nvgRoundedRect(vg, 1.5f, (h - slot) * 0.5f, w - 3.0f, slot, slot * 0.5f);
		else
			nvgRoundedRect(vg, (w - slot) * 0.5f, 1.5f, slot, h - 3.0f, slot * 0.5f);
		nvgFillColor(vg, t.faderTrack);
		nvgFill(vg);
		nvgStrokeColor(vg, t.knobRim);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);

		// 刻度（槽旁, 7 段）
		nvgBeginPath(vg);
		for (int i = 0; i <= 6; ++i) {
			float f = (float)i / 6.0f;
			if (horiz) {
				float x = 2.5f + f * (w - 5.0f);
				nvgMoveTo(vg, x, h * 0.5f + slot * 0.5f + 0.7f);
				nvgLineTo(vg, x, h * 0.5f + slot * 0.5f + 2.2f);
			} else {
				float y = h - 2.5f - f * (h - 5.0f);
				nvgMoveTo(vg, w * 0.5f + slot * 0.5f + 0.7f, y);
				nvgLineTo(vg, w * 0.5f + slot * 0.5f + 2.2f, y);
			}
		}
		nvgStrokeColor(vg, t.textDim);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		// 细帽（严格在槽内: 行程两端留 3px 内边距, 带浅阴影;
		// 帽位 = 归一化值 × travel —— 与 normAt/onDragMove 同源）
		float v = norm();
		float capW = horiz ? 7.0f : 7.0f;
		float capH = horiz ? 5.0f : 5.0f;
		float travel = horiz ? (w - capW - 6.0f) : (h - capH - 6.0f);
		float cx, cy;
		if (horiz) {
			cx = 3.0f + capW * 0.5f + travel * v;
			cy = h * 0.5f;
		} else {
			cx = w * 0.5f;
			cy = h - 3.0f - capH * 0.5f - travel * v;
		}
		nvgBeginPath(vg);
		nvgRoundedRect(vg, cx - capW * 0.5f + 0.8f, cy - capH * 0.5f + 1.0f, capW, capH, 1.4f);
		nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x40));
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, cx - capW * 0.5f, cy - capH * 0.5f, capW, capH, 1.4f);
		nvgFillColor(vg, t.faderCap);
		nvgFill(vg);
		nvgStrokeColor(vg, t.textDim);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
	}
};

// ---------- 主题感知自绘开关（Switch 点击行为 + nvg 绘制） ----------
struct ThemedSwitch : app::Switch {
	ThemedSwitch() { box.size = mm2px(Vec(8.0f, 4.5f)); }
	void draw(const DrawArgs& args) override {
		const Theme& t = themeFor(module);
		NVGcontext* vg = args.vg;
		float w = box.size.x;
		float h = box.size.y;
		bool on = getParamQuantity() ? getParamQuantity()->getValue() > 0.5f : false;

		// 开关体（扁平）
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.0f, 0.0f, w, h, 1.2f);
		nvgFillColor(vg, on ? t.switchOn : t.switchOff);
		nvgFill(vg);
		nvgStrokeColor(vg, t.textDim);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);

		// 状态条
		float y0 = on ? h * 0.18f : h * 0.55f;
		float y1 = on ? h * 0.45f : h * 0.82f;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, w * 0.18f, y0, w * 0.64f, y1 - y0, 1.0f);
		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x90));
		nvgFill(vg);
	}
};

// ---------- 主题感知自绘插孔（PortWidget 接线行为 + nvg 绘制） ----------
struct ThemedPort : app::PortWidget {
	ThemedPort() { box.size = mm2px(Vec(9.0f, 9.0f)); }
	void draw(const DrawArgs& args) override {
		const Theme& t = themeFor(module);
		NVGcontext* vg = args.vg;
		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;
		float r = std::fmin(cx, cy) - 1.0f;

		// 外圈
		nvgBeginPath(vg);
		nvgCircle(vg, cx, cy, r);
		nvgFillColor(vg, t.jackRing);
		nvgFill(vg);
		// 内孔
		nvgBeginPath(vg);
		nvgCircle(vg, cx, cy, r * 0.55f);
		nvgFillColor(vg, t.jackHole);
		nvgFill(vg);
		// 内孔描边
		nvgBeginPath(vg);
		nvgCircle(vg, cx, cy, r * 0.55f);
		nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x30));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);
	}
};

// ---------- 主题感知彩色活动灯（ModuleLightWidget + band 色） ----------
struct ThemedLight : app::ModuleLightWidget {
	ThemedLight() { box.size = mm2px(Vec(3.2f, 3.2f)); }
	void setColor(NVGcolor c) {
		addBaseColor(c);
		color = c;
		bgColor = themeFor(module).panelLine;
		borderColor = themeFor(module).knobRim;
	}
	void step() override {
		// 每模块主题切换后同步灯的外圈/描边
		bgColor = themeFor(module).panelLine;
		borderColor = themeFor(module).knobRim;
		ModuleLightWidget::step();
	}
};

} // namespace sdpanel
