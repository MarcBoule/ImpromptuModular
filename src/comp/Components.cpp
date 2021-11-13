//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Components.hpp"


// Dark management
// ----------------------------------------

void saveThemeAndContrastAsDefault(int themeDefault = 0, float contrastDefault = panelContrastDefault) {
	json_t *settingsJ = json_object();
	json_object_set_new(settingsJ, "themeDefault", json_integer(themeDefault));
	json_object_set_new(settingsJ, "contrastDefault", json_real(contrastDefault));
	std::string settingsFilename = asset::user("ImpromptuModular.json");
	FILE *file = fopen(settingsFilename.c_str(), "w");
	if (file) {
		json_dumpf(settingsJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
		fclose(file);
	}
	json_decref(settingsJ);
}


void loadThemeAndContrastFromDefault(int* panelTheme, float* panelContrast) {
	*panelTheme = 0;
	*panelContrast = panelContrastDefault;
	
	std::string settingsFilename = asset::user("ImpromptuModular.json");
	FILE *file = fopen(settingsFilename.c_str(), "r");
	if (!file) {
		saveThemeAndContrastAsDefault();
		return;
	}
	json_error_t error;
	json_t *settingsJ = json_loadf(file, 0, &error);
	if (!settingsJ) {
		// invalid setting json file
		fclose(file);
		saveThemeAndContrastAsDefault();
		return;
	}
	
	json_t *themeDefaultJ = json_object_get(settingsJ, "themeDefault");
	if (themeDefaultJ) {
		*panelTheme = json_integer_value(themeDefaultJ);
	}
	
	json_t *contrastDefaultJ = json_object_get(settingsJ, "contrastDefault");
	if (contrastDefaultJ) {
		*panelContrast = json_number_value(contrastDefaultJ);
	}
	
	fclose(file);
	json_decref(settingsJ);
	return;
}



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

void DynamicSVGScrew::step() {
    int newMode = isDark(mode) ? 1 : 0;
	if (newMode != oldMode) {
        if (newMode > 0 && !frameAltName.empty()) {// JIT loading of alternate skin
			frames.push_back(APP->window->loadSvg(frameAltName));
			frameAltName.clear();// don't reload!
		}
        setSvg(frames[newMode]);
        oldMode = newMode;
    }
	SvgWidget::step();
}



// Ports
// ----------

// none


// Buttons and switches
// ----------

struct Margins {
	static constexpr float l = 1.0f;
	static constexpr float r = 1.0f;
	static constexpr float t = 1.0f;
	static constexpr float b = 1.0f;
};
Margins margins;


struct SwitchOutlineWidget : Widget {
	int** mode = NULL;
	
	SwitchOutlineWidget(int** _mode, Vec _size) {
		mode = _mode;
		box.size = _size;
	}
	void draw(const DrawArgs& args) override {
		if (mode && isDark(*mode)) {
			nvgBeginPath(args.vg);
			NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTop, colBot);	
			nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 1.5f);
			nvgFillPaint(args.vg, grad);
			nvgFill(args.vg);
		}
		Widget::draw(args);
	}
};


IMSwitch2V::IMSwitch2V() {
	shadow->hide();

	// add margins:
	fb->box.size = fb->box.size.plus(Vec(margins.l + margins.r, margins.t + margins.b));
	box.size = fb->box.size;
	sw->box.pos = sw->box.pos.plus(Vec(margins.l, margins.t));

	// add switch outline:
	SwitchOutlineWidget* sow = new SwitchOutlineWidget(&mode, box.size);// box.size already includes margins
	sow->box.pos = Vec(0, 0);
	fb->addChildBottom(sow);
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
	
	// add margins:
	fb->box.size = fb->box.size.plus(Vec(margins.l + margins.r, margins.t + margins.b));
	box.size = fb->box.size;
	tw->box.pos = tw->box.pos.plus(Vec(margins.l, margins.t));

	// add switch outline:
	SwitchOutlineWidget* sow = new SwitchOutlineWidget(&mode, box.size);// box.size already includes margins
	sow->box.pos = Vec(0, 0);
	fb->addChildBottom(sow);
}


IMSwitch3VInv::IMSwitch3VInv() {
	addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_2.svg")));
	addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_1.svg")));
	addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_0.svg")));
	
	// add margins:
	fb->box.size = fb->box.size.plus(Vec(margins.l + margins.r, margins.t + margins.b));
	box.size = fb->box.size;
	sw->box.pos = sw->box.pos.plus(Vec(margins.l, margins.t));

	// add switch outline:
	SwitchOutlineWidget* sow = new SwitchOutlineWidget(&mode, box.size);// box.size already includes margins
	sow->box.pos = Vec(0, 0);
	fb->addChildBottom(sow);
}


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
		nvgRoundedRect(args.vg, -1.0f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 1.5f);
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
	}
	SvgWidget::draw(args);
}


void KeyboardMed::draw(const DrawArgs& args) {
	// already framebuffered when added to module's main panel
	if (isDark(mode)) {
		nvgBeginPath(args.vg);
		NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTopD, colBotD);	
		nvgRoundedRect(args.vg, -1.0f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 1.5f);
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