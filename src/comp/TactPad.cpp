//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc BoulĂ©
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#include "TactPad.hpp"


TactPad::TactPad() {
	box.size = Vec(padWidth, padHeight);
}


void TactPad::onDragMove(const event::DragMove &e) {
	if (paramQuantity && e.button == GLFW_MOUSE_BUTTON_LEFT) {
		float dragMouseY = APP->scene->rack->mousePos.y;
		setTactParam(onButtonPosY + dragMouseY - onButtonMouseY);
	}
	ParamWidget::onDragMove(e);
}


void TactPad::onButton(const event::Button &e) {
	if (paramQuantity) {
		onButtonMouseY = APP->scene->rack->mousePos.y;
		onButtonPosY = e.pos.y;
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			setTactParam(onButtonPosY);
		}
	}
	ParamWidget::onButton(e);
}


void TactPad::setTactParam(float posY) { // posY in pixel space
	float oneTwelvethBoxY = box.size.y / 12.0f;
	float val = paramQuantity->getMinValue();
	if (posY <= oneTwelvethBoxY) { // overflow area top
		val = paramQuantity->getMaxValue();
	}
	else {
		float posYAdjusted = (posY - oneTwelvethBoxY);
		float tenTwelveths = oneTwelvethBoxY * 10.0f;
		if (posYAdjusted <= tenTwelveths) { // normal range
			val = rescale(posYAdjusted, tenTwelveths, 0.0f, paramQuantity->getMinValue(), paramQuantity->getMaxValue());
			val = clamp(val, paramQuantity->getMinValue(), paramQuantity->getMaxValue());
		}
	}
	// else overflow area bottom, nothing to since val already at minValue
	paramQuantity->setValue(val);	
}


void TactPad::reset() {
	if (paramQuantity) {
		paramQuantity->reset();
	}
}


void TactPad::randomize() {
	if (paramQuantity) {
		float value = math::rescale(random::uniform(), 0.f, 1.f, paramQuantity->getMinValue(), paramQuantity->getMaxValue());
		paramQuantity->setValue(value);
	}
}
