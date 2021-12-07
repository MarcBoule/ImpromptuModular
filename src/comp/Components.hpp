//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "rack.hpp"
#include "PanelTheme.hpp"

using namespace rack;

extern Plugin *pluginInstance;


static const int colDelta = 50;
static const NVGcolor colTop = nvgRGB(128 - colDelta, 128 - colDelta, 128 - colDelta);
static const NVGcolor colBot = nvgRGB(128 + colDelta, 128 + colDelta, 128 + colDelta);

static const int colDeltaD = 30;
static const NVGcolor colTopD = nvgRGB(128 - colDeltaD, 128 - colDeltaD, 128 - colDeltaD);
static const NVGcolor colBotD = nvgRGB(128 + colDeltaD, 128 + colDeltaD, 128 + colDeltaD);



// Lights
// ----------

struct OrangeLightIM : GrayModuleLightWidget {
	OrangeLightIM() {
		addBaseColor(SCHEME_ORANGE);
	}
};

struct WhiteLightIM : GrayModuleLightWidget {
	WhiteLightIM() {
		addBaseColor(SCHEME_WHITE);
	}
};

struct GreenLightIM : GrayModuleLightWidget {
	GreenLightIM() {
		addBaseColor(SCHEME_GREEN_IM);
	}
};

struct RedLightIM : GrayModuleLightWidget {
	RedLightIM() {
		addBaseColor(SCHEME_RED_IM);
	}
};

struct GreenRedLightIM : GrayModuleLightWidget {
	GreenRedLightIM() {
		addBaseColor(SCHEME_GREEN_IM);
		addBaseColor(SCHEME_RED_IM);
	}
};

struct WhiteBlueLight : GrayModuleLightWidget {
	WhiteBlueLight() {
		addBaseColor(SCHEME_WHITE);
		addBaseColor(SCHEME_BLUE);
	}
};

struct YellowGreenLightIM : GrayModuleLightWidget {
	YellowGreenLightIM() {
		addBaseColor(SCHEME_YELLOW);
		addBaseColor(SCHEME_GREEN_IM);
	}
};

struct GreenRedWhiteLightIM : GrayModuleLightWidget {
	GreenRedWhiteLightIM() {
		addBaseColor(SCHEME_GREEN_IM);
		addBaseColor(SCHEME_RED_IM);
		addBaseColor(SCHEME_WHITE);
	}
};



// Helpers
// ----------------------------------------

// Dynamic widgets
template <class TDynamicScrew>
TDynamicScrew* createDynamicScrew(Vec pos, int* mode) {
	TDynamicScrew *dynScrew = createWidget<TDynamicScrew>(pos);
	dynScrew->mode = mode;
	dynScrew->refreshForTheme();// all TDynamicScrew must have this
	return dynScrew;
}


// Dynamic ports
template <class TDynamicPort>
TDynamicPort* createDynamicPortCentered(Vec pos, bool isInput, Module *module, int portId, int* mode) {
	TDynamicPort *dynPort = isInput ? 
		createInput<TDynamicPort>(pos, module, portId) :
		createOutput<TDynamicPort>(pos, module, portId);
	dynPort->mode = mode;
	dynPort->box.pos = dynPort->box.pos.minus(dynPort->box.size.div(2));// centering
	return dynPort;
}


// Dynamic params
template <class TDynamicParam>
TDynamicParam* createDynamicParamCentered(Vec pos, Module *module, int paramId, int* mode) {
	TDynamicParam *dynParam = createParam<TDynamicParam>(pos, module, paramId);
	dynParam->mode = mode;
	dynParam->box.pos = dynParam->box.pos.minus(dynParam->box.size.div(2));// centering
	return dynParam;
}



// Variations on existing knobs, lights, etc
// ----------------------------------------

// Screws
// ----------

struct DynamicSVGScrew : SvgWidget {
    int* mode = NULL;
    int oldMode = -1;
    std::vector<std::shared_ptr<Svg>> frames;
	std::string frameAltName;

    void addFrame(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltName = filename;}
	void refreshForTheme();
    void step() override;
};


struct IMScrew : DynamicSVGScrew {
	IMScrew() {
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/ScrewSilver.svg")));
		addFrameAlt(asset::system("res/ComponentLibrary/ScrewBlack.svg"));
	}
};



// Ports
// ----------

