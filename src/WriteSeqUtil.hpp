//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#pragma once

#include "ImpromptuModular.hpp"


inline float quantize(float cv, bool enable) {
	return enable ? (std::round(cv * 12.0f) / 12.0f) : cv;
};


struct ArrowModeItem : MenuItem {
	int* stepRotatesSrc;

	struct ArrowModeSubItem : MenuItem {
		int* stepRotatesSrc;
		void onAction(const event::Action &e) override {
			*stepRotatesSrc ^= 0x1;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		ArrowModeSubItem *step0Item = createMenuItem<ArrowModeSubItem>("Step", CHECKMARK(*stepRotatesSrc == 0));
		step0Item->stepRotatesSrc = stepRotatesSrc;
		menu->addChild(step0Item);

		ArrowModeSubItem *step1Item = createMenuItem<ArrowModeSubItem>("Rotate", CHECKMARK(*stepRotatesSrc != 0));
		step1Item->stepRotatesSrc = stepRotatesSrc;
		menu->addChild(step1Item);

		return menu;
	}
};	


inline void rotateSeq(int delta, int numSteps, float *cv, int *gates) {
	float rotCV;
	int rotGate;
	int iStart = 0;
	int iEnd = numSteps - 1;
	int iRot = iStart;
	int iDelta = 1;
	if (delta == 1) {
		iRot = iEnd;
		iDelta = -1;
	}
	rotCV = cv[iRot];
	rotGate = gates[iRot];
	for ( ; ; iRot += iDelta) {
		if (iDelta == 1 && iRot >= iEnd) break;
		if (iDelta == -1 && iRot <= iStart) break;
		cv[iRot] = cv[iRot + iDelta];
		gates[iRot] = gates[iRot + iDelta];
	}
	cv[iRot] = rotCV;
	gates[iRot] = rotGate;
};

