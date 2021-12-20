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


#include "comp/PianoKey.hpp"
#include "Interop.hpp"



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

	static const int MAX_LENGTH = 32;// if this becomes bigger, will need to change bitfield types for ProbKey::stepLock and ProbKey::stepLocks
	
	
	private:
	
	float buf[MAX_LENGTH];// holds CVs or ProbKernel::IDEM_CV
	float lastCv;// sample and hold, must never hold ProbKernel::IDEM_CV
	int step;
	
	
	public:
	
	void reset() {
		for (int i = 0; i < MAX_LENGTH; i++) {
			buf[i] = 0.0f;
		}
		lastCv = 0.0f;
		step = 0;
	}
	
	void randomize() {
		// not done here
	}
	
	void dataToJson(json_t *rootJ, int id) {
		// buf
		json_t *bufJ = json_array();
		for (int i = 0; i < MAX_LENGTH; i++) {
			json_array_insert_new(bufJ, i, json_real(buf[i]));
		}
		json_object_set_new(rootJ, string::f("buf%i", id).c_str(), bufJ);
		
		// lastCv
		json_object_set_new(rootJ, string::f("lastCv%i", id).c_str(), json_real(lastCv));

		// step
		json_object_set_new(rootJ, string::f("step%i", id).c_str(), json_integer(step));
	}
	
	void dataFromJson(json_t *rootJ, int id) {
		// buf
		json_t *bufJ = json_object_get(rootJ, string::f("buf%i", id).c_str());
		if (bufJ) {
			for (int i = 0; i < MAX_LENGTH; i++) {
				json_t *bufArrayJ = json_array_get(bufJ, i);
				if (bufArrayJ) {
					buf[i] = json_number_value(bufArrayJ);
				}
			}
		}

		// lastCv
		json_t *lastCvJ = json_object_get(rootJ, string::f("lastCv%i", id).c_str());
		if (lastCvJ) {
			lastCv = json_number_value(lastCvJ);
		}

		// step
		json_t *stepJ = json_object_get(rootJ, "step");
		if (stepJ) {
			step = json_integer_value(stepJ);
		}

	}


	void stepWithKeepOld(int length) {
		// minCv unaffected
		step = (step + 1) % length;
		if (buf[step] != ProbKernel::IDEM_CV) {
			lastCv = buf[step];
		}
	}
	
	void stepWithCopy(int length) {
		// lastCv unaffected
		float currCv = buf[step];
		step = (step + 1) % length;
		buf[step] = currCv;
	}
		
	void stepWithInsertNew(float newCv, int length) {
		stepWithCopy(length);
		buf[step] = newCv;
		if (buf[step] != ProbKernel::IDEM_CV) {
			lastCv = buf[step];
		}
	}
		
	float getCv() {
		return lastCv;
	}
	float getBuf(int i) {
		return buf[i];
	}
	void setBuf(float cv, int i) {
		buf[i] = cv;
	}
	bool getGateEnable() {
		return buf[step] != ProbKernel::IDEM_CV;
	}
	int getNextStep(int length) {
		return (step + 1) % length;
	}
	void resetPlayHead() {
		step = 0;
		lastCv = buf[0];
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
	// none
		
	// Constants
	enum ModeIds {MODE_PROB, MODE_ANCHOR, MODE_RANGE};
	static const int NUM_INDEXES = 25;// C4 to C6 incl
	static constexpr float infoTracerTime = 3.0f;// seconds
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	int editMode;
	float overlap;
	int indexCvCap12;
	int showTracer;
	int perIndexManualLocks;// this really is two distinct modes and different stepLock bitfields are used below
	uint32_t stepLock;// bit 0 is step 0 in output kernel, etc. (applies to poly chan 0 only and is not per index)
	uint32_t stepLocks[NUM_INDEXES];// stepLock when perIndexManualLocks (applies to poly chan 0 only and is per index)
	float stepLocksCvs[NUM_INDEXES][OutputKernel::MAX_LENGTH];// only save active members as indicated by stepLocks bitfields
	ProbKernel probKernels[NUM_INDEXES];
	OutputKernel outputKernels[PORT_MAX_CHANNELS];
	
	// No need to save, with reset
	DisplayManager dispManager;
	long infoTracer;
	bool infoTracerLockedStep;// only valid when infoTracer != 0

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
	int getLength() {// 1 indexed
		int length0 = (int)std::round(params[LENGTH_PARAM].getValue() + inputs[LENGTH_INPUT].getVoltage() * 1.6f);
		return clamp(length0, 0, OutputKernel::MAX_LENGTH - 1 ) + 1;
	}
	bool getStepLock(uint32_t s, int i) {
		if (perIndexManualLocks != 0) {
			return (stepLocks[i] & (0x1ul << s)) != 0ul;
		}
		return (stepLock & (0x1ul << s)) != 0ul;
	}
	void toggleStepLock(uint32_t s, int i) {
		if (perIndexManualLocks != 0) {
			stepLocks[i] ^= (0x1ul << s);
			if (getStepLock(s, i)) {
				stepLocksCvs[i][s] = outputKernels[0].getBuf(s);
			}
		}
		else {
			stepLock ^= (0x1ul << s);
		}
	}
	void setStepLocksForAllActive(int i) {
		for (int s = 0; s < getLength(); s++) {
			float cv = outputKernels[0].getBuf(s);
			if (cv != ProbKernel::IDEM_CV && !getStepLock(s, i)) {
				toggleStepLock(s, i);
			}
		}
	}
	void clearStepLock() {
		stepLock = 0ul;
	}
	void clearStepLocks(int i) {
		stepLocks[i] = 0ul;
	}


	ProbKey() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(INDEX_PARAM, 0.0f, 24.0f, 0.0f, "Index", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset
		paramQuantities[INDEX_PARAM]->snapEnabled = true;
		configParam(LENGTH_PARAM, 0.0f, (float)(OutputKernel::MAX_LENGTH - 1), (float)(OutputKernel::MAX_LENGTH - 1), "Lock length", "", 0.0f, 1.0f, 1.0f);
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
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
		
		getParamQuantity(LOCK_KNOB_PARAM)->randomizeEnabled = false;		
		getParamQuantity(LENGTH_PARAM)->randomizeEnabled = false;		
		getParamQuantity(INDEX_PARAM)->randomizeEnabled = false;		
		getParamQuantity(DENSITY_PARAM)->randomizeEnabled = false;		
		getParamQuantity(SQUASH_PARAM)->randomizeEnabled = false;		
		getParamQuantity(OFFSET_PARAM)->randomizeEnabled = false;		
		
		configInput(INDEX_INPUT, "Index");
		configInput(LENGTH_INPUT, "Length");
		configInput(LOCK_INPUT, "Lock");
		configInput(OFFSET_INPUT, "Offset");
		configInput(SQUASH_INPUT, "Squash");
		configInput(GATE_INPUT, "Gate");
		configInput(HOLD_INPUT, "Hold");
		configInput(DENSITY_INPUT, "Density");

		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV_OUTPUT, "CV");

		pkInfo.showMarks = 1;
		
		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	void onReset() override {
		editMode = MODE_PROB;
		overlap = 0.5f;// must be 0 to 1
		indexCvCap12 = 0;
		showTracer = 1;
		perIndexManualLocks = 0;
		clearStepLock(); // this is stepLock;
		for (int i = 0; i < NUM_INDEXES; i++) {
			probKernels[i].reset();
			clearStepLocks(i); // this is stepLocks[];
		}
		for (int i = 0; i < NUM_INDEXES; i++) {
			for (int j = 0; j < OutputKernel::MAX_LENGTH; j++) {
				stepLocksCvs[i][j] = 0.0f;
			}
		}
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			outputKernels[i].reset();
		}
		resetNonJson();
	}
	void resetNonJson() {
		dispManager.reset();
		infoTracer = 0;
		infoTracerLockedStep = false;
	}


	void onRandomize() override {
		// only randomize the lock buffers
		int index = getIndex();

		for (int c = 0; c < inputs[GATE_INPUT].getChannels(); c ++) {
			float offset = getOffset(c);
			float squash = getSquash(c);
			float density = getDensity(c);
			for (int i = 0; i < OutputKernel::MAX_LENGTH; i++) {
				float newCv = probKernels[index].calcRandomCv(offset, squash, density, overlap);
				outputKernels[c].stepWithInsertNew(newCv, OutputKernel::MAX_LENGTH - 1);
			}
		}
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// editMode
		json_object_set_new(rootJ, "editMode", json_integer(editMode));

		// overlap
		json_object_set_new(rootJ, "overlap", json_real(overlap));

		// indexCvCap12
		json_object_set_new(rootJ, "indexCvCap12", json_integer(indexCvCap12));

		// showTracer
		json_object_set_new(rootJ, "showTracer", json_integer(showTracer));

		// perIndexManualLocks
		json_object_set_new(rootJ, "perIndexManualLocks", json_integer(perIndexManualLocks));

		// stepLock
		json_object_set_new(rootJ, "stepLock", json_integer(stepLock));

		// stepLocks
		json_t *stepLocksJ = json_array();
		for (int i = 0; i < NUM_INDEXES; i++) {
			json_array_insert_new(stepLocksJ, i, json_integer(stepLocks[i]));
		}
		json_object_set_new(rootJ, "stepLocks", stepLocksJ);

		// stepLocksCvs
		json_t *stepLocksCvsJ = json_array();
		for (int i = 0; i < NUM_INDEXES; i++) {
			if (stepLocks[i] != 0x0) {
				for (int j = 0; j < OutputKernel::MAX_LENGTH; j++) {
					if ( ((stepLocks[i] >> j) & 0x1) != 0) {
						json_array_append_new(stepLocksCvsJ, json_real(stepLocksCvs[i][j]));
					}
				}
			}
		}
		json_object_set_new(rootJ, "stepLocksCvs", stepLocksCvsJ);
		
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

		// panelContrast
		json_t *panelContrastJ = json_object_get(rootJ, "panelContrast");
		if (panelContrastJ)
			panelContrast = json_number_value(panelContrastJ);

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

		// perIndexManualLocks
		json_t *perIndexManualLocksJ = json_object_get(rootJ, "perIndexManualLocks");
		if (perIndexManualLocksJ) {
			perIndexManualLocks = json_integer_value(perIndexManualLocksJ);
		}

		// stepLock
		json_t *stepLockJ = json_object_get(rootJ, "stepLock");
		if (stepLockJ) {
			stepLock = json_integer_value(stepLockJ);
		}

		// stepLocks
		json_t *stepLocksJ = json_object_get(rootJ, "stepLocks");
		if (stepLocksJ && json_is_array(stepLocksJ)) {
			for (int i = 0; i < NUM_INDEXES; i++) {
				json_t *stepLocksArrayJ = json_array_get(stepLocksJ, i);
				if (stepLocksArrayJ) {
					stepLocks[i] = json_integer_value(stepLocksArrayJ);
				}
			}
			
			// stepLocksCvs
			json_t *stepLocksCvsJ = json_object_get(rootJ, "stepLocksCvs");
			if (stepLocksCvsJ && json_is_array(stepLocksCvsJ)) {
				size_t slsize = json_array_size(stepLocksCvsJ);
				unsigned int sli = 0;
				for (int i = 0; i < NUM_INDEXES; i++) {
					if (stepLocks[i] != 0x0) {
						for (int j = 0; j < OutputKernel::MAX_LENGTH; j++) {
							if ( ((stepLocks[i] >> j) & 0x1) != 0) {
								json_t *stepLocksCvsArrayJ = json_array_get(stepLocksCvsJ, sli);
								sli++;
								if (stepLocksCvsArrayJ) {
									stepLocksCvs[i][j] = json_number_value(stepLocksCvsArrayJ);;
								}
								if (sli >= slsize) {
									i = NUM_INDEXES;
									break;
								}
							}
						}
					}
				}
			}
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
		int seqLen = getLength();
		IoStep* ioSteps = new IoStep[seqLen];
		
		// Populate ioSteps array
		float lastCv = 0.0f;
		for (int i = 0; i < seqLen; i++) {
			float cv = outputKernels[0].getBuf(i);
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
		for (int i = 0; i < OutputKernel::MAX_LENGTH; i++) {
			outputKernels[0].setBuf(ProbKernel::IDEM_CV, i);
		}
		for (int i = 0; i < seqLen; i++) {
			if (ioSteps[i].gate) {
				outputKernels[0].setBuf(ioSteps[i].pitch, i);
			}
		}
	}

	
	void process(const ProcessArgs &args) override {		
		int index = getIndex();
		int length = getLength();
				
		// bool expanderPresent = rightExpander.module && (rightExpander.module->model == modelProbKeyExpander);

		
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
				
				// mannual lock has higher precedence, since will use manual lock memory; but works only for poly chan 0
				bool isLockedStep = false;
				if (c == 0 && getStepLock(outputKernels[c].getNextStep(length), index)) {
					// recycle CV (WILL CHANGE TO USE MANUAL LOCK CV MEMORY)
					if (perIndexManualLocks != 0) {
						int nextStep = outputKernels[c].getNextStep(length);
						float newCv = stepLocksCvs[index][nextStep];
						outputKernels[c].stepWithInsertNew(newCv, length);
					}
					else {
						outputKernels[c].stepWithKeepOld(length);
					}
					
					isLockedStep = true;
				}
				// knob lock has lower precedence, since it has no special lock memory (it uses the output register)	
				else if (getLock() > random::uniform()) {
					// recycle CV
					outputKernels[c].stepWithKeepOld(length);
					isLockedStep = true;
				}
				// generate new random CV (taking hold into account though)
				else {
					bool hold = inputs[HOLD_INPUT].isConnected() && inputs[HOLD_INPUT].getVoltage(std::min(c, inputs[HOLD_INPUT].getChannels())) > 1.0f;
					if (hold) {
						outputKernels[c].stepWithCopy(length);
					}
					else {
						float newCv = probKernels[index].calcRandomCv(getOffset(c), getSquash(c), getDensity(c), overlap);
						outputKernels[c].stepWithInsertNew(newCv, length);
					}
				}
				
				if (c == 0) {
					infoTracer = (long) (infoTracerTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					infoTracerLockedStep = isLockedStep;
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
			
			// keyboard lights (green, red, white)
			if (editMode == MODE_PROB) {
				int tracer = -1;
				if (showTracer != 0 && infoTracer > 0) {// inputs[GATE_INPUT].getVoltage(0) >= 1.0f) {
					float tcv = outputKernels[0].getCv();
					tracer = (int)std::round(tcv * 12.0f);
					tracer = eucMod(tracer, 12);
				}
				for (int i = 0; i < 12; i++) {
					setKeyLightsProb(i, probKernels[index].getNoteProb(i), tracer == i, infoTracerLockedStep);
				}
			}
			else if (editMode == MODE_ANCHOR) {
				for (int i = 0; i < 12; i++) {
					setKeyLightsAnchor(i, probKernels[index].getNoteAnchor(i), probKernels[index].isProbNonNull(i));
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
	}
	
	void setKeyLightsProb(int key, float prob, bool tracer, bool tracerLockedStep) {
		for (int j = 0; j < 4; j++) {// 0 to 3 is bottom to top
			lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 0].setBrightness((tracer && (j == 3)) ? 0.0f : (prob * 4.0f - (float)j));
			lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 1].setBrightness((tracer && (j == 3) && tracerLockedStep) ? 1.0f : 0.0f);
			lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 2].setBrightness((tracer && (j == 3) && !tracerLockedStep) ? 1.0f : 0.0f);
		}
	}
	void setKeyLightsAnchor(int key, float anch, bool active) {
		if (active) {
			anch = ProbKernel::quantizeAnchor(anch);
			for (int j = 0; j < 4; j++) {
				lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 0].setBrightness(0.0f);
				lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 1].setBrightness(anch * 4.0f - (float)j);
				lights[KEY_LIGHTS + key * 4 * 3 + j * 3 + 2].setBrightness(0.0f);
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

	struct MainDisplayWidget : TransparentWidget {
		ProbKey *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		
		MainDisplayWidget() {
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, 18);
				nvgFontFaceId(args.vg, font->handle);
				//nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = VecPx(6, 24);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~~~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
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
						snprintf(displayStr, 5, " L%2u", module->getLength());
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
		}
	};
	
	
	struct LengthKnob : IMMediumKnob {
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
	
	struct StepLockSubItem : MenuItem {
		ProbKey *module;
		int stepNum;
		int index;
		void onAction(const event::Action &e) override {
			module->toggleStepLock(stepNum, index);
			e.unconsume();
		}
		void step() override {
			rightText = CHECKMARK(module->getStepLock(stepNum, index));
			if (module->perIndexManualLocks != 0 && module->getStepLock(stepNum, index)) {
				if (module->stepLocksCvs[index][stepNum] != module->outputKernels[0].getBuf(stepNum)) {
					rightText.insert(0, "*");
				}
			}
			MenuItem::step();
		}
	};

	
	void appendContextMenu(Menu *menu) override {
		ProbKey *module = dynamic_cast<ProbKey*>(this->module);
		assert(module);
		
		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());
		
		InteropSeqItem *interopSeqItem = createMenuItem<InteropSeqItem>(portableSequenceID, RIGHT_ARROW);
		interopSeqItem->module = module;
		menu->addChild(interopSeqItem);

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createSubmenuItem("Manual step lock", "", [=](Menu* menu) {
			menu->addChild(createMenuItem("Clear all locks", "",
				[=]() {if (module->perIndexManualLocks != 0) {
						module->clearStepLocks(module->getIndex());
					}
					else {
						module->clearStepLock();
					}
				}
			));
			menu->addChild(createMenuItem("Set locks of active steps", "",
				[=]() {module->setStepLocksForAllActive(module->getIndex());}
			));
			menu->addChild(createMenuItem("Reset playhead", "",
				[=]() {for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
						module->outputKernels[c].resetPlayHead();
					}
				}
			));
			menu->addChild(createCheckMenuItem("Per index manual locks", "",
				[=]() {return module->perIndexManualLocks != 0;},
				[=]() {module->perIndexManualLocks ^= 0x1;}
			));
			
			menu->addChild(new MenuSeparator());
			
			char buf[8];
			int index = module->getIndex();
			for (int s = 0; s < module->getLength(); s++) {
				float cv = module->outputKernels[0].getBuf(s);
				if (cv == ProbKernel::IDEM_CV) {
					buf[0] = 0;
				}
				else {
					printNote(cv, buf, true);
				}
				std::string noteStr(buf);
				std::replace(noteStr.begin(), noteStr.end(), '\"', '#');
				
				int oct = eucDiv((int)std::round(cv * 12.0f), 12);
				oct = clamp(oct + 4, 0, 9);
				noteStr.insert(0, std::string(oct * 2, ' '));
				noteStr.insert(0, std::string("-"));
				
				StepLockSubItem *slockItem = createMenuItem<StepLockSubItem>(noteStr, "");
				slockItem->module = module;
				slockItem->stepNum = s;
				slockItem->index = index;
				menu->addChild(slockItem);
			}
		}));			

		OverlapSlider *ovlpSlider = new OverlapSlider(&(module->overlap));
		ovlpSlider->box.size.x = 200.0f;
		menu->addChild(ovlpSlider);
		
		menu->addChild(createCheckMenuItem("Index mode 12", "",
			[=]() {return module->indexCvCap12;},
			[=]() {module->indexCvCap12 ^= 0x1;}
		));

		menu->addChild(createCheckMenuItem("Show generated note", "",
			[=]() {return module->showTracer;},
			[=]() {module->showTracer ^= 0x1;}
		));
	}


	ProbKeyWidget(ProbKey *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ProbKey.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));



		// ****** Top portion (keys) ******

		static const Vec keyboardPos = mm2px(Vec(6.474f, 11.757f));
		svgPanel->fb->addChild(new KeyboardBig(keyboardPos, mode));
		
		for (int k = 0; k < 12; k++) {
			Vec keyPos = keyboardPos + mm2px(bigKeysPos[k]);
			addChild(createPianoKey<PianoKeyBig>(keyPos, k, module ? &module->pkInfo : NULL));
		
			Vec offsetLeds = Vec(PianoKeyBig::sizeX * 0.5f, PianoKeyBig::sizeY * 7.0f / 8.0f);
			addChild(createLightCentered<SmallLight<GreenRedWhiteLightIM>>(keyPos + offsetLeds, module, ProbKey::KEY_LIGHTS + k * (4 * 3) + 0 * 3));
			offsetLeds.y = PianoKeyBig::sizeY * 5.0f / 8.0f;
			addChild(createLightCentered<SmallLight<GreenRedWhiteLightIM>>(keyPos + offsetLeds, module, ProbKey::KEY_LIGHTS + k * (4 * 3) + 1 * 3)); \
			offsetLeds.y = PianoKeyBig::sizeY * 3.0f / 8.0f;
			addChild(createLightCentered<SmallLight<GreenRedWhiteLightIM>>(keyPos + offsetLeds, module, ProbKey::KEY_LIGHTS + k * (4 * 3) + 2 * 3)); \
			offsetLeds.y = PianoKeyBig::sizeY * 1.0f / 8.0f;
			addChild(createLightCentered<SmallLight<GreenRedWhiteLightIM>>(keyPos + offsetLeds, module, ProbKey::KEY_LIGHTS + k * (4 * 3) + 3 * 3));
		}

		
		// ****** Bottom portion ******
		
		static const float row0 = 83.5f;
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
		addParam(createDynamicParamCentered<IMMediumKnob>(mm2px(Vec(col0, row0)), module, ProbKey::INDEX_PARAM, mode));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, row1)), true, module, ProbKey::INDEX_INPUT, mode));

		// Gate input
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, row2)), true, module, ProbKey::GATE_INPUT, mode));
	

		// **** col1 ****

		// density knob, led and input
		addParam(createDynamicParamCentered<IMMediumKnob>(mm2px(Vec(col1, row0)), module, ProbKey::DENSITY_PARAM, mode));	
		addChild(createLightCentered<SmallLight<GreenRedLightIM>>(mm2px(Vec(col1 - 4.5f, row1 - 6.3f)), module, ProbKey::DENSITY_LIGHT));			
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row1)), true, module, ProbKey::DENSITY_INPUT, mode));

		// Hold input
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row2)), true, module, ProbKey::HOLD_INPUT, mode));


		// **** col2 ****

		// Squash knob and input
		addParam(createDynamicParamCentered<IMMediumKnob>(mm2px(Vec(col2, row0)), module, ProbKey::SQUASH_PARAM, mode));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2, row1)), true, module, ProbKey::SQUASH_INPUT, mode));

		// Main display
		MainDisplayWidget *displayMain = new MainDisplayWidget();
		displayMain->box.size = VecPx(71, 30);// 4 characters
		displayMain->box.pos = mm2px(Vec((col2 + col3) * 0.5f, row2 - 2.0f)).minus(displayMain->box.size.div(2));
		displayMain->module = module;
		addChild(displayMain);
		svgPanel->fb->addChild(new DisplayBackground(displayMain->box.pos, displayMain->box.size, mode));


		// **** col3 ****

		// Offset knob and input
		addParam(createDynamicParamCentered<IMMediumKnob>(mm2px(Vec(col3, row0)), module, ProbKey::OFFSET_PARAM, mode));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col3, row1)), true, module, ProbKey::OFFSET_INPUT, mode));


		// **** col4 ****

		// Lock knob, led, button and input
		addParam(createDynamicParamCentered<IMBigKnob>(mm2px(Vec(col4, row0 + 1.0f)), module, ProbKey::LOCK_KNOB_PARAM, mode));
		addChild(createLightCentered<SmallLight<RedLightIM>>(mm2px(Vec(col4 - 5.0f, row1 + 4.0f - 6.8f)), module, ProbKey::LOCK_LIGHT));	
		addParam(createDynamicParamCentered<IMBigPushButton>(mm2px(Vec(col4, row1 + 4.0f)), module, ProbKey::LOCK_BUTTON_PARAM, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col4, row2)), true, module, ProbKey::LOCK_INPUT, mode));


		// **** col5 ****

		// Length knob and input
		LengthKnob* lengthKnob = createDynamicParamCentered<LengthKnob>(mm2px(Vec(col5, row0)), module, ProbKey::LENGTH_PARAM, module ? &module->panelTheme : NULL);
		addParam(lengthKnob);
		if (module) {
			lengthKnob->dispManagerSrc = &(module->dispManager);
		}
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col5, row1)), true, module, ProbKey::LENGTH_INPUT, mode));

		// CV output
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col5, row2)), false, module, ProbKey::CV_OUTPUT, mode));


		// **** col6 ****
		static const float r6dy = 13.0f;
		static const float rowm1 = 67.2f;
		
		// Transpose buttons
		addParam(createDynamicParamCentered<IMPushButton>(mm2px(Vec(col6, rowm1 - 4.0f * r6dy)), module, ProbKey::TR_UP_PARAM, mode));		
		addParam(createDynamicParamCentered<IMPushButton>(mm2px(Vec(col6, rowm1 - 3.0f * r6dy - 1.0f)), module, ProbKey::TR_DOWN_PARAM, mode));		

		// Mode led-button - MODE_PROB
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col6, rowm1 - 2.0f * r6dy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_PROB));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(mm2px(Vec(col6, rowm1 - 2.0f * r6dy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_PROB * 2));

		// Mode led-button - MODE_ANCHOR
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col6, rowm1 - 1.0f * r6dy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_ANCHOR));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(mm2px(Vec(col6, rowm1 - 1.0f * r6dy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_ANCHOR * 2));

		// Mode led-button - MODE_RANGE
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col6, rowm1)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_RANGE));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(mm2px(Vec(col6, rowm1)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_RANGE * 2));

		// Copy-paste buttons
		addParam(createDynamicParamCentered<IMPushButton>(mm2px(Vec(col6, row0)), module, ProbKey::COPY_PARAM, mode));		
		addParam(createDynamicParamCentered<IMPushButton>(mm2px(Vec(col6, row1)), module, ProbKey::PASTE_PARAM, mode));		

		// Gate output
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col6, row2)), false, module, ProbKey::GATE_OUTPUT, mode));
	}
};

Model *modelProbKey = createModel<ProbKey, ProbKeyWidget>("Prob-Key");
