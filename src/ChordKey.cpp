//***********************************************************************************************
//Chord-based keyboard module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "comp/PianoKey.hpp"


struct ChordKey : Module {
	enum ParamIds {
		ENUMS(OCTINC_PARAMS, 4),
		ENUMS(OCTDEC_PARAMS, 4),
		INDEX_PARAM,
		FORCE_PARAM,
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
	static constexpr float warningTime = 0.7f;// seconds
	
	// Need to save, no reset
	int panelTheme;
	
	
	// Need to save, with reset
	int octs[NUM_CHORDS][4];// -1 to 9 (-1 means not used, i.e. no gate can be emitted)
	int keys[NUM_CHORDS][4];// 0 to 11 for the 12 keys
	int mergeOutputs;// 0 = none, 1 = merge A with B, 2 = merge A with B and C, 3 = merge A with All

	
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
	dsp::BooleanTrigger keyTrigger;
	PianoKeyInfo pkInfo;
	
	
	int getIndex() {
		int index = (int)std::round(params[INDEX_PARAM].getValue() + inputs[INDEX_INPUT].getVoltage() * 12.0f);
		return clamp(index, 0, NUM_CHORDS - 1 );
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
		configParam(FORCE_PARAM, 0.0f, 1.0f, 0.0f, "Force gate on");
		
		pkInfo.showMarks = 4;
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
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

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

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

		resetNonJson();
	}

	
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
			
			// piano keys
			if (keyTrigger.process(pkInfo.gate)) {
				int cni = clamp((int)(pkInfo.vel * 4.0f), 0, 3);
				if (octs[index][cni] >= 0) {
					keys[index][cni] = pkInfo.key;
				}
				else {
					offWarning = (long) (warningTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
			}			
		}// userInputs refresh


		
		
		//********** Outputs and lights **********
		
		// gate and cv outputs
		bool forcedGate = params[FORCE_PARAM].getValue() >= 0.5f;
		float gateOuts[4];
		float cvOuts[4];
		for (int cni = 0; cni < 4; cni++) {
			gateOuts[cni] = ((octs[index][cni] >= 0) && ((inputs[GATE_INPUT].getVoltage() >= 1.0f) || forcedGate))  ? 10.0f : 0.0f;
			cvOuts[cni] = (octs[index][cni] >= 0) ? (((float)(octs[index][cni] - 4)) + ((float)keys[index][cni]) / 12.0f) : 0.0f;
		}
		if (mergeOutputs == 0) {
			outputs[GATE_OUTPUTS + 0].setChannels(1);
			outputs[CV_OUTPUTS + 0].setChannels(1);
			for (int cni = 0; cni < 4; cni++) {			
				outputs[GATE_OUTPUTS + cni].setVoltage(gateOuts[cni]);
				outputs[CV_OUTPUTS + cni].setVoltage(cvOuts[cni]);
			}
		}
		else if (mergeOutputs == 1) {
			outputs[GATE_OUTPUTS + 0].setChannels(2);
			outputs[CV_OUTPUTS + 0].setChannels(2);
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
			outputs[GATE_OUTPUTS + 0].setChannels(3);
			outputs[CV_OUTPUTS + 0].setChannels(3);
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
			outputs[GATE_OUTPUTS + 0].setChannels(4);
			outputs[CV_OUTPUTS + 0].setChannels(4);
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
	}
};


struct ChordKeyWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct OctDisplayWidget : TransparentWidget {
		ChordKey *module;
		int index;
		std::shared_ptr<Font> font;
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f; // 18.2f for 14 pt, 19.7f for 15pt
		
		OctDisplayWidget(Vec _pos, Vec _size, ChordKey *_module, int _index) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			index = _index;
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, textFontSize);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -0.4);

