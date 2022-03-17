//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************

// inkscape font sizes: 8 (small), 10.3 (normal) and 15.5 (title)


#pragma once

#include "rack.hpp"
#include "comp/Components.hpp"

using namespace rack;


extern Plugin *pluginInstance;


// All modules that are part of pluginInstance go here
extern Model *modelAdaptiveQuantizer;
extern Model *modelBigButtonSeq;
extern Model *modelBigButtonSeq2;
extern Model *modelChordKey;
extern Model *modelChordKeyExpander;
extern Model *modelClocked;
extern Model *modelClockedExpander;
extern Model *modelClkd;
extern Model *modelCvPad;
extern Model *modelFoundry;
extern Model *modelFoundryExpander;
extern Model *modelFourView;
extern Model *modelGateSeq64;
extern Model *modelGateSeq64Expander;
extern Model *modelHotkey;
extern Model *modelPart;
extern Model *modelPhraseSeq16;
extern Model *modelPhraseSeq32;
extern Model *modelPhraseSeqExpander;
extern Model *modelProbKey;
extern Model *modelSemiModularSynth;
extern Model *modelSygen;
extern Model *modelTact;
extern Model *modelTact1;
extern Model *modelTactG;
extern Model *modelTwelveKey;
extern Model *modelVariations;
extern Model *modelWriteSeq32;
extern Model *modelWriteSeq64;
extern Model *modelBlankPanel;




// General constants
static const bool retrigGatesOnReset = true;
static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)

static const unsigned int expanderRefreshStepSkips = 4;
static const NVGcolor displayColOn = nvgRGB(0xaf, 0xd2, 0x2c);



// General objects

struct ClockMaster {// should not need to have mutex since only menu driven
	int64_t id = -1;
	bool resetClockOutputsHigh;
	
	void setAsMaster(int64_t _id, bool _resetClockOutputsHigh) {
		id = _id,
		resetClockOutputsHigh = _resetClockOutputsHigh;
	}
	void setAsMasterIfNoMasterExists(int64_t _id, bool _resetClockOutputsHigh) {
		if (id == -1) {
			setAsMaster(_id, _resetClockOutputsHigh);
		}
	}
	void removeMaster() {
		id = -1;
	}
	void removeAsMasterIfThisIsMaster(int64_t _id) {
		if (id == _id) {
			removeMaster();
		}	
	}
	
	bool validateClockModule() {
		for (Widget* widget : APP->scene->rack->getModuleContainer()->children) {
			ModuleWidget* moduleWidget = dynamic_cast<ModuleWidget *>(widget);
			if (moduleWidget && moduleWidget->module->id == id) {
				if (moduleWidget->model->slug.substr(0, 7) == std::string("Clocked")) {
					return true;
				}
			}
		}
		return false;
	}
}; 
extern ClockMaster clockMaster;


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

inline void calcNoteAndOct(float cv, int* note12, int* oct0) {
	// note12 is a note index (0 to 11) representing the C to B keys respectively
	// oct0 is an octave number, 0 representing octave 4 (as in C4 for example)
	eucDivMod((int)std::round(cv * 12.0f), 12, oct0, note12);
}
void printNoteNoOct(int note, char* text, bool sharp);
int printNote(float cvVal, char* text, bool sharp);
int printNoteOrig(float cvVal, char* text, bool sharp);

int moveIndex(int index, int indexNext, int numSteps);


struct InstantiateExpanderItem : MenuItem {
	Module* module;
	Model* model;
	Vec posit;
	void onAction(const event::Action &e) override;
};
