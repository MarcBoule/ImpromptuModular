//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "rack.hpp"

using namespace rack;

extern Plugin *pluginInstance;


static const float blurRadiusRatio = 0.06f;



// ******** Dynamic Widgets ********

// General Dynamic Widget creation
template <class TWidget>
TWidget* createDynamicWidget(Vec pos, int* mode) {
	TWidget *dynWidget = createWidget<TWidget>(pos);
	dynWidget->mode = mode;
	return dynWidget;
}

struct DynamicSVGScrew : SvgWidget {
    int* mode = NULL;
    int oldMode = -1;
    std::vector<std::shared_ptr<Svg>> frames;
	std::string frameAltName;

    void addFrame(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltName = filename;}
    void step() override;
};


struct IMScrew : DynamicSVGScrew {
	IMScrew() {
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/ScrewSilver.svg")));
		// addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/comp/ScrewSilver.svg")));
		addFrameAlt(asset::system("res/ComponentLibrary/ScrewBlack.svg"));
	}
};


// ******** Dynamic Ports ********

template <class TDynamicPort>
TDynamicPort* createDynamicPortCentered(Vec pos, bool isInput, Module *module, int portId, int* mode) {
	TDynamicPort *dynPort = isInput ? 
		createInput<TDynamicPort>(pos, module, portId) :
		createOutput<TDynamicPort>(pos, module, portId);
	dynPort->mode = mode;
	dynPort->box.pos = dynPort->box.pos.minus(dynPort->box.size.div(2));// centering
	return dynPort;
}

// struct DynamicSVGPort : SvgPort {
    // int* mode = NULL;
    // int oldMode = -1;
    // std::vector<std::shared_ptr<Svg>> frames;
	// std::string frameAltName;

    // void addFrame(std::shared_ptr<Svg> svg);
    // void addFrameAlt(std::string filename) {frameAltName = filename;}
    // void step() override;
// };


struct IMPort : PJ301MPort  {
	int* mode = NULL;
	IMPort() {
		// addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/PJ301M.svg")));
		// addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/comp/PJ301M.svg")));
		// addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/PJ301M.svg"));
		// shadow->blurRadius = 1.0f;
		// shadow->opacity = 0.8;
	}
};


// ******** Dynamic Params ********

template <class TDynamicParam>
TDynamicParam* createDynamicParamCentered(Vec pos, Module *module, int paramId, int* mode) {
	TDynamicParam *dynParam = createParam<TDynamicParam>(pos, module, paramId);
	dynParam->mode = mode;
	dynParam->box.pos = dynParam->box.pos.minus(dynParam->box.size.div(2));// centering
	return dynParam;
}

// struct DynamicSVGSwitch : SvgSwitch {
    // int* mode = NULL;
    // int oldMode = -1;
	// std::vector<std::shared_ptr<Svg>> framesAll;
	// std::string frameAltName0;
	// std::string frameAltName1;
	// TransformWidget *tw;
	
	// void addFrameAll(std::shared_ptr<Svg> svg);
    // void addFrameAlt0(std::string filename) {frameAltName0 = filename;}
    // void addFrameAlt1(std::string filename) {frameAltName1 = filename;}
	// void setSizeRatio(float ratio);
    // void step() override;
// };

// struct DynamicSVGKnob : SvgKnob {
    // int* mode = NULL;
    // int oldMode = -1;
	// std::vector<std::shared_ptr<Svg>> framesAll;
	// SvgWidget* effect = NULL;
	// std::string frameAltName;
	// std::string frameEffectName;

	// void addFrameAll(std::shared_ptr<Svg> svg);
    // void addFrameAlt(std::string filename) {frameAltName = filename;}	
	// void addFrameEffect(std::string filename) {frameEffectName = filename;}	
    // void step() override;
// };


