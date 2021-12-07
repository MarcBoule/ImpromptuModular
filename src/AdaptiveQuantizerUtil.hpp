//***********************************************************************************************
//Adaptive reference-based quantizer module for VCV Rack by Marc Boulé and Sam Burford
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************

#pragma once

#include "ImpromptuModular.hpp"


struct WeightAndIndex {
	float w;
	int i;
};

bool weightComp(WeightAndIndex w1, WeightAndIndex w2) { 
	return (w1.w > w2.w); 
}



// ****************************************************************************
// Lights
// ****************************************************************************

static const NVGcolor AQ_RED_LOW = nvgRGB(226, 61, 60);// C (less saturation)
static const NVGcolor AQ_DARK_PURPLE = nvgRGB(133, 20, 255);// C#
static const NVGcolor AQ_YELLOW_LOW = nvgRGB(236, 217, 53);// D
static const NVGcolor AQ_BROWN_RED = nvgRGB(173, 61, 127);// D#
static const NVGcolor AQ_LIGHT_CYAN = nvgRGB(81, 199, 61);// E
static const NVGcolor AQ_DARK_RED = nvgRGB(175, 54, 43);// F
static const NVGcolor AQ_BLUE = nvgRGB(0x29, 0x82, 0xef);// F#
static const NVGcolor AQ_ORANGE = nvgRGB(0xf2, 0xb1, 0x20);// G
static const NVGcolor AQ_LIGHT_PURPLE = nvgRGB(178, 106, 252);// G#
static const NVGcolor AQ_GREEN = nvgRGB(0x90, 0xc7, 0x3e);;// A
static const NVGcolor AQ_DARK_BLUE = nvgRGB(15, 102, 203);// A#
static const NVGcolor AQ_OTHER_CYAN = nvgRGB(10, 234, 240);// B (a less agressive cyan)

NVGcolor PitchColors[12] = {AQ_RED_LOW, AQ_DARK_PURPLE, AQ_YELLOW_LOW, AQ_BROWN_RED, AQ_LIGHT_CYAN, AQ_DARK_RED, AQ_BLUE, AQ_ORANGE, AQ_LIGHT_PURPLE, AQ_GREEN, AQ_DARK_BLUE, AQ_OTHER_CYAN};


struct PitchMatrixLight : WhiteLightIM {
	bool* showDataTable = NULL;
	int* qdist;
	float* weight;
	uint64_t key;
	uint64_t y;
	uint64_t *route;
	bool* thru;
	float* datapic;
	
	void step() override {
		if (showDataTable != NULL) {
			if (*showDataTable) {
				module->lights[firstLightId].setBrightness(*datapic);
				if (*datapic > 0.5f) {
					baseColors[0] = SCHEME_GREEN;
				}
				else {
					baseColors[0] = SCHEME_WHITE;
				}
			}
			else {
				if ( ( (*route) & ( ((uint64_t)0x1) << (key * ((uint64_t)5) + y)) ) != ((uint64_t)0) ) {
					// route
					module->lights[firstLightId].setBrightness(1.0f);
					baseColors[0] = SCHEME_WHITE;
				}
				else {
					// weights
					if (*thru) {
						module->lights[firstLightId].setBrightness(0.0f);
					}
					else {
						float val = (*weight * 5.0f - (float)y);
						module->lights[firstLightId].setBrightness(val);
						baseColors[0] = PitchColors[eucMod(key + *qdist, 12)];
					}
				}
			}
		}
		WhiteLightIM::step();
	}
};


// ****************************************************************************
// Knobs
// ****************************************************************************

struct PersistenceKnob : IMBigKnob {
	static constexpr float dataShowTime = 5.0f;
	long* infoDataTablePtr = NULL;
	
	// void onButton(const event::Button& e) override {
		// *infoDataTablePtr = 1;// show alternate representation
		// IMBigKnob::onButton(e);
	// }
	void onDragMove(const event::DragMove& e) override {
		ParamQuantity* paramQuantity = getParamQuantity();
		if (paramQuantity && infoDataTablePtr) {
			*infoDataTablePtr = (long) (dataShowTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
		}
		IMBigKnob::onDragMove(e);
	}
};


struct OffsetKnob : IMMediumKnob {
	static constexpr float dataShowTime = 5.0f;
	long* infoDataTablePtr = NULL;
	
	// void onButton(const event::Button& e) override {
		// *infoDataTablePtr = 1;// show alternate representation
		// IMMediumKnob::onButton(e);
	// }
	void onDragMove(const event::DragMove& e) override {
		ParamQuantity* paramQuantity = getParamQuantity();
		if (paramQuantity && infoDataTablePtr) {
			*infoDataTablePtr = (long) (dataShowTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
		}
		IMMediumKnob::onDragMove(e);
	}
};


struct WeightingKnob : IMMediumKnob {
	// long* infoDataTablePtr = NULL;

	// void onButton(const event::Button& e) override {
		// *infoDataTablePtr = 0;// show normal representation
		// IMMediumKnob::onButton(e);
	// }
};


struct PitchesKnob : IMBigKnob {
	// long* infoDataTablePtr = NULL;

	// void onButton(const event::Button& e) override {
		// *infoDataTablePtr = 0;// show normal representation
		// IMBigKnob::onButton(e);
	// }
};





// ****************************************************************************
// Other
// ****************************************************************************

