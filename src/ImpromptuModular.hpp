//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************

#ifndef IMPROMPU_MODULAR_HPP
#define IMPROMPU_MODULAR_HPP


#include "rack.hpp"
#include "comp/DynamicComponents.hpp"
#include "comp/GenericComponents.hpp"

using namespace rack;


extern Plugin *pluginInstance;


// All modules that are part of pluginInstance go here
extern Model *modelTact;
extern Model *modelTact1;
extern Model *modelCvPad;
extern Model *modelTwelveKey;
extern Model *modelChordKey;
extern Model *modelChordKeyExpander;
extern Model *modelClocked;
extern Model *modelClockedExpander;
extern Model *modelClkd;
extern Model *modelFoundry;
extern Model *modelFoundryExpander;
extern Model *modelGateSeq64;
extern Model *modelGateSeq64Expander;
extern Model *modelPhraseSeq16;
extern Model *modelPhraseSeq32;
extern Model *modelPhraseSeqExpander;
extern Model *modelWriteSeq32;
extern Model *modelWriteSeq64;
extern Model *modelBigButtonSeq;
extern Model *modelBigButtonSeq2;
extern Model *modelFourView;
extern Model *modelSemiModularSynth;
extern Model *modelHotkey;
extern Model *modelPart;
extern Model *modelBlankPanel;


// General constants
static const bool retrigGatesOnReset = true;
static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
static const int displayAlpha = 23;
static const std::string darkPanelID = "Dark-valor";
static const unsigned int expanderRefreshStepSkips = 4;



// General objects

struct VecPx : Vec {
	// temporary method to avoid having to convert all px coordinates to mm; no use when making a new module (since mm is the standard)
	static constexpr float scl = 5.08f / 15.0f;
	VecPx(float _x, float _y) {
		x = mm2px(_x * scl);
		y = mm2px(_y * scl);
	}
};

struct RefreshCounter {
	// Note: because of stagger, and asyncronous dataFromJson, should not assume this processInputs() will return true on first run
	// of module::process()
	static const unsigned int displayRefreshStepSkips = 256;
	static const unsigned int userInputsStepSkipMask = 0xF;// sub interval of displayRefreshStepSkips, since inputs should be more responsive than lights
	// above value should make it such that inputs are sampled > 1kHz so as to not miss 1ms triggers
	
	unsigned int refreshCounter = (random::u32() % displayRefreshStepSkips);// stagger start values to avoid processing peaks when many Geo and Impromptu modules in the patch
	
	bool processInputs() {
		return ((refreshCounter & userInputsStepSkipMask) == 0);
	}
	bool processLights() {// this must be called even if module has no lights, since counter is decremented here
		refreshCounter++;
		bool process = refreshCounter >= displayRefreshStepSkips;
		if (process) {
			refreshCounter = 0;
		}
		return process;
	}
};


struct Trigger : dsp::SchmittTrigger {
	// implements a 0.1V - 1.0V SchmittTrigger (see include/dsp/digital.hpp) instead of 
	//   calling SchmittTriggerInstance.process(math::rescale(in, 0.1f, 1.f, 0.f, 1.f))
	bool process(float in) {
		if (state) {
			// HIGH to LOW
			if (in <= 0.1f) {
				state = false;
			}
		}
		else {
			// LOW to HIGH
			if (in >= 1.0f) {
				state = true;
				return true;
			}
		}
		return false;
	}	
};	


struct TriggerRiseFall {
	bool state = false;

	void reset() {
		state = false;
	}

	int process(float in) {
		if (state) {
			// HIGH to LOW
			if (in <= 0.1f) {
				state = false;
				return -1;
			}
		}
		else {
			// LOW to HIGH
			if (in >= 1.0f) {
				state = true;
				return 1;
			}
		}
		return 0;
	}	
};	


struct HoldDetect {
	long modeHoldDetect;// 0 when not detecting, downward counter when detecting
	
	void reset() {
		modeHoldDetect = 0l;
	}
	
	void start(long startValue) {
		modeHoldDetect = startValue;
	}

	bool process(float paramValue) {
		bool ret = false;
		if (modeHoldDetect > 0l) {
			if (paramValue < 0.5f)
				modeHoldDetect = 0l;
			else {
				if (modeHoldDetect == 1l) {
					ret = true;
				}
				modeHoldDetect--;
			}
		}
		return ret;
	}
};



// General functions


inline bool calcWarningFlash(long count, long countInit) {
	if ( (count > (countInit * 2l / 4l) && count < (countInit * 3l / 4l)) || (count < (countInit * 1l / 4l)) )
		return false;
	return true;
}	

NVGcolor prepareDisplay(NVGcontext *vg, Rect *box, int fontSize);

inline void calcNoteAndOct(float cv, int* note12, int* oct0) {
	// note12 is a note index (0 to 11) representing the C to B keys respectively
	// oct0 is an octave number, 0 representing octave 4 (as in C4 for example)
	eucDivMod((int)std::round(cv * 12.0f), 12, oct0, note12);
}
void printNoteNoOct(int note, char* text, bool sharp);
int printNote(float cvVal, char* text, bool sharp);

int moveIndex(int index, int indexNext, int numSteps);

void saveDarkAsDefault(bool darkAsDefault);
bool loadDarkAsDefault();

struct DarkDefaultItem : MenuItem {
	void onAction(const event::Action &e) override {
		saveDarkAsDefault(rightText.empty());// implicitly toggled
	}
};	

struct InstantiateExpanderItem : MenuItem {
	Model *model;
	Vec posit;
	void onAction(const event::Action &e) override;
};


#endif
