#!/usr/bin/env python3
# Spectral Dissector — VCV Rack 2 plugin
# Copyright (C) 2026 Xiehuo
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ============================================================
# gen_panel.py — P5.4 面板生成器（仅 Python 标准库）
# ------------------------------------------------------------
# 输入: layout JSON（layout.html 拖拽设计器导出; 坐标 mm, 左上原点;
#       部件类型 input/output/switch/knob/attenuator/faderV/faderH/
#       light/label/analyzer; bind = plugin.cpp 枚举名）
# 输出:
#   plugin/res/SpectralDissector.svg        深主题面板背景
#   plugin/res/SpectralDissector_light.svg  浅主题面板背景
#   plugin/src/panel_layout.inc             部件放置代码（plugin.cpp include）
#   layout.default.json                     默认布局（本脚本合成, 亦为
#                                           layout.html 的初始布局来源）
# 坐标口径: Rack 2 mm2px = 75dpi（1mm = 2.9527559055px, 1HP = 15px,
#           面板高 380px = 128.69mm; 与 docs/07 §6 同源）。
# 调色板与 src/panel.hpp（sdpanel::Theme）/ layout.html 一致。
# 用法:
#   python3 gen_panel.py                     # 用 layout.json（用户回传）
#   python3 gen_panel.py --default           # 合成默认布局（本文件内）
#   python3 gen_panel.py path/to/layout.json
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
RES = os.path.join(PLUGIN, "res")
SRC = os.path.join(PLUGIN, "src")

PX_PER_MM = 75.0 / 25.4          # 2.9527559055
WIDTH_HP = 48
WIDTH_MM = WIDTH_HP * 5.08       # 203.2
HEIGHT_PX = 380                  # 128.69mm
WIDTH_PX = WIDTH_HP * 15         # 600

DARK = {
    "bg": "#101016", "line": "#2c2c36", "text": "#e9e9f0", "textDim": "#8f8f9c",
    "knob": "#1c1c24", "knobRim": "#34343f",
    "faderTrack": "#0a0a10", "faderCap": "#d9d9e2",
    "switchOn": "#5aa7ff", "switchOff": "#2c2c36",
    "jackRing": "#d9d9e2", "jackHole": "#08080c",
    "analyzerBg": "#08080d", "analyzerGrid": "#1d1d26",
}
LIGHT = {
    "bg": "#e9e9ee", "line": "#c6c6d0", "text": "#1d1d24", "textDim": "#70707e",
    "knob": "#dcdce4", "knobRim": "#ababb8",
    "faderTrack": "#c4c4ce", "faderCap": "#2a2a33",
    "switchOn": "#2b7fd4", "switchOff": "#c4c4ce",
    "jackRing": "#b8b8c6", "jackHole": "#f2f2f6",
    "analyzerBg": "#f2f2f6", "analyzerGrid": "#d2d2dc",
}

# scope.maxpat 视觉参照 band 色（docs/08; 与 panel.hpp sdpanel::bandColor 一致）
BAND_COLORS = ["#d97f14", "#d9d914", "#4ad914", "#14d9b5", "#14a3d9",
               "#144ad9", "#6e14d9", "#d914d9", "#d9d9d9", "#d91414"]

MM = PX_PER_MM


def px(mm):
    return mm * MM


def f2(v):
    s = ("%.3f" % v).rstrip("0")
    if s.endswith("."):
        s += "0"
    return s


def round1(v):
    return round(v * 10) / 10


