//***********************************************************************************************
//Adaptive reference-based quantizer module for VCV Rack by Marc Boulé and Sam Burford
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "AdaptiveQuantizerUtil.hpp"


struct AdaptiveQuantizer : Module {
	enum ParamIds {
		PITCHES_PARAM,
		OCTW_PARAM,
		DURW_PARAM,
		OFFSET_PARAM,
		PERSIST_PARAM,
		RESET_PARAM,
		FREEZE_PARAM,
		THRU_PARAM,
		SH_PARAM,
		CHORD_PARAM,
		INTERVAL_PARAM,// none , last, most
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		GATE_INPUT,
		REFCV_INPUT,
		REFGATE_INPUT,
		RESET_INPUT,
		FREEZE_INPUT,
		PITCHES_INPUT,
		PERSIST_INPUT,
		THRU_INPUT,
		OFFSET_INPUT,
		OCTW_INPUT,
		DURW_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE_OUTPUT,
		CHORD_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(WEIGHT_LIGHTS, 12 * 5),// White (single dummy color since custom coloring in widget)
		ENUMS(TARGET_LIGHTS, 12 * 2),// WhiteBlue
		RESET_LIGHT,
		FREEZE_LIGHT,
		THRU_LIGHT,
		SH_LIGHT,
		OFFSET_LIGHT,
		TITLE_LIGHT,
		ENUMS(INTERVAL_LIGHT, 2),// room for yellowGreen
		NUM_LIGHTS
	};
	
	
	// Expander
	// none

		
	// Constants
	static const int DTSIZE = 240;// should be a multiple of 60 (since there are 60 pitch matrix leds and alternate display will be done)
	static const int EVENTS_PER_LED = DTSIZE / (12 * 5);
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool thru;
	bool freeze;
	bool sampHold;
	int resetClearsDataTable;
	float cvOut;
	float chordOut[5];
	int8_t notes[DTSIZE] = {};// contains note numbers from 0 to 11 (chromatic pitches)
	int8_t octs[DTSIZE] = {};// contains octave number for notes
	int8_t intervals[DTSIZE] = {};// contains intevals of notes (interval of a given note wrt the previous note)
	float durations[DTSIZE] = {};// contains duration of gate for notes
	int head;// position of next note to be entered
	bool full;
	int intervalMode;
	int ignoreRepetitions;
	
	// No need to save, with reset
	int numPitch;// must update target when changes
	int persistence;// must update weights when changes
	int offset;// must update weights when changes
	float octw;// must update weights when changes
	float durw;// must update weights when changes
	float weights[12];// must update target when changes; weights are 0 to 1
	int weightAges[12];// follows weights, 0 when no weight, > 0 to indicate youngest age for each notes
	std::vector<WeightAndIndex> sortedWeights;// follows weights[]
	int targets;// aka target pitches, must follow weights and numPitch, must call updateRanges() when changed, bit0 = C, bit11 = B
	int qdist[12];// number of semitones distance (offset) in order to quantize; must follow targets
	long infoDataTable;// 0 when no info (i.e. just weights), positive downward step counter timer when showing data fill status
	bool pending;
	float pendingCv;
	long durationCount;

	// No need to save, no reset
	RefreshCounter refresh;
	float resetLight = 0.0f;
	Trigger thruTrigger;
	Trigger thruCvTrigger;
	Trigger resetTrigger;
	Trigger freezeTrigger;
	Trigger sampHoldTrigger;
	// Trigger sampHoldCvTrigger;
	Trigger liveGateTrigger;
	Trigger intervalModeTrigger;
	TriggerRiseFall refGateTrigger;


	int getNumPitch() {
		int npit = (int)std::round(params[PITCHES_PARAM].getValue() + inputs[PITCHES_INPUT].getVoltage() * 1.1f);
		return clamp(npit, 1, 12);
	}
	
	int getPersistence() {
		int persist = std::round(params[PERSIST_PARAM].getValue() + inputs[PERSIST_INPUT].getVoltage() * 0.1f * (float)DTSIZE);
		return clamp(persist, 4, DTSIZE);
	}
	
	int getOffset() {
		int ofst = std::round(params[OFFSET_PARAM].getValue() + inputs[OFFSET_INPUT].getVoltage() * 0.1f * (float)DTSIZE);
		return clamp(ofst, 0, DTSIZE);
	}
	
	int getChordPoly() {
		int cpol = std::round(params[CHORD_PARAM].getValue());
		return clamp(cpol, 1, 5);
	}
	
	float getOctw() {
		float owgt = params[OCTW_PARAM].getValue() + inputs[OCTW_INPUT].getVoltage() * 0.2f;
		return clamp(owgt, -1.0f, 1.0f);
	}
	
	float getDurw() {
		float dwgt = params[DURW_PARAM].getValue() + inputs[DURW_INPUT].getVoltage() * 0.2f;
		return clamp(dwgt, -1.0f, 1.0f);
	}
	
	
	AdaptiveQuantizer() : sortedWeights(12) {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		// diplay params are: base, mult, offset
		configParam(PITCHES_PARAM, 1.0f, 12.0f, 12.0f, "Number of pitches", "");
		paramQuantities[PITCHES_PARAM]->snapEnabled = true;
		configParam(OCTW_PARAM, -1.0f, 1.0f, 0.0f, "Octave weighting", " %", 0.0f, 100.0f, 0.0f);
		configParam(DURW_PARAM, -1.0f, 1.0f, 0.0f, "Duration weighting", " %", 0.0f, 100.0f, 0.0f);
		configParam(OFFSET_PARAM, 0.0f, (float)DTSIZE, 0.0f, "Offset", " events");
		paramQuantities[OFFSET_PARAM]->snapEnabled = true;
		configParam(PERSIST_PARAM, 4.0f, (float)DTSIZE, (float)DTSIZE, "Persistence", " events");
		paramQuantities[PERSIST_PARAM]->snapEnabled = true;
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset", "");
		configParam(FREEZE_PARAM, 0.0f, 1.0f, 0.0f, "Freeze", "");
		configParam(THRU_PARAM, 0.0f, 1.0f, 0.0f, "Thru", "");
		configParam(SH_PARAM, 0.0f, 1.0f, 0.0f, "Sample and hold", "");
		configParam(CHORD_PARAM, 1.0f, 5.0f, 3.0f, "Chord polyphony", " note(s)");
		paramQuantities[CHORD_PARAM]->snapEnabled = true;
		configParam(INTERVAL_PARAM, 0.0f, 1.0f, 0.0f, "Interval mode", " (none/last/most)");
		
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(REFCV_INPUT, "Reference CV");
		configInput(REFGATE_INPUT, "Reference Gate");
		configInput(RESET_INPUT, "Reset");
		configInput(FREEZE_INPUT, "Freeze");
		configInput(PITCHES_INPUT, "Number of pitches");
		configInput(PERSIST_INPUT, "Persistence");
		configInput(THRU_INPUT, "Thru");
		configInput(OFFSET_INPUT, "Offset");
		configInput(OCTW_INPUT, "Octave weighting");
		configInput(DURW_INPUT, "Duration weighting");

		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CHORD_OUTPUT, "Chord");