struct IMBigPushButton : CKD6 {
	int* mode = NULL;
	TransformWidget *tw;
	IMBigPushButton() {
		// momentary = true;
		// addFrameAll(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKD6_0.svg")));
		// addFrameAll(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKD6_1.svg")));
		// addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/CKD6_0.svg"));
		// addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/CKD6_1.svg"));
		setSizeRatio(0.9f);		
		// shadow->blurRadius = 1.0f;
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
	IMPushButton() {
		// momentary = true;
		// addFrameAll(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_0.svg")));
		// addFrameAll(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_1.svg")));
		// addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		// addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
	}
};


// struct IMKnob : DynamicSVGKnob {
	// IMKnob() {
		// minAngle = -0.83*float(M_PI);
		// maxAngle = 0.83*float(M_PI);
	// }
// };

template<bool makeSnap>
struct IMBigKnob : Rogan1PSWhite  {
	int* mode = NULL;
	IMBigKnob() {
		// addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/comp/BlackKnobLargeWithMark.svg")));
		// addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/BlackKnobLargeWithMark.svg"));
		// addFrameEffect(asset::plugin(pluginInstance, "res/dark/comp/BlackKnobLargeEffects.svg"));
		// shadow->blurRadius = box.size.y * blurRadiusRatio;
		// shadow->opacity = 0.1;
		snap = makeSnap;
	}
};

struct IMBigKnobInf : Rogan1PSWhite {
	int* mode = NULL;
	IMBigKnobInf() {
		// addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/comp/BlackKnobLarge.svg")));
		// addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/BlackKnobLarge.svg"));
		// addFrameEffect(asset::plugin(pluginInstance, "res/dark/comp/BlackKnobLargeEffects.svg"));
		// shadow->blurRadius = box.size.y * blurRadiusRatio;
		// shadow->opacity = 0.1;
		speed = 0.9f;				
	}
};

template<bool makeSnap>
struct IMSmallKnob : RoundSmallBlackKnob {
	int* mode = NULL;
	IMSmallKnob() {
		// addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/comp/RoundSmallBlackKnob.svg")));
		// addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/RoundSmallBlackKnob.svg"));
		// addFrameEffect(asset::plugin(pluginInstance, "res/dark/comp/RoundSmallBlackKnobEffects.svg"));		
		// shadow->blurRadius = box.size.y * blurRadiusRatio;
		// shadow->opacity = 0.1;
		// shadow->box.pos = Vec(0.0, box.size.y * 0.15);
		snap = makeSnap;
	}
};

struct IMMediumKnobInf : RoundBlackKnob {
	int* mode = NULL;
	IMMediumKnobInf() {
		// addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/comp/RoundMediumBlackKnobNoMark.svg")));
		// addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/RoundMediumBlackKnobNoMark.svg"));
		// addFrameEffect(asset::plugin(pluginInstance, "res/dark/comp/RoundMediumBlackKnobEffects.svg"));
		// shadow->blurRadius = box.size.y * blurRadiusRatio;
		// shadow->opacity = 0.1;
		// shadow->box.pos = Vec(0.0, box.size.y * 0.15);
		speed = 0.9f;				
	}
};
template<bool makeSnap>
struct IMMediumKnob : RoundBlackKnob {
	int* mode = NULL;
	IMMediumKnob() {
		// addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/comp/RoundMediumBlackKnob.svg")));
		// addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/RoundMediumBlackKnob.svg"));
		// addFrameEffect(asset::plugin(pluginInstance, "res/dark/comp/RoundMediumBlackKnobEffects.svg"));
		// shadow->blurRadius = box.size.y * blurRadiusRatio;
		// shadow->opacity = 0.1;
		// shadow->box.pos = Vec(0.0, box.size.y * 0.15);
		snap = makeSnap;			
	}
};

struct IMFivePosSmallKnob : IMSmallKnob<true> {
	IMFivePosSmallKnob() {
		speed = 1.6f;
		minAngle = -0.5*float(M_PI);
		maxAngle = 0.5*float(M_PI);
	}
};

struct IMFivePosMediumKnob : IMMediumKnob<true> {
	IMFivePosMediumKnob() {
		speed = 1.6f;
		minAngle = -0.5*float(M_PI);
		maxAngle = 0.5*float(M_PI);
	}
};

struct IMSixPosBigKnob : IMBigKnob<true> {
	IMSixPosBigKnob() {
		speed = 1.3f;
		minAngle = -0.4*float(M_PI);
		maxAngle = 0.4*float(M_PI);
	}
};
