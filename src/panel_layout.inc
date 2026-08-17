// ============================================================
// panel_layout.inc — 由 plugin/tools/gen_panel.py 生成（勿手改）
// 来源: layout JSON（layout.html 设计器导出）; 坐标 mm2px(75dpi)。
// 部件: sdpanel 主题感知自绘控件（panel.hpp）+ SpectrumAnalyzerWidget。
// 标签: sdpanel::LabelWidget（nvgText 真实字体, 2026-08-18 起;
//        SVG 不再生成点阵 path）。
// ============================================================
{
	auto* f = new sdpanel::FrameWidget;
	f->module = module;
	f->box.pos = mm2px(Vec(270.0f, 3.4f));
	f->box.size = mm2px(Vec(36.0f, 52.0f));
	addChild(f);
}
{
	auto* f = new sdpanel::FrameWidget;
	f->module = module;
	f->box.pos = mm2px(Vec(234.0f, 3.4f));
	f->box.size = mm2px(Vec(36.0f, 52.0f));
	addChild(f);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(10.5f, 118.5f)), module, SpectralDissectorModule::INPUT_AUDIO));
// analyzer (140.0 × 99.0)mm
{
	auto* w = new SpectrumAnalyzerWidget;
	w->module = module;
	w->box.pos = mm2px(Vec(4.0f, 12.0f));
	w->box.size = mm2px(Vec(140.0f, 99.0f));
	addChild(w);
}
addParam(createParamCentered<sdpanel::ThemedKnob>(mm2px(Vec(225.0f, 16.0f)), module, SpectralDissectorModule::PARAM_SPACING));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(225.0f, 32.0f)), module, SpectralDissectorModule::PARAM_CVATT_SPACING);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(225.0f, 48.0f)), module, SpectralDissectorModule::INPUT_CV_SPACING));
addParam(createParamCentered<sdpanel::ThemedKnob>(mm2px(Vec(279.0f, 16.0f)), module, SpectralDissectorModule::PARAM_GATE));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(279.0f, 32.0f)), module, SpectralDissectorModule::PARAM_CVATT_GATE);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(279.0f, 48.0f)), module, SpectralDissectorModule::INPUT_CV_GATE));
addParam(createParamCentered<sdpanel::ThemedKnob>(mm2px(Vec(243.0f, 16.0f)), module, SpectralDissectorModule::PARAM_BLUR));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(243.0f, 32.0f)), module, SpectralDissectorModule::PARAM_CVATT_BLUR);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(243.0f, 48.0f)), module, SpectralDissectorModule::INPUT_CV_BLUR));
addParam(createParamCentered<sdpanel::ThemedKnob>(mm2px(Vec(261.0f, 16.0f)), module, SpectralDissectorModule::PARAM_PERC));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(261.0f, 32.0f)), module, SpectralDissectorModule::PARAM_CVATT_PERC);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(261.0f, 48.0f)), module, SpectralDissectorModule::INPUT_CV_PERC));
addParam(createParamCentered<sdpanel::ThemedKnob>(mm2px(Vec(297.0f, 16.0f)), module, SpectralDissectorModule::PARAM_DETAIL));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(297.0f, 32.0f)), module, SpectralDissectorModule::PARAM_CVATT_DETAIL);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(297.0f, 48.0f)), module, SpectralDissectorModule::INPUT_CV_DETAIL));
addParam(createParamCentered<sdpanel::ThemedKnob>(mm2px(Vec(315.0f, 16.0f)), module, SpectralDissectorModule::PARAM_RISE_MS));
addParam(createParamCentered<sdpanel::ThemedKnob>(mm2px(Vec(315.0f, 32.0f)), module, SpectralDissectorModule::PARAM_FALL_MS));
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(153.0f, 31.0f)), module, SpectralDissectorModule::PARAM_THRESHOLD);
	w->box.size = mm2px(Vec(10.0f, 46.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(163.5f, 32.0f)), module, SpectralDissectorModule::PARAM_CVATT_THRESHOLD);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(163.5f, 48.0f)), module, SpectralDissectorModule::INPUT_CV_THRESHOLD));
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(174.0f, 31.0f)), module, SpectralDissectorModule::PARAM_FOCUS);
	w->box.size = mm2px(Vec(10.0f, 46.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(184.5f, 32.0f)), module, SpectralDissectorModule::PARAM_CVATT_FOCUS);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(184.5f, 48.0f)), module, SpectralDissectorModule::INPUT_CV_FOCUS));
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(195.0f, 31.0f)), module, SpectralDissectorModule::PARAM_TILT);
	w->box.size = mm2px(Vec(10.0f, 46.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(205.5f, 32.0f)), module, SpectralDissectorModule::PARAM_CVATT_TILT);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
