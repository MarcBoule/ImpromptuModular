//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc BoulĂ©
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "../ImpromptuModular.hpp"

using namespace rack;


struct TactPad : ParamWidget {
	// Note: double-click initialize makes no sense in this type of controller since clicking is not an offset for a param but a direct position action
	// Note: double-click initialize doesn't work in this setup because onDragMove() gets some calls after onDoubleClick() and since it works differently that a Knob.hpp the double click happens but gets re-written over afterwards 
	float onButtonMouseY;
	float onButtonPosY;
	static const int padWidth = 45;
	static const int padHeight = 200;// 1/12th of vertical height is used as overflow top, same for bottom
	int8_t *momentarySrc = NULL;// a.k.a. autoreturn
	float momentaryStartValue;
	
	TactPad();
	void onDragMove(const event::DragMove &e) override;
	void onDragStart(const event::DragStart &e) override;
	void onDragEnd(const event::DragEnd &e) override;
	void onButton(const event::Button &e) override;
	void setTactParam(float posY);
	void reset() override;
	void randomize() override;
};
