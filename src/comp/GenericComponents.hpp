//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "rack.hpp"

using namespace rack;

extern Plugin *pluginInstance;



// Variations on existing knobs, lights, etc

// Screws

// none



// Ports

// none



// Buttons and switches

struct CKSSNoRandom : CKSS {
	void randomize() override {}
};

struct CKSSH : CKSS {
	CKSSH();
	
	// void draw(const DrawArgs& args) override;
};
struct CKSSHNoRandom : CKSSH {
	void randomize() override {}
};

struct CKSSThreeInv : app::SvgSwitch {
	CKSSThreeInv() {
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_2.svg")));
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_1.svg")));
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_0.svg")));
	}
};

struct CKSSThreeInvNoRandom : CKSSThreeInv {
	void randomize() override {}
};


struct LEDBezelBig : SvgSwitch {
	TransformWidget *tw;
	LEDBezelBig();
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
