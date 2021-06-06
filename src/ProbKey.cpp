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



class ProbKernel {
	public: 
	
	static constexpr float IDEM_CV = -100.0f;
	static constexpr float MAX_ANCHOR_DELTA = 4.0f;// number of octaves, must be a positive integer
	
	private:
	
	float noteProbs[12];// [0.0f : 1.0f]
	float noteAnchors[12];// [0.0f : 1.0f];  0.5f=oct"4", 1.0f=oct"4+MAX_ANCHOR_DELTA"; not quantized
	float noteRanges[7];// [0] is -3, [6] is +3
	
	
	public:
	
	void reset() {
		for (int i = 0; i < 12; i++) {
			noteProbs[i] = 0.0f;
			noteAnchors[i] = octToAnchor(0.0f);
		}
		noteProbs[0] = 0.25f;
		noteProbs[4] = 0.25f;
		noteProbs[7] = 0.25f;
		
		for (int i = 0; i < 7; i++) {
			noteRanges[i] = 0.0f;
		}
		noteRanges[3] = 1.0f;
	}
	
	void randomize() {
		
	}
	
	void dataToJson(json_t *rootJ, int id) {
		// noteProbs
		json_t *noteProbsJ = json_array();
		for (int i = 0; i < 12; i++) {
			json_array_insert_new(noteProbsJ, i, json_real(noteProbs[i]));
		}
		json_object_set_new(rootJ, string::f("noteProbs%i", id).c_str(), noteProbsJ);
		
		// noteAnchors
		json_t *noteAnchorsJ = json_array();
		for (int i = 0; i < 12; i++) {
			json_array_insert_new(noteAnchorsJ, i, json_real(noteAnchors[i]));
		}
		json_object_set_new(rootJ, string::f("noteAnchors%i", id).c_str(), noteAnchorsJ);
		
		// noteRanges
		json_t *noteRangesJ = json_array();
		for (int i = 0; i < 7; i++) {
			json_array_insert_new(noteRangesJ, i, json_real(noteRanges[i]));
		}
		json_object_set_new(rootJ, string::f("noteRanges%i", id).c_str(), noteRangesJ);
		
	}
	
