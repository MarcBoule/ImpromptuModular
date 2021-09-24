//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "GenericComponents.hpp"

int colDelta = 50;
NVGcolor colTop = nvgRGB(128 - colDelta, 128 - colDelta, 128 - colDelta);
NVGcolor colBot = nvgRGB(128 + colDelta, 128 + colDelta, 128 + colDelta);

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
	// nvgBeginPath(args.vg);
	// nvgFillColor(args.vg, SCHEME_WHITE);	
	// nvgRoundedRect(args.vg, -0.7f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 0.5f);
	// nvgFill(args.vg);
	// nvgClosePath(args.vg);	
	
	// nvgBeginPath(args.vg);
	// NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, colTop, colBot);	
	// nvgRoundedRect(args.vg, -0.6f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 1.5f);
	// nvgFillPaint(args.vg, grad);
	// nvgFill(args.vg);
	// nvgClosePath(args.vg);	
	
	// CKSS::draw(args);
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