struct IMPort : PJ301MPort  {
	int* mode = NULL;
	IMPort() {
	}
};



// Buttons and switches
// ----------

struct LEDLightBezelBig : LEDLightBezel<RedLightIM> {
	TransformWidget *tw;
	LEDLightBezelBig();
};


struct IMBigPushButton : CKD6 {
	int* mode = NULL;
	TransformWidget *tw;
	IMBigPushButton() {
		setSizeRatio(0.9f);		
	}
	void setSizeRatio(float ratio) {
		sw->box.size = sw->box.size.mult(ratio);
		fb->removeChild(sw);
		tw = new TransformWidget();
		tw->addChild(sw);
		tw->scale(Vec(ratio, ratio));
		tw->box.size = sw->box.size; 
		fb->addChild(tw);
		box.size = sw->box.size; 
		shadow->box.size = sw->box.size; 
	}
};


struct IMPushButton : TL1105 {
	int* mode = NULL;
	IMPushButton() {}
};


struct SwitchOutlineWidget : Widget {
	int* mode = NULL;
	
	SwitchOutlineWidget(int* _mode, Vec _size, Vec _pos) {
		mode = _mode;
		box.size = _size;
		box.pos = _pos;
	}
	void draw(const DrawArgs& args) override;
};

// Dynamic switch (has framebuffered outline in dark mode)
template <class TDynamicParam>
TDynamicParam* createDynamicSwitchCentered(Vec pos, Module *module, int paramId, int* mode, SvgPanel* panel) {
	TDynamicParam *dynParam = createParam<TDynamicParam>(pos, module, paramId);
	dynParam->mode = mode;
	dynParam->box.pos = dynParam->box.pos.minus(dynParam->box.size.div(2));// centering
	SwitchOutlineWidget* sow = new SwitchOutlineWidget(mode, dynParam->box.size, dynParam->box.pos);
	panel->fb->addChild(sow);
	return dynParam;
}

struct IMSwitch2V : CKSS {
	int* mode = NULL;
	IMSwitch2V() {
		shadow->hide();
	}
};


struct IMSwitch2H : CKSS {
	int* mode = NULL;
	IMSwitch2H();// has a rotation transform
};


struct IMSwitch3VInv : SvgSwitch {
	int* mode = NULL;
	IMSwitch3VInv() {
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_2.svg")));
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_1.svg")));
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_0.svg")));		
	}
};




// Knobs
// ----------

struct Rogan1PSWhiteIM : Rogan {
	Rogan1PSWhiteIM() {
		setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PSWhite.svg")));
		bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PS_bg.svg")));
		// fg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PSWhite_fg.svg")));
		fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/Rogan1PSWhite_fg.svg")));
	}
};


struct IMBigKnob : Rogan1PSWhiteIM  {
	int* mode = NULL;
	IMBigKnob() {}
};


struct Rogan1SWhite : Rogan {
	Rogan1SWhite() {
		// setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PSWhite.svg")));
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/Rogan1S.svg")));
		bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PS_bg.svg")));
		// fg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PSWhite_fg.svg")));
		fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/Rogan1PSWhite_fg.svg")));
	}
};


struct IMBigKnobInf : Rogan1SWhite {
	int* mode = NULL;
	IMBigKnobInf() {
		speed = 0.9f;				
	}
};


struct TrimpotSmall : app::SvgKnob {
	widget::SvgWidget* bg;

	TrimpotSmall() {
		minAngle = -0.83 * M_PI;
		maxAngle = 0.83 * M_PI;

		bg = new widget::SvgWidget;
		fb->addChildBelow(bg, tw);

		setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/Trimpot.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/Trimpot_bg.svg")));
	}
};


struct IMSmallKnob : TrimpotSmall {
	int* mode = NULL;
	IMSmallKnob() {}
};


struct Rogan1White : Rogan {
	Rogan1White() {
		// setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PSWhite.svg")));
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/Rogan1.svg")));
		bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
		// fg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PSWhite_fg.svg")));
		fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/Rogan1PWhite_fg.svg")));
	}
};


struct IMMediumKnobInf : Rogan1White {
	int* mode = NULL;
	IMMediumKnobInf() {
		speed = 0.9f;				
	}
};


struct Rogan1PWhiteIM : Rogan {
	Rogan1PWhiteIM() {
		setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1PWhite.svg")));
		bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
		fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/Rogan1PWhite_fg.svg")));
	}
};


