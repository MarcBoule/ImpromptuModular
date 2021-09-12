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

