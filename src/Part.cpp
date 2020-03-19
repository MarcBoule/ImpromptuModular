//***********************************************************************************************
//Polyphonic gate splitter module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
// The idea for this module was provided by Andre Belt
// https://community.vcvrack.com/t/module-ideas/7175/231
// 
//***********************************************************************************************


#include "ImpromptuModular.hpp"

struct Part : Module {
	enum ParamIds {
		SPLIT_PARAM,
		MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		GATE_INPUT,
		SPLIT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LOW_OUTPUT,
		HIGH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
		
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	bool showSharp;

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	RefreshCounter refresh;


	float getSplitValue() {return clamp(params[SPLIT_PARAM].getValue() + inputs[SPLIT_INPUT].getVoltage(), -10.0f, 10.0f);}


	Part() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(SPLIT_PARAM, -10.0f, 10.0f, 0.0f, "Split point");
		configParam(MODE_PARAM, 0.0f, 1.0f, 0.0f, "Display mode");// default is 0.0f meaning left position (= V)
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		showSharp = true;
		resetNonJson();
	}
	void resetNonJson() {
	}
	
	
	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// showSharp
		json_object_set_new(rootJ, "showSharp", json_boolean(showSharp));
		
		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
		
		// showSharp
		json_t *showSharpJ = json_object_get(rootJ, "showSharp");
		if (showSharpJ)
			showSharp = json_is_true(showSharpJ);
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		int numChan = inputs[GATE_INPUT].getChannels();
		
		if (refresh.processInputs()) {
			outputs[LOW_OUTPUT].setChannels(numChan);
			outputs[HIGH_OUTPUT].setChannels(numChan);
		}// userInputs refresh
		
		
		for (int c = 0; c < numChan; c++) {
			bool isHigh = inputs[CV_INPUT].getVoltage(c) >= params[SPLIT_PARAM].getValue();// unconnected CV_INPUT or insufficient channels will cause 0.0f to be used
			float inGate = inputs[GATE_INPUT].getVoltage(c);// unconnected GATE_INPUT or insufficient channels will cause 0.0f to be used
			outputs[LOW_OUTPUT].setVoltage(isHigh ? 0.0f : inGate, c);
			outputs[HIGH_OUTPUT].setVoltage(isHigh ? inGate : 0.0f, c);
		}
		
		
		// lights
		if (refresh.processLights()) {
			// none, but need this since refresh counter is stepped in processLights()
		}
	}
};


struct PartWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct SplitDisplayWidget : TransparentWidget {
		Part *module;
		std::shared_ptr<Font> font;
		char displayStr[5 + 1];// room for two chars left of decimal point, then decimal point, then two chars right of decimal point, plus null
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f;
		
		SplitDisplayWidget(Vec _pos, Vec _size, Part *_module) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void draw(const DrawArgs &args) override {
			static const float offsetXfrac = 16.5f;
			NVGcolor textColor = prepareDisplay(args.vg, &box, textFontSize);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -0.4);

			Vec textPos = Vec(6.3f, textOffsetY);
			printText();
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~~", NULL);
			nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, ".~~", NULL);
			nvgFillColor(args.vg, textColor);
			nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, &displayStr[2], NULL);// print decimal point and two chars to the right of decimal point
			displayStr[2] = 0;
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);// print two chars to the left of decimal point
		}

		void printText() {
			if (module == NULL) {
				snprintf(displayStr, 6, " 0.00");
			}
			else {
				float cvValPrint = module->getSplitValue();
				if (module->params[Part::MODE_PARAM].getValue() < 0.5f) {
					// Display is -10 to 10V
					bool neg = cvValPrint <= -0.005f;
					cvValPrint = std::fabs(cvValPrint);
					if (cvValPrint >= 9.995f) {
						snprintf(displayStr, 6, "   10");
						if (neg) {
							displayStr[1] = '-';
						}
					}
					else {
						snprintf(displayStr, 6, " %3.2f", cvValPrint);// Three-wide, two positions after the decimal, left-justified
						displayStr[2] = '.';// in case locals in printf
						if (neg) {
							displayStr[0] = '-';
						}
					}
				}
				else {
					// Display is semitone
					printNote(cvValPrint, &displayStr[0], module->showSharp);// given str pointer must be 4 chars (3 display and one end of string)
					// Move accidental over into decimal portion and remove decimal point
					displayStr[3] = displayStr[2];
					displayStr[2] = ' ';
					displayStr[4] = 0;
				}
			}
		}
	};

	struct PanelThemeItem : MenuItem {
		Part *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	struct SharpItem : MenuItem {
		Part *module;
		void onAction(const event::Action &e) override {
			module->showSharp = !module->showSharp;
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Part *module = dynamic_cast<Part*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *darkItem = createMenuItem<PanelThemeItem>(darkPanelID, CHECKMARK(module->panelTheme));
		darkItem->module = module;
		menu->addChild(darkItem);

		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));
		
		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		SharpItem *shrpItem = createMenuItem<SharpItem>("Sharp (unchecked is flat)", CHECKMARK(module->showSharp));
		shrpItem->module = module;
		menu->addChild(shrpItem);
	}	
	
	
	PartWidget(Part *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/Part.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/Part_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));
		
		
		const float colM = box.size.x / 2;
		static const int colL = 25;
		static const int colR = 65;
		
		static const int row0 = 60;// mode switch
		static const int row1 = 111;// display
		static const int row2 = row1 + 45;// split knob
		static const int row3 = row2 + 45;// split knob CV
		static const int row4 = 260;// CV + Gate inputs
		static const int row5 = 324;// Gate outputs
		

		// Display mode switch
		addParam(createParamCentered<CKSSH>(Vec(colM, row0), module, Part::MODE_PARAM));		
		
		// Display
		addChild(new SplitDisplayWidget(Vec(colM, row1), Vec(65, 24), module));// 4 characters + decimal point
		
		// Split knob and its CV input
		addParam(createDynamicParamCentered<IMBigKnob<true, false>>(Vec(colM, row2), module, Part::SPLIT_PARAM, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(Vec(colM, row3), true, module, Part::SPLIT_INPUT, module ? &module->panelTheme : NULL));		
		
		
		// CV and Gate inputs
		addInput(createDynamicPortCentered<IMPort>(Vec(colL, row4), true, module, Part::CV_INPUT, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPortCentered<IMPort>(Vec(colR, row4), true, module, Part::GATE_INPUT, module ? &module->panelTheme : NULL));		
		
		// Gate outputs
		addOutput(createDynamicPortCentered<IMPort>(Vec(colL, row5), false, module, Part::LOW_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(colR, row5), false, module, Part::HIGH_OUTPUT, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Part*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Part*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};


Model *modelPart = createModel<Part, PartWidget>("Part-Gate-Split");