struct IMMediumKnob : Rogan1PWhiteIM {
	int* mode = NULL;
	IMMediumKnob() {}
};


struct IMFivePosSmallKnob : IMSmallKnob {
	IMFivePosSmallKnob() {
		speed = 1.6f;
		minAngle = -0.5 * float(M_PI);
		maxAngle = 0.5 * float(M_PI);
	}
};


struct IMFivePosMediumKnob : IMMediumKnob {
	IMFivePosMediumKnob() {
		speed = 1.6f;
		minAngle = -0.5 * float(M_PI);
		maxAngle = 0.5 * float(M_PI);
	}
};


struct IMSixPosBigKnob : IMBigKnob {
	IMSixPosBigKnob() {
		speed = 1.3f;
		minAngle = -0.4 * float(M_PI);
		maxAngle = 0.4 * float(M_PI);
	}
};



// Svg Widgets
// ----------

struct DisplayBackground : TransparentWidget {
	int* mode = NULL;
	DisplayBackground(Vec(_pos), Vec(_size), int* _mode) {
		box.size = _size;
		box.pos = _pos; 
		mode = _mode;
	}
	void draw(const DrawArgs& args) override;
};


static const float bigKeyBlkdy = 1.79f;
static const float bigKeyWhtdy = 27.09f;
static const Vec bigKeysPos[12] = {	Vec(1.78f,  bigKeyWhtdy), Vec(8.806f, bigKeyBlkdy),
									Vec(15.79f, bigKeyWhtdy), Vec(22.69f, bigKeyBlkdy),
									Vec(29.67f, bigKeyWhtdy), 
									Vec(43.69f, bigKeyWhtdy), Vec(50.65f, bigKeyBlkdy),
									Vec(57.57f, bigKeyWhtdy), Vec(64.54f, bigKeyBlkdy),
									Vec(71.46f, bigKeyWhtdy), Vec(78.42f, bigKeyBlkdy),
									Vec(85.34f, bigKeyWhtdy)};// Top-left PianoKeyBig locations within KeyboardBig
struct KeyboardBig : SvgWidget {
	int* mode = NULL;
	KeyboardBig(Vec(_pos), int* _mode) {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/KeyboardBig.svg")));
		box.pos = _pos; 
		mode = _mode;
	}
	void draw(const DrawArgs& args) override;
};


static const float smaKeyBlkdy = 1.35f;
static const float smaKeyWhtdy = 14.37f;
static const Vec smaKeysPos[12] = {	Vec(1.30f,  smaKeyWhtdy), Vec(6.08f, smaKeyBlkdy),
									Vec(10.85f, smaKeyWhtdy), Vec(15.58f, smaKeyBlkdy),
									Vec(20.34f, smaKeyWhtdy), 
									Vec(30.01f, smaKeyWhtdy), Vec(34.78f, smaKeyBlkdy),
									Vec(39.49f, smaKeyWhtdy), Vec(44.25f, smaKeyBlkdy),
									Vec(48.97f, smaKeyWhtdy), Vec(53.78f, smaKeyBlkdy),
									Vec(58.50f, smaKeyWhtdy)};// Top-left PianoKeySmall locations within KeyboardSmall

struct KeyboardSmall : SvgWidget {
	int* mode = NULL;
	KeyboardSmall(Vec(_pos), int* _mode) {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/KeyboardSmall.svg")));
		box.pos = _pos; 
		mode = _mode;
	}
	void draw(const DrawArgs& args) override;
};


struct TactPadSvg : SvgWidget {
	int* mode = NULL;
	TactPadSvg(Vec(_pos), int* _mode) {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/TactPad.svg")));
		box.pos = _pos; 
		mode = _mode;
	}
	void draw(const DrawArgs& args) override;
};


struct CvPadSvg : SvgWidget {
	int* mode = NULL;
	CvPadSvg(Vec(_pos), int* _mode) {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/CvPad.svg")));
		box.pos = _pos; 
		mode = _mode;
	}
	void draw(const DrawArgs& args) override;
};

struct AqLedBg : SvgWidget {
	int* mode = NULL;
	AqLedBg(Vec(_pos), int* _mode) {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/AqLedBg.svg")));
		box.pos = _pos; 
		mode = _mode;
	}
};


