//***********************************************************************************************
//Keyboard-based chord generator module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the Stochastic Inspiration Generator by Stochastic Instruments
//***********************************************************************************************


#include "ProbKeyUtil.hpp"
#include "comp/PianoKey.hpp"
#include "Interop.hpp"


// inkscape font sizez: 8, 10.3 and 15.5



class ProbKernel {
	public: 
	
	static constexpr float IDEM_CV = -100.0f;
	static constexpr float MAX_ANCHOR_DELTA = 4.0f;// number of octaves, must be a positive integer
	
	private:
	
	float noteProbs[12];// [0.0f : 1.0f]
	float noteAnchors[12];// [0.0f : 1.0f];  0.5f=oct"4"=0V, 1.0f=oct"4+MAX_ANCHOR_DELTA"; not quantized
	float noteRanges[7];// [0] is -3, [6] is +3
	
	
	public:
	
	void reset() {
		for (int i = 0; i < 12; i++) {
			noteProbs[i] = 0.0f;
			noteAnchors[i] = octToAnchor(0.0f);
		}
		noteProbs[0] = 1.0f;
		noteProbs[4] = 1.0f;
		noteProbs[7] = 1.0f;
		
		for (int i = 0; i < 7; i++) {
			noteRanges[i] = 0.0f;
		}
		noteRanges[3] = 1.0f;
	}
	
	
	void randomize() {
		// not randomized
	}
	
	
	json_t* dataToJsonProb() {
		json_t* probJ = json_object();
		
		// noteProbs
		json_t *noteProbsJ = json_array();
		for (int i = 0; i < 12; i++) {
			json_array_insert_new(noteProbsJ, i, json_real(noteProbs[i]));
		}
		json_object_set_new(probJ, "noteProbs", noteProbsJ);
		
		// noteAnchors
		json_t *noteAnchorsJ = json_array();
		for (int i = 0; i < 12; i++) {
			json_array_insert_new(noteAnchorsJ, i, json_real(noteAnchors[i]));
		}
		json_object_set_new(probJ, "noteAnchors", noteAnchorsJ);
		
		// noteRanges
		json_t *noteRangesJ = json_array();
		for (int i = 0; i < 7; i++) {
			json_array_insert_new(noteRangesJ, i, json_real(noteRanges[i]));
		}
		json_object_set_new(probJ, "noteRanges", noteRangesJ);
		
		return probJ;
	}
	
	
	void dataFromJsonProb(json_t *probJ) {
		// noteProbs
		json_t *noteProbsJ = json_object_get(probJ, "noteProbs");
		if (noteProbsJ && json_is_array(noteProbsJ)) {
			for (int i = 0; i < 12; i++) {
				json_t *noteProbsArrayJ = json_array_get(noteProbsJ, i);
				if (noteProbsArrayJ) {
					noteProbs[i] = json_number_value(noteProbsArrayJ);
				}
			}
		}
		
		// noteAnchors
		json_t *noteAnchorsJ = json_object_get(probJ, "noteAnchors");
		if (noteAnchorsJ && json_is_array(noteAnchorsJ)) {
			for (int i = 0; i < 12; i++) {
				json_t *noteAnchorsArrayJ = json_array_get(noteAnchorsJ, i);
				if (noteAnchorsArrayJ) {
					noteAnchors[i] = json_number_value(noteAnchorsArrayJ);
				}
			}
		}
		
		// noteRanges
		json_t *noteRangesJ = json_object_get(probJ, "noteRanges");
		if (noteRangesJ && json_is_array(noteRangesJ)) {
			for (int i = 0; i < 7; i++) {
				json_t *noteRangesArrayJ = json_array_get(noteRangesJ, i);
				if (noteRangesArrayJ) {
					noteRanges[i] = json_number_value(noteRangesArrayJ);
				}
			}
		}
	}
	
	
	// getters
	float getNoteProb(int note) {
		return noteProbs[note];
	}
	float getNoteAnchor(int note) {
		return noteAnchors[note];
	}
	float getNoteRange(int note12) {
		return noteRanges[key12to7(note12)];
	}
	
	
	// setters
	void setNoteProb(int note, float prob, bool withSymmetry) {
		noteProbs[note] = prob;
		if (withSymmetry) {
			for (int i = 0; i < 12; i++) {
				if (noteProbs[i] != 0.0f) {
					noteProbs[i] = prob;
				}
			}
		}
	}
	void setNoteAnchor(int note, float anch, bool withSymmetry) {
		if (withSymmetry) {
			if (noteProbs[note] != 0.0f) {
				for (int i = 0; i < 12; i++) {
					if (noteProbs[i] != 0.0f) {
						noteAnchors[i] = anch;
					}
				}
			}
		}
		else {
			noteAnchors[note] = anch;
		}
	}
	void setNoteRange(int note12, float range, bool withSymmetry) {
		int note7 = key12to7(note12);
		noteRanges[note7] = range;
		if (withSymmetry) {
			noteRanges[6 - note7] = range;
		}
	}
	
	
	bool isProbNonNull(int note) {
		return noteProbs[note] > 0.0f;
	}
	
	
	float getCumulProbTotal() {
		float cumulProbTotal = 0.0f;
		for (int i = 0; i < 12; i++) {
			cumulProbTotal += noteProbs[i];
		}
		return cumulProbTotal;
	}
	