		configBypass(CV_INPUT, CV_OUTPUT);
		configBypass(GATE_INPUT, GATE_OUTPUT);

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	void onReset() override {
		thru = false;
		freeze = false;
		sampHold = false;
		resetClearsDataTable = 1;// 0 means not cleared, 1 means cleared, 2 means copy last n events to start of data table
		cvOut = 0.0f;
		for (int i = 0; i < 5; i++) {
			chordOut[i] = 0.0f;
		}
		clearDataTable();
		intervalMode = 0;
		ignoreRepetitions = 0;
		resetNonJson();
	}
	void clearDataTable() {
		// notes: no need to clear
		// octs: no need to clear
		// intervals: no need to clear
		// duration: no need to clear
		head = 0;
		full = false;
	}
	void resetNonJson() {
		numPitch = getNumPitch();
		persistence = getPersistence();
		offset = getOffset();
		octw = getOctw();
		durw = getDurw();
		updateWeights();// weights[], weightAges[], sortedWeights, targets, qdist[]
		infoDataTable = 0l;
		pending = false;
		pendingCv = 0.0f;
		durationCount = 0;
	}
	
	
	void onRandomize() override {
	}


	void clearDataTableWithPriming() {
		static const int numPrimed = 4;
		int8_t pNotes[numPrimed];
		int8_t pOcts[numPrimed];
		int8_t pIntervals[numPrimed];
		float pDurations[numPrimed];
		int actualPrimed = 0;
		
		// preserve numPrimed last events
		for (int i = (head - 1); i > (head - numPrimed - 1); i--) {
			if (!full && i < 0) {
				break;
			}
			int dti = eucMod(i, DTSIZE);
			pNotes[actualPrimed] = notes[dti];
			pOcts[actualPrimed] = octs[dti];
			pIntervals[actualPrimed] = intervals[dti];
			pDurations[actualPrimed] = durations[dti];
			actualPrimed++;
		}
		
		full = false;
		
		// restore preserved events at start of data table
		for (head = 0; head < actualPrimed; head++) {
			notes[head] = pNotes[head];
			octs[head] = pOcts[head];
			intervals[head] = pIntervals[head];
			durations[head] = pDurations[head];
		}
		intervals[0] = 0;
	}	
		

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// thru
		json_object_set_new(rootJ, "thru", json_boolean(thru));
		
		// freeze
		json_object_set_new(rootJ, "freeze", json_boolean(freeze));
		
		// sampHold
		json_object_set_new(rootJ, "sampHold", json_boolean(sampHold));
		
		// resetClearsDataTable
		json_object_set_new(rootJ, "resetClearsDataTable", json_integer(resetClearsDataTable));
		
		// cvOut
		json_object_set_new(rootJ, "cvOut", json_real(cvOut));

		// chordOut
		json_t *chordOutJ = json_array();
		for (int i = 0; i < 5; i++) {
			json_array_insert_new(chordOutJ, i, json_real(chordOut[i]));
		}
		json_object_set_new(rootJ, "chordOut", chordOutJ);

		// notes
		json_t *notesJ = json_array();
		for (int i = 0; i < DTSIZE; i++) {
			json_array_insert_new(notesJ, i, json_integer(notes[i]));
		}
		json_object_set_new(rootJ, "notes", notesJ);
		
		// octs
		json_t *octsJ = json_array();
		for (int i = 0; i < DTSIZE; i++) {
			json_array_insert_new(octsJ, i, json_integer(octs[i]));
		}
		json_object_set_new(rootJ, "octs", octsJ);
		
		// intervals
		json_t *intervalsJ = json_array();
		for (int i = 0; i < DTSIZE; i++) {
			json_array_insert_new(intervalsJ, i, json_integer(intervals[i]));
		}
		json_object_set_new(rootJ, "intervals", intervalsJ);
		
		// durations
		json_t *durationsJ = json_array();
		for (int i = 0; i < DTSIZE; i++) {
			json_array_insert_new(durationsJ, i, json_real(durations[i]));
		}
		json_object_set_new(rootJ, "durations", durationsJ);
		
		// head
		json_object_set_new(rootJ, "head", json_integer(head));
		
		// full
		json_object_set_new(rootJ, "full", json_boolean(full));

		// intervalMode
		json_object_set_new(rootJ, "intervalMode", json_integer(intervalMode));
		
