//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Components.hpp"


// Helpers
// ----------------------------------------

// none


// Variations on existing knobs, lights, etc
// ----------------------------------------

// Screws
// ----------

void DynamicSVGScrew::addFrame(std::shared_ptr<Svg> svg) {
	frames.push_back(svg);
    if(frames.size() == 1) {
        setSvg(svg);
	}
}

void DynamicSVGScrew::refreshForTheme() {
	int newMode = isDark(mode) ? 1 : 0;
	if (newMode != oldMode) {
        if (newMode > 0 && !frameAltName.empty()) {// JIT loading of alternate skin
			frames.push_back(APP->window->loadSvg(frameAltName));
			frameAltName.clear();// don't reload!
		}
        setSvg(frames[newMode]);
        oldMode = newMode;
    }
}

void DynamicSVGScrew::step() {
	refreshForTheme();
	SvgWidget::step();
}



// Ports
// ----------

// none


// Buttons and switches
// ----------

LEDLightBezelBig::LEDLightBezelBig() {
	float ratio = 2.13f;
	sw->box.size = sw->box.size.mult(ratio);
	fb->removeChild(sw);
	tw = new TransformWidget();
	tw->addChild(sw);
	tw->scale(Vec(ratio, ratio));
	tw->box.size = sw->box.size; 
	fb->addChild(tw);
	box.size = sw->box.size; 
	light->box.size *= ratio; 
	light->box.pos *= ratio;
	shadow->box.size = sw->box.size; 
}


void SwitchOutlineWidget::draw(const DrawArgs& args) {
	if (isDark(mode)) {
		nvgBeginPath(args.vg);
		NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTop, colBot);	
		nvgRoundedRect(args.vg, 0 - 1, 0 - 1, box.size.x + 2, box.size.y + 2, 1.5f);
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
	}
	Widget::draw(args);
}


IMSwitch2H::IMSwitch2H() {
	shadow->hide();
	
	TransformWidget *tw = new TransformWidget();
	tw->box.size = sw->box.size;
	fb->removeChild(sw);
	tw->addChild(sw);
	fb->addChild(tw);

	tw->rotate(float(M_PI_2));
	tw->translate(Vec(0, -sw->box.size.y));
	
	sw->box.size = sw->box.size.flip();
	tw->box.size = sw->box.size;
	fb->box.size = sw->box.size;
	box.size = sw->box.size;
}



// Knobs
// ----------

// none



// Lights
// ----------

// none



// Svg Widgets
// ----------

void DisplayBackground::draw(const DrawArgs& args) {
	if (isDark(mode)) {
		nvgBeginPath(args.vg);
		NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTopD, colBotD);	
		nvgRoundedRect(args.vg, -1.5f, -1.5f, box.size.x + 3.0f, box.size.y + 3.0f, 5.0f);
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
	}

	NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38); 
	NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
	nvgBeginPath(args.vg);
	nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0f);
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1.0);
	nvgStrokeColor(args.vg, borderColor);
	nvgStroke(args.vg);
}


void KeyboardBig::draw(const DrawArgs& args) {
	// already framebuffered when added to module's main panel
	if (isDark(mode)) {
		nvgBeginPath(args.vg);
		NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTopD, colBotD);	
		nvgRoundedRect(args.vg, -1.0f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 3.0f);
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
	}
	SvgWidget::draw(args);
}


void KeyboardSmall::draw(const DrawArgs& args) {
	// already framebuffered when added to module's main panel
	if (isDark(mode)) {
		nvgBeginPath(args.vg);
		NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTopD, colBotD);	
		nvgRoundedRect(args.vg, -1.0f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 2.3f);
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
	}
	SvgWidget::draw(args);
}


void TactPadSvg::draw(const DrawArgs& args) {
	// already framebuffered when added to module's main panel
	if (isDark(mode)) {
		nvgBeginPath(args.vg);
		NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTopD, colBotD);	
		nvgRoundedRect(args.vg, -1.0f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 5.0f);
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
	}
	SvgWidget::draw(args);
}


void CvPadSvg::draw(const DrawArgs& args) {
	// already framebuffered when added to module's main panel
	if (isDark(mode)) {
		nvgBeginPath(args.vg);
		NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTopD, colBotD);	
		nvgRoundedRect(args.vg, -1.0f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 5.0f);
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
	}
	SvgWidget::draw(args);
}