	void calcOffsetAndSquash(float* destRange, float offset, float squash, float overlap) {
		// destRange[] has to be pre-allocated and all 0.0f!!
		// uses noteRanges[] as const source
		
		// calc squash from noteRanges and put in new array
		float sqRange[7];
		float sqMult = 1.0f + 2.0f * (1.0f - overlap);
		squash = (1.0f - squash) * sqMult;
		float sq06 = clamp(squash - 2.0f * (1.0f - overlap), 0.0f, 1.0f);
		sqRange[0] = noteRanges[0] * sq06;
		sqRange[6] = noteRanges[6] * sq06;
		float sq15 = clamp(squash - (1.0f - overlap), 0.0f, 1.0f);
		sqRange[1] = noteRanges[1] * sq15;
		sqRange[5] = noteRanges[5] * sq15;
		float sq24 = clamp(squash, 0.0f, 1.0f);
		sqRange[2] = noteRanges[2] * sq24;
		sqRange[4] = noteRanges[4] * sq24;
		sqRange[3] = noteRanges[3];
		
		// calc offset from squash and put in destRange
		for (int i = 0; i < 7; i++) {
			float ofsi = (float)i - offset;
			int ileft = (int)std::floor(ofsi);
			float frac = ofsi - std::floor(ofsi);
			if (ileft >= 0 && ileft <= 6) {
				destRange[i] += sqRange[ileft] * (1.0f - frac);
			}
			int iright = ileft + 1;
			if (iright >= 0 && iright <= 6) {
				destRange[i] += sqRange[iright] * frac;
			}
		}		
	}
	
	
	float calcRandomCv(float offset, float squash, float density, float overlap) {
		// not optimized for audio rate
		// returns a cv value or IDEM_CV when a note gets randomly skipped (only possible when sum of probs < 1)
		
		// generate a (base) note according to noteProbs (base note only, C4=0 to B4)
		float cumulProbs[12];
		cumulProbs[0] = noteProbs[0];
		for (int i = 1; i < 12; i++) {
			cumulProbs[i] = cumulProbs[i - 1] + noteProbs[i];
		}
		
		float diceNote = random::uniform() * std::max(cumulProbs[11], 1.0f);		
		int note = 0;
		for (; note < 12; note++) {
			if (diceNote < cumulProbs[note]) {
				break;
			}
		}
		
		float cv;
		float diceDensity = random::uniform();
		if (note < 12 && diceDensity < density) {
			// base note
			cv = ((float)note) / 12.0f;
			
			// anchor
			cv += anchorToOct(noteAnchors[note]);
			
			// offset and squash
			float noteRangesMod[7] = {};
			calcOffsetAndSquash(noteRangesMod, offset, squash, overlap);
			
			// probabilistically transpose note according to octave ranges
			float cumulRanges[7];
			cumulRanges[0] = noteRangesMod[0];
			for (int i = 1; i < 7; i++) {
				cumulRanges[i] = cumulRanges[i - 1] + noteRangesMod[i];
			}
			
			float diceOct = random::uniform() * cumulRanges[6];
			int oct = 0;
			for (; oct < 7; oct++) {
				if (diceOct < cumulRanges[oct]) {
					break;
				}
			}
			if (oct < 7) {
				oct -= 3;
				cv += (float)oct;
			}
		}
		else {
			// skip note (this can happen when sum of probabilities is less than 1)
			cv = IDEM_CV;
		}

		return cv;
	}
	
	
	void transposeUp() {
		// rotate noteProbs[] and noteAnchors[] right by 1, and increment noteAnchor of B note
		// if noteAnchor of B note goes above MAX_ANCHOR_DELTA, do nothing
		float noteAnchorB = noteAnchors[11] + 1.0f / (2.0f * MAX_ANCHOR_DELTA);
		if (anchorToOct(noteAnchorB) <= MAX_ANCHOR_DELTA) {// comparison must be done in oct space
			float noteProbB = noteProbs[11];
			for (int i = 11; i > 0; i--) {
				noteProbs[i] = noteProbs[i - 1];
				noteAnchors[i] = noteAnchors[i - 1];	
			}
			noteProbs[0] = noteProbB;
			noteAnchors[0] = noteAnchorB;
		}
	}
	
	
	void transposeDown() {
		// rotate noteProbs[] and noteAnchors[] left by 1, and decrement noteAnchor of C note
		// if noteAnchor of C note goes below -MAX_ANCHOR_DELTA, do nothing
		float noteAnchorC = noteAnchors[0] - 1.0f / (2.0f * MAX_ANCHOR_DELTA);
		if (anchorToOct(noteAnchorC) >= -MAX_ANCHOR_DELTA) {// comparison must be done in oct space
			float noteProbC = noteProbs[0];
			for (int i = 0; i < 11; i++) {
				noteProbs[i] = noteProbs[i + 1];
				noteAnchors[i] = noteAnchors[i + 1];	
			}
			noteProbs[11] = noteProbC;
			noteAnchors[11] = noteAnchorC;
		}
	}
	
		
	// anchor helpers
	static float anchorToOct(float anch) {
		return std::round((anch - 0.5f) * 2.0f * MAX_ANCHOR_DELTA);
	}
	static float octToAnchor(float oct) {
		return oct / (2.0f * MAX_ANCHOR_DELTA) + 0.5f;
	}
	static float quantizeAnchor(float anch) {
		return std::round(anch * 2.0f * MAX_ANCHOR_DELTA) / (2.0f * MAX_ANCHOR_DELTA);
	}
	
	
	// range helpers
	static int key12to7(int key12) {
		// convert white key numbers from size 12 to size 7
		if (key12 > 4) {
			key12++;
		}
		return key12 >> 1; 
	}
	static int key7to12(int key7) {
		// convert white key numbers from size 7 to size 12
		key7 <<= 1;
		if (key7 > 4) {
			key7--;
		}
		return key7;// this is now a key 12  
	}
};


// ----------------------------------------------------------------------------


class OutputKernel {
	public:

	static const int MAX_LENGTH = 32;
	
	
	private:
	
	float shiftReg[MAX_LENGTH];// holds CVs or ProbKernel::IDEM_CV
	float lastCv;// sample and hold, must never hold ProbKernel::IDEM_CV
	float minCv;
	
	
	public:
	
	void reset() {
		for (int i = 0; i < MAX_LENGTH; i++) {
			shiftReg[i] = 0.0f;
		}
		lastCv = 0.0f;
		minCv = 0.0f;
	}
	
	void randomize() {
		// not done here
	}
	
	void dataToJson(json_t *rootJ, int id) {
		// shiftReg
		json_t *shiftRegJ = json_array();
		for (int i = 0; i < MAX_LENGTH; i++) {
			json_array_insert_new(shiftRegJ, i, json_real(shiftReg[i]));
		}
		json_object_set_new(rootJ, string::f("shiftReg%i", id).c_str(), shiftRegJ);
		
		// lastCv
		json_object_set_new(rootJ, string::f("lastCv%i", id).c_str(), json_real(lastCv));

		// minCv
		json_object_set_new(rootJ, string::f("minCv%i", id).c_str(), json_real(minCv));

	}
	
	void dataFromJson(json_t *rootJ, int id) {
		// shiftReg
		json_t *shiftRegJ = json_object_get(rootJ, string::f("shiftReg%i", id).c_str());
		if (shiftRegJ) {
			for (int i = 0; i < MAX_LENGTH; i++) {
				json_t *shiftRegArrayJ = json_array_get(shiftRegJ, i);
				if (shiftRegArrayJ) {
					shiftReg[i] = json_number_value(shiftRegArrayJ);
				}
			}
		}

		// lastCv
		json_t *lastCvJ = json_object_get(rootJ, string::f("lastCv%i", id).c_str());
		if (lastCvJ) {
			lastCv = json_number_value(lastCvJ);
		}

		// minCv
		json_t *minCvJ = json_object_get(rootJ, string::f("minCv%i", id).c_str());
		if (minCvJ) {
			minCv = json_number_value(minCvJ);
		}
	}


