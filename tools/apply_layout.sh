#!/bin/bash
# Spectral Dissector — VCV Rack 2 plugin
# Copyright (C) 2026 Xiehuo
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ============================================================
# apply_layout.sh — P5.4 一键收口: layout.json → 面板 → 构建 → 验证
# ------------------------------------------------------------
# 前置: 用户把 layout.html 导出的 layout.json 放在工作区根目录。
# 流程: gen_panel.py（两套 SVG + panel_layout.inc）→ 编译零警告
#       → host_tree 树检查 → p5_unit 单元检查。
# 用法: plugin/tools/apply_layout.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
WS="$(cd "$HERE/../.." && pwd)"
LAYOUT="$WS/layout.json"

if [ ! -f "$LAYOUT" ]; then
    echo "缺少 $LAYOUT —— 请先在 layout.html 设计器导出并放回工作区根目录。"
    exit 1
fi

echo "[1/4] gen_panel.py ($LAYOUT)"
python3 "$HERE/gen_panel.py" "$LAYOUT"

echo "[2/4] 编译（零警告）"
cd "$WS/plugin"
make RACK_DIR=../sdk 2>&1 | tee /tmp/apply_layout_make.log
if grep -qiE "warning|error" /tmp/apply_layout_make.log; then
    echo "构建出现 warning/error，中止"; exit 1
fi

echo "[3/4] host_tree（模块树 + 主题接线）"
./test/run_host_tree.sh

echo "[4/4] p5_unit（DSP 增量 + 分析仪管线）"
./test/run_p5_unit.sh

echo "apply_layout: 全部通过。剩余: 全量对拍回归 + Rack GUI 复核 + git 提交（P5.4）。"
