//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "GenericComponents.hpp"



// Screws

// nothing



// Ports

// nothing



// Buttons and switches


CKSSH::CKSSH() {
	shadow->opacity = 0.0;
	fb->removeChild(sw);
	
	TransformWidget *tw = new TransformWidget();
	tw->addChild(sw);
	fb->addChild(tw);

	Vec center = sw->box.getCenter();
	tw->translate(center);
	tw->rotate(float(M_PI_2));
	tw->translate(Vec(center.y, sw->box.size.x).neg());
	
	tw->box.size = sw->box.size.flip();
	box.size = tw->box.size;
}

// void CKSSH::draw(const DrawArgs& args) {
	// nvgSave(args.vg);

	// NVGstate* state = nvg__getState(args);
	// state->tint.a *= 1.0f;
	
	// nvgBeginPath(args.vg);
	// nvgFillColor(args.vg, SCHEME_WHITE);	
	// nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
	// nvgGlobalCompositeOperation(args.vg, NVG_XOR);
	// nvgFill(args.vg);
	// nvgClosePath(args.vg);
	
	// nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_SRC_COLOR, NVG_DST_COLOR );// src, dest
	// nvgGlobalCompositeBlendFuncSeparate(args.vg, NVG_SRC_COLOR, NVG_DST_COLOR, NVG_SRC_ALPHA, NVG_DST_ALPHA);
	
	// nvgBeginPath(args.vg);
	// nvgFillColor(args.vg, SCHEME_RED);	
	// nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
	// nvgFill(args.vg);
	// nvgClosePath(args.vg);
	
	// CKSS::draw(args);
	
	// nvgRestore(args.vg);
// }


LEDBezelBig::LEDBezelBig() {
	momentary = true;
	float ratio = 2.13f;
	addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/LEDBezel.svg")));
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



// Knobs

// nothing



// Lights

// nothing