	void calcMinCv(int length0) {
		// will leave minCv untouched if all CVs within length are IDEM_CV
		float srMin = 100.0f;
		for (int i = 0; i <= length0; i++) {
			if (shiftReg[i] != ProbKernel::IDEM_CV && shiftReg[i] < srMin) {
				srMin = shiftReg[i];
			}
		}
		if (srMin != 100.0f) {
			minCv = srMin;
		}
	}


	void shiftWithHold(int length0) {
		for (int i = MAX_LENGTH - 1; i > 0; i--) {
			shiftReg[i] = shiftReg[i - 1];
		}
		calcMinCv(length0);
	}
		
	void shiftWithInsertNew(float newCv, int length0) {
		shiftWithHold(length0);
		shiftReg[0] = newCv;
		if (shiftReg[0] != ProbKernel::IDEM_CV) {
			lastCv = shiftReg[0];
			if (length0 == 0) {
				minCv = lastCv;
			}
			else {
				minCv = std::min(minCv, lastCv);
			}
		}
	}
		
	void shiftWithRecycle(int length0) {
		shiftWithInsertNew(shiftReg[length0], length0);
	}
	
	float getCv() {
		return lastCv;
	}
	float getMinCv() {
		return minCv;
	}
	float getReg(int i) {
		return shiftReg[i];
	}
	void setReg(float cv, int i) {
		shiftReg[i] = cv;
	}
	bool getGateEnable() {
		return shiftReg[0] != ProbKernel::IDEM_CV;
	}
};


// ----------------------------------------------------------------------------


class DisplayManager {
	long dispCounter;
	int dispMode;
	char buf[5];// only used when dispMode != DISP_NORMAL
	
	public:
	
	enum DispIds {DISP_NORMAL, DISP_PROB, DISP_ANCHOR, DISP_LENGTH, DISP_COPY, DISP_PASTE};
	
	int getMode() {
		return dispMode;
	}
	char* getText() {// only used when DISP_PROB, DISP_ANCHOR
		return buf;
	}
	
	void reset() {
		dispMode = DISP_NORMAL;
		dispCounter = 0;
		buf[0] = 0;
	}
	
	void process() {
		if (dispCounter > 0) {
			dispCounter--;
			if (dispCounter <= 0) {
				dispMode = DISP_NORMAL;
			}
		}
	}
	
	void displayProb(float probVal) {
		dispMode = DISP_PROB;
		dispCounter = (long)(2.5f * (APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips));
		int prob = (int)(probVal * 100.0f + 0.5f);
		if ( prob>= 100) { 
			snprintf(buf, 5, " 1,0");
		}	
		else if (prob >= 1) {
			snprintf(buf, 5, "0,%02u", (unsigned) prob);
		}
		else {
			snprintf(buf, 5, "   0");
		}
	}
	void displayAnchor(int key, int oct) {
		dispMode = DISP_ANCHOR;
		dispCounter = (long)(2.5f * (APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips));
		float cv = (float)oct + ((float)key) / 12.0f;
		printNote(cv, buf, true);
		
	}
	void displayLength() {
		dispMode = DISP_LENGTH;
		dispCounter = (long)(2.5f * (APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips));
	}
	void displayCopy() {
		dispMode = DISP_COPY;
		dispCounter = (long)(1.0f * (APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips));
		snprintf(buf, 5, "COPY");
	}
	void displayPaste() {
		dispMode = DISP_PASTE;
		dispCounter = (long)(1.0f * (APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips));
		snprintf(buf, 5, "PSTE");
	}
	
};


// ----------------------------------------------------------------------------


struct ProbKey : Module {
	enum ParamIds {
		INDEX_PARAM,
		LENGTH_PARAM,
		LOCK_KNOB_PARAM, // higher priority than hold
		LOCK_BUTTON_PARAM,
		OFFSET_PARAM,
		SQUASH_PARAM,
		ENUMS(MODE_PARAMS, 3), // see ModeIds enum
		DENSITY_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		TR_UP_PARAM,
		TR_DOWN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		INDEX_INPUT,
		LENGTH_INPUT,
		LOCK_INPUT,
		OFFSET_INPUT,
		SQUASH_INPUT,
		GATE_INPUT,
		HOLD_INPUT, // can hold an empty step when sum of probs < 1
		DENSITY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(KEY_LIGHTS, 12 * 4 * 3),// room for GreenRedWhite
		ENUMS(MODE_LIGHTS, 3 * 2), // see ModeIds enum; room for GreenRed
		ENUMS(DENSITY_LIGHT, 2),// room for GreenRed 
		LOCK_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Expander
	// none (nothing from ProbKeyExpander)
		
	// Constants
	enum ModeIds {MODE_PROB, MODE_ANCHOR, MODE_RANGE};
	static const int NUM_INDEXES = 25;// C4 to C6 incl
	static constexpr float infoTracerTime = 3.0f;// seconds
	
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int editMode;
	float overlap;
	int indexCvCap12;
	int showTracer;
	ProbKernel probKernels[NUM_INDEXES];
	OutputKernel outputKernels[PORT_MAX_CHANNELS];
	
	// No need to save, with reset
	DisplayManager dispManager;
	long infoTracer;

	// No need to save, no reset
	RefreshCounter refresh;
	PianoKeyInfo pkInfo;
	Trigger modeTriggers[3];
	Trigger gateInTriggers[PORT_MAX_CHANNELS];
	Trigger copyTrigger;
	Trigger pasteTrigger;
	Trigger tranUpTrigger;
	Trigger tranDownTrigger;
	
	
	int getIndex() {
		int index = (int)std::round(params[INDEX_PARAM].getValue() + inputs[INDEX_INPUT].getVoltage() * 12.0f);
		if (indexCvCap12 != 0) {
			return eucMod(index, 12);
		}	
		return clamp(index, 0, NUM_INDEXES - 1 );
	}
	float getDensity(int chan) {
		float density = params[DENSITY_PARAM].getValue(); 
		if (inputs[DENSITY_INPUT].isConnected()) {
			chan = std::min(chan, inputs[DENSITY_INPUT].getChannels() - 1);// getChannels() is >= 1
			density += inputs[DENSITY_INPUT].getVoltage(chan) * 0.1f;
			density = clamp(density, 0.0f, 1.0f);
		}
		return density;
	}
	float getOffset(int chan) {
		float offset = params[OFFSET_PARAM].getValue();
		if (inputs[OFFSET_INPUT].isConnected()) {
			chan = std::min(chan, inputs[OFFSET_INPUT].getChannels() - 1);// getChannels() is >= 1
			offset += inputs[OFFSET_INPUT].getVoltage(chan) * 0.3f;
			offset = clamp(offset, -3.0f, 3.0f);
		}
		return offset;
	}
	float getSquash(int chan) {
		float squash = params[SQUASH_PARAM].getValue();
		if (inputs[SQUASH_INPUT].isConnected()) {
			chan = std::min(chan, inputs[SQUASH_INPUT].getChannels() - 1);// getChannels() is >= 1
			squash += inputs[SQUASH_INPUT].getVoltage(chan) * 0.1f;
			squash = clamp(squash, 0.0f, 1.0f);
		}
		return squash;
	}
	float getLock() {
		float lock = params[LOCK_KNOB_PARAM].getValue();
		if (params[LOCK_BUTTON_PARAM].getValue() >= 0.5f) {
			if (lock >= 0.5f) {
				return 0.0f;
			}	
			else {
				return 1.0f;
			}
		}
		// check CV input only when button not pressed
		lock += inputs[LOCK_INPUT].getVoltage() * 0.1f;
		return clamp(lock, 0.0f, 1.0f);
	}
	int getLength0() {// 0 indexed
		int length = (int)std::round(params[LENGTH_PARAM].getValue() + inputs[LENGTH_INPUT].getVoltage() * 1.6f);
		return clamp(length, 0, OutputKernel::MAX_LENGTH - 1 );
	}