addInput(createInputCentered<sdpanel::ThemedPort>(mm2px(Vec(205.5f, 48.0f)), module, SpectralDissectorModule::INPUT_CV_TILT));
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(23.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_MIX));
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(35.5f, 118.5f)), module, SpectralDissectorModule::OUTPUT_DRY));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(153.0f, 64.0f)), module, SpectralDissectorModule::PARAM_OFF1);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(153.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN1);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(158.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND1);
	w->setColor(sdpanel::bandColor(0));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(153.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B1));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(171.0f, 64.0f)), module, SpectralDissectorModule::PARAM_OFF2);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(171.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN2);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(176.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND2);
	w->setColor(sdpanel::bandColor(1));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(171.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B2));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(189.0f, 64.0f)), module, SpectralDissectorModule::PARAM_OFF3);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(189.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN3);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(194.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND3);
	w->setColor(sdpanel::bandColor(2));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(189.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B3));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(207.0f, 64.0f)), module, SpectralDissectorModule::PARAM_OFF4);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(207.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN4);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(212.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND4);
	w->setColor(sdpanel::bandColor(3));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(207.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B4));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(225.0f, 64.0f)), module, SpectralDissectorModule::PARAM_OFF5);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(225.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN5);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(230.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND5);
	w->setColor(sdpanel::bandColor(4));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(225.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B5));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(243.0f, 64.0f)), module, SpectralDissectorModule::PARAM_OFF6);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(243.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN6);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(248.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND6);
	w->setColor(sdpanel::bandColor(5));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(243.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B6));
{
	auto* w = createParam<sdpanel::ThemedKnob>(mm2px(Vec(261.0f, 64.0f)), module, SpectralDissectorModule::PARAM_OFF7);
	w->box.size = mm2px(Vec(7.5f, 7.5f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	addParam(w);
}
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(261.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN7);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(266.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND7);
	w->setColor(sdpanel::bandColor(6));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(261.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B7));
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(279.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN8);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(284.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND8);
	w->setColor(sdpanel::bandColor(7));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(279.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B8));
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(297.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN9);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(302.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND9);
	w->setColor(sdpanel::bandColor(8));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(297.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B9));
{
	auto* w = createParam<sdpanel::ThemedFader>(mm2px(Vec(315.0f, 89.0f)), module, SpectralDissectorModule::PARAM_GAIN10);
	w->box.size = mm2px(Vec(10.0f, 40.0f));
	w->box.pos = w->box.pos.minus(w->box.size.div(2.0f));
	w->horizontal = false;
	w->forceLinear = true;
	addParam(w);
}
{
	auto* w = createLightCentered<sdpanel::ThemedLight>(mm2px(Vec(320.5f, 112.0f)), module, SpectralDissectorModule::LIGHT_BAND10);
	w->setColor(sdpanel::bandColor(9));
	addChild(w);
}
addOutput(createOutputCentered<sdpanel::ThemedPort>(mm2px(Vec(315.0f, 118.5f)), module, SpectralDissectorModule::OUTPUT_B10));
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "SPECTRAL DISSECTOR";
	lw->fontSize = 16.5f;
	lw->center = false;
	lw->module = module;
	lw->box.pos = mm2px(Vec(5.0f, 2.435f));
	lw->box.size = mm2px(Vec(63.717f, 6.943f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "泻火";
	lw->fontSize = 12.0f;
	lw->center = false;
	lw->dim = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(59.6f, 3.959f));
	lw->box.size = mm2px(Vec(6.394f, 5.419f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Xiehuo";
	lw->fontSize = 11.0f;
	lw->center = false;
	lw->dim = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(65.5f, 4.397f));
	lw->box.size = mm2px(Vec(15.213f, 5.08f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "SPACING";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(219.178f, 3.952f));
	lw->box.size = mm2px(Vec(11.643f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "GATE";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(275.383f, 3.952f));
	lw->box.size = mm2px(Vec(7.234f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "BLUR";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(239.383f, 3.952f));
	lw->box.size = mm2px(Vec(7.234f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "PERC";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(257.383f, 3.952f));
	lw->box.size = mm2px(Vec(7.234f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "DETAIL";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(291.913f, 3.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "RISE";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(311.383f, 3.952f));
	lw->box.size = mm2px(Vec(7.234f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "FALL";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(311.383f, 37.452f));
	lw->box.size = mm2px(Vec(7.234f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "THRESH";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(147.913f, 3.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "FOCUS";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(169.648f, 3.952f));
	lw->box.size = mm2px(Vec(8.704f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "TILT";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(191.383f, 3.952f));
	lw->box.size = mm2px(Vec(7.234f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "IN";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(8.353f, 123.952f));
	lw->box.size = mm2px(Vec(4.294f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "MIX";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(20.118f, 123.952f));
	lw->box.size = mm2px(Vec(5.764f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "DRY";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(32.618f, 123.952f));
	lw->box.size = mm2px(Vec(5.764f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Band 1";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(147.913f, 123.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Band 2";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(165.913f, 123.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Band 3";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(183.913f, 123.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Band 4";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(201.913f, 123.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Band 5";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(219.913f, 123.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Band 6";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(237.913f, 123.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Band 7";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(255.913f, 123.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Band 8";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(273.913f, 123.952f));
	lw->box.size = mm2px(Vec(10.174f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Noise";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(292.648f, 123.952f));
	lw->box.size = mm2px(Vec(8.704f, 3.725f));
	addChild(lw);
}
{
	auto* lw = new sdpanel::LabelWidget;
	lw->text = "Perc";
	lw->fontSize = 7.0f;
	lw->center = true;
	lw->module = module;
	lw->box.pos = mm2px(Vec(311.383f, 123.952f));
	lw->box.size = mm2px(Vec(7.234f, 3.725f));
	addChild(lw);
}
