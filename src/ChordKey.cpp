//***********************************************************************************************
//Keyboard-based chord generator module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "comp/PianoKey.hpp"
#include "Interop.hpp"


struct ChordKey : Module {
	enum ParamIds {
		ENUMS(OCTINC_PARAMS, 4),
		ENUMS(OCTDEC_PARAMS, 4),
		INDEX_PARAM,
		FORCE_PARAM,
		TRANSPOSEUP_PARAM,
		TRANSPOSEDOWN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		INDEX_INPUT,
		GATE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),	
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(KEY_LIGHTS, 12 * 4),
		NUM_LIGHTS
	};
	
	
	// Expander
	// none
		
	// Constants
	static const int NUM_CHORDS = 25;// C4 to C6 incl
	const float warningTime = 0.7f;// seconds (no static since referenced from ChordKeyWidget)
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	
	// Need to save, with reset
	int octs[NUM_CHORDS][4];// -1 to 9 (-1 means not used, i.e. no gate can be emitted)
	int keys[NUM_CHORDS][4];// 0 to 11 for the 12 keys
	int mergeOutputs;// 0 = none, 1 = merge A with B, 2 = merge A with B and C, 3 = merge A with All
	int keypressEmitGate;// 1 = yes (default), 0 = no
	int autostepPaste;

	
	// No need to save, with reset
	unsigned long noteLightCounter;// 0 when no key to light, downward step counter timer when key lit
	int octsCP[4];// copy paste
	int keysCP[4];// copy paste
	long offWarning;// 0 when no warning, positive downward step counter timer when warning

	// No need to save, no reset
	RefreshCounter refresh;
	Trigger octIncTriggers[4];
	Trigger octDecTriggers[4];
	Trigger maxVelTrigger;
	Trigger transposeUpTrigger;
	Trigger transposeDownTrigger;
	dsp::BooleanTrigger keyTrigger;
	PianoKeyInfo pkInfo;
	int offWarningChan; // valid only when offWarning is non-zero
	
	
	int getIndex() {
		int index = (int)std::round(params[INDEX_PARAM].getValue() + inputs[INDEX_INPUT].getVoltage() * 12.0f);
		return clamp(index, 0, NUM_CHORDS - 1 );
	}
	float calcCV(int index, int cni) {
		return (octs[index][cni] >= 0) ? (((float)(octs[index][cni] - 4)) + ((float)keys[index][cni]) / 12.0f) : 0.0f;
	}
	void setCV(int index, int cni, float cv) {
		// optional TODO: optimize using eucDivMod()
		int note = (int)std::round(cv * 12.0f);
		octs[index][cni] = clamp(eucDiv(note, 12) + 4, 0, 9);
		keys[index][cni] = eucMod(note, 12);
	}
	void applyDelta(int index, int cni, int delta) {
		// optional TODO: optimize using eucDivMod()
		int newKey = (keys[index][cni] + delta);
		keys[index][cni] = eucMod(newKey, 12);
		int newOct = octs[index][cni] + eucDiv(newKey, 12);
		octs[index][cni] = clamp(newOct, 0, 9);
	}


	ChordKey() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		char strBuf[32];
		for (int cni = 0; cni < 4; cni++) {// chord note index
			snprintf(strBuf, 32, "Oct down %i", cni + 1);
			configParam(OCTDEC_PARAMS + cni, 0.0, 1.0, 0.0, strBuf);
			snprintf(strBuf, 32, "Oct up %i", cni + 1);
			configParam(OCTINC_PARAMS + cni, 0.0, 1.0, 0.0, strBuf);
		}
		configParam(INDEX_PARAM, 0.0f, 24.0f, 0.0f, "Index", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset
		paramQuantities[INDEX_PARAM]->snapEnabled = true;
		configSwitch(FORCE_PARAM, 0.0f, 1.0f, 0.0f, "Force gate on", {"No", "Yes"});
		configParam(TRANSPOSEUP_PARAM, 0.0f, 1.0f, 0.0f, "Transpose up");
		configParam(TRANSPOSEDOWN_PARAM, 0.0f, 1.0f, 0.0f, "Transpose down");
		
		getParamQuantity(INDEX_PARAM)->randomizeEnabled = false;		
		
		configInput(INDEX_INPUT, "Index");
		configInput(GATE_INPUT, "Gate");

		for (int i = 0; i < 4; i++) {
			configOutput(GATE_OUTPUTS + i, string::f("Gate %i", i + 1));
			configOutput(CV_OUTPUTS + i, string::f("CV %i", i + 1));
		}

		pkInfo.showMarks = 4;
		
		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	void onReset() override {
		for (int ci = 0; ci < NUM_CHORDS; ci++) { // chord index
			// C-major triad with base note on C4
			keys[ci][0] = 0;
			keys[ci][1] = 4;
			keys[ci][2] = 7;
			keys[ci][3] = 0;
			octs[ci][0] = 4;
			octs[ci][1] = 4;
			octs[ci][2] = 4;
			octs[ci][3] = -1;// turned off
		}
		mergeOutputs = 0;// no merging
		keypressEmitGate = 1;// yes
		autostepPaste = 0;
		resetNonJson();
	}
	void resetNonJson() {
		noteLightCounter = 0ul;
		// C-major triad with base note on C4
		keysCP[0] = 0;
		keysCP[1] = 4;
		keysCP[2] = 7;
		keysCP[3] = 0;
		octsCP[0] = 4;
		octsCP[1] = 4;
		octsCP[2] = 4;
		octsCP[3] = -1;// turned off
		offWarning = 0ul;
	}

	void onRandomize() override {
		for (int ci = 0; ci < NUM_CHORDS; ci++) { // chord index
			for (int cni = 0; cni < 4; cni++) {// chord note index
				octs[ci][cni] = random::u32() % 10;
				keys[ci][cni] = random::u32() % 12;
			}
		}					
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// octs
		json_t *octJ = json_array();
		for (int ci = 0; ci < NUM_CHORDS; ci++) {// chord index
			for (int cni = 0; cni < 4; cni++) {// chord note index
				json_array_insert_new(octJ, cni + (ci * 4), json_integer(octs[ci][cni]));
			}
		}
		json_object_set_new(rootJ, "octs", octJ);
		
		// keys
		json_t *keyJ = json_array();
		for (int ci = 0; ci < NUM_CHORDS; ci++) {// chord index
			for (int cni = 0; cni < 4; cni++) {// chord note index
				json_array_insert_new(keyJ, cni + (ci * 4), json_integer(keys[ci][cni]));
			}
		}
		json_object_set_new(rootJ, "keys", keyJ);
		
		// mergeOutputs
		json_object_set_new(rootJ, "mergeOutputs", json_integer(mergeOutputs));

		// keypressEmitGate
		json_object_set_new(rootJ, "keypressEmitGate", json_integer(keypressEmitGate));

		// autostepPaste
		json_object_set_new(rootJ, "autostepPaste", json_integer(autostepPaste));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// panelContrast
		json_t *panelContrastJ = json_object_get(rootJ, "panelContrast");
		if (panelContrastJ)
			panelContrast = json_number_value(panelContrastJ);

		// octs
		json_t *octJ = json_object_get(rootJ, "octs");
		if (octJ) {
			for (int ci = 0; ci < NUM_CHORDS; ci++) {// chord index
				for (int cni = 0; cni < 4; cni++) {// chord note index
					json_t *octArrayJ = json_array_get(octJ, cni + (ci * 4));
					if (octArrayJ)
						octs[ci][cni] = json_number_value(octArrayJ);
				}
			}
		}

		// keys
		json_t *keyJ = json_object_get(rootJ, "keys");
		if (keyJ) {
			for (int ci = 0; ci < NUM_CHORDS; ci++) {// chord index
				for (int cni = 0; cni < 4; cni++) {// chord note index
					json_t *keyArrayJ = json_array_get(keyJ, cni + (ci * 4));
					if (keyArrayJ)
						keys[ci][cni] = json_number_value(keyArrayJ);
				}
			}
		}
		
		// mergeOutputs
		json_t *mergeOutputsJ = json_object_get(rootJ, "mergeOutputs");
		if (mergeOutputsJ)
			mergeOutputs = json_integer_value(mergeOutputsJ);

		// keypressEmitGate
		json_t *keypressEmitGateJ = json_object_get(rootJ, "keypressEmitGate");
		if (keypressEmitGateJ)
			keypressEmitGate = json_integer_value(keypressEmitGateJ);

		// autostepPaste
		json_t *autostepPasteJ = json_object_get(rootJ, "autostepPaste");
		if (autostepPasteJ)
			autostepPaste = json_integer_value(autostepPasteJ);

		resetNonJson();
	}

	
	IoStep* fillIoSteps(int *seqLenPtr) {// caller must delete return array
		int index = getIndex();
		IoStep* ioSteps = new IoStep[4];
		
		// populate ioSteps array
		int j = 0;// write head also
		for (int i = 0; i < 4; i++) {
			if (octs[index][i] >= 0) {
				ioSteps[j].pitch = calcCV(index, i);
				ioSteps[j].gate = true;
				ioSteps[j].tied = false;
				ioSteps[j].vel = -1.0f;// no concept of velocity in BigButton2
				ioSteps[j].prob = -1.0f;// no concept of probability in BigButton2
				j++;
			}
		}
		
		// return values 
		*seqLenPtr = j;
		return ioSteps;
	}
	
	std::vector<IoNote>* fillIoNotes(int *seqLenPtr) {// caller must delete return vector
		int index = getIndex();
		std::vector<IoNote> *ioNotes = new std::vector<IoNote>;
		
		// populate ioNotes array
		int j = 0;// write head also
		for (int i = 0; i < 4; i++) {
			if (octs[index][i] >= 0) {
				IoNote newNote;
				newNote.start = 0.0f;
				newNote.length = 0.5f;
				newNote.pitch = calcCV(index, i);
				newNote.vel = -1.0f;// no concept of velocity in BigButton2
				newNote.prob = -1.0f;// no concept of probability in BigButton2
				ioNotes->push_back(newNote);
				j++;
			}
		}
		
		// return values 
		*seqLenPtr = j;
		return ioNotes;
	}
	
	
	void emptyIoNotesSeq(std::vector<IoNote> *ioNotes) {// grabs first four notes it sees, regardless of start time
		int index = getIndex();
		
		// populate notes of the chord
		int i = 0;
		for (; i < std::min(4, (int)(ioNotes->size())); i++) {
			setCV(index, i, (*ioNotes)[i].pitch);
		}
		for (; i < 4; i++) {
			octs[index][i] = -1;
			keys[index][i] = 0;
		}
	}	


	void emptyIoNotesChord(std::vector<IoNote> *ioNotes) {// grabs only the notes with the same start time as the first note seen
		int index = getIndex();
		
		// populate notes of the chord
		int j = 0;// write head
		if (ioNotes->size() > 0) {
			float firstTime = (*ioNotes)[0].start;
			for (int i = 0; i < std::min(4, (int)(ioNotes->size())); i++) {
				if ((*ioNotes)[i].start == firstTime) {
					setCV(index, j, (*ioNotes)[i].pitch);
					j++;
				}
			}
		}
		
		for (; j < 4; j++) {// set rest of chord notes as unused
			octs[index][j] = -1;
			keys[index][j] = 0;
		}
	}	


	void interopCopySeq() {
		int seqLen;
		IoStep* ioSteps = fillIoSteps(&seqLen);
		interopCopySequence(seqLen, ioSteps);
		delete[] ioSteps;
	};
	void interopCopyChord() {
		int seqLen;
		std::vector<IoNote>* ioNotes = fillIoNotes(&seqLen);
		interopCopySequenceNotes(seqLen, ioNotes);
		delete ioNotes;
	};
	void interopPasteSeq() {
		int seqLen;
		std::vector<IoNote> *ioNotes = interopPasteSequenceNotes(1024, &seqLen);// 1024 not used to alloc anything
		if (ioNotes) {
			emptyIoNotesSeq(ioNotes);
			delete ioNotes;
			if (autostepPaste) {
				params[ChordKey::INDEX_PARAM].setValue(
					clamp(params[ChordKey::INDEX_PARAM].getValue() + 1.0f, 0.0f, 24.0f));
			}
		}
	};
	void interopPasteChord() {
		int seqLen;
		std::vector<IoNote> *ioNotes = interopPasteSequenceNotes(1024, &seqLen);// 1024 not used to alloc anything
		if (ioNotes) {
			emptyIoNotesChord(ioNotes);
			delete ioNotes;
			if (autostepPaste) {
				params[ChordKey::INDEX_PARAM].setValue(
					clamp(params[ChordKey::INDEX_PARAM].getValue() + 1.0f, 0.0f, 24.0f));
			}
		}
	};
		
		
	void process(const ProcessArgs &args) override {		
		int index = getIndex();
		
		//********** Buttons, knobs, switches and inputs **********
		
		if (refresh.processInputs()) {
			// oct inc/dec
			for (int cni = 0; cni < 4; cni++) {
				if (octIncTriggers[cni].process(params[OCTINC_PARAMS + cni].getValue())) {
					octs[index][cni] = clamp(octs[index][cni] + 1, -1, 9);
				}
				if (octDecTriggers[cni].process(params[OCTDEC_PARAMS + cni].getValue())) {
					octs[index][cni] = clamp(octs[index][cni] - 1, -1, 9);
				}
			}
			
			// Transpose buttons
			int delta = 0;
			if (transposeUpTrigger.process(params[TRANSPOSEUP_PARAM].getValue())) {
				delta = 1;
			}
			if (transposeDownTrigger.process(params[TRANSPOSEDOWN_PARAM].getValue())) {
				delta = -1;
			}
			if (delta != 0) {
				for (int cni = 0; cni < 4; cni++) {
					if (octs[index][cni] >= 0) {
						applyDelta(index, cni, delta);
					}
				}				
			}
			
			// piano keys
			if (keyTrigger.process(pkInfo.gate)) {
				int cni = clamp((int)(pkInfo.vel * 4.0f), 0, 3);
				if (octs[index][cni] >= 0) {
					keys[index][cni] = pkInfo.key;
				}
				else {
					offWarning = (long) (warningTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					offWarningChan = cni;
				}
			}	

			// Top output channels
			if (mergeOutputs == 0) {
				outputs[GATE_OUTPUTS + 0].setChannels(1);
				outputs[CV_OUTPUTS + 0].setChannels(1);
			}
			else if (mergeOutputs == 1) {
				outputs[GATE_OUTPUTS + 0].setChannels(2);
				outputs[CV_OUTPUTS + 0].setChannels(2);
			}
			else if (mergeOutputs == 2) {
				outputs[GATE_OUTPUTS + 0].setChannels(3);
				outputs[CV_OUTPUTS + 0].setChannels(3);
			}
			else {
				outputs[GATE_OUTPUTS + 0].setChannels(4);
				outputs[CV_OUTPUTS + 0].setChannels(4);
			}
		
			
		}// userInputs refresh


		
		
		//********** Outputs and lights **********
		
		// gate and cv outputs
		bool forcedGate = params[FORCE_PARAM].getValue() >= 0.5f;
		float gateOuts[4];
		float cvOuts[4];
		for (int cni = 0; cni < 4; cni++) {
			// external (poly)gate with force 
			bool extGateWithForce = forcedGate;
			if (!forcedGate && inputs[GATE_INPUT].isConnected()) {
				int numGateChan = inputs[GATE_INPUT].getChannels();// when connected, we are assured that num channels > 0
				extGateWithForce |= (inputs[GATE_INPUT].getVoltage(std::min(numGateChan - 1, cni)) >= 1.0f);
			}
			// keypress (with mouse gate)
			bool keypressGate = false;
			if (pkInfo.gate && keypressEmitGate != 0) {
				int keyPressed = clamp((int)(pkInfo.vel * 4.0f), 0, 3);
				if (pkInfo.isRightClick) // mouse play one
					keypressGate = (octs[index][keyPressed] >= 0) && keyPressed == cni;
				else// leftclick: mouse play all
					keypressGate = (octs[index][keyPressed] >= 0);
			}
			gateOuts[cni] = ((octs[index][cni] >= 0) && (extGateWithForce || keypressGate))  ? 10.0f : 0.0f;
			cvOuts[cni] = calcCV(index, cni);
		}
		if (mergeOutputs == 0) {
			for (int cni = 0; cni < 4; cni++) {			
				outputs[GATE_OUTPUTS + cni].setVoltage(gateOuts[cni]);
				outputs[CV_OUTPUTS + cni].setVoltage(cvOuts[cni]);
			}
		}
		else if (mergeOutputs == 1) {
			outputs[GATE_OUTPUTS + 1].setVoltage(0.0f);
			outputs[CV_OUTPUTS + 1].setVoltage(0.0f);
			for (int cni = 0; cni < 2; cni++) {			
				outputs[GATE_OUTPUTS + 0].setVoltage(gateOuts[cni], cni);
				outputs[CV_OUTPUTS + 0].setVoltage(cvOuts[cni], cni);
			}	
			for (int cni = 2; cni < 4; cni++) {			
				outputs[GATE_OUTPUTS + cni].setVoltage(gateOuts[cni]);
				outputs[CV_OUTPUTS + cni].setVoltage(cvOuts[cni]);
			}			
		}
		else if (mergeOutputs == 2) {
			for (int cni = 1; cni < 3; cni++) {
				outputs[GATE_OUTPUTS + cni].setVoltage(0.0f);
				outputs[CV_OUTPUTS + cni].setVoltage(0.0f);
			}
			for (int cni = 0; cni < 3; cni++) {			
				outputs[GATE_OUTPUTS + 0].setVoltage(gateOuts[cni], cni);
				outputs[CV_OUTPUTS + 0].setVoltage(cvOuts[cni], cni);
			}	
			outputs[GATE_OUTPUTS + 3].setVoltage(gateOuts[3]);
			outputs[CV_OUTPUTS + 3].setVoltage(cvOuts[3]);
		}
		else {
			for (int cni = 1; cni < 4; cni++) {
				outputs[GATE_OUTPUTS + cni].setVoltage(0.0f);
				outputs[CV_OUTPUTS + cni].setVoltage(0.0f);
			}
			for (int cni = 0; cni < 4; cni++) {			
				outputs[GATE_OUTPUTS + 0].setVoltage(gateOuts[cni], cni);
				outputs[CV_OUTPUTS + 0].setVoltage(cvOuts[cni], cni);
			}	
		}
		
		
		// lights
		if (refresh.processLights()) {
			for (int ki = 0; ki < 12; ki++) {
				for (int cni = 0; cni < 4; cni++) {
					lights[KEY_LIGHTS + ki * 4 + cni].setBrightness((ki == keys[index][cni] && octs[index][cni] >= 0) ? 1.0f : 0.0f);
				}
			}
			
			if (offWarning > 0l)
				offWarning--;
		}// processLights()
		

		if (refresh.processInputs()) {
			// To Expander
			if (rightExpander.module && (rightExpander.module->model == modelFourView || rightExpander.module->model == modelChordKeyExpander)) {
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				for (int cni = 0; cni < 4; cni++) {
					messageToExpander[cni] = octs[index][cni] >= 0 ? cvOuts[cni] : -100.0f;
				}
				messageToExpander[4] = (float)panelTheme;
				messageToExpander[5] = panelContrast;
				rightExpander.module->leftExpander.messageFlipRequested = true;
			}
		}
	}
};


struct ChordKeyWidget : ModuleWidget {
	struct OctDisplayWidget : TransparentWidget {
		ChordKey *module;
		int index;
		std::shared_ptr<Font> font;
		std::string fontPath;
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f; // 18.2f for 14 pt, 19.7f for 15pt
		
		OctDisplayWidget(Vec _pos, Vec _size, ChordKey *_module, int _index) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			index = _index;
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, textFontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, -0.4);

				Vec textPos = VecPx(6.7f, textOffsetY);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				int octaveNum = module ? module->octs[module->getIndex()][index] : 4;
				char displayStr[2];
				if (octaveNum >= 0) {
					displayStr[0] = 0x30 + (char)(octaveNum);
				}
				else {
					displayStr[0] = '-';
					if (module->offWarning > 0l && index == module->offWarningChan) {
						bool warningFlashState = calcWarningFlash(module->offWarning, (long) (module->warningTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips));
						if (!warningFlashState) 
							displayStr[0] = 'X';
					}
				}
				displayStr[1] = 0;
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};
	struct IndexDisplayWidget : TransparentWidget {
		ChordKey *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f; // 18.2f for 14 pt, 19.7f for 15pt
		
		IndexDisplayWidget(Vec _pos, Vec _size, ChordKey *_module) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, textFontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, -0.4);

				Vec textPos = VecPx(6.7f, textOffsetY);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				char displayStr[3];
				int indexNum = module ? module->getIndex() + 1 : 1;
				snprintf(displayStr, 3, "%2u", (unsigned) indexNum);
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};
	
	struct CopyChordItem : MenuItem {
		ChordKey *module;
		void onAction(const event::Action &e) override {
			int index = module->getIndex();
			for (int cni = 0; cni < 4; cni++) {
				module->octsCP[cni] = module->octs[index][cni];
				module->keysCP[cni] = module->keys[index][cni];
			}
		}
	};
	struct PasteChordItem : MenuItem {
		ChordKey *module;
		void onAction(const event::Action &e) override {
			int index = module->getIndex();
			for (int cni = 0; cni < 4; cni++) {
				module->octs[index][cni] = module->octsCP[cni];
				module->keys[index][cni] = module->keysCP[cni];
			}
		}
	};
	
	struct TransposeQuantity : Quantity {
		ChordKey* module;
		float valueLocal;// must be reset to 0 when enter menu (i.e. constructor)
		int valueIntLocal;
		int valueIntLocalLast;
		  
		TransposeQuantity() {
			valueLocal = 0.0f;
			valueIntLocal = 0;
			valueIntLocalLast = 0;
		}
		void setValue(float value) override {
			valueLocal = math::clamp(value, getMinValue(), getMaxValue());; 
			valueIntLocal = (int)(std::round(valueLocal));
			int delta = valueIntLocal - valueIntLocalLast;// delta is number of semitones
			if (delta != 0) {
				int index = module->getIndex();
				for (int cni = 0; cni < 4; cni++) {
					if (module->octs[index][cni] >= 0) {
						module->applyDelta(index, cni, delta);
					}
				}
				valueIntLocalLast = valueIntLocal;
			}
		}
		float getValue() override {
			return valueLocal;
		}
		float getMinValue() override {return -60.0f;}
		float getMaxValue() override {return 60.0f;}
		float getDefaultValue() override {return 0.0f;}
		float getDisplayValue() override {return getValue();}
		std::string getDisplayValueString() override {
			return string::f("%i", (int)std::round(getDisplayValue()));
		}
		void setDisplayValue(float displayValue) override {setValue(displayValue);}
		std::string getLabel() override {return "Transpose";}
		std::string getUnit() override {return " semitone(s)";}
	};

	
	struct TransposeSlider : ui::Slider {
		TransposeSlider(ChordKey* _module) {
			quantity = new TransposeQuantity();
			((TransposeQuantity*)quantity)->module = _module;
		}
		~TransposeSlider() {
			delete quantity;
		}
	};	

	struct InteropSeqItem : MenuItem {
		struct InteropCopySeqItem : MenuItem {
			ChordKey *module;
			void onAction(const event::Action &e) override {
				module->interopCopySeq();
			}
		};
		struct InteropCopyChordItem : MenuItem {
			ChordKey *module;
			void onAction(const event::Action &e) override {
				module->interopCopyChord();
			}
		};
		struct InteropPasteSeqItem : MenuItem {
			ChordKey *module;
			void onAction(const event::Action &e) override {
				module->interopPasteSeq();
			}
		};
		struct InteropPasteChordItem : MenuItem {
			ChordKey *module;
			void onAction(const event::Action &e) override {
				module->interopPasteChord();
			}
		};
		ChordKey *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			InteropCopyChordItem *interopCopyChordItem = createMenuItem<InteropCopyChordItem>("Copy chord", "");
			interopCopyChordItem->module = module;
			menu->addChild(interopCopyChordItem);		
			
			InteropPasteChordItem *interopPasteChordItem = createMenuItem<InteropPasteChordItem>("Paste chord", "");
			interopPasteChordItem->module = module;
			menu->addChild(interopPasteChordItem);		

			InteropCopySeqItem *interopCopySeqItem = createMenuItem<InteropCopySeqItem>("Copy chord as sequence", "");
			interopCopySeqItem->module = module;
			menu->addChild(interopCopySeqItem);		

			InteropPasteSeqItem *interopPasteSeqItem = createMenuItem<InteropPasteSeqItem>("Paste sequence as chord", "");
			interopPasteSeqItem->module = module;
			menu->addChild(interopPasteSeqItem);		

			menu->addChild(createCheckMenuItem("Autostep after paste", "",
				[=]() {return module->autostepPaste;},
				[=]() {module->autostepPaste ^= 0x1;}
			));

			return menu;
		}
	};	
	void appendContextMenu(Menu *menu) override {
		ChordKey *module = dynamic_cast<ChordKey*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());
		
		InteropSeqItem *interopSeqItem = createMenuItem<InteropSeqItem>(portableSequenceID, RIGHT_ARROW);
		interopSeqItem->module = module;
		menu->addChild(interopSeqItem);		
				
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));

		menu->addChild(createCheckMenuItem("Keypress monitoring", "",
			[=]() {return module->keypressEmitGate;},
			[=]() {module->keypressEmitGate ^= 0x1;}
		));
		
		menu->addChild(createSubmenuItem("Poly merge outputs into top note", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("None", "",
				[=]() {return module->mergeOutputs == 0;},
				[=]() {module->mergeOutputs = 0;}
			));
			menu->addChild(createCheckMenuItem("Second", "",
				[=]() {return module->mergeOutputs == 1;},
				[=]() {module->mergeOutputs = 1;}
			));
			menu->addChild(createCheckMenuItem("Second and third", "",
				[=]() {return module->mergeOutputs == 2;},
				[=]() {module->mergeOutputs = 2;}
			));
			menu->addChild(createCheckMenuItem("Second, third and fourth", "",
				[=]() {return module->mergeOutputs == 3;},
				[=]() {module->mergeOutputs = 3;}
			));
		}));			
		
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Actions"));

		CopyChordItem *cvCopyItem = createMenuItem<CopyChordItem>("Copy chord (internal)");
		cvCopyItem->module = module;
		menu->addChild(cvCopyItem);
		
		PasteChordItem *cvPasteItem = createMenuItem<PasteChordItem>("Paste chord (internal)");
		cvPasteItem->module = module;
		menu->addChild(cvPasteItem);	
		
		// transpose
		TransposeSlider *transposeSlider = new TransposeSlider(module);
		transposeSlider->box.size.x = 200.0f;
		menu->addChild(transposeSlider);

		InstantiateExpanderItem *expItem = createMenuItem<InstantiateExpanderItem>("Add expander (6HP right side)", "");
		expItem->module = module;
		expItem->model = modelChordKeyExpander;
		expItem->posit = box.pos.plus(math::Vec(box.size.x,0));
		menu->addChild(expItem);	
	}	
	
	
	ChordKeyWidget(ChordKey *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/ChordKey.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));



		// ****** Top portion (keys) ******

		static const Vec keyboardPos = mm2px(Vec(3.894f, 11.757f));
		svgPanel->fb->addChild(new KeyboardBig(keyboardPos, mode));

		for (int k = 0; k < 12; k++) {
			Vec keyPos = keyboardPos + mm2px(bigKeysPos[k]);
			addChild(createPianoKey<PianoKeyBig>(keyPos, k, module ? &module->pkInfo : NULL));
			
			Vec offsetLeds = Vec(PianoKeyBig::sizeX * 0.5f, PianoKeyBig::sizeY * 1.0f / 8.0f);
			addChild(createLightCentered<SmallLight<RedLightIM>>(keyPos + offsetLeds, module, ChordKey::KEY_LIGHTS + k * 4 + 0));
			offsetLeds.y = PianoKeyBig::sizeY * 3.0f / 8.0f;
			addChild(createLightCentered<SmallLight<OrangeLightIM>>(keyPos + offsetLeds, module, ChordKey::KEY_LIGHTS + k * 4 + 1));
			offsetLeds.y = PianoKeyBig::sizeY * 5.0f / 8.0f;
			addChild(createLightCentered<SmallLight<YellowLight>>(keyPos + offsetLeds, module, ChordKey::KEY_LIGHTS + k * 4 + 2));
			offsetLeds.y = PianoKeyBig::sizeY * 7.0f / 8.0f;
			addChild(createLightCentered<SmallLight<GreenLightIM>>(keyPos + offsetLeds, module, ChordKey::KEY_LIGHTS + k * 4 + 3));
		
		}


		
		// ****** Bottom portion ******

		// Column rulers (horizontal positions)
		static const int col0 = 30;
		static const int col1 = 72;
		static const int col2 = 117;// oct -
		static const int col3 = 158;// oct +
		static const int col4 = 200;// oct disp
		static const int col5 = 245;// cv
		static const int col6 = 282;// gate
		
		// Row rulers (vertical positions)
		static const int rowY = 229;
		static const int rowYd = 34;
		
		// Other constants
		static const int displayHeights = 24; // 22 for 14pt, 24 for 15pt
			
		// Transpose buttons
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(col0, rowY - 16 - 6.429f), module, ChordKey::TRANSPOSEDOWN_PARAM, mode));		
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(col1, rowY - 16 - 6.429f), module, ChordKey::TRANSPOSEUP_PARAM, mode));		
			
		// Index display
		IndexDisplayWidget* indexDisplayWidget = new IndexDisplayWidget(VecPx((col0 + col1) / 2 + 6, rowY + rowYd / 2 - 4) - 6.429f, VecPx(36, displayHeights), module);// 2 characters
		addChild(indexDisplayWidget);
		svgPanel->fb->addChild(new DisplayBackground(indexDisplayWidget->box.pos, indexDisplayWidget->box.size, mode));
		
		// Index input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col0, rowY + rowYd * 2 - 9), true, module, ChordKey::INDEX_INPUT, mode));
		// Index knob
		addParam(createDynamicParamCentered<IMMediumKnob>(VecPx(col1, rowY + rowYd * 2 - 9), module, ChordKey::INDEX_PARAM, mode));	
	
		// Gate input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col0, rowY + rowYd * 3 + 8), true, module, ChordKey::GATE_INPUT, mode));
		// Gate force switch
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(col1, rowY + rowYd * 3 + 8), module, ChordKey::FORCE_PARAM, mode, svgPanel));
	
		// oct buttons, oct displays, gate and cv outputs
		for (int cni = 0; cni < 4; cni++) {
			// Octave buttons
			addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col2, rowY + rowYd * cni), module, ChordKey::OCTDEC_PARAMS + cni, mode));
			addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col3, rowY + rowYd * cni), module, ChordKey::OCTINC_PARAMS + cni, mode));

			// oct displays
			OctDisplayWidget* octDisplayWidget = new OctDisplayWidget(VecPx(col4, rowY + rowYd * cni), VecPx(23, displayHeights), module, cni);// 1 character
			addChild(octDisplayWidget);
			svgPanel->fb->addChild(new DisplayBackground(octDisplayWidget->box.pos, octDisplayWidget->box.size, mode));

			// cv outputs
			addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, rowY + rowYd * cni), false, module, ChordKey::CV_OUTPUTS + cni, mode));
			
			// gate outputs
			addOutput(createDynamicPortCentered<IMPort>(VecPx(col6, rowY + rowYd * cni), false, module, ChordKey::GATE_OUTPUTS + cni, mode));
		}

	}
		
	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			if (e.key == GLFW_KEY_C) {
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					((ChordKey*)module)->interopCopyChord();
					e.consume(this);
					return;
				}
				else if ((e.mods & RACK_MOD_MASK) == (GLFW_MOD_SHIFT | GLFW_MOD_ALT)) {
					((ChordKey*)module)->interopCopySeq();
					e.consume(this);
					return;
				}						
			}
			else if (e.key == GLFW_KEY_V) {
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					((ChordKey*)module)->interopPasteChord();
					e.consume(this);
					return;
				}
				else if ((e.mods & RACK_MOD_MASK) == (GLFW_MOD_SHIFT | GLFW_MOD_ALT)) {
					((ChordKey*)module)->interopPasteSeq();
					e.consume(this);
					return;
				}						
			}
		}
		ModuleWidget::onHoverKey(e);
	}
};

Model *modelChordKey = createModel<ChordKey, ChordKeyWidget>("Chord-Key");