	ProbKey() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(INDEX_PARAM, 0.0f, 24.0f, 0.0f, "Index", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset
		configParam(LENGTH_PARAM, 0.0f, (float)(OutputKernel::MAX_LENGTH - 1), (float)(OutputKernel::MAX_LENGTH - 1), "Lock length", "", 0.0f, 1.0f, 1.0f);
		configParam(LOCK_KNOB_PARAM, 0.0f, 1.0f, 0.0f, "Lock sequence", " %", 0.0f, 100.0f, 0.0f);
		configParam(LOCK_BUTTON_PARAM, 0.0f, 1.0f, 0.0f, "Manual lock opposite");
		configParam(OFFSET_PARAM, -3.0f, 3.0f, 0.0f, "Oct range offset", "");
		configParam(SQUASH_PARAM, 0.0f, 1.0f, 0.0f, "Oct range squash", "", 0.0f, 1.0f, 0.0f);
		configParam(MODE_PARAMS + MODE_PROB, 0.0f, 1.0f, 0.0f, "Edit note probabilities", "");
		configParam(MODE_PARAMS + MODE_ANCHOR, 0.0f, 1.0f, 0.0f, "Edit note octave refs", "");
		configParam(MODE_PARAMS + MODE_RANGE, 0.0f, 1.0f, 0.0f, "Edit octave range", "");
		configParam(DENSITY_PARAM, 0.0f, 1.0f, 1.0f, "Density", " %", 0.0f, 100.0f, 0.0f);
		configParam(COPY_PARAM, 0.0f, 1.0f, 0.0f, "Copy keyboard values");
		configParam(PASTE_PARAM, 0.0f, 1.0f, 0.0f, "Paste keyboard values");
		configParam(TR_UP_PARAM, 0.0f, 1.0f, 0.0f, "Transpose up 1 semitone");
		configParam(TR_DOWN_PARAM, 0.0f, 1.0f, 0.0f, "Transpose down 1 semitone");
		
		pkInfo.showMarks = 1;
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}


	void onReset() override {
		editMode = MODE_PROB;
		overlap = 0.5f;// must be 0 to 1
		indexCvCap12 = 0;
		showTracer = 1;
		for (int i = 0; i < NUM_INDEXES; i++) {
			probKernels[i].reset();
		}
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			outputKernels[i].reset();
		}
		resetNonJson();
	}
	void resetNonJson() {
		dispManager.reset();
		infoTracer = 0;
	}


