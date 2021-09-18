//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "DynamicComponents.hpp"



// Dynamic SVGWidget

void DynamicSVGScrew::addFrame(std::shared_ptr<Svg> svg) {
	frames.push_back(svg);
    if(frames.size() == 1) {
        sw->setSvg(svg);
	}
}

void DynamicSVGScrew::step() {
    if(mode != NULL && *mode != oldMode) {
        if (*mode > 0 && !frameAltName.empty()) {// JIT loading of alternate skin
			frames.push_back(APP->window->loadSvg(frameAltName));
			frameAltName.clear();// don't reload!
		}
        sw->setSvg(frames[*mode]);
        oldMode = *mode;
        fb->dirty = true;
    }
	SvgScrew::step();
}



// Dynamic SVGPort

void DynamicSVGPort::addFrame(std::shared_ptr<Svg> svg) {
    frames.push_back(svg);
    if(frames.size() == 1) {
        SvgPort::setSvg(svg);
	}
}

void DynamicSVGPort::step() {
    if(mode != NULL && *mode != oldMode) {
        if (*mode > 0 && !frameAltName.empty()) {// JIT loading of alternate skin
			frames.push_back(APP->window->loadSvg(frameAltName));
			frameAltName.clear();// don't reload!
		}
        sw->setSvg(frames[*mode]);
        oldMode = *mode;
        fb->dirty = true;
    }
	SvgPort::step();
}



// Dynamic SVGSwitch

void DynamicSVGSwitch::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 2) {
		addFrame(framesAll[0]);
		addFrame(framesAll[1]);
	}
}

void DynamicSVGSwitch::setSizeRatio(float ratio) {
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

void DynamicSVGSwitch::step() {
    if(mode != NULL && *mode != oldMode) {
        if (*mode > 0 && !frameAltName0.empty() && !frameAltName1.empty()) {// JIT loading of alternate skin
			framesAll.push_back(APP->window->loadSvg(frameAltName0));
			framesAll.push_back(APP->window->loadSvg(frameAltName1));
			frameAltName0.clear();// don't reload!
			frameAltName1.clear();// don't reload!
		}
		if ((*mode) == 0) {
			frames[0]=framesAll[0];
			frames[1]=framesAll[1];
		}
		else {
			frames[0]=framesAll[2];
			frames[1]=framesAll[3];
		}
        oldMode = *mode;
		onChange(*(new event::Change()));// required because of the way SVGSwitch changes images, we only change the frames above.
		fb->dirty = true;// dirty is not sufficient when changing via frames assignments above (i.e. onChange() is required)
    }
	SvgSwitch::step();
}



// Dynamic SVGKnob

void DynamicSVGKnob::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 1) {
		setSvg(svg);
	}
}

void DynamicSVGKnob::step() {
    if(mode != NULL && *mode != oldMode) {
        if (*mode > 0 && !frameAltName.empty() && !frameEffectName.empty()) {// JIT loading of alternate skin
			framesAll.push_back(APP->window->loadSvg(frameAltName));
			effect = new SvgWidget();
			effect->setSvg(APP->window->loadSvg(frameEffectName));
			effect->visible = false;
			addChild(effect);
			frameAltName.clear();// don't reload!
			frameEffectName.clear();// don't reload!
		}
        if ((*mode) == 0) {
			setSvg(framesAll[0]);
			if (effect != NULL)
				effect->visible = false;
		}
		else {
			setSvg(framesAll[1]);
			effect->visible = true;
		}
        oldMode = *mode;
		fb->dirty = true;
    }
	SvgKnob::step();
}