# ============================================================
# 默认布局合成（用户可改; 亦为 layout.html 初始布局）
# ============================================================
def default_layout():
    w = []
    # 标题与输入（D19: 音频输入 = 1 个 poly 口 2ch L/R）
    w.append({"type": "label", "text": "SPECTRAL DISSECTOR", "x": 5, "y": 8, "size": 10, "bold": True})
    w.append({"type": "label", "text": "26.08.13 port \u00b7 Xiehuo", "x": 5, "y": 14, "size": 6.5})
    w.append({"type": "label", "text": "IN L/R", "x": 9.5, "y": 26.5, "size": 6.5})
    w.append({"type": "input", "bind": "INPUT_AUDIO", "x": 13.5, "y": 20})
    # 分析仪（最左, 图例由 widget 运行时绘制）
    w.append({"type": "analyzer", "x": 4, "y": 30, "w": 54, "h": 94})

    # 上部控制区: 5 个带 CV 的旋钮列 + Rise/Fall + Threshold/Focus 竖推子 + Tilt 横推子
    ctl = [("SPACING", "PARAM_SPACING", "INPUT_CV_SPACING", "PARAM_CVATT_SPACING"),
           ("GATE", "PARAM_GATE", "INPUT_CV_GATE", "PARAM_CVATT_GATE"),
           ("BLUR", "PARAM_BLUR", "INPUT_CV_BLUR", "PARAM_CVATT_BLUR"),
           ("PERC", "PARAM_PERC", "INPUT_CV_PERC", "PARAM_CVATT_PERC"),
           ("DETAIL", "PARAM_DETAIL", "INPUT_CV_DETAIL", "PARAM_CVATT_DETAIL")]
    w.append({"type": "label", "text": "CONTROLS", "x": 62, "y": 6.5, "size": 7})
    for i, (name, p, cv, att) in enumerate(ctl):
        x = 63 + 15 * i
        w.append({"type": "label", "text": name, "x": x, "y": 6.5, "size": 6, "anchor": "middle"})
        w.append({"type": "knob", "bind": p, "x": x, "y": 11, "value": True})
        w.append({"type": "attenuator", "bind": att, "x": x, "y": 25, "value": True})
        w.append({"type": "input", "bind": cv, "x": x, "y": 37})
    for i, (name, p) in enumerate([("RISE", "PARAM_RISE_MS"), ("FALL", "PARAM_FALL_MS")]):
        x = 139 + 15 * i
        w.append({"type": "label", "text": name, "x": x, "y": 6.5, "size": 6, "anchor": "middle"})
        w.append({"type": "knob", "bind": p, "x": x, "y": 11, "value": True})
    w.append({"type": "label", "text": "THRESH", "x": 171, "y": 6.5, "size": 6, "anchor": "middle"})
    w.append({"type": "faderV", "bind": "PARAM_THRESHOLD", "x": 171, "y": 25, "h": 32})
    w.append({"type": "attenuator", "bind": "PARAM_CVATT_THRESHOLD", "x": 182, "y": 17, "value": True})
    w.append({"type": "input", "bind": "INPUT_CV_THRESHOLD", "x": 182, "y": 30.5})
    w.append({"type": "label", "text": "FOCUS", "x": 195, "y": 6.5, "size": 6, "anchor": "middle"})
    w.append({"type": "faderV", "bind": "PARAM_FOCUS", "x": 195, "y": 25, "h": 32})
    w.append({"type": "attenuator", "bind": "PARAM_CVATT_FOCUS", "x": 206, "y": 17, "value": True})
    w.append({"type": "input", "bind": "INPUT_CV_FOCUS", "x": 206, "y": 30.5})
    w.append({"type": "label", "text": "TILT", "x": 219, "y": 6.5, "size": 6, "anchor": "middle"})
    w.append({"type": "faderV", "bind": "PARAM_TILT", "x": 219, "y": 25, "h": 32})
    # TILT CV 对（2026-08-18 R2）: 默认布局推子右侧无空位（FOCUS CV 对/MIX 口）,
    # 放推子正下方（att y=45.5 / in y=52.5, 与 band 条无冲突）
    w.append({"type": "attenuator", "bind": "PARAM_CVATT_TILT", "x": 219, "y": 45.5, "value": True})
    w.append({"type": "input", "bind": "INPUT_CV_TILT", "x": 219, "y": 52.5})
    # D20: MIX 输出口（右上角, 用户排版时自行移动）
    w.append({"type": "label", "text": "MIX (ALL)", "x": 232, "y": 27.5, "size": 5.5, "anchor": "middle"})
    w.append({"type": "output", "bind": "OUTPUT_MIX", "x": 232, "y": 20})

    # 频带条: Dry + B1..B10（自适应宽度铺满）
    strip_pitch = (WIDTH_MM - 58.0 - 6.0) / 10.0
    w.append({"type": "label", "text": "BAND STRIPS", "x": 58, "y": 56.5, "size": 7})
    for i in range(11):
        x = round1(58.0 + strip_pitch * i)
        if i == 0:
            w.append({"type": "label", "text": "DRY", "x": x, "y": 62.5, "size": 6, "anchor": "middle"})
            w.append({"type": "switch", "bind": "PARAM_DRY", "x": x, "y": 72})
        else:
            b = i - 1
            # Offset 仅 B1..B7 且为 attenuator 小尺寸（第六轮;
            # 26.08.13 gen~[main] 权威: off1..off7, docs/00 §7）
            if i <= 7:
                w.append({"type": "attenuator", "bind": "PARAM_OFF%d" % i, "x": x, "y": 68.5})
            w.append({"type": "faderV", "bind": "PARAM_GAIN%d" % i, "x": x, "y": 88, "h": 29})
            # band mute 改由分析仪底部图例芯片承担（2026-08-17 定稿）
            w.append({"type": "light", "bind": "LIGHT_BAND%d" % i, "x": x + 5.5, "y": 106.5,
                      "color": BAND_COLORS[b], "band": b})
        # D19: 每 band 1 个 poly 输出口（2ch L/R）, 取代原 L/R 双 mono 口
        w.append({"type": "output",
                  "bind": "OUTPUT_DRY" if i == 0 else "OUTPUT_B%d" % i,
                  "x": x, "y": 117.5})
        # R5 (2026-08-18 用户指示): 输出口下方标注 —— B1..B8="Band N",
        # B9="Noise", B10="Perc"（取代第五轮的 B 字样删除）
        if i >= 1:
            text = "Noise" if i == 9 else ("Perc" if i == 10 else "Band %d" % i)
            w.append({"type": "label", "text": text, "x": x, "y": 127.2,
                      "size": 5.5, "anchor": "middle"})
    return {
        "format": "sd-layout-1",
        "meta": {"name": "Spectral Dissector", "default": True},
        "panel": {"widthHP": WIDTH_HP, "widthMm": round1(WIDTH_MM),
                  "heightPx": HEIGHT_PX, "heightMm": round1(HEIGHT_PX / MM),
                  "pxPerMm": PX_PER_MM},
        "widgets": w,
    }