		// ignoreRepetitions
		json_object_set_new(rootJ, "ignoreRepetitions", json_integer(ignoreRepetitions));
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) {
			panelTheme = json_integer_value(panelThemeJ);
		}

		// panelContrast
		json_t *panelContrastJ = json_object_get(rootJ, "panelContrast");
		if (panelContrastJ)
			panelContrast = json_number_value(panelContrastJ);

		// thru
		json_t *thruJ = json_object_get(rootJ, "thru");
		if (thruJ) {
			thru = json_is_true(thruJ);
		}

		// freeze
		json_t *freezeJ = json_object_get(rootJ, "freeze");
		if (freezeJ) {
			freeze = json_is_true(freezeJ);
		}

		// sampHold
		json_t *sampHoldJ = json_object_get(rootJ, "sampHold");
		if (sampHoldJ) {
			sampHold = json_is_true(sampHoldJ);
		}

		// resetClearsDataTable
		json_t *resetClearsDataTableJ = json_object_get(rootJ, "resetClearsDataTable");
		if (resetClearsDataTableJ) {
			resetClearsDataTable = json_integer_value(resetClearsDataTableJ);
		}

		// cvOut
		json_t *cvOutJ = json_object_get(rootJ, "cvOut");
		if (cvOutJ) {
			cvOut = json_number_value(cvOutJ);
		}

		// chordOut
		json_t *chordOutJ = json_object_get(rootJ, "chordOut");
		if (chordOutJ && json_is_array(chordOutJ)) {
			for (int i = 0; i < 5; i++) {
				json_t *chordOutArrayJ = json_array_get(chordOutJ, i);
				if (chordOutArrayJ) {
					chordOut[i] = json_number_value(chordOutArrayJ);
				}
			}
		}

		// notes
		json_t *notesJ = json_object_get(rootJ, "notes");
		if (notesJ && json_is_array(notesJ)) {
			for (int i = 0; i < DTSIZE; i++) {
				json_t *notesArrayJ = json_array_get(notesJ, i);
				if (notesArrayJ) {
					notes[i] = json_integer_value(notesArrayJ);
				}
			}
		}

		// octs
		json_t *octsJ = json_object_get(rootJ, "octs");
		if (octsJ && json_is_array(octsJ)) {
			for (int i = 0; i < DTSIZE; i++) {
				json_t *octsArrayJ = json_array_get(octsJ, i);
				if (octsArrayJ) {
					octs[i] = json_integer_value(octsArrayJ);
				}
			}
		}

		// intervals
		json_t *intervalsJ = json_object_get(rootJ, "intervals");
		if (intervalsJ && json_is_array(intervalsJ)) {
			for (int i = 0; i < DTSIZE; i++) {
				json_t *intervalsArrayJ = json_array_get(intervalsJ, i);
				if (intervalsArrayJ) {
					intervals[i] = json_integer_value(intervalsArrayJ);
				}
			}
		}

		// durations
		json_t *durationsJ = json_object_get(rootJ, "durations");
		if (durationsJ && json_is_array(durationsJ)) {
			for (int i = 0; i < DTSIZE; i++) {
				json_t *durationsArrayJ = json_array_get(durationsJ, i);
				if (durationsArrayJ) {
					durations[i] = json_number_value(durationsArrayJ);
				}
			}
		}

		// head
		json_t *headJ = json_object_get(rootJ, "head");
		if (headJ) {
			head = json_integer_value(headJ);
		}
		
		// full
		json_t *fullJ = json_object_get(rootJ, "full");
		if (fullJ) {
			full = json_is_true(fullJ);
		}
		
		// intervalMode
		json_t *intervalModeJ = json_object_get(rootJ, "intervalMode");
		if (intervalModeJ) {
			intervalMode = json_integer_value(intervalModeJ);
		}
		
		// ignoreRepetitions
		json_t *ignoreRepetitionsJ = json_object_get(rootJ, "ignoreRepetitions");
		if (ignoreRepetitionsJ) {
			ignoreRepetitions = json_integer_value(ignoreRepetitionsJ);
		}
		
		resetNonJson();
	}
		

	void onSampleRateChange() override {
		onReset();
	}		

	
	void process(const ProcessArgs &args) override {		
		
		//********** Buttons, knobs, switches and inputs **********
		
		if (refresh.processInputs()) {
			// freeze
			if (freezeTrigger.process(params[FREEZE_PARAM].getValue() + inputs[FREEZE_INPUT].getVoltage())) {
				freeze = !freeze;
				pending = false;
				durationCount = 0;
			}

			// thru
			if (thruTrigger.process(params[THRU_PARAM].getValue())) {
				thru = !thru;
			}
			// thru cv
			if (thruCvTrigger.process(inputs[THRU_INPUT].getVoltage())) {
				thru = !thru;
			}
			
			// sampHold
			if (sampHoldTrigger.process(params[SH_PARAM].getValue())) {
				sampHold = !sampHold;
			}
			
			// intervalMode
			if (intervalModeTrigger.process(params[INTERVAL_PARAM].getValue())) {
				if (intervalMode < 2) {
					intervalMode++;
				}
				else {
					intervalMode = 0;
				};				
				updateQdist();
			}

			// numPitch
			int newNumPitch = getNumPitch();
			if (numPitch != newNumPitch) {
				numPitch = newNumPitch;
				updateTargets();
			}
			
			// persistence
			int newPersistence = getPersistence();
			if (persistence != newPersistence) {
				persistence = newPersistence;
				updateWeights();
			}
			
			// offset
			int newOffset = getOffset();
			if (offset != newOffset) {
				offset = newOffset;
				updateWeights();
			}
			
			// octave weighting
			float newOctw = getOctw();
			if (octw != newOctw) {
				octw = newOctw;
				updateWeights();
			}
			
			// duration weighting
			float newDurw = getDurw();
			if (durw != newDurw) {
				durw = newDurw;
				updateWeights();
			}
			
			// chord poly output portion
			outputs[CHORD_OUTPUT].setChannels(getChordPoly());
		}// userInputs refresh
		
		

		// Ref Gate
		int refGateEdge = refGateTrigger.process(inputs[REFGATE_INPUT].getVoltage());
		if (refGateEdge != 0 && !freeze) {
			if (inputs[REFCV_INPUT].isConnected()) {// don't start when ref cv cable not present	
				if (refGateEdge == 1) {
					// rising edge
					pending = true;
					pendingCv = inputs[REFCV_INPUT].getVoltage();
					durationCount = 1;
				}
				else {// if (refGateEdge == -1) {
					// falling edge
					if (pending) {
						enterNote(pendingCv, ((float)durationCount) * args.sampleTime);
						updateWeights();
						pending = false;
						durationCount = 0;
					}
				}
			}
		}
		if (durationCount > 0) {
			durationCount++;
		}		
		
		// Reset
		if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			resetLight = 1.0f;
			freeze = false;
			cvOut = 0.0f;
			for (int i = 0; i < 5; i++) {
				chordOut[i] = 0.0f;
			}
			if (resetClearsDataTable != 0) {
				if (resetClearsDataTable == 2) {
					clearDataTableWithPriming();
				}
				else {
					clearDataTable();
				}
			}
			updateWeights();
		}
		
		
		
		//********** Outputs and lights **********
		
		// Gate and CV outputs
		bool liveGateRise = liveGateTrigger.process(inputs[GATE_INPUT].getVoltage());
		if (thru) {
			// 12TET quantizing
			cvOut = std::round(inputs[CV_INPUT].getVoltage() * 12.0f) / 12.0f;
		}	
		else if (!sampHold || liveGateRise) {
			// cvOut
			if (targets != 0) {
				// AQ quantizing
				cvOut = aqQuantize(inputs[CV_INPUT].getVoltage());
			}
		}
		outputs[GATE_OUTPUT].setVoltage(targets != 0 ? inputs[GATE_INPUT].getVoltage() : 0.0f);
		outputs[CV_OUTPUT].setVoltage(cvOut);
		
		// Chord output
		if (!sampHold || liveGateRise) {
			// chordOut
			float lastOut = 0.0f;
			for (int i = 0; i < 5; i++) {
				if (targets == 0) {
					chordOut[i] = 0.0f;
				}
				else {
					if (sortedWeights[i].w > 0.0f) {
						lastOut = ( (float)(sortedWeights[i].i) / 12.0f);
						if (sortedWeights[i].i < sortedWeights[0].i) {
							lastOut += 1.0f;
						}
					}
					chordOut[i] = lastOut;
				}
			}
		}		
		if (outputs[CHORD_OUTPUT].isConnected()) {
			for (int c = 0; c < getChordPoly(); c++) {
				outputs[CHORD_OUTPUT].setVoltage(chordOut[c], c);
			}
		}

		
		// Lights
		if (refresh.processLights()) {
			// Reset light
			lights[RESET_LIGHT].setSmoothBrightness(resetLight, args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));	
			resetLight = 0.0f;

			// freeze
			lights[FREEZE_LIGHT].setBrightness(freeze ? 1.0f : 0.0f);
			// thru
			lights[THRU_LIGHT].setBrightness(thru ? 1.0f : 0.0f);
			// sampHold
			lights[SH_LIGHT].setBrightness(sampHold ? 1.0f : 0.0f);
			// interval
			lights[INTERVAL_LIGHT + 0].setBrightness(intervalMode == 1 ? 1.0f : 0.0f);
			lights[INTERVAL_LIGHT + 1].setBrightness(intervalMode == 2 ? 1.0f : 0.0f);
			
			// offset
			bool offsetBad = !full && (offset >= head) && offset != 0;
			lights[OFFSET_LIGHT].setBrightness(offsetBad ? 1.0f : 0.0f);
			
			// offset
			lights[TITLE_LIGHT].setBrightness(!thru && (offsetBad || targets == 0 || targets == 0xFFF) ? 1.0f : 0.0f);
			
			// target pitches
			for (int k = 0; k < 12; k++) {
				bool targetK = (targets & (0x1 << k)) != 0;
				lights[TARGET_LIGHTS + k * 2 + 0].setBrightness((freeze || !targetK) && !thru ? 0.0f : 1.0f);// white
				lights[TARGET_LIGHTS + k * 2 + 1].setBrightness(freeze &&  targetK && !thru ? 1.0f : 0.0f);// blue
			}
			
			// pitch matrix
			// done in widget
			
			if (infoDataTable > 0l) {
				infoDataTable--;
			}

		}// processLights()
	}// process()
	
	
	float aqQuantize(float inCv) {
		float noteInFloat = inCv * 12.0f;
		int noteIn = (int)std::round(noteInFloat);
		int noteIn12 = eucMod(noteIn, 12);
		int dist = qdist[noteIn12];
		if (dist == -6 && noteInFloat > (float)noteIn) {// "greater than" is used so that perfectly equidistant 6 half-step quantizations goes low
			dist = 6;
		}
		
		return (float)(noteIn + dist) / 12.0f;
	}
	
	
	void enterNote(float cv, float gateDuration) {
		// dependants: weights
		int newNote = 0;
		int newOct = 0;
		eucDivMod((int)std::round(cv * 12.0f), 12, &newOct, &newNote);

		if ((ignoreRepetitions != 0) && (full || head != 0)) {
			int dti = eucMod(head - 1, DTSIZE);// previous event
			if (notes[dti] == newNote && octs[dti] == newOct) {
				return;// skip repeats of same note
			}
		}

		notes[head] = newNote;
		octs[head] = newOct;
		intervals[head] = (!full && head == 0) ? 0 : notes[head] - notes[eucMod(head - 1, DTSIZE)];// intervals in -11 to 11
		durations[head] = gateDuration;
		
		head = (head + 1);
		if (head >= DTSIZE) {
			full = true;
			head = 0;
		}
		
		updateWeights();
	}
	
	
	/* 
	//original version (slow), uses std::pow()
	float convertNormalizedToWeightingRatio(float normalized, float control) {
		// normalized is 0.0f to 1.0f
		// a normalized value of 0.5f should always return 1.0f, irrespective of control
		static const float nValue = 5.0f;
		static const float cCoeff = 1.0f / nValue;
		static const float aCoeff = nValue - cCoeff;
		static const float bCoeff = std::log(1.0f / (nValue + 1.0f)) / std::log(1.0f / 2.0f);
		
		if (control < 0.0) {
			normalized = 1.0f - normalized;
			control *= -1.0f;
		}
		float y = aCoeff * std::pow(normalized, bCoeff) + cCoeff;
		return crossfade(1.0f, y, control);
	}*/
	
	
	// optimized version (fast), hardcoded for nValue = 5.0f
	// this is a curve fit with a degree 3 polynomial to the function above
	float convertNormalizedToWeightingRatio(float normalized, float control) {
		// normalized is 0.0f to 1.0f
		// a normalized value of 0.5f should always return 1.0f, irrespective of control
		static const float aCoeff = 128.0f / 45.0f;
		static const float bCoeff = 32.0f / 15.0f;
		static const float cCoeff = -8.0f / 45.0f;
		static const float dCoeff = 1.0f / 5.0f;
		
		
		if (control < 0.0) {
			normalized = 1.0f - normalized;
			control *= -1.0f;
		}
		float y = dCoeff + normalized * (cCoeff + normalized * (bCoeff + normalized * aCoeff));
		return crossfade(1.0f, y, control);
	}
	
	
	void updateWeights() { 
		// depends on: data table and many knobs (persistence, offset, weightings)
		// dependants: targets
		int numEvents = full ? DTSIZE : head;
		int numPersistEvents = std::min(persistence, numEvents);
		
		for (int i = 0; i < 12; i++) {
			weightAges[i] = 0;
		}
		
		if (numPersistEvents <= 0) {
			for (int i = 0; i < 12; i++) {
				weights[i] = 0.0f;
			}
		}
		else {
			float weightedFreq[12] = {};
			float maxDuration = 0.0f;
			float sumDuration = 0.0f;
			
			// calc duration and age statistics
			for (int i = 0; i < numPersistEvents; i++) {
				int dti = eucMod(head - 1 - i - offset, DTSIZE);
				if (!full && dti >= head) {
					break;
				}
				maxDuration = std::max(maxDuration, durations[dti]);
				sumDuration += durations[dti];
				if (weightAges[notes[dti]] == 0) {
					weightAges[notes[dti]] = i + 1;
				}
			}
			
			// calc freqs
			for (int i = 0; i < numPersistEvents; i++) {
				int dti = eucMod(head - 1 - i - offset, DTSIZE);
				if (!full && dti >= head) {
					break;
				}
				weightedFreq[notes[dti]] += 1.0f;
				// apply duration weighting
				if (maxDuration > 0.0f && durw != 0.0f) {
					// original version:
					// float durWeightOffset = clamp(durw * (2.0f * durations[dti] / maxDuration - 1.0f), -1.0f, 1.0f);
					// weightedFreq[notes[dti]] += durWeightOffset;
					// new version:
					float avgDuration = sumDuration / (float)numPersistEvents;
					float maxDeltaDuration = std::max(avgDuration, maxDuration - avgDuration);
					float normalized = (durations[dti] - avgDuration) / maxDeltaDuration;// range here is -1 to +1 when symmetrical delta;
					normalized = normalized * 0.5f + 0.5f;
					float ratio = convertNormalizedToWeightingRatio(normalized, durw);
					weightedFreq[notes[dti]] *= ratio;
				}
				// apply oct weighting
				if (octw != 0.0f) {
					// original version:
					// float octWeightOffset = clamp(octw * ((float)octs[dti]) / 3.0f, -1.0f, 1.0f);
					// weightedFreq[notes[dti]] = clamp(weightedFreq[notes[dti]] + octWeightOffset, 0.0f, 2.0f);
					// new version:
					float normalized = clamp( ((float)(octs[dti] + 3)) / 6.0f, 0.0f, 1.0f);
					float ratio = convertNormalizedToWeightingRatio(normalized, octw);
					weightedFreq[notes[dti]] *= ratio;
				}
			}

			// calc sum and max of weighted freqs
			float sum = 0.0f;
			float maxWeightedFreq = 0.0f;
			for (int i = 0; i < 12; i++) {
				sum += weightedFreq[i];
				maxWeightedFreq = std::max(maxWeightedFreq, weightedFreq[i]);
			}
			
			// convert weightedFreqs to normalized weights
			for (int i = 0; i < 12; i++) {
				float w = (maxWeightedFreq <= 0.0f ? 0.0f : weightedFreq[i] / maxWeightedFreq);
				weights[i] = w;
			}
		}
		
		updateTargets();
	}
	
	
	void updateTargets() {
		// depends on: weights, numPitch
		// dependants: qdist
		for (int k = 0; k < 12; k++) {
			sortedWeights[k].w = weights[k];
			sortedWeights[k].i = k;
		}
		
		std::sort(sortedWeights.begin(), sortedWeights.end(), weightComp);
		int newTargets = 0;
		for (int k = 0; k < numPitch; k++) {
			if (sortedWeights[k].w <= 0.0f) break;
			newTargets |= (0x1 << sortedWeights[k].i);
		}
		targets = newTargets;
		
		updateQdist();
	}


	int filterIntervalToTargetPitch(int qdi, int intv) {
		// used in updateQdist() only
		// returns the intv if the pitch corresponding to the interval offset (i.e. "qdi + intv", with proper modulo) falls on a target pitch, else returns 0
		// calling this method with intv = 0, because of use context, should always lead to a 0 return value (see attemptIntervalUpdateQdistIndex() comments)
		int iOffset12 = eucMod(qdi + intv, 12);
		if ((targets & (0x1 << iOffset12)) != 0) {
			// here the interval check has hit a target pitch
			return intv;
		}
		return 0;// interval check has not hit a target pitch
	}

	
	void updateQdist() {
		// depends on: weights, intervalMode
		// dependants: -
		// a qdist is from -6 to +5
		
		// for interval tests, when applicable
		int intervalFreq[12] = {};// only used when intervalMode == 2. intervalFreq[0] is not used, since bit number "qdi + 0" in targets will always be zero anytime the code that uses this is reached (qdi appears further below in this method). This is also not used when the datatable is empty because of a guard further below.
		
		if (intervalMode == 2) {
			int numEvents = full ? DTSIZE : head;
			int numPersistEvents = std::min(persistence, numEvents);
			for (int i = 0; i < numPersistEvents; i++) {
				int dti = eucMod(head - 1 - i - offset, DTSIZE);
				if (!full && dti >= head) {
					break;
				}
				
				// count freqs of intervals
				int intervalI = intervals[dti];// -11 to +11
				if (intervalI == 0) continue;
				if (intervalI < 0) intervalI += 12;// now +1 to +11
				intervalFreq[intervalI]++;
			}	
		}
		
		for (int qdi = 0; qdi < 12; qdi++) {
			// check RULE 1: direct match
			if ( (targets == 0) || ((targets & (0x1 << qdi)) != 0) ) {
				qdist[qdi] = 0;// spot on a target pitch, or no targets exist and quantize to all
			}
			else {
				// check INTERVAL OVERRIDE of rules 2 and 3
				bool intervalSuccess = false;
				if (intervalMode != 0 && (full || head != 0)) {
					int interval = 0;// -11 to +11
					if (intervalMode == 1) {
						interval = filterIntervalToTargetPitch(qdi, intervals[eucMod(head - 1, DTSIZE)]);// -11 to +11
					}
					else {// if (intervalMode == 2) {
						int intervalFreqI[12] = {};// a copy of intervalFreq but zero out the freq corresponding to intervalled pitches that are not in the target pitches.
						int intervalMost = -1;
						for (int i = 1; i < 12; i++) {
							if (intervalFreq[i] == 0) continue;
							// copy freqs of intervals that land on target pitches
							if (filterIntervalToTargetPitch(qdi, i) != 0) {
								intervalFreqI[i] = intervalFreq[i];
								// track most popular interval
								if (intervalFreqI[i] > intervalMost) {
									intervalMost = intervalFreqI[i];
								}
							}
						}
						
						// if any interval freqs are tied make interval be that of the smallest interval (+1 or +11 first, +2 or +10 second best, and so on), so smaller pitch jump when quantizing
						// (another idea could be to just choose interval that lands on the target pitch with greatest weight)
						// if no ties, this method will still choose the correct interval
						if (intervalMost != -1) {
							for (int i = 1; i < 6; i++) {// test i and 12 - i in the loop
								int intervalL = (intervalFreqI[i] == intervalMost) ? i : -1;
								int intervalH = (intervalFreqI[12 - i] == intervalMost) ? (12 - i) : -1;
								if (intervalL != -1 && intervalH != -1) {
									// here we have two intervals that are same distance, and have same freq, and both land on target pitches. choose the one with biggest weight to break the tie
									if (weights[intervalL] >= weights[intervalH]) {
										interval = intervalL;
									}
									else {
										interval = intervalH;
									}
									break;
								}
								else if (intervalL != -1) {
									interval = intervalL;
									break;
								}
								else if (intervalH != -1) {
									interval = intervalH;
									break;
								}
							}
						}
					}
					if (interval != 0) {
						if (interval < -6) interval += 12;
						if (interval >= 6) interval -= 12;// interval is now -6 to 5
						qdist[qdi] = interval;
						intervalSuccess = true;
					}
				} 					
				if (!intervalSuccess) {
					// apply RULES 2 and 3: check closest, and if two closest are tied distance, then check weights, and if weights are tied, check age. These rules always succeed
					int dist = 1;
					for (; dist <= 5; dist++) {
						int bestDist = 100;
						float bestWeight = -1e3;
						// lower
						int di12l = eucMod(qdi - dist, 12);
						if ( ((targets & (0x1 << di12l)) != 0) ) {
							bestDist = -dist;
							bestWeight = weights[di12l];
						}
						// upper (must check even if lower works, since could have a better weight (and age if weights tied)
						int di12h = eucMod(qdi + dist, 12);
						if ( ((targets & (0x1 << di12h)) != 0) ) {
							if (bestDist == 100 || weights[di12h] > bestWeight || 
												  (weights[di12h] == bestWeight && weightAges[di12h] < weightAges[di12l])) {									
								bestDist = dist;
								bestWeight = weights[di12h];
							}
						}
						if (bestDist != 100) {
							qdist[qdi] = bestDist;
							break;
						}
					}
					if (dist > 5) {
						// dist 6 maps to same place, no need to check weights, will assume -6 (no difference for PM LEDs, need to check input voltage when quantizing in order to see -6 or +6)
						qdist[qdi] = -6;
					}
				}
			}
		}
	}
	
};// struct AdaptiveQuantizer