	void onRandomize() override {
		// only randomize the lock buffers
		int index = getIndex();
		int length0 = getLength0();

		for (int c = 0; c < inputs[GATE_INPUT].getChannels(); c ++) {
			float offset = getOffset(c);
			float squash = getSquash(c);
			float density = getDensity(c);
			for (int i = 0; i < OutputKernel::MAX_LENGTH; i++) {
				float newCv = probKernels[index].calcRandomCv(offset, squash, density, overlap);
				outputKernels[c].shiftWithInsertNew(newCv, length0);
			}
		}
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// editMode
		json_object_set_new(rootJ, "editMode", json_integer(editMode));

		// overlap
		json_object_set_new(rootJ, "overlap", json_real(overlap));

		// indexCvCap12
		json_object_set_new(rootJ, "indexCvCap12", json_integer(indexCvCap12));

		// showTracer
		json_object_set_new(rootJ, "showTracer", json_integer(showTracer));

		// probKernels
		json_t* probsJ = json_array();
		for (size_t i = 0; i < NUM_INDEXES; i++) {
			json_t* probJ = probKernels[i].dataToJsonProb();
			json_array_insert_new(probsJ, i , probJ);
		}
		json_object_set_new(rootJ, "probKernels", probsJ);

		
		// outputKernels
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			outputKernels[i].dataToJson(rootJ, i);
		}
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) {
			panelTheme = json_integer_value(panelThemeJ);
		}

		// editMode
		json_t *editModeJ = json_object_get(rootJ, "editMode");
		if (editModeJ) {
			editMode = json_integer_value(editModeJ);
		}

		// overlap
		json_t *overlapJ = json_object_get(rootJ, "overlap");
		if (overlapJ) {
			overlap = json_number_value(overlapJ);
		}

		// indexCvCap12
		json_t *indexCvCap12J = json_object_get(rootJ, "indexCvCap12");
		if (indexCvCap12J) {
			indexCvCap12 = json_integer_value(indexCvCap12J);
		}

		// showTracer
		json_t *showTracerJ = json_object_get(rootJ, "showTracer");
		if (showTracerJ) {
			showTracer = json_integer_value(showTracerJ);
		}

		// probKernels
		json_t* probsJ = json_object_get(rootJ, "probKernels");
		if (probsJ && json_is_array(probsJ)) {
			for (size_t i = 0; i < std::min((size_t)NUM_INDEXES, json_array_size(probsJ)); i++) {
				json_t* probJ = json_array_get(probsJ, i);
				probKernels[i].dataFromJsonProb(probJ);
			}
		}

		// outputKernels
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			outputKernels[i].dataFromJson(rootJ, i);
		}

		resetNonJson();
	}
		

	IoStep* fillIoSteps(int *seqLenPtr) {// caller must delete return array
		int seqLen = getLength0() + 1;
		IoStep* ioSteps = new IoStep[seqLen];
		
		// Populate ioSteps array
		// must read outputKernel register backwards!, so all calls to getReg() should be mirrored
		float lastCv = 0.0f;
		for (int i = 0; i < seqLen; i++) {
			float cv = outputKernels[0].getReg(seqLen - 1 - i);
			if (cv == ProbKernel::IDEM_CV) {
				ioSteps[i].pitch = lastCv;// don't care if init value of 0.0f is used when no gate encountered yet, will have no effect
				ioSteps[i].gate = false;
			}
			else {
				ioSteps[i].pitch = cv;
				lastCv = cv;
				ioSteps[i].gate = true;
			}
			ioSteps[i].tied = false;
			ioSteps[i].vel = -1.0f;// not relevant in ProbKey
			ioSteps[i].prob = -1.0f;// not relevant in ProbKey
		}
		
		// return values 
		*seqLenPtr = seqLen;
		return ioSteps;
	}
	
	
	void emptyIoSteps(IoStep* ioSteps, int seqLen) {
		params[LENGTH_PARAM].setValue(seqLen - 1);
		
		// Populate steps in the sequencer
		// must write outputKernel register backwards!, so all calls to setReg() should be mirrored
		for (int i = 0; i < OutputKernel::MAX_LENGTH; i++) {
			outputKernels[0].setReg(ProbKernel::IDEM_CV, seqLen - 1 - i);
		}
		for (int i = 0; i < seqLen; i++) {
			if (ioSteps[i].gate) {
				outputKernels[0].setReg(ioSteps[i].pitch, seqLen - 1 - i);
			}
		}
	}

	
	void process(const ProcessArgs &args) override {		
		int index = getIndex();
		int length0 = getLength0();
		
		//********** Buttons, knobs, switches and inputs **********
		
		if (refresh.processInputs()) {
			// poly cable sizes
			outputs[CV_OUTPUT].setChannels(inputs[GATE_INPUT].getChannels());
			outputs[GATE_OUTPUT].setChannels(inputs[GATE_INPUT].getChannels());
			
			// mode led-buttons
			for (int i = 0; i < 3; i++) {
				if (modeTriggers[i].process(params[MODE_PARAMS + i].getValue())) {
					editMode = i;
					dispManager.reset();
				}
			}
			
			// copy button
			if (copyTrigger.process(params[COPY_PARAM].getValue())) {
				dispManager.displayCopy();
				// Clipboard version: 
				json_t* probJ = probKernels[index].dataToJsonProb();
				json_t* clipboardJ = json_object();		
				json_object_set_new(clipboardJ, "Impromptu ProbKey keyboard values", probJ);
				char* probClip = json_dumps(clipboardJ, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
				json_decref(clipboardJ);
				glfwSetClipboardString(APP->window->win, probClip);
				free(probClip);
			}	
			// paste button
			if (pasteTrigger.process(params[PASTE_PARAM].getValue())) {
				dispManager.displayPaste();
				// Clipboard version: 
				const char* probClip = glfwGetClipboardString(APP->window->win);
				if (!probClip) {
					WARN("IOP error getting clipboard string");
				}
				else {
					json_error_t error;
					json_t* clipboardJ = json_loads(probClip, 0, &error);
					if (!clipboardJ) {
						WARN("IOP error json parsing clipboard");
					}
					else {
						DEFER({json_decref(clipboardJ);});
						json_t* probJ = json_object_get(clipboardJ, "Impromptu ProbKey keyboard values");
						if (!probJ) {
							WARN("IOP error no Impromptu ProbKey keyboard values present in clipboard");
						}
						else {
							probKernels[index].dataFromJsonProb(probJ);
						}
					}	
				}
			}	
			
			// transpose buttons
			if (tranUpTrigger.process(params[TR_UP_PARAM].getValue())) {
				if (editMode == MODE_PROB || editMode == MODE_ANCHOR) {
					probKernels[index].transposeUp();
				}
			}
			if (tranDownTrigger.process(params[TR_DOWN_PARAM].getValue())) {
				if (editMode == MODE_PROB || editMode == MODE_ANCHOR) {
					probKernels[index].transposeDown();
				}
			}
			
			// piano keys if applicable 
			if (pkInfo.gate && !pkInfo.isRightClick) {
				bool withSymmetry = (APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
				if (editMode == MODE_PROB) {
					float prob = 1.0f - pkInfo.vel;
					probKernels[index].setNoteProb(pkInfo.key, prob, withSymmetry);
					dispManager.displayProb(prob);
				}
				else if (editMode == MODE_ANCHOR) {
					if (probKernels[index].isProbNonNull(pkInfo.key)) {
						float anchor = 1.0f - pkInfo.vel;
						probKernels[index].setNoteAnchor(pkInfo.key, anchor, withSymmetry);
						int oct = (int)ProbKernel::anchorToOct(anchor);
						dispManager.displayAnchor(pkInfo.key, oct);
					}
				}
				else {// editMode == MODE_RANGE
					if (pkInfo.key != 1 && pkInfo.key != 3 && pkInfo.key != 6 && pkInfo.key != 8 && pkInfo.key != 10) {
						probKernels[index].setNoteRange(pkInfo.key, 1.0f - pkInfo.vel, withSymmetry);
					}
					
				}
			}
		}// userInputs refresh


		
		
		//********** Outputs and lights **********
		
		for (int c = 0; c < inputs[GATE_INPUT].getChannels(); c ++) {
			// gate input triggers
			if (gateInTriggers[c].process(inputs[GATE_INPUT].getVoltage(c))) {
				// got rising edge on gate input poly channel c

				if (getLock() > random::uniform()) {
					// recycle CV
					outputKernels[c].shiftWithRecycle(length0);
				}
				else {
					// generate new random CV (taking hold into account though)
					bool hold = inputs[HOLD_INPUT].isConnected() && inputs[HOLD_INPUT].getVoltage(std::min(c, inputs[HOLD_INPUT].getChannels())) > 1.0f;
					if (hold) {
						outputKernels[c].shiftWithHold(length0);
					}
					else {
						float newCv = probKernels[index].calcRandomCv(getOffset(c), getSquash(c), getDensity(c), overlap);
						outputKernels[c].shiftWithInsertNew(newCv, length0);
					}
				}
				
				if (c == 0) {
					infoTracer = (long) (infoTracerTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
				}					
			}
			
			
			// output CV and gate
			outputs[CV_OUTPUT].setVoltage(outputKernels[c].getCv(), c);
			float gateOut = outputKernels[c].getGateEnable() && gateInTriggers[c].state ? inputs[GATE_INPUT].getVoltage(c) : 0.0f;
			outputs[GATE_OUTPUT].setVoltage(gateOut, c);
		}


		
		// lights
		if (refresh.processLights()) {
			// mode lights (green, red)
			lights[MODE_LIGHTS + MODE_PROB * 2 + 0].setBrightness(editMode == MODE_PROB ? 1.0f : 0.0f);
			lights[MODE_LIGHTS + MODE_PROB * 2 + 1].setBrightness(0.0f);
			lights[MODE_LIGHTS + MODE_ANCHOR * 2 + 0].setBrightness(0.0f);
			lights[MODE_LIGHTS + MODE_ANCHOR * 2 + 1].setBrightness(editMode == MODE_ANCHOR ? 1.0f : 0.0f);
			lights[MODE_LIGHTS + MODE_RANGE * 2 + 0].setBrightness(editMode == MODE_RANGE ? 1.0f : 0.0f);
			lights[MODE_LIGHTS + MODE_RANGE * 2 + 1].setBrightness(editMode == MODE_RANGE ? 1.0f : 0.0f);
			
			// keyboard lights (green, red)
			int tracer = -1;
			if (showTracer != 0 && infoTracer > 0) {// inputs[GATE_INPUT].getVoltage(0) >= 1.0f) {
				float tcv = outputKernels[0].getCv();
				tracer = (int)std::round(tcv * 12.0f);
				tracer = eucMod(tracer, 12);
			}
			if (editMode == MODE_PROB) {
				for (int i = 0; i < 12; i++) {
					setKeyLightsProb(i, probKernels[index].getNoteProb(i), tracer == i);
				}
			}
			else if (editMode == MODE_ANCHOR) {
				for (int i = 0; i < 12; i++) {
					setKeyLightsAnchor(i, probKernels[index].getNoteAnchor(i), probKernels[index].isProbNonNull(i), tracer == i);
				}
			}
			else {
				setKeyLightsOctRange(index);
			}
			
			// lock led
			lights[LOCK_LIGHT].setBrightness(getLock());
			
			// density led
			float cumulProb = probKernels[index].getCumulProbTotal();
			float density = getDensity(0);
			float prod = std::min(cumulProb, 1.0f) * density;
			if (prod >= 1.0f) {
				lights[DENSITY_LIGHT + 0].setBrightness(1.0f);// green
				lights[DENSITY_LIGHT + 1].setBrightness(0.0f);// red
			}
			else {
				lights[DENSITY_LIGHT + 0].setBrightness(0.0f);// green
				lights[DENSITY_LIGHT + 1].setBrightness(1.0f);// red
			}
			
			// other stuff
			if (infoTracer > 0l) {
				infoTracer--;
			}
			dispManager.process();
		}// processLights()
		
		// To Expander
		if (rightExpander.module && rightExpander.module->model == modelProbKeyExpander) {
			PkxInterface *messagesToExpander = (PkxInterface*)(rightExpander.module->leftExpander.producerMessage);
			messagesToExpander->panelTheme = panelTheme;
			messagesToExpander->minCvChan0 = outputKernels[0].getMinCv();
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}
	}
	
	void setKeyLightsProb(int key, float prob, bool tracer) {
		for (int j = 0; j < 4; j++) {// 0 to 3 is bottom to top
			lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 0].setBrightness((tracer && (j == 3)) ? 0.0f : (prob * 4.0f - (float)j));
			lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 1].setBrightness(0.0f);
			lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 2].setBrightness((tracer && (j == 3)) ? 1.0f : 0.0f);
		}
	}
	void setKeyLightsAnchor(int key, float anch, bool active, bool tracer) {
		if (active) {
			anch = ProbKernel::quantizeAnchor(anch);
			for (int j = 0; j < 4; j++) {
				lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 0].setBrightness(0.0f);
				lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 1].setBrightness((tracer && (j == 3)) ? 0.0f : (anch * 4.0f - (float)j));
				lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 2].setBrightness((tracer && (j == 3)) ? 1.0f : 0.0f);
			}
		}
		else {
			for (int j = 0; j < 4 * 3; j++) {
				lights[KEY_LIGHTS + key * 4 * 3 + j].setBrightness(0.0f);
			}
		}
	}
	void setKeyLightsOctRange(int index) {
		float modRanges[7] = {};
		probKernels[index].calcOffsetAndSquash(modRanges, getOffset(0), getSquash(0), overlap);
		
		for (int i = 0; i < 12; i++) {
			if (i != 1 && i != 3 && i != 6 && i != 8 && i != 10) {
				for (int j = 0; j < 4; j++) {
					int i7 = ProbKernel::key12to7(i);
					float normalRange = probKernels[index].getNoteRange(i);
					lights[KEY_LIGHTS + i * 4 * 3 + j * 3 + 0].setBrightness(normalRange   * 4.0f - (float)j);
					lights[KEY_LIGHTS + i * 4 * 3 + j * 3 + 1].setBrightness(modRanges[i7] * 4.0f - (float)j);
					lights[KEY_LIGHTS + i * 4 * 3 + j * 3 + 2].setBrightness(0.0f);				
				}
			}
			else {
				for (int j = 0; j < 4 * 3; j++) {
					lights[KEY_LIGHTS + i * 4 * 3 + j].setBrightness(0.0f);
				}
			}
		}
	}
};