# ============================================================
# 5×7 点阵笔画字体（文字 → 矢量路径; Rack 的 nanosvg 不支持
# <text>, Fundamental 等全部用路径文字 —— 本生成器同法,
# LED 风格与"现代扁平"一致）。字形定义 = 7 行 × 5 列 bit。
# ============================================================
FONT5X7 = {
    "A": (0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001),
    "B": (0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110),
    "C": (0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110),
    "D": (0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110),
    "E": (0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111),
    "F": (0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000),
    "G": (0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01111),
    "H": (0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001),
    "I": (0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110),
    "J": (0b00111,0b00010,0b00010,0b00010,0b00010,0b10010,0b01100),
    "K": (0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001),
    "L": (0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111),
    "M": (0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001),
    "N": (0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001),
    "O": (0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110),
    "P": (0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000),
    "Q": (0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101),
    "R": (0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001),
    "S": (0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110),
    "T": (0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100),
    "U": (0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110),
    "V": (0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100),
    "W": (0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010),
    "X": (0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001),
    "Y": (0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100),
    "Z": (0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111),
    "0": (0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110),
    "1": (0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110),
    "2": (0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111),
    "3": (0b11111,0b00010,0b00100,0b00010,0b00001,0b10001,0b01110),
    "4": (0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010),
    "5": (0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110),
    "6": (0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110),
    "7": (0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000),
    "8": (0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110),
    "9": (0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100),
    " ": (0,0,0,0,0,0,0),
    "\u00b7": (0,0,0b00100,0b00100,0,0,0),          # ·
    ".": (0,0,0,0,0,0,0b00100),
    ",": (0,0,0,0,0,0b00100,0b01000),
    "/": (0b00001,0b00001,0b00010,0b00100,0b01000,0b10000,0b10000),
    "(": (0b00010,0b00100,0b01000,0b01000,0b01000,0b00100,0b00010),
    ")": (0b01000,0b00100,0b00010,0b00010,0b00010,0b00100,0b01000),
    ":": (0,0,0b00100,0,0b00100,0,0),
    "-": (0,0,0,0b01110,0,0,0),
    "=": (0,0,0b01110,0,0b01110,0,0),
    "+": (0,0,0b00100,0b01110,0b00100,0,0),
    "%": (0b11001,0b11010,0b00010,0b00100,0b01000,0b01011,0b10011),
}