	void dataFromJson(json_t *rootJ, int id) {
		// noteProbs
		json_t *noteProbsJ = json_object_get(rootJ, string::f("noteProbs%i", id).c_str());
		if (noteProbsJ) {
			for (int i = 0; i < 12; i++) {
				json_t *noteProbsArrayJ = json_array_get(noteProbsJ, i);
				if (noteProbsArrayJ) {
					noteProbs[i] = json_number_value(noteProbsArrayJ);
				}
			}
		}
		
		// noteAnchors
		json_t *noteAnchorsJ = json_object_get(rootJ, string::f("noteAnchors%i", id).c_str());
		if (noteAnchorsJ) {
			for (int i = 0; i < 12; i++) {
				json_t *noteAnchorsArrayJ = json_array_get(noteAnchorsJ, i);
				if (noteAnchorsArrayJ) {
					noteAnchors[i] = json_number_value(noteAnchorsArrayJ);
				}
			}
		}
		
		// noteRanges
		json_t *noteRangesJ = json_object_get(rootJ, string::f("noteRanges%i", id).c_str());
		if (noteRangesJ) {
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
	void setNoteProb(int note, float prob) {
		noteProbs[note] = prob;
	}
	void setNoteAnchor(int note, float anch) {
		noteAnchors[note] = anch;
	}
	void setNoteRange(int note12, float range) {
		noteRanges[key12to7(note12)] = range;
	}
	
	
	bool probNonNull(int note) {
		return noteProbs[note] > 0.0f;
	}
	
	
	float calcRandomCv() {
		// not optimized for audio rate
		// returns a cv value or IDEM_CV when a note gets randomly skipped (only possible when sum of probs < 1)
		
		// generate a note according to noteProbs (base note only, C4=0 to B4)
		float cumulProbs[12];
		cumulProbs[0] = noteProbs[0];
		for (int i = 1; i < 12; i++) {
			cumulProbs[i] = cumulProbs[i - 1] + noteProbs[i];
		}
		
		float dice = random::uniform() * std::max(cumulProbs[11], 1.0f);
		int note = 0;
		for (; note < 12; note++) {
			if (dice < cumulProbs[note]) {
				break;
			}
		}
		
		float cv;
		if (note < 12) {
			cv = ((float)note) / 12.0f;
			
			cv += anchorToOct(noteAnchors[note]);
			
			// probabilistically transpose note according to ranges
			float cumulRanges[7];
			cumulRanges[0] = noteRanges[0];
			for (int i = 1; i < 7; i++) {
				cumulRanges[i] = cumulRanges[i - 1] + noteRanges[i];
			}
			
			float dice2 = random::uniform() * cumulRanges[6];
			int oct = 0;
			for (; oct < 7; oct++) {
				if (dice2 < cumulRanges[oct]) {
					break;
				}
			}
			if (oct < 7) {
				oct -= 3;
				cv += (float)oct;
			}
		}
		else {
			cv = IDEM_CV;
		}

		return cv;
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

	static const int MAX_LENGTH = 16;
	
	
	private:
	
	float shiftReg[MAX_LENGTH];// holds CVs or ProbKernel::IDEM_CV
	float lastCv;// sample and hold, must never hold ProbKernel::IDEM_CV
	
	
	public:
	
	void reset() {
		for (int i = 0; i < MAX_LENGTH; i++) {
			shiftReg[i] = 0.0f;
		}
		lastCv = 0.0f;
	}
	
	void randomize() {
		// none
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
	}

	void shiftWithHold() {
		for (int i = MAX_LENGTH - 1; i > 0; i--) {
			shiftReg[i] = shiftReg[i - 1];
		}
	}
		
	void shiftWithInsertNew(float newCv) {
		shiftWithHold();
		shiftReg[0] = newCv;
		if (shiftReg[0] != ProbKernel::IDEM_CV) {
			lastCv = shiftReg[0];
		}
	}
		
	void shiftWithRecycle(int length0) {
		shiftWithInsertNew(shiftReg[length0]);
	}
	
	float getCv() {
		return lastCv;
	}
	bool getGateEnable() {
		return shiftReg[0] != ProbKernel::IDEM_CV;
	}
};


// ----------------------------------------------------------------------------


struct ProbKey : Module {
	enum ParamIds {
		INDEX_PARAM,
		LENGTH_PARAM,
		LOCK_PARAM, // higher priority than hold
		OFFSET_PARAM,
		SQUASH_PARAM,
		ENUMS(MODE_PARAMS, 3), // see ModeIds enum
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
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(KEY_LIGHTS, 12 * 4 * 2),// room for GreenRed
		ENUMS(MODE_LIGHTS, 3 * 2), // see ModeIds enum; room for GreenRed
		NUM_LIGHTS
	};
	
	
	// Expander
	// none
		
	// Constants
	enum ModeIds {MODE_PROB, MODE_ANCHOR, MODE_RANGE};
	static const int NUM_INDEXES = 25;// C4 to C6 incl
	static const int MAX_LENGTH = 16;
	
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int editMode;
	ProbKernel probKernels[NUM_INDEXES];
	OutputKernel outputKernels[PORT_MAX_CHANNELS];
	
	// No need to save, with reset
	// none

	// No need to save, no reset
	RefreshCounter refresh;
	PianoKeyInfo pkInfo;
	Trigger modeTriggers[3];
	Trigger gateInTriggers[PORT_MAX_CHANNELS];
	
	
	int getIndex() {
		int index = (int)std::round(params[INDEX_PARAM].getValue() + inputs[INDEX_INPUT].getVoltage() * 12.0f);
		return clamp(index, 0, NUM_INDEXES - 1 );
	}
	int getLength0() {// 0 indexed
		int length = (int)std::round(params[LENGTH_PARAM].getValue() + inputs[LENGTH_INPUT].getVoltage() * 12.0f);
		return clamp(length, 0, OutputKernel::MAX_LENGTH - 1 );
	}


	ProbKey() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(INDEX_PARAM, 0.0f, 24.0f, 0.0f, "Index", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset
		configParam(LENGTH_PARAM, 0.0f, (float)(OutputKernel::MAX_LENGTH - 1), (float)(OutputKernel::MAX_LENGTH - 1), "Lock length", "", 0.0f, 1.0f, 1.0f);
		configParam(LOCK_PARAM, 0.0f, 1.0f, 0.0f, "Lock (loop) pattern", " %", 0.0f, 100.0f, 0.0f);
		configParam(OFFSET_PARAM, -3.0f, 3.0f, 0.0f, "Range offset", "");
		configParam(SQUASH_PARAM, 0.0f, 1.0f, 0.0f, "Range squash", "");
		configParam(MODE_PARAMS + MODE_PROB, 0.0f, 1.0f, 0.0f, "Edit note probabilities", "");
		configParam(MODE_PARAMS + MODE_ANCHOR, 0.0f, 1.0f, 0.0f, "Edit note octave refs", "");
		configParam(MODE_PARAMS + MODE_RANGE, 0.0f, 1.0f, 0.0f, "Edit octave range", "");
		
		pkInfo.showMarks = 1;
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}


	void onReset() override {
		editMode = MODE_PROB;
		for (int i = 0; i < NUM_INDEXES; i++) {
			probKernels[i].reset();
		}
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			outputKernels[i].reset();
		}
		resetNonJson();
	}
	void resetNonJson() {
		// none
	}


	void onRandomize() override {
		// only randomize the currently selected probKernel
		probKernels[getIndex()].randomize();
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// editMode
		json_object_set_new(rootJ, "editMode", json_integer(editMode));

		// probKernels
		for (int i = 0; i < NUM_INDEXES; i++) {
			probKernels[i].dataToJson(rootJ, i);
		}
		
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

		// probKernels
		for (int i = 0; i < NUM_INDEXES; i++) {
			probKernels[i].dataFromJson(rootJ, i);
		}

		// outputKernels
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			outputKernels[i].dataFromJson(rootJ, i);
		}

		resetNonJson();
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
				}
			}
			