struct ProbKeyWidget : ModuleWidget {
	SvgPanel* darkPanel;

	
	struct PanelThemeItem : MenuItem {
		ProbKey *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	
	struct IndexCvCap12Item : MenuItem {
		ProbKey *module;
		void onAction(const event::Action &e) override {
			module->indexCvCap12 ^= 0x1;
		}
	};
	
	struct ShowTracerItem : MenuItem {
		ProbKey *module;
		void onAction(const event::Action &e) override {
			module->showTracer ^= 0x1;
		}
	};
	
	struct OverlapQuantity : Quantity {
		float *overlapSrc = NULL;
		  
		OverlapQuantity(float *_overlapSrc) {
			overlapSrc = _overlapSrc;
		}
		void setValue(float value) override {
			*overlapSrc = math::clamp(value, getMinValue(), getMaxValue());
		}
		float getValue() override {
			return *overlapSrc;
		}
		float getMinValue() override {return 0.0f;}
		float getMaxValue() override {return 1.0f;}
		float getDefaultValue() override {return 0.25f;}
		float getDisplayValue() override {return getValue();}
		std::string getDisplayValueString() override {
			return string::f("%.1f", getDisplayValue() * 100.0f);
		}
		void setDisplayValue(float displayValue) override {setValue(displayValue);}
		std::string getLabel() override {return "Squash overlap";}
		std::string getUnit() override {
			return " %";
		}
	};
	struct OverlapSlider : ui::Slider {
		OverlapSlider(float *_overlapSrc) {
			quantity = new OverlapQuantity(_overlapSrc);
		}
		~OverlapSlider() {
			delete quantity;
		}
	};