def stroke_text(x_px, y_px, text, size_px, color, anchor="start", bold=False):
    """5×7 点阵文字 → SVG path（y_px = 基线; 自动大写）。

    2026-08-18 修订（用户反馈"深主题文字不显示"）:
    最小字号 7px、笔画宽 ≥1.1px、标签统一用高对比 text 色 ——
    6px/0.92px 细笔画 + textDim #8f8f9c 在深色底上过淡。
    """
    s = max(7.0, size_px)
    cell = s / 7.0
    sw = max(1.1, cell * (1.4 if bold else 1.15))
    upper = text.upper()
    adv = cell * 6.0
    width = adv * len(upper)
    x0 = x_px if anchor == "start" else x_px - width / 2.0
    segs = []
    for i, ch in enumerate(upper):
        rows = FONT5X7.get(ch)
        if not rows:
            continue
        cx = x0 + i * adv
        for r, bits in enumerate(rows):
            if not bits:
                continue
            cy = (y_px - s) + (r + 0.5) * cell
            c0 = None
            for c in range(5):
                on = (bits >> (4 - c)) & 1
                if on and c0 is None:
                    c0 = c
                if not on and c0 is not None:
                    segs.append("M%.2f %.2fH%.2f" % (cx + (c0 + 0.5) * cell, cy, cx + (c - 0.5) * cell))
                    c0 = None
            if c0 is not None:
                segs.append("M%.2f %.2fH%.2f" % (cx + (c0 + 0.5) * cell, cy, cx + (4 + 0.5) * cell))
    if not segs:
        return ""
    return ('<path d="%s" fill="none" stroke="%s" stroke-width="%.2f"/>'
            % ("".join(segs), color, sw))

