//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "rack.hpp"

using namespace rack;

extern Plugin *pluginInstance;


// Component offset constants

static const int hOffsetCKSS = 5;
static const int vOffsetCKSS = 2;
static const int vOffsetCKSSThree = -2;
static const int hOffsetCKSSH = 2;
static const int vOffsetCKSSH = 5;
static const int offsetCKD6b = 0;//does both h and v
static const int vOffsetDisplay = -2;
static const int offsetIMBigKnob = -6;//does both h and v
static const int offsetIMSmallKnob = 0;//does both h and v
static const int offsetMediumLight = 9;
static const float offsetLEDbutton = 3.0f;//does both h and v
static const float offsetLEDbuttonLight = 4.4f;//does both h and v
static const int offsetTL1105 = 4;//does both h and v
static const int offsetLEDbezel = 1;//does both h and v
static const float offsetLEDbezelLight = 2.2f;//does both h and v
static const float offsetLEDbezelBig = -11;//does both h and v



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
struct MuteLight : BASE {
	MuteLight() {
		this->box.size = mm2px(Vec(6.0f, 6.0f));
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