struct AdaptiveQuantizerWidget : ModuleWidget {
	SvgPanel* darkPanel;
	PitchMatrixLight* pitchLightsWidgets[12 * 5];
	uint64_t route = 0;
	bool showDataTable = false;
	float datapic[12 * 5];// this is indexed according like this: [0] = bottom right, [11] = bottom left, [59] = top left

	
	void appendContextMenu(Menu *menu) override {
		AdaptiveQuantizer *module = dynamic_cast<AdaptiveQuantizer*>(this->module);
		assert(module);
			
		menu->addChild(createMenuLabel("Concept and design by Sam Burford"));

		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createCheckMenuItem("Skip repeats of same ref note", "",
			[=]() {return module->ignoreRepetitions;},
			[=]() {module->ignoreRepetitions ^= 0x1;}
		));

		menu->addChild(createSubmenuItem("Reset of data table", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("None", "",
				[=]() {return module->resetClearsDataTable == 0;},
				[=]() {module->resetClearsDataTable = 0;}
			));
			menu->addChild(createCheckMenuItem("Clear all (default)", "",
				[=]() {return module->resetClearsDataTable == 1;},
				[=]() {module->resetClearsDataTable = 1;}
			));
			menu->addChild(createCheckMenuItem("Clear with priming", "",
				[=]() {return module->resetClearsDataTable == 2;},
				[=]() {module->resetClearsDataTable = 2;}
			));
		}));	
	}


	// 4mm LED based on the component library's LEDs
	template <typename TBase>
	struct MediumLargeLight : TSvgLight<TBase> {
		MediumLargeLight() {
			this->setSvg(Svg::load(asset::plugin(pluginInstance, "res/comp/complib/MediumLargeLight.svg")));
			this->box.size = mm2px(Vec(4.177f, 4.177f));// 4 mm LED
		}
		void drawHalo(const DrawArgs& args) override {};
	};


	AdaptiveQuantizerWidget(AdaptiveQuantizer *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/AdaptiveQuantizer.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		// title led
		addChild(createLightCentered<SmallLight<RedLightIM>>(mm2px(Vec(85.38f, 6.32f)), module, AdaptiveQuantizer::TITLE_LIGHT));					
		static const Vec ledBgPos = mm2px(Vec(21.42f, 12.0f));
		svgPanel->fb->addChild(new AqLedBg(ledBgPos, mode));


		// ****** Top portion (weight keys and active lights) ******

		// target and weights
		static constexpr float xCenter = 58.42f;
		static constexpr float yTarget = 16.0f;
		static constexpr float yPitch = 26.0f;
		static constexpr float dxAll = 6.0f;
		static constexpr float dyPitch = 6.0f;

		for (int k = 0; k < 12; k++) {
			float xLeftK = xCenter + dxAll * ((float)k - 5.5f);
			
			// target pitch lights
			addChild(createLightCentered<MediumLargeLight<WhiteBlueLight>>(mm2px(Vec(xLeftK, yTarget)), module, AdaptiveQuantizer::TARGET_LIGHTS + k * 2));	
			
			// pitch matrix lights
			for (int y = 0; y < 5; y++) {// light index 0 on bottom
				int lightId = k * (5 * 1) + y * 1 + 0;
				addChild(pitchLightsWidgets[lightId] = createLightCentered<MediumLargeLight<PitchMatrixLight>>(mm2px(Vec(xLeftK, yPitch + dyPitch * ((5 - 1) - y))), module, AdaptiveQuantizer::WEIGHT_LIGHTS + lightId));
				if (module) {
					pitchLightsWidgets[lightId]->showDataTable = &showDataTable;
					pitchLightsWidgets[lightId]->qdist = &(module->qdist[k]);
					pitchLightsWidgets[lightId]->weight = &(module->weights[k]);
					pitchLightsWidgets[lightId]->key = k;
					pitchLightsWidgets[lightId]->y = y;
					pitchLightsWidgets[lightId]->route = &route;
					pitchLightsWidgets[lightId]->thru = &(module->thru);
					pitchLightsWidgets[lightId]->datapic = &(datapic[12 * y + 12 - 1 - k]);
				}
			}
		}

		
		static constexpr float rowKnob = 77.5f;
		static constexpr float rowCv = 93.868f - 0.5f;
		static constexpr float rowBot = 112.0f + 0.5f;
		
		// static constexpr float widthModule = 5.08f * 23.0f;
		static constexpr float coldx = 19.0f;
		static constexpr float col0 = 11.0f;
		static constexpr float col1 = col0 + coldx;
		static constexpr float col2 = col1 + coldx;
		static constexpr float col3 = col2 + coldx;
		static constexpr float col4 = col3 + coldx;
		static constexpr float col5 = col4 + coldx;



		// ***** Top left *****
		
		static constexpr float rowT0 = 19.0f;
		static constexpr float rowT1 = rowT0 + 12.0f;
		static constexpr float rowT2 = 47.5f;
		static constexpr float rowT3 = rowT2 + 12.0f;
		
		static constexpr float rowT0l = 18.0f;
		static constexpr float rowT1l = rowT0l + 11.0f;
		
		// Thru LED Button
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col0, rowT0l)), module, AdaptiveQuantizer::THRU_PARAM));
		addChild(createLightCentered<MediumLight<RedLightIM>>(mm2px(Vec(col0, rowT0l)), module, AdaptiveQuantizer::THRU_LIGHT));

		// Thru CV
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, rowT1l)), true, module, AdaptiveQuantizer::THRU_INPUT, module ? &module->panelTheme : NULL));
		
		// S&H LED Button
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col0, 44.5f)), module, AdaptiveQuantizer::SH_PARAM));
		addChild(createLightCentered<MediumLight<RedLightIM>>(mm2px(Vec(col0, 44.5f)), module, AdaptiveQuantizer::SH_LIGHT));
		
		// Interval LED Button
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col0, rowT3)), module, AdaptiveQuantizer::INTERVAL_PARAM));
		addChild(createLightCentered<MediumLight<YellowGreenLightIM>>(mm2px(Vec(col0, rowT3)), module, AdaptiveQuantizer::INTERVAL_LIGHT));

		
		
		// ***** Top right *****
		
		// Freeze LED bezel and light
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(col5, rowT0)), module, AdaptiveQuantizer::FREEZE_PARAM));
		addChild(createLightCentered<LEDBezelLight<BlueLight>>(mm2px(Vec(col5, rowT0)), module, AdaptiveQuantizer::FREEZE_LIGHT));
		
		// Freeze CV
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col5, rowT1)), true, module, AdaptiveQuantizer::FREEZE_INPUT, module ? &module->panelTheme : NULL));		

		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(col5, rowT2)), module, AdaptiveQuantizer::RESET_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(mm2px(Vec(col5, rowT2)), module, AdaptiveQuantizer::RESET_LIGHT));
		
		// Reset CV
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col5, rowT3)), true, module, AdaptiveQuantizer::RESET_INPUT, module ? &module->panelTheme : NULL));
	
				
		
		// ***** Knobs row *****
		OffsetKnob* offsetKnob;
		PersistenceKnob* persistenceKnob;
		WeightingKnob* octaveKnob;
		WeightingKnob* durationKnob;
		PitchesKnob* pitchesKnob;
		
		// number of pitches
		addParam(pitchesKnob = createDynamicParamCentered<PitchesKnob>(mm2px(Vec(col0, rowKnob + 1.35f)), module, AdaptiveQuantizer::PITCHES_PARAM, module ? &module->panelTheme : NULL));	
		// persistence
		addParam(persistenceKnob = createDynamicParamCentered<PersistenceKnob>(mm2px(Vec(col1, rowKnob + 1.35f)), module, AdaptiveQuantizer::PERSIST_PARAM, module ? &module->panelTheme : NULL));	
		// offset and led
		addParam(offsetKnob = createDynamicParamCentered<OffsetKnob>(mm2px(Vec(col2, rowKnob)), module, AdaptiveQuantizer::OFFSET_PARAM, module ? &module->panelTheme : NULL));	
		addChild(createLightCentered<SmallLight<RedLightIM>>(mm2px(Vec(col2 + 8.1f, rowKnob - 8.6f)), module, AdaptiveQuantizer::OFFSET_LIGHT));					
		// oct weighting
		addParam(octaveKnob = createDynamicParamCentered<WeightingKnob>(mm2px(Vec(col3, rowKnob)), module, AdaptiveQuantizer::OCTW_PARAM, module ? &module->panelTheme : NULL));	
		// duration weighting
		addParam(durationKnob = createDynamicParamCentered<WeightingKnob>(mm2px(Vec(col4, rowKnob)), module, AdaptiveQuantizer::DURW_PARAM, module ? &module->panelTheme : NULL));	
		// chord
		addParam(createDynamicParamCentered<IMMediumKnob>(mm2px(Vec(col5, rowKnob)), module, AdaptiveQuantizer::CHORD_PARAM, module ? &module->panelTheme : NULL));
		
		if (module) {
			offsetKnob->infoDataTablePtr = &(module->infoDataTable);
			persistenceKnob->infoDataTablePtr = &(module->infoDataTable);
			// octaveKnob->infoDataTablePtr = &(module->infoDataTable);
			// durationKnob->infoDataTablePtr = &(module->infoDataTable);
			// pitchesKnob->infoDataTablePtr = &(module->infoDataTable);
		}


		// ***** CV row *****
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, rowCv)), true, module, AdaptiveQuantizer::PITCHES_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, rowCv)), true, module, AdaptiveQuantizer::PERSIST_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2, rowCv)), true, module, AdaptiveQuantizer::OFFSET_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col3, rowCv)), true, module, AdaptiveQuantizer::OCTW_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col4, rowCv)), true, module, AdaptiveQuantizer::DURW_INPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col5, rowCv)), false, module, AdaptiveQuantizer::CHORD_OUTPUT, module ? &module->panelTheme : NULL));
		
		
		
		// ***** Bottom row *****
		
		// CV and Gate inputs
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, rowBot)), true, module, AdaptiveQuantizer::CV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, rowBot)), true, module, AdaptiveQuantizer::GATE_INPUT, module ? &module->panelTheme : NULL));

		// Ref CV and Ref Gate inputs
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2, rowBot)), true, module, AdaptiveQuantizer::REFCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col3, rowBot)), true, module, AdaptiveQuantizer::REFGATE_INPUT, module ? &module->panelTheme : NULL));

		// CV and Gate outputs
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col4, rowBot)), false, module, AdaptiveQuantizer::CV_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col5, rowBot)), false, module, AdaptiveQuantizer::GATE_OUTPUT, module ? &module->panelTheme : NULL));
	}
	
	
	void step() override {
		if (module) {
			AdaptiveQuantizer *module = dynamic_cast<AdaptiveQuantizer*>(this->module);
						
			if (module->infoDataTable != 0l) {
				// prepare data-table picture (for alternate representation i.e. data-table visual)
				int numEvents = module->full ? AdaptiveQuantizer::DTSIZE : module->head;
				int numPersistEvents = module->persistence;//std::min(module->persistence, numEvents);
				int intStart = module->offset;
				int intEnd = intStart + numPersistEvents;
				
				for (int i = 0; i < 12 * 5; i++) {
					int intLedLevel = 
						clamp((intEnd -   (i * AdaptiveQuantizer::EVENTS_PER_LED)), 0, AdaptiveQuantizer::EVENTS_PER_LED) - 
						clamp((intStart - (i * AdaptiveQuantizer::EVENTS_PER_LED)), 0, AdaptiveQuantizer::EVENTS_PER_LED);
					// float ledLevel = ((float)intLedLevel) / ((float)AdaptiveQuantizer::EVENTS_PER_LED);// orig
					float ledLevel = 0.5f *((float)intLedLevel) / ((float)AdaptiveQuantizer::EVENTS_PER_LED);// new
					// if (ledLevel == 0.0f) {// orig
						ledLevel += (i * AdaptiveQuantizer::EVENTS_PER_LED < numEvents) ? 0.5f : 0.0f;
					// }// orig
					datapic[i] = ledLevel;
				}
				
				showDataTable = true;
			}
			else {
				// prepare route (for normal display mode)
				if (module->inputs[AdaptiveQuantizer::GATE_INPUT].getVoltage() >= 1.0f) {
					uint64_t routeTmp = 0;// update route in one shot
					
					// input CV quantized (one LED)
					float noteInFloat = module->inputs[AdaptiveQuantizer::CV_INPUT].getVoltage() * 12.0f;
					int inNote = (int)std::round(noteInFloat);
					int inNote12 = eucMod(inNote, 12);
					routeTmp |= ( ((uint64_t)0x1) << ((uint64_t)inNote12 * (uint64_t)5) );

					// output CV quantized (vertical strip of LEDs)
					int dist = module->thru ? 0 : module->qdist[inNote12];
					int outNote = inNote + dist;
					int outNote12 = eucMod(outNote, 12);
					for (uint64_t y = 0; y < 5; y++) {
						routeTmp |= ( ((uint64_t)0x1) << ((uint64_t)outNote12 * (uint64_t)5 + y) );
					}
					
					// horizontal connection
					if (dist == -6 && noteInFloat > (float)inNote) {// "greater than" is used so that perfectly equidistant goes low
						dist = 6;
					}
					if (dist > 1) {
						for (int x = inNote12 + 1; x < inNote12 + dist; x++) {
							int horizNote12 = eucMod(x, 12);
							routeTmp |= ( ((uint64_t)0x1) << ((uint64_t)horizNote12 * (uint64_t)5) );
						}
					}
					else if (dist < -1) {
						for (int x = inNote12 - 1; x > inNote12 + dist; x--) {
							int horizNote12 = eucMod(x, 12);
							routeTmp |= ( ((uint64_t)0x1) << ((uint64_t)horizNote12 * (uint64_t)5) );
						}
					}
					route = routeTmp;
				}
				else {
					route = 0;
				}
				
				showDataTable = false;
			}
		}
		Widget::step();
	}
};

Model *modelAdaptiveQuantizer = createModel<AdaptiveQuantizer, AdaptiveQuantizerWidget>("Adaptive-Quantizer");