# ============================================================
# SVG 生成
# ============================================================
def svg_panel(layout, pal):
    parts = []
    W = layout["panel"]["widthMm"] * MM
    H = layout["panel"]["heightPx"]
    parts.append(
        '<svg width="%d" height="%d" viewBox="0 0 %d %d" xmlns="http://www.w3.org/2000/svg">'
        % (round(W), H, round(W), H))
    parts.append('<rect x="0" y="0" width="%d" height="%d" rx="5" fill="%s"/>'
                 % (round(W), H, pal["bg"]))
    parts.append('<rect x="2" y="2" width="%d" height="%d" rx="4" fill="none" '
                 'stroke="%s" stroke-width="2"/>' % (round(W) - 4, H - 4, pal["line"]))
    # 2026-08-18 用户指示: 面板不画上角螺丝（运行时同样不挂 ThemedSvgScrew）

    for w in layout["widgets"]:
        t = w["type"]
        x, y = w["x"], w["y"]
        if t == "label":
            # 2026-08-18 修订: 标签不再生成 SVG path（5×7 点阵伪影,
            # 深主题不清晰）。Rack 运行时由 panel_layout.inc 生成的
            # sdpanel::LabelWidget 用 nvgText 真实字体绘制; SVG 仅作
            # 静态背景, 浏览器/设计器预览用 layout.html 的真实 <text>。
            continue
        elif t == "analyzer":
            aw, ah = px(w["w"]), px(w["h"])
            parts.append('<rect x="%s" y="%s" width="%s" height="%s" rx="3" fill="%s" '
                         'stroke="%s" stroke-width="1"/>'
                         % (f2(px(x)), f2(px(y)), f2(aw), f2(ah),
                            pal["analyzerBg"], pal["line"]))
            # 网格（电平 4 线 + 频率 100Hz/1k/10k 竖线, 静态占位;
            # 几何与 plugin.cpp SpectrumAnalyzerWidget 同源:
            #   legendH=13, legendGap=11, 谱底 = y+2+specH, 图例顶 = y+ah-11）
            gd = []
            legend_h, legend_gap = 13.0, 11.0
            spec_h = ah - legend_h - legend_gap
            for i in range(1, 5):
                gy = px(y) + 2 + spec_h * i / 5.0
                gd.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="0.8"/>'
                          % (f2(px(x) + 2), f2(gy), f2(px(x) + aw - 2), f2(gy), pal["analyzerGrid"]))
            for fx in (0.2, 0.5, 0.8):
                gx = px(x) + 2 + (aw - 4) * fx
                gd.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="0.8"/>'
                          % (f2(gx), f2(px(y) + 2), f2(gx), f2(px(y) + 2 + spec_h), pal["analyzerGrid"]))
            # 底部图例 10 色条（静态占位; 运行时由 widget 按开关态重绘）
            gap = 2.0
            chipW = (aw - 4 - gap * 9) / 10.0
            chH = 8.0
            ly = px(y) + ah - legend_h + 2.0
            for b in range(10):
                x0 = px(x) + 2 + b * (chipW + gap)
                gd.append('<rect x="%s" y="%s" width="%s" height="%s" rx="1.5" fill="%s"/>'
                          % (f2(x0), f2(ly), f2(chipW), f2(chH), BAND_COLORS[b]))
            parts.append("".join(gd))
        elif t in ("input", "output"):
            r = 4.5
            parts.append('<circle cx="%s" cy="%s" r="%s" fill="%s"/>'
                         % (f2(px(x)), f2(px(y)), f2(r), pal["jackRing"]))
            parts.append('<circle cx="%s" cy="%s" r="%s" fill="%s"/>'
                         % (f2(px(x)), f2(px(y)), f2(r * 0.55), pal["jackHole"]))
        elif t == "switch":
            parts.append('<rect x="%s" y="%s" width="%s" height="%s" rx="1.5" fill="%s" '
                         'stroke="%s" stroke-width="1"/>'
                         % (f2(px(x - 4)), f2(px(y - 2.25)), f2(px(8)), f2(px(4.5)),
                            pal["switchOff"], pal["textDim"]))
        elif t == "knob":
            r = 4.75
            a = []
            # 2026-08-18 用户定稿: 外圈纹路（弧轨 + 刻度）全部删除;
            # 只保留扁平盘 + 细描边 + 静态中位指针。
            # 2026-08-18: SVG 不再画静态指针 —— 运行时由 ThemedKnob 自绘,
            # 否则 Rack 面板上会出现"固定顶部竖线"伪影。
            a.append('<circle cx="%s" cy="%s" r="%s" fill="%s" stroke="%s" stroke-width="1"/>'
                     % (f2(px(x)), f2(px(y)), f2(px(r - 0.6)), pal["knob"], pal["knobRim"]))
            parts.append("".join(a))
        elif t == "attenuator":
            r = 3.75
            a = []
            # 外圈纹路删除（同 knob）; 只保留小盘 + 细描边 + 静态中位指针。
            a.append('<circle cx="%s" cy="%s" r="%s" fill="%s" stroke="%s" stroke-width="0.8"/>'
                     % (f2(px(x)), f2(px(y)), f2(px(r - 0.5)), pal["knob"], pal["knobRim"]))
            parts.append("".join(a))
        elif t == "faderV":
            hh = w.get("h", 30)
            slot = px(4.2)   # 凹槽宽 4.2mm
            a = ['<rect x="%s" y="%s" width="%s" height="%s" rx="1.6" fill="%s" stroke="%s" stroke-width="0.8"/>'
                 % (f2(px(x) - slot / 2), f2(px(y - hh / 2)), f2(slot), f2(px(hh)),
                    pal["faderTrack"], pal["knobRim"])]
            for i in range(7):
                yy = px(y - hh / 2 + 1.5) + (px(hh) - 3) * i / 6.0
                a.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="0.8"/>'
                         % (f2(px(x) + slot / 2 + 0.7), f2(yy), f2(px(x) + slot / 2 + 2.2), f2(yy),
                            pal["textDim"]))
            parts.append("".join(a))
        elif t == "faderH":
            ww = w.get("w", 30)
            slot = px(4.2)
            a = ['<rect x="%s" y="%s" width="%s" height="%s" rx="1.6" fill="%s" stroke="%s" stroke-width="0.8"/>'
                 % (f2(px(x - ww / 2)), f2(px(y) - slot / 2), f2(px(ww)), f2(slot),
                    pal["faderTrack"], pal["knobRim"])]
            for i in range(7):
                xx = px(x - ww / 2 + 1.5) + (px(ww) - 3) * i / 6.0
                a.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="0.8"/>'
                         % (f2(xx), f2(px(y) + slot / 2 + 0.7), f2(xx), f2(px(y) + slot / 2 + 2.2),
                            pal["textDim"]))
            parts.append("".join(a))
        elif t == "light":
            parts.append('<circle cx="%s" cy="%s" r="1.4" fill="%s"/>'
                         % (f2(px(x)), f2(px(y)), w.get("color", "#888888")))
    parts.append("</svg>")
    return "\n".join(parts)


