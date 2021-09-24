//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "rack.hpp"

using namespace rack;

extern Plugin *pluginInstance;


static const int colDelta = 50;
static const NVGcolor colTop = nvgRGB(128 - colDelta, 128 - colDelta, 128 - colDelta);
static const NVGcolor colBot = nvgRGB(128 + colDelta, 128 + colDelta, 128 + colDelta);

static const int colDeltaD = 30;
static const NVGcolor colTopD = nvgRGB(128 - colDeltaD, 128 - colDeltaD, 128 - colDeltaD);
static const NVGcolor colBotD = nvgRGB(128 + colDeltaD, 128 + colDeltaD, 128 + colDeltaD);


// Variations on existing knobs, lights, etc

// Screws

// none



// Ports

typedef PJ301MPort IMPort2;




// Buttons and switches

struct CKSSV : CKSS {
	void draw(const DrawArgs& args) override;
};
struct CKSSVNoRandom : CKSSV {
	void randomize() override {}
};


struct CKSSH : CKSS {
	CKSSH();
	void draw(const DrawArgs& args) override;
};
struct CKSSHNoRandom : CKSSH {
	void randomize() override {}
};


struct CKSSThreeInv : SvgSwitch {
	CKSSThreeInv() {
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_2.svg")));
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_1.svg")));
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_0.svg")));
	}
	void draw(const DrawArgs& args) override;
};
struct CKSSThreeInvNoRandom : CKSSThreeInv {
	void randomize() override {}
};


struct LEDBezelBig : SvgSwitch {
	TransformWidget *tw;
	LEDBezelBig();
};


struct KeyboardBig : SvgWidget {
	KeyboardBig(Vec(_pos)) {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/comp/KeyboardBig.svg")));
		box.pos = _pos; 
	}
	void draw(const DrawArgs& args) override;
};



// Knobs

// none



// Lights

struct OrangeLight : GrayModuleLightWidget {
	OrangeLight() {
		addBaseColor(SCHEME_ORANGE);
	}
};
struct GreenRedWhiteLight : GrayModuleLightWidget {
	GreenRedWhiteLight() {
		addBaseColor(SCHEME_GREEN);
		addBaseColor(SCHEME_RED);
		addBaseColor(SCHEME_WHITE);
	}
};


template <typename BASE>
struct GiantLight : BASE {
	GiantLight() {
		this->box.size = mm2px(Vec(19.0f, 19.0f));
	}
};
template <typename BASE>
struct GiantLight2 : BASE {
	GiantLight2() {
		this->box.size = mm2px(Vec(12.8f, 12.8f));
	}
};
