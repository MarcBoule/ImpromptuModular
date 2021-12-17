//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc BoulĂ©
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "../ImpromptuModular.hpp"

using namespace rack;

static const int NUM_AUTORETURN = 15;
static constexpr float autoreturnVoltages[NUM_AUTORETURN] = {0.0f, 1.0f, 2.0f, 2.5f, 3.0f, 3.33333f, 4.0f, 5.0f, 6.0f, 6.66667f, 7.0f, 7.5f, 8.0f, 9.0f, 10.0f};


struct TactPad : ParamWidget {
	// Note: double-click initialize makes no sense in this type of controller since clicking is not an offset for a param but a direct position action
	// Note: double-click initialize doesn't work in this setup because onDragMove() gets some calls after onDoubleClick() and since it works differently that a Knob.hpp the double click happens but gets re-written over afterwards 
	float onButtonMouseY;
	float onButtonPosY;
	static const int padWidth = 45;
	static const int padHeight = 200;// 1/12th of vertical height is used as overflow top, same for bottom
	int8_t *autoReturnSrc = NULL;
	int8_t *gateSrc = NULL;
	
	TactPad();
	void onDragMove(const event::DragMove &e) override;
	void onDragStart(const event::DragStart &e) override;
	void onDragEnd(const event::DragEnd &e) override;
	void onButton(const event::Button &e) override;
	void setTactParam(float posY);
};


struct AutoReturnItem : MenuItem {
	int8_t *autoReturnSrc;
	Param* tactParamSrc;
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string autoReturnNames[NUM_AUTORETURN + 1] = {
			"Off (default)", 
			"0 %", 
			"10 %", 
			"20 %",
			"25 %",
			"30 %",
			"33.3 %",
			"40 %",
			"50 %",
			"60 %",
			"66.7 %",
			"70 %",
			"75 %",
			"80 %",
			"90 %",
			"100 %"
		};
			
		for (int i = 0; i < (NUM_AUTORETURN + 1); i++) {
			menu->addChild(createCheckMenuItem(autoReturnNames[i], "",
				[=]() {return *autoReturnSrc == (i - 1);},
				[=]() {*autoReturnSrc = i - 1;
						if ((i - 1) >= 0) {
							tactParamSrc->setValue(autoreturnVoltages[i - 1]);
						}
					}
			));
		}
		
		return menu;
	}
};