	struct MainDisplayWidget : LightWidget {
		ProbKey *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		
		MainDisplayWidget() {
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void draw(const DrawArgs &args) override {
			if (!(font = APP->window->loadFont(fontPath))) {
				return;
			}
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			//nvgTextLetterSpacing(args.vg, 2.5);

			Vec textPos = VecPx(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~~~~", NULL);
			nvgFillColor(args.vg, textColor);
			char displayStr[5];
			if (module) {
				if (module->dispManager.getMode() == DisplayManager::DISP_NORMAL) {
					if (module->indexCvCap12 != 0) {
						snprintf(displayStr, 5, "*%3u", module->getIndex() + 1);
					}
					else {
						snprintf(displayStr, 5, "%4u", module->getIndex() + 1);
					}
				}
				else if (module->dispManager.getMode() == DisplayManager::DISP_LENGTH) {
					snprintf(displayStr, 5, " L%2u", module->getLength0() + 1);
				}
				else {
					memcpy(displayStr, module->dispManager.getText(), 5);
				}
			}
			else {
				snprintf(displayStr, 5, "1");
			}
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};
	
	
	struct LengthKnob : IMMediumKnob<false, true> {
		DisplayManager* dispManagerSrc = NULL;
		
		void onDragMove(const event::DragMove& e) override {
			IMMediumKnob::onDragMove(e);
			dispManagerSrc->displayLength();
		}
	};


	struct InteropSeqItem : MenuItem {
		struct InteropCopySeqItem : MenuItem {
			ProbKey *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = module->fillIoSteps(&seqLen);
				interopCopySequence(seqLen, ioSteps);
				delete[] ioSteps;
			}
		};
		struct InteropPasteSeqItem : MenuItem {
			ProbKey *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = interopPasteSequence(OutputKernel::MAX_LENGTH, &seqLen);
				if (ioSteps != nullptr) {
					module->emptyIoSteps(ioSteps, seqLen);
					delete[] ioSteps;
				}
			}
		};
		ProbKey *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			InteropCopySeqItem *interopCopySeqItem = createMenuItem<InteropCopySeqItem>(portableSequenceCopyID, "");
			interopCopySeqItem->module = module;
			interopCopySeqItem->disabled = disabled;
			menu->addChild(interopCopySeqItem);		
			
			InteropPasteSeqItem *interopPasteSeqItem = createMenuItem<InteropPasteSeqItem>(portableSequencePasteID, "");
			interopPasteSeqItem->module = module;
			interopPasteSeqItem->disabled = disabled;
			menu->addChild(interopPasteSeqItem);		

			return menu;
		}
	};	
	
	
	void appendContextMenu(Menu *menu) override {
		ProbKey *module = dynamic_cast<ProbKey*>(this->module);
		assert(module);
				
		InteropSeqItem *interopSeqItem = createMenuItem<InteropSeqItem>(portableSequenceID, RIGHT_ARROW);
		interopSeqItem->module = module;
		menu->addChild(interopSeqItem);		

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *darkItem = createMenuItem<PanelThemeItem>(darkPanelID, CHECKMARK(module->panelTheme));
		darkItem->module = module;
		menu->addChild(darkItem);
		
		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));
		
		// menu->addChild(new MenuLabel());// empty line
		
		// MenuLabel *actionsLabel = new MenuLabel();
		// actionsLabel->text = "Actions";
		// menu->addChild(actionsLabel);