			// piano keys if applicable 
			if (pkInfo.gate && !pkInfo.isRightClick) {
				if (editMode == MODE_PROB) {
					probKernels[index].setNoteProb(pkInfo.key, 1.0f - pkInfo.vel);
				}
				else if (editMode == MODE_ANCHOR) {
					if (probKernels[index].probNonNull(pkInfo.key)) {
						probKernels[index].setNoteAnchor(pkInfo.key, 1.0f - pkInfo.vel);
					}
				}
				else {// editMode == MODE_RANGE
					if (pkInfo.key != 1 && pkInfo.key != 3 && pkInfo.key != 6 && pkInfo.key != 8 && pkInfo.key != 10) {
						probKernels[index].setNoteRange(pkInfo.key, 1.0f - pkInfo.vel);
					}
					
				}
			}
		}// userInputs refresh


		
		
		//********** Outputs and lights **********
		
		for (int i = 0; i < inputs[GATE_INPUT].getChannels(); i ++) {
			// gate input triggers
			if (gateInTriggers[i].process(inputs[GATE_INPUT].getVoltage(i))) {
				// got rising edge on gate input poly channel i

				if (params[LOCK_PARAM].getValue() > random::uniform()) {
					// recycle CV
					outputKernels[i].shiftWithRecycle(length0);
				}
				else {
					// generate new random CV (taking hold into account though)
					bool hold = inputs[HOLD_INPUT].isConnected() && inputs[HOLD_INPUT].getVoltage(std::min(i, inputs[HOLD_INPUT].getChannels())) > 1.0f;
					if (hold) {
						outputKernels[i].shiftWithHold();
					}
					else {
						float newCv = probKernels[index].calcRandomCv();
						outputKernels[i].shiftWithInsertNew(newCv);
					}
				}
			}
			
			
			// output CV and gate
			
			outputs[CV_OUTPUT].setVoltage(outputKernels[i].getCv(), i);
			float gateOut = outputKernels[i].getGateEnable() ? inputs[GATE_INPUT].getVoltage(i) : 0.0f;
			outputs[GATE_OUTPUT].setVoltage(gateOut, i);
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
			if (editMode == MODE_PROB) {
				for (int i = 0; i < 12; i++) {
					setKeyLightsProb(i, probKernels[index].getNoteProb(i));
				}
			}
			else if (editMode == MODE_ANCHOR) {
				for (int i = 0; i < 12; i++) {
					setKeyLightsAnchor(i, probKernels[index].getNoteAnchor(i), probKernels[index].probNonNull(i));
				}
			}
			else {
				setKeyLightsRange(index);
			}
		}// processLights()
	}
	
	void setKeyLightsProb(int key, float prob) {
		for (int j = 0; j < 4; j++) {
			lights[KEY_LIGHTS + key * 4 * 2 + j * 2 + 0].setBrightness(prob * 4.0f - (float)j);
			lights[KEY_LIGHTS + key * 4 * 2 + j * 2 + 1].setBrightness(0.0f);
		}
	}
	void setKeyLightsAnchor(int key, float anch, bool active) {
		if (active) {
			anch = ProbKernel::quantizeAnchor(anch);
			for (int j = 0; j < 4; j++) {
				lights[KEY_LIGHTS + key * 4 * 2 + j * 2 + 0].setBrightness(0.0f);
				lights[KEY_LIGHTS + key * 4 * 2 + j * 2 + 1].setBrightness(anch * 4.0f - (float)j);
			}
		}
		else {
			for (int j = 0; j < 4 * 2; j++) {
				lights[KEY_LIGHTS + key * 4 * 2 + j].setBrightness(0.0f);
			}
		}
	}
	void setKeyLightsRange(int index) {
		for (int i = 0; i < 12; i++) {
			float range = 0.0f;
			if (i != 1 && i != 3 && i != 6 && i != 8 && i != 10) {
				range = probKernels[index].getNoteRange(i);
			}
			for (int j = 0; j < 4; j++) {
				lights[KEY_LIGHTS + i * 4 * 2 + j * 2 + 0].setBrightness(range * 4.0f - (float)j);
				lights[KEY_LIGHTS + i * 4 * 2 + j * 2 + 1].setBrightness(range * 4.0f - (float)j);
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
			nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(args.vg, textColor);
			char displayStr[4];
			unsigned dispVal = 1;
			// if (module) {
				// dispVal = (unsigned)(module->params[BigButtonSeq2::DISPMODE_PARAM].getValue() < 0.5f ?  module->length : module->indexStep + 1);
			// }
			snprintf(displayStr, 4, "%3u",  dispVal);
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};

	void appendContextMenu(Menu *menu) override {
		ProbKey *module = dynamic_cast<ProbKey*>(this->module);
		assert(module);
				
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *darkItem = createMenuItem<PanelThemeItem>(darkPanelID, CHECKMARK(module->panelTheme));
		darkItem->module = module;
		menu->addChild(darkItem);
		
		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));
		
		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *actionsLabel = new MenuLabel();
		actionsLabel->text = "Actions";
		menu->addChild(actionsLabel);

		// todo

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);

		// todo
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

		static const int ex = 15.0f;
		static const float olx = 16.7f;
		static const float dly = 70.0f / 4.0f;
		static const float dlyd2 = 70.0f / 8.0f;
		
		static const int posWhiteY = 115;
		static const float posBlackY = 40.0f;


		#define DROP_LIGHTS(xLoc, yLoc, pNum) \
			addChild(createLightCentered<SmallLight<GreenRedLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*3), module, ProbKey::KEY_LIGHTS + pNum * (4 * 2) + 0 * 2)); \
			addChild(createLightCentered<SmallLight<GreenRedLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*2), module, ProbKey::KEY_LIGHTS + pNum * (4 * 2) + 1 * 2)); \
			addChild(createLightCentered<SmallLight<GreenRedLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*1), module, ProbKey::KEY_LIGHTS + pNum * (4 * 2) + 2 * 2)); \
			addChild(createLightCentered<SmallLight<GreenRedLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*0), module, ProbKey::KEY_LIGHTS + pNum * (4 * 2) + 3 * 2));

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
		
		static const float row0 = 82.0f;
		static const float row1 = 96.0f;
		static const float row2 = 114.0f;
		
		static const float col0 = 11.0f;
		static const float col1 = 30.0f;
		const float col2 = 116.84f * 0.5f;
		const float col3 = 116.84f - col1;
		const float col4 = 116.84f - col0;
		
		
		// **** Left side ****
		
		// Index knob and input
		addParam(createDynamicParamCentered<IMMediumKnob<false, true>>(mm2px(Vec(col0, row0)), module, ProbKey::INDEX_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, row1)), true, module, ProbKey::INDEX_INPUT, module ? &module->panelTheme : NULL));

		// Gate input
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, row2)), true, module, ProbKey::GATE_INPUT, module ? &module->panelTheme : NULL));
	

		// Length knob and input
		addParam(createDynamicParamCentered<IMMediumKnob<false, true>>(mm2px(Vec(col1, row0)), module, ProbKey::LENGTH_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row1)), true, module, ProbKey::LENGTH_INPUT, module ? &module->panelTheme : NULL));

		// Hold input
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row2)), true, module, ProbKey::HOLD_INPUT, module ? &module->panelTheme : NULL));
		
		
		// **** Center ****

		// Mode led-button - MODE_PROB
		static constexpr float mdx = 11.5f;
		static constexpr float mdy = 1.75f;
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col2 - mdx, row0 - mdy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_PROB));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(col2 - mdx, row0 - mdy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_PROB * 2));

		// Mode led-button - MODE_ANCHOR
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col2 + mdx, row0 - mdy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_ANCHOR));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(col2 + mdx, row0 - mdy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_ANCHOR * 2));

		// Mode led-button - MODE_RANGE
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col2, row0 - mdy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_RANGE));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(col2, row0 - mdy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_RANGE * 2));


		// Lock knob and input
		addParam(createDynamicParamCentered<IMBigKnob<false, false>>(mm2px(Vec(col2 - 9.0f, row1)), module, ProbKey::LOCK_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2 + 9.0f, row1)), true, module, ProbKey::LOCK_INPUT, module ? &module->panelTheme : NULL));

		// Main display
		MainDisplayWidget *displayMain = new MainDisplayWidget();
		displayMain->box.size = VecPx(55, 30);// 3 characters
		displayMain->box.pos = mm2px(Vec(col2, row2 - 2.0f)).minus(displayMain->box.size.div(2));
		displayMain->module = module;
		addChild(displayMain);



		// **** Right side ****

		// Offset knob and input
		addParam(createDynamicParamCentered<IMMediumKnob<false, false>>(mm2px(Vec(col3, row0)), module, ProbKey::OFFSET_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col3, row1)), true, module, ProbKey::OFFSET_INPUT, module ? &module->panelTheme : NULL));

		// CV output
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col3, row2)), false, module, ProbKey::CV_OUTPUT, module ? &module->panelTheme : NULL));
	

		// Squash knob and input
		addParam(createDynamicParamCentered<IMMediumKnob<false, false>>(mm2px(Vec(col4, row0)), module, ProbKey::SQUASH_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col4, row1)), true, module, ProbKey::SQUASH_INPUT, module ? &module->panelTheme : NULL));

		// Gate output
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col4, row2)), false, module, ProbKey::GATE_OUTPUT, module ? &module->panelTheme : NULL));
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