			Vec textPos = Vec(6.7f, textOffsetY);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
			nvgFillColor(args.vg, textColor);
			int octaveNum = module ? module->octs[module->getIndex()][index] : 4;
			char displayStr[2];
			if (octaveNum >= 0) {
				displayStr[0] = 0x30 + (char)(octaveNum);
			}
			else {
				displayStr[0] = '-';
				if (module->offWarning > 0l) {
					bool warningFlashState = calcWarningFlash(module->offWarning, (long) (module->warningTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips));
					if (!warningFlashState) 
						displayStr[0] = 'X';
				}
			}
			displayStr[1] = 0;
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};
	struct IndexDisplayWidget : TransparentWidget {
		ChordKey *module;
		std::shared_ptr<Font> font;
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f; // 18.2f for 14 pt, 19.7f for 15pt
		
		IndexDisplayWidget(Vec _pos, Vec _size, ChordKey *_module) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, textFontSize);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -0.4);

			Vec textPos = Vec(6.7f, textOffsetY);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
			nvgFillColor(args.vg, textColor);
			char displayStr[3];
			int indexNum = module ? module->getIndex() + 1 : 1;
			snprintf(displayStr, 3, "%2u", (unsigned) indexNum);
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};

	
	struct PanelThemeItem : MenuItem {
		ChordKey *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
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
		float valueLocalLast;// must be reset to 0 when enter menu (i.e. constructor)
		int valueIntLocal;
		int valueIntLocalLast;
		float increment;
		  
		TransposeQuantity() {
			valueLocal = 0.0f;
			valueIntLocal = 0;
			valueIntLocalLast = 0;
		}
		float quantize(float cv) {
			return std::round(cv * 12.0f) / 12.0f;
		}
		void setValue(float value) override {
			valueLocal = math::clamp(value, getMinValue(), getMaxValue());; 
			valueIntLocal = (int)(std::round(valueLocal));
			int delta = valueIntLocal - valueIntLocalLast;// delta is number of semitones
			if (delta != 0) {
				int index = module->getIndex();
				for (int cni = 0; cni < 4; cni++) {
					module->keys[index][cni] = ((module->keys[index][cni] + delta) % 12);
				}
				valueIntLocalLast = valueIntLocal;
			}
		}
		float getValue() override {
			return valueLocal;
		}
		float getMinValue() override {return -100.0f;}
		float getMaxValue() override {return 100.0f;}
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

	struct MergeOutputsItem : MenuItem {
		struct MergeOutputsSubItem : MenuItem {
			ChordKey *module;
			int setVal = 0;
			void onAction(const event::Action &e) override {
				module->mergeOutputs = setVal;
			}
		};
		ChordKey *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			MergeOutputsSubItem *merge0Item = createMenuItem<MergeOutputsSubItem>("None", CHECKMARK(module->mergeOutputs == 0));
			merge0Item->module = this->module;
			menu->addChild(merge0Item);

			MergeOutputsSubItem *merge1Item = createMenuItem<MergeOutputsSubItem>("Second", CHECKMARK(module->mergeOutputs == 1));
			merge1Item->module = this->module;
			merge1Item->setVal = 1;
			menu->addChild(merge1Item);

			MergeOutputsSubItem *merge2Item = createMenuItem<MergeOutputsSubItem>("Second and third", CHECKMARK(module->mergeOutputs == 2));
			merge2Item->module = this->module;
			merge2Item->setVal = 2;
			menu->addChild(merge2Item);

			MergeOutputsSubItem *merge3Item = createMenuItem<MergeOutputsSubItem>("Second, third and fourth", CHECKMARK(module->mergeOutputs == 3));
			merge3Item->module = this->module;
			merge3Item->setVal = 3;
			menu->addChild(merge3Item);

			return menu;
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		ChordKey *module = dynamic_cast<ChordKey*>(this->module);
		assert(module);

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

		CopyChordItem *cvCopyItem = createMenuItem<CopyChordItem>("Copy chord");
		cvCopyItem->module = module;
		menu->addChild(cvCopyItem);
		
		PasteChordItem *cvPasteItem = createMenuItem<PasteChordItem>("Paste chord");
		cvPasteItem->module = module;
		menu->addChild(cvPasteItem);	
		
		// transpose
		TransposeSlider *transposeSlider = new TransposeSlider(module);
		transposeSlider->box.size.x = 200.0f;
		menu->addChild(transposeSlider);

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);

		MergeOutputsItem *mergeItem = createMenuItem<MergeOutputsItem>("Poly merge outputs into top note", RIGHT_ARROW);
		mergeItem->module = module;
		menu->addChild(mergeItem);
	}	
	
	
	ChordKeyWidget(ChordKey *module) {
		setModule(module);
		
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/ChordKey.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/ChordKey_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));



		// ****** Top portion (keys) ******

		static const float olx = 16.7f;
		static const float dly = 70.0f / 4.0f;
		static const float dlyd2 = 70.0f / 8.0f;
		
		static const int posWhiteY = 115;
		static const float posBlackY = 40.0f;

		// Black keys
		addChild(createPianoKey<PianoKeyBig>(Vec(37.5f, posBlackY), 1, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++) 
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(37.5f+olx, posBlackY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 1 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(78.5f, posBlackY), 3, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++) 
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(78.5f+olx, posBlackY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 3 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(161.5f, posBlackY), 6, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++) 
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(161.5f+olx, posBlackY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 6 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(202.5f, posBlackY), 8, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++) 
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(202.5f+olx, posBlackY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 8 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(243.5f, posBlackY), 10, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++) 
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(243.5f+olx, posBlackY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 10 * 4 + y));

		// White keys
		addChild(createPianoKey<PianoKeyBig>(Vec(17.5, posWhiteY), 0, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++)
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(17.5+olx, posWhiteY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 0 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(58.5f, posWhiteY), 2, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++)
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(58.5f+olx, posWhiteY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 2 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(99.5f, posWhiteY), 4, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++)
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(99.5f+olx, posWhiteY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 4 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(140.5f, posWhiteY), 5, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++)
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(140.5f+olx, posWhiteY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 5 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(181.5f, posWhiteY), 7, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++)
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(181.5f+olx, posWhiteY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 7 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(222.5f, posWhiteY), 9, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++)
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(222.5f+olx, posWhiteY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 9 * 4 + y));
		addChild(createPianoKey<PianoKeyBig>(Vec(263.5f, posWhiteY), 11, module ? &module->pkInfo : NULL));
		for (int y = 0; y < 4; y++)
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(263.5f+olx, posWhiteY+dlyd2+dly*y), module, ChordKey::KEY_LIGHTS + 11 * 4 + y));
		
		
		// ****** Bottom portion ******

		// Column rulers (horizontal positions)
		static const int col0 = 30;
		static const int col1 = 72;
		static const int col2 = 115;// oct -
		static const int col3 = 158;// oct +
		static const int col4 = 200;// oct disp
		static const int col5 = 245;// cv
		static const int col6 = 282;// gate
		
		// Row rulers (vertical positions)
		static const int rowY = 229;
		static const int rowYd = 34;
		
		// Other constants
		static const int displayHeights = 24; // 22 for 14pt, 24 for 15pt
			
		// Index display
		addChild(new IndexDisplayWidget(Vec((col0 + col1) / 2, rowY), Vec(36, displayHeights), module));// 2 characters
		
		// Index input
		addInput(createDynamicPortCentered<IMPort>(Vec(col0, rowY + rowYd * 1), true, module, ChordKey::INDEX_INPUT, module ? &module->panelTheme : NULL));
		// Index knob
		addParam(createDynamicParamCentered<IMMediumKnob<false, true>>(Vec(col1, rowY + rowYd * 1), module, ChordKey::INDEX_PARAM, module ? &module->panelTheme : NULL));	
	
		// Gate input
		addInput(createDynamicPortCentered<IMPort>(Vec(col0, rowY + rowYd * 3), true, module, ChordKey::GATE_INPUT, module ? &module->panelTheme : NULL));
		// Gate force switch
		addParam(createParamCentered<CKSS>(Vec(col1, rowY + rowYd * 3), module, ChordKey::FORCE_PARAM));
	
		// oct buttons, oct displays, gate and cv outputs
		for (int cni = 0; cni < 4; cni++) {
			// Octave buttons
			addParam(createDynamicParamCentered<IMBigPushButton>(Vec(col2, rowY + rowYd * cni), module, ChordKey::OCTDEC_PARAMS + cni, module ? &module->panelTheme : NULL));
			addParam(createDynamicParamCentered<IMBigPushButton>(Vec(col3, rowY + rowYd * cni), module, ChordKey::OCTINC_PARAMS + cni, module ? &module->panelTheme : NULL));

			// oct displays
			addChild(new OctDisplayWidget(Vec(col4, rowY + rowYd * cni), Vec(23, displayHeights), module, cni));// 1 character

			// cv outputs
			addOutput(createDynamicPortCentered<IMPort>(Vec(col5, rowY + rowYd * cni), false, module, ChordKey::CV_OUTPUTS + cni, module ? &module->panelTheme : NULL));
			
			// gate outputs
			addOutput(createDynamicPortCentered<IMPort>(Vec(col6, rowY + rowYd * cni), false, module, ChordKey::GATE_OUTPUTS + cni, module ? &module->panelTheme : NULL));
		}

	}
	
	void step() override {
		if (module) {
			panel->visible = ((((ChordKey*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((ChordKey*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelChordKey = createModel<ChordKey, ChordKeyWidget>("Chord-Key");