		// todo

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);

		OverlapSlider *ovlpSlider = new OverlapSlider(&(module->overlap));
		ovlpSlider->box.size.x = 200.0f;
		menu->addChild(ovlpSlider);
		
		IndexCvCap12Item *cv12Item = createMenuItem<IndexCvCap12Item>("Index mode 12", CHECKMARK(module->indexCvCap12));
		cv12Item->module = module;
		menu->addChild(cv12Item);
		
		ShowTracerItem *tracerItem = createMenuItem<ShowTracerItem>("Show generated note", CHECKMARK(module->showTracer));
		tracerItem->module = module;
		menu->addChild(tracerItem);
		
	}	
	
	
	ProbKeyWidget(ProbKey *module) {
		setModule(module);
		
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/ProbKey.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/ProbKey_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), module ? &module->panelTheme : NULL));



		// ****** Top portion (keys) ******

		static const float ex = 15.0f - 3.0f / 5.08f * 15.0f;
		static const float olx = 16.7f;
		static const float dly = 70.0f / 4.0f;
		static const float dlyd2 = 70.0f / 8.0f;
		
		static const float posWhiteY = 115;
		static const float posBlackY = 40.0f;


		#define DROP_LIGHTS(xLoc, yLoc, pNum) \
			addChild(createLightCentered<SmallLight<GreenRedWhiteLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*3), module, ProbKey::KEY_LIGHTS + pNum * (4 * 3) + 0 * 3)); \
			addChild(createLightCentered<SmallLight<GreenRedWhiteLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*2), module, ProbKey::KEY_LIGHTS + pNum * (4 * 3) + 1 * 3)); \
			addChild(createLightCentered<SmallLight<GreenRedWhiteLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*1), module, ProbKey::KEY_LIGHTS + pNum * (4 * 3) + 2 * 3)); \
			addChild(createLightCentered<SmallLight<GreenRedWhiteLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*0), module, ProbKey::KEY_LIGHTS + pNum * (4 * 3) + 3 * 3));

		// Black keys
		addChild(createPianoKey<PianoKeyBig>(VecPx(37.5f + ex, posBlackY), 1, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(37.5f, posBlackY, 1);
		addChild(createPianoKey<PianoKeyBig>(VecPx(78.5f + ex, posBlackY), 3, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(78.5f, posBlackY, 3);
		addChild(createPianoKey<PianoKeyBig>(VecPx(161.5f + ex, posBlackY), 6, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(161.5f, posBlackY, 6);
		addChild(createPianoKey<PianoKeyBig>(VecPx(202.5f + ex, posBlackY), 8, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(202.5f, posBlackY, 8);
		addChild(createPianoKey<PianoKeyBig>(VecPx(243.5f + ex, posBlackY), 10, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(243.5f, posBlackY, 10);

		// White keys
		addChild(createPianoKey<PianoKeyBig>(VecPx(17.5f + ex, posWhiteY), 0, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(17.5f, posWhiteY, 0);
		addChild(createPianoKey<PianoKeyBig>(VecPx(58.5f + ex, posWhiteY), 2, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(58.5f, posWhiteY, 2);
		addChild(createPianoKey<PianoKeyBig>(VecPx(99.5f + ex, posWhiteY), 4, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(99.5f, posWhiteY, 4);
		addChild(createPianoKey<PianoKeyBig>(VecPx(140.5f + ex, posWhiteY), 5, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(140.5f, posWhiteY, 5);
		addChild(createPianoKey<PianoKeyBig>(VecPx(181.5f + ex, posWhiteY), 7, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(181.5f, posWhiteY, 7);
		addChild(createPianoKey<PianoKeyBig>(VecPx(222.5f + ex, posWhiteY), 9, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(222.5f, posWhiteY, 9);
		addChild(createPianoKey<PianoKeyBig>(VecPx(263.5f + ex, posWhiteY), 11, module ? &module->pkInfo : NULL));
		DROP_LIGHTS(263.5f, posWhiteY, 11);


		
		// ****** Bottom portion ******
		
		static const float row0 = 82.5f;
		static const float row1 = 96.5f;
		static const float row2 = 114.0f;
		
		static const float coldx = 17.0f;
		static const float bigdx = 2.0f;// for big knob
		static const float col0 = 11.0f;
		static const float col1 = col0 + coldx;
		static const float col2 = col1 + coldx;
		static const float col3 = col2 + coldx;
		static const float col4 = col3 + coldx + bigdx;
		static const float col5 = col4 + coldx + bigdx;
		static const float col6 = col5 + coldx;

		
		
		// **** col0 ****
		
		// Index knob and input
		addParam(createDynamicParamCentered<IMMediumKnob<false, true>>(mm2px(Vec(col0, row0)), module, ProbKey::INDEX_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, row1)), true, module, ProbKey::INDEX_INPUT, module ? &module->panelTheme : NULL));

		// Gate input
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, row2)), true, module, ProbKey::GATE_INPUT, module ? &module->panelTheme : NULL));
	

		// **** col1 ****

		// density knob, led and input
		addParam(createDynamicParamCentered<IMMediumKnob<false, false>>(mm2px(Vec(col1, row0)), module, ProbKey::DENSITY_PARAM, module ? &module->panelTheme : NULL));	
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(col1 - 4.5f, row1 - 6.3f)), module, ProbKey::DENSITY_LIGHT));			
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row1)), true, module, ProbKey::DENSITY_INPUT, module ? &module->panelTheme : NULL));

		// Hold input
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row2)), true, module, ProbKey::HOLD_INPUT, module ? &module->panelTheme : NULL));


		// **** col2 ****

		// Squash knob and input
		addParam(createDynamicParamCentered<IMMediumKnob<false, false>>(mm2px(Vec(col2, row0)), module, ProbKey::SQUASH_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2, row1)), true, module, ProbKey::SQUASH_INPUT, module ? &module->panelTheme : NULL));

		// Main display
		MainDisplayWidget *displayMain = new MainDisplayWidget();
		displayMain->box.size = VecPx(71, 30);// 4 characters
		displayMain->box.pos = mm2px(Vec((col2 + col3) * 0.5f, row2 - 2.0f)).minus(displayMain->box.size.div(2));
		displayMain->module = module;
		addChild(displayMain);


		// **** col3 ****

		// Offset knob and input
		addParam(createDynamicParamCentered<IMMediumKnob<false, false>>(mm2px(Vec(col3, row0)), module, ProbKey::OFFSET_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col3, row1)), true, module, ProbKey::OFFSET_INPUT, module ? &module->panelTheme : NULL));


		// **** col4 ****

		// Lock knob, led, button and input
		addParam(createDynamicParamCentered<IMBigKnob<false, false>>(mm2px(Vec(col4, row0 + 2.0f)), module, ProbKey::LOCK_KNOB_PARAM, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(col4 - 5.0f, row1 + 4.0f - 6.8f)), module, ProbKey::LOCK_LIGHT));	
		addParam(createDynamicParamCentered<IMBigPushButton>(mm2px(Vec(col4, row1 + 4.0f)), module, ProbKey::LOCK_BUTTON_PARAM, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col4, row2)), true, module, ProbKey::LOCK_INPUT, module ? &module->panelTheme : NULL));


		// **** col5 ****

		// Length knob and input
		LengthKnob* lengthKnob = createDynamicParamCentered<LengthKnob>(mm2px(Vec(col5, row0)), module, ProbKey::LENGTH_PARAM, module ? &module->panelTheme : NULL);
		addParam(lengthKnob);
		if (module) {
			lengthKnob->dispManagerSrc = &(module->dispManager);
		}
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col5, row1)), true, module, ProbKey::LENGTH_INPUT, module ? &module->panelTheme : NULL));

		// CV output
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col5, row2)), false, module, ProbKey::CV_OUTPUT, module ? &module->panelTheme : NULL));


		// **** col6 ****
		static const float r6dy = 13.0f;
		static const float rowm1 = 67.2f;
		
		// Transpose buttons
		addParam(createDynamicParamCentered<IMPushButton>(mm2px(Vec(col6, rowm1 - 4.0f * r6dy)), module, ProbKey::TR_UP_PARAM, module ? &module->panelTheme : NULL));		
		addParam(createDynamicParamCentered<IMPushButton>(mm2px(Vec(col6, rowm1 - 3.0f * r6dy - 1.0f)), module, ProbKey::TR_DOWN_PARAM, module ? &module->panelTheme : NULL));		

		// Mode led-button - MODE_PROB
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col6, rowm1 - 2.0f * r6dy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_PROB));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(col6, rowm1 - 2.0f * r6dy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_PROB * 2));

		// Mode led-button - MODE_ANCHOR
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col6, rowm1 - 1.0f * r6dy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_ANCHOR));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(col6, rowm1 - 1.0f * r6dy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_ANCHOR * 2));

		// Mode led-button - MODE_RANGE
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col6, rowm1)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_RANGE));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(col6, rowm1)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_RANGE * 2));

		// Copy-paste buttons
		addParam(createDynamicParamCentered<IMPushButton>(mm2px(Vec(col6, row0)), module, ProbKey::COPY_PARAM, module ? &module->panelTheme : NULL));		
		addParam(createDynamicParamCentered<IMPushButton>(mm2px(Vec(col6, row1)), module, ProbKey::PASTE_PARAM, module ? &module->panelTheme : NULL));		

		// Gate output
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col6, row2)), false, module, ProbKey::GATE_OUTPUT, module ? &module->panelTheme : NULL));
	}
	
	
	void step() override {
		if (module) {
			panel->visible = ((((ProbKey*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((ProbKey*)module)->panelTheme) == 1);
		}
		Widget::step();
	}

};

Model *modelProbKey = createModel<ProbKey, ProbKeyWidget>("Prob-Key");
