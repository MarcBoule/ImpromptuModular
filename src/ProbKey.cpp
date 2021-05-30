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
	float noteProbs[12];
	float noteAnchors[12];
	float noteRanges[7];
	
	
	public:
	
	void reset() {
		
	}
	
	void randomize() {
		
	}
	
	void dataToJson(json_t *rootJ) {
		
	}
	
	void dataFromJson(json_t *rootJ) {
		
	}
};



struct ProbKey : Module {
	enum ParamIds {
		INDEX_PARAM,
		LENGTH_PARAM,
		LOCK_PARAM,
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
		HOLD_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(KEY_LIGHTS, 12 * 4 * 2),// room for GreenRed
		ENUMS(MODE_LIGHTS, 3), // see ModeIds enum
		NUM_LIGHTS
	};
	
	
	// Expander
	// none
		
	// Constants
	enum ModeIds {MODE_PROB, MODE_ANCHOR, MODE_RANGE};
	static const int NUM_INDEXES = 25;// C4 to C6 incl
	
	// Need to save, no reset
	int panelTheme;
	
	
	// Need to save, with reset
	ProbKernel probKernel[NUM_INDEXES];

	
	// No need to save, with reset

	// No need to save, no reset
	RefreshCounter refresh;
	PianoKeyInfo pkInfo;
	
	
	int getIndex() {
		int index = (int)std::round(params[INDEX_PARAM].getValue() + inputs[INDEX_INPUT].getVoltage() * 12.0f);
		return clamp(index, 0, NUM_INDEXES - 1 );
	}


	ProbKey() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(INDEX_PARAM, 0.0f, 24.0f, 0.0f, "Index", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset
		configParam(LENGTH_PARAM, 0.0f, 15.0f, 15.0f, "Lock length", "", 0.0f, 1.0f, 1.0f);
		configParam(LOCK_PARAM, 0.0f, 1.0f, 0.0f, "Lock (loop) pattern", " %");
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
		for (int i = 0; i < NUM_INDEXES; i++) {
			probKernel[i].reset();
		}
		resetNonJson();
	}
	void resetNonJson() {
		probKernel[getIndex()].randomize();
	}

	void onRandomize() override {
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// probKernel
		for (int i = 0; i < NUM_INDEXES; i++) {
			probKernel[i].dataToJson(rootJ);
		}
		
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) {
			panelTheme = json_integer_value(panelThemeJ);
		}

		// probKernel
		for (int i = 0; i < NUM_INDEXES; i++) {
			probKernel[i].dataFromJson(rootJ);
		}

		resetNonJson();
	}
		
		
	void process(const ProcessArgs &args) override {		
		int index = getIndex();
		
		//********** Buttons, knobs, switches and inputs **********
		
		if (refresh.processInputs()) {
			
		}// userInputs refresh


		
		
		//********** Outputs and lights **********
		

		
		// lights
		if (refresh.processLights()) {

		}// processLights()
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
			unsigned dispVal = 128;
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
			addChild(createLightCentered<SmallLight<GreenRedLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*0), module, ProbKey::KEY_LIGHTS + pNum * (4 * 2) + 0 * 2)); \
			addChild(createLightCentered<SmallLight<GreenRedLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*1), module, ProbKey::KEY_LIGHTS + pNum * (4 * 2) + 1 * 2)); \
			addChild(createLightCentered<SmallLight<GreenRedLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*2), module, ProbKey::KEY_LIGHTS + pNum * (4 * 2) + 2 * 2)); \
			addChild(createLightCentered<SmallLight<GreenRedLight>>(VecPx(xLoc+ex+olx, yLoc+dlyd2+dly*3), module, ProbKey::KEY_LIGHTS + pNum * (4 * 2) + 3 * 2));

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
		static constexpr float mdy = 1.5f;
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col2 - mdx, row0 - mdy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_PROB));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(col2 - mdx, row0 - mdy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_PROB));

		// Mode led-button - MODE_ANCHOR
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col2 + mdx, row0 - mdy)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_ANCHOR));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(col2 + mdx, row0 - mdy)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_ANCHOR));

		// Mode led-button - MODE_RANGE
		addParam(createParamCentered<LEDButton>(mm2px(Vec(col2, row0 - mdy - 5.0f)), module, ProbKey::MODE_PARAMS + ProbKey::MODE_RANGE));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(col2, row0 - mdy - 5.0f)), module, ProbKey::MODE_LIGHTS + ProbKey::MODE_RANGE));


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
