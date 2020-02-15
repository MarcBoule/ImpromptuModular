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
	
	// Need to save, no reset
	int panelTheme;
	
	
	// Need to save, with reset
	int octs[NUM_CHORDS][4];// -1 to 9 (-1 means not used, i.e. no gate can be emitted)
	float cvs[NUM_CHORDS][4];
	
	
	// No need to save, with reset
	unsigned long noteLightCounter;// 0 when no key to light, downward step counter timer when key lit


	// No need to save, no reset
	RefreshCounter refresh;
	Trigger octIncTriggers[4];
	Trigger octDecTriggers[4];
	Trigger maxVelTrigger;
	dsp::BooleanTrigger keyTrigger;
	PianoKeyInfo pkInfo;
	
	int getIndex() {
		int index = (int)std::round(params[INDEX_PARAM].getValue() + inputs[INDEX_INPUT].getVoltage());
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
		configParam(INDEX_PARAM, 0.0f, 24.0f, 0.0f, "Index");
			
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	void onReset() override {
		for (int ci = 0; ci < 4; ci++) { // chord index
			for (int cni = 0; cni < 4; cni++) {// chord note index
				octs[ci][cni] = 4;
				cvs[ci][cni] = 0.0f;
			}
		}
		resetNonJson();
	}
	void resetNonJson() {
		noteLightCounter = 0ul;
	}

	void onRandomize() override {
		for (int ci = 0; ci < 4; ci++) { // chord index
			for (int cni = 0; cni < 4; cni++) {// chord note index
				octs[ci][cni] = random::u32() % 10;
				cvs[ci][cni] = ((float)(octs[ci][cni]  - 4)) + ((float)(random::u32() % 12)) / 12.0f;
			}
		}					
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// octs
		json_t *cvJ = json_array();
		for (int ci = 0; ci < NUM_CHORDS; ci++) {// chord index
			for (int cni = 0; cni < 4; cni++) {// chord note index
				json_array_insert_new(cvJ, cni + (ci * 4), json_integer(octs[ci][cni]));
			}
		}
		json_object_set_new(rootJ, "octs", cvJ);
		
		// cvs
		json_t *octJ = json_array();
		for (int ci = 0; ci < NUM_CHORDS; ci++) {// chord index
			for (int cni = 0; cni < 4; cni++) {// chord note index
				json_array_insert_new(octJ, cni + (ci * 4), json_real(cvs[ci][cni]));
			}
		}
		json_object_set_new(rootJ, "cvs", octJ);
		
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

		// cvs
		json_t *cvJ = json_object_get(rootJ, "cvs");
		if (cvJ) {
			for (int ci = 0; ci < NUM_CHORDS; ci++) {// chord index
				for (int cni = 0; cni < 4; cni++) {// chord note index
					json_t *cvArrayJ = json_array_get(cvJ, cni + (ci * 4));
					if (cvArrayJ)
						cvs[ci][cni] = json_number_value(cvArrayJ);
				}
			}
		}

		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		
		//********** Buttons, knobs, switches and inputs **********
		
		if (refresh.processInputs()) {

		}// userInputs refresh


		
		
		//********** Outputs and lights **********
		
		// gate and cv outputs
		for (int ci = 0; ci < 4; ci++) {
			outputs[GATE_OUTPUTS + ci].setVoltage(0.0f);
			outputs[CV_OUTPUTS + ci].setVoltage(0.0f);
		}
		

		// lights
		if (refresh.processLights()) {

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

			Vec textPos = Vec(5.7f, textOffsetY);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
			nvgFillColor(args.vg, textColor);
			int octaveNum = module ? module->octs[module->getIndex()][index] : 4;
			char displayStr[2];
			displayStr[0] = 0x30 + (char)(octaveNum);
			displayStr[1] = 0;
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};
	
	
	struct PanelThemeItem : MenuItem {
		ChordKey *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
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
		
		// menu->addChild(new MenuLabel());// empty line
		
		// MenuLabel *settingsLabel = new MenuLabel();
		// settingsLabel->text = "Settings";
		// menu->addChild(settingsLabel);
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

		static const int offsetKeyLEDx = 12;
		static const int offsetKeyLEDy = 41;// 32
		
		static const int posWhiteY = 115;
		static const int posBlackY = 40;

		// Black keys
		addChild(createPianoKey<PianoKeyBig>(Vec(37.5f, posBlackY), 1, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(37.5f+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 1));
		addChild(createPianoKey<PianoKeyBig>(Vec(78.5f, posBlackY), 3, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(78.5f+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 3));
		addChild(createPianoKey<PianoKeyBig>(Vec(161.5f, posBlackY), 6, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(161.5f+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 6));
		addChild(createPianoKey<PianoKeyBig>(Vec(202.5f, posBlackY), 8, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(202.5f+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 8));
		addChild(createPianoKey<PianoKeyBig>(Vec(243.5f, posBlackY), 10, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(243.5f+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 10));

		// White keys
		addChild(createPianoKey<PianoKeyBig>(Vec(17.5, posWhiteY), 0, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(17.5+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 0));
		addChild(createPianoKey<PianoKeyBig>(Vec(58.5f, posWhiteY), 2, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(58.5f+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 2));
		addChild(createPianoKey<PianoKeyBig>(Vec(99.5f, posWhiteY), 4, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(99.5f+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 4));
		addChild(createPianoKey<PianoKeyBig>(Vec(140.5f, posWhiteY), 5, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(140.5f+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 5));
		addChild(createPianoKey<PianoKeyBig>(Vec(181.5f, posWhiteY), 7, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(181.5f+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 7));
		addChild(createPianoKey<PianoKeyBig>(Vec(222.5f, posWhiteY), 9, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(222.5f+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 9));
		addChild(createPianoKey<PianoKeyBig>(Vec(263.5f, posWhiteY), 11, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(263.5f+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, ChordKey::KEY_LIGHTS + 11));
		
		
		// ****** Bottom portion ******

		// Column rulers (horizontal positions)
		static const int col0 = 20;
		static const int col1 = 20;
		static const int col2 = 115;
		static const int col3 = 158;
		static const int col4 = 200;
		static const int col5 = 245;
		static const int col6 = 282;
		
		// Row rulers (vertical positions)
		static const int rowY = 229;
		static const int rowYd = 34;
		
		// Other constants
		static const int displayHeights = 24; // 22 for 14pt, 24 for 15pt

		
		// oct buttons, oct displays, gate and cv outputs
		for (int cni = 0; cni < 4; cni++) {
			// Octave buttons
			addParam(createDynamicParamCentered<IMBigPushButton>(Vec(col2, rowY + rowYd * cni), module, ChordKey::OCTDEC_PARAMS + cni, module ? &module->panelTheme : NULL));
			addParam(createDynamicParamCentered<IMBigPushButton>(Vec(col3, rowY + rowYd * cni), module, ChordKey::OCTINC_PARAMS + cni, module ? &module->panelTheme : NULL));

			// oct displays
			addChild(new OctDisplayWidget(Vec(col4, rowY + rowYd * cni), Vec(22, displayHeights), module, cni));// 1 character

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