# ============================================================
# C++ 部件代码生成（panel_layout.inc）
# ============================================================
def _cpp_str(s):
    return s.replace("\\", "\\\\").replace("\"", "\\\"")


def cpp_widgets(layout):
    L = []
    L.append("// ============================================================")
    L.append("// panel_layout.inc — 由 plugin/tools/gen_panel.py 生成（勿手改）")
    L.append("// 来源: layout JSON（layout.html 设计器导出）; 坐标 mm2px(75dpi)。")
    L.append("// 部件: sdpanel 主题感知自绘控件（panel.hpp）+ SpectrumAnalyzerWidget。")
    L.append("// 标签: sdpanel::LabelWidget（nvgText 真实字体, 2026-08-18 起;")
    L.append("//        SVG 不再生成点阵 path）。")
    L.append("// ============================================================")
    for w in layout["widgets"]:
        t = w["type"]
        if t == "label":
            continue  # 标签统一在本函数末尾批量生成（置于最上层）
        x, y = f2(w["x"]), f2(w["y"])
        bind = w.get("bind", "")
        if t == "analyzer":
            L.append("// analyzer (%s × %s)mm" % (f2(w["w"]), f2(w["h"])))
            L.append("{")
            L.append("\tauto* w = new SpectrumAnalyzerWidget;")
            L.append("\tw->module = module;")
            L.append("\tw->box.pos = mm2px(Vec(%sf, %sf));" % (x, y))
            L.append("\tw->box.size = mm2px(Vec(%sf, %sf));" % (f2(w["w"]), f2(w["h"])))
            L.append("\taddChild(w);")
            L.append("}")
        elif t == "input":
            L.append("addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(%sf, %sf)), "
                     "module, SpectralDissectorModule::%s));" % (x, y, bind))
        elif t == "output":
            L.append("addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(%sf, %sf)), "
                     "module, SpectralDissectorModule::%s));" % (x, y, bind))
        elif t == "switch":
            L.append("addParam(createParamCentered<sdpanel::ThemedSwitch>(mm2px(Vec(%sf, %sf)), "
                     "module, SpectralDissectorModule::%s));" % (x, y, bind))
        elif t == "knob":
            L.append("addParam(createParamCentered<sdpanel::ThemedKnob>(mm2px(Vec(%sf, %sf)), "
                     "module, SpectralDissectorModule::%s));" % (x, y, bind))
        elif t == "attenuator":
            L.append("{")
            L.append("\tauto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(%sf, %sf)), "
                     "module, SpectralDissectorModule::%s);" % (x, y, bind))
            L.append("\tw->box.size = mm2px(Vec(7.5f, 7.5f));")
            L.append("\tw->box.pos = w->box.pos.minus(w->box.size.div(2.0f));")
            L.append("\taddParam(w);")
            L.append("}")
        elif t == "faderV":
            L.append("{")
            L.append("\tauto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(%sf, %sf)), "
                     "module, SpectralDissectorModule::%s);" % (x, y, bind))
            L.append("\tw->box.size = mm2px(Vec(10.0f, %sf));" % f2(w.get("h", 30)))
            L.append("\tw->box.pos = w->box.pos.minus(w->box.size.div(2.0f));")
            L.append("\tw->horizontal = false;")
            L.append("\tw->forceLinear = true;")
            L.append("\taddParam(w);")
            L.append("}")
        elif t == "faderH":
            L.append("{")
            L.append("\tauto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(%sf, %sf)), "
                     "module, SpectralDissectorModule::%s);" % (x, y, bind))
            L.append("\tw->box.size = mm2px(Vec(%sf, 10.0f));" % f2(w.get("w", 30)))
            L.append("\tw->box.pos = w->box.pos.minus(w->box.size.div(2.0f));")
            L.append("\tw->horizontal = true;")
            L.append("\tw->forceLinear = true;")
            L.append("\taddParam(w);")
            L.append("}")
        elif t == "light":
            band = w.get("band")
            if band is None:
                import re
                m = re.search(r"LIGHT_BAND(\d+)", bind)
                band = int(m.group(1)) - 1 if m else 0
            L.append("{")
            L.append("\tauto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(%sf, %sf)), "
                     "module, SpectralDissectorModule::%s);" % (x, y, bind))
            L.append("\tw->setColor(sdpanel::bandColor(%d));" % int(band))
            L.append("\taddChild(w);")
            L.append("}")

    # ---- 标签（nvgText 真实字体）: 最后添加 ⇒ 绘制在其他部件之上 ----
    # box.pos = bbox 左上角; box.size = 预估 bbox; draw 内基线 = size.y-2。
    # 字体最小 7px（2026-08-18: 6px 与点阵伪影均被用户反馈不清晰）。
    mm = 75.0 / 25.4
    for w in layout["widgets"]:
        if w.get("type") != "label":
            continue
        text = w.get("text", "")
        size_px = max(7.0, float(w.get("size", 8)))
        center = w.get("anchor", "start") == "middle"
        width_px = max(10.0, len(text) * size_px * 0.62 + 4.0)
        height_px = size_px + 4.0
        x0_mm = float(w["x"]) - (width_px / mm / 2.0 if center else 0.0)
        y0_mm = float(w["y"]) - (height_px - 2.0) / mm
        L.append("{")
        L.append("\tauto* lw = new sdpanel::LabelWidget;")
        L.append("\tlw->text = \"%s\";" % _cpp_str(text))
        L.append("\tlw->fontSize = %sf;" % f2(size_px))
        L.append("\tlw->center = %s;" % ("true" if center else "false"))
        if w.get("dim"):
            L.append("\tlw->dim = true;")
        L.append("\tlw->module = module;")
        L.append("\tlw->box.pos = mm2px(Vec(%sf, %sf));" % (f2(x0_mm), f2(y0_mm)))
        L.append("\tlw->box.size = mm2px(Vec(%sf, %sf));" % (f2(width_px / mm), f2(height_px / mm)))
        L.append("\taddChild(lw);")
        L.append("}")
    return "\n".join(L) + "\n"


def main():
    if len(sys.argv) >= 2 and sys.argv[1] not in ("--default", "-d"):
        path = sys.argv[1]
        with open(path, encoding="utf-8") as fh:
            layout = json.load(fh)
        print("layout: %s" % path)
    else:
        layout = default_layout()
        out_json = os.path.join(PLUGIN, "tools", "layout.default.json")
        with open(out_json, "w", encoding="utf-8") as fh:
            json.dump(layout, fh, ensure_ascii=False, indent=2)
            fh.write("\n")
        print("layout: 默认布局 -> %s" % out_json)

    os.makedirs(RES, exist_ok=True)
    with open(os.path.join(RES, "SpectralDissector.svg"), "w", encoding="utf-8") as fh:
        fh.write(svg_panel(layout, DARK) + "\n")
    with open(os.path.join(RES, "SpectralDissector_light.svg"), "w", encoding="utf-8") as fh:
        fh.write(svg_panel(layout, LIGHT) + "\n")
    with open(os.path.join(SRC, "panel_layout.inc"), "w", encoding="utf-8") as fh:
        fh.write(cpp_widgets(layout))
    print("SVG: %s + SpectralDissector_light.svg" % os.path.join(RES, "SpectralDissector.svg"))
    print("C++: %s" % os.path.join(SRC, "panel_layout.inc"))


if __name__ == "__main__":
    main()
