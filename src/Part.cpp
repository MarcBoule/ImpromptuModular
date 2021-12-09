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
		CVTHRU_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
		
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool showSharp;
	bool showPlusMinus;
	bool applyEpsilonForSplit;

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	RefreshCounter refresh;


	float getSplitValue() {return clamp(params[SPLIT_PARAM].getValue() + inputs[SPLIT_INPUT].getVoltage(), -10.0f, 10.0f);}


	Part() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(SPLIT_PARAM, -10.0f, 10.0f, 0.0f, "Split point", " V");
		configSwitch(MODE_PARAM, 0.0f, 1.0f, 0.0f, "Display mode", {"Volts", "Note"});// default is 0.0f meaning left position (= V)
		
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(SPLIT_INPUT, "Split");

		configOutput(LOW_OUTPUT, "Gate for low notes");
		configOutput(HIGH_OUTPUT, "Gate for high notes");
		configOutput(CVTHRU_OUTPUT, "CV thru");

		configBypass(CV_INPUT, CVTHRU_OUTPUT);
		configBypass(GATE_INPUT, LOW_OUTPUT);
		configBypass(GATE_INPUT, HIGH_OUTPUT);

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	
	void onReset() override {
		showSharp = true;
		showPlusMinus = true;
		applyEpsilonForSplit = true;
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

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// showSharp
		json_object_set_new(rootJ, "showSharp", json_boolean(showSharp));
		
		// showPlusMinus
		json_object_set_new(rootJ, "showPlusMinus", json_boolean(showPlusMinus));
		
		// applyEpsilonForSplit
		json_object_set_new(rootJ, "applyEpsilonForSplit", json_boolean(applyEpsilonForSplit));
		
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

		// showSharp
		json_t *showSharpJ = json_object_get(rootJ, "showSharp");
		if (showSharpJ)
			showSharp = json_is_true(showSharpJ);
		
		// showPlusMinus
		json_t *showPlusMinusJ = json_object_get(rootJ, "showPlusMinus");
		if (showPlusMinusJ)
			showPlusMinus = json_is_true(showPlusMinusJ);
		
		// applyEpsilonForSplit
		json_t *applyEpsilonForSplitJ = json_object_get(rootJ, "applyEpsilonForSplit");
		if (applyEpsilonForSplitJ)
			applyEpsilonForSplit = json_is_true(applyEpsilonForSplitJ);
		else 
			applyEpsilonForSplit = false;
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		int numChan = inputs[GATE_INPUT].getChannels();
		
		if (refresh.processInputs()) {
			outputs[LOW_OUTPUT].setChannels(numChan);
			outputs[HIGH_OUTPUT].setChannels(numChan);
			outputs[CVTHRU_OUTPUT].setChannels(inputs[CV_INPUT].getChannels());
		}// userInputs refresh
		
		
		float splitPoint = getSplitValue() - (applyEpsilonForSplit ? 0.001f : 0.0f);// unconnected CV_INPUT or insufficient channels will cause 0.0f to be used
		for (int c = 0; c < numChan; c++) {
			bool isHigh = inputs[CV_INPUT].getVoltage(c) >= splitPoint;
			float inGate = inputs[GATE_INPUT].getVoltage(c);// unconnected GATE_INPUT or insufficient channels will cause 0.0f to be used
			outputs[LOW_OUTPUT].setVoltage(isHigh ? 0.0f : inGate, c);
			outputs[HIGH_OUTPUT].setVoltage(isHigh ? inGate : 0.0f, c);
		}
		for (int c = 0; c < inputs[CV_INPUT].getChannels(); c++) {
			outputs[CVTHRU_OUTPUT].setVoltage(inputs[CV_INPUT].getVoltage(c), c);
		}
		
		
		// lights
		if (refresh.processLights()) {
			// none, but need this since refresh counter is stepped in processLights()
		}
	}
};


struct PartWidget : ModuleWidget {
	struct SplitDisplayWidget : TransparentWidget {
		Part *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char displayStr[5 + 1];// room for two chars left of decimal point, then decimal point, then two chars right of decimal point, plus null
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f;
		
		SplitDisplayWidget(Vec _pos, Vec _size, Part *_module) {
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
				static const float offsetXfrac = 16.5f;
				nvgFontSize(args.vg, textFontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, -0.4);

				Vec textPos = VecPx(6.3f, textOffsetY);
				printText();
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~~", NULL);
				nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, ".~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, &displayStr[2], NULL);// print decimal point and two chars to the right of decimal point
				displayStr[2] = 0;
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);// print two chars to the left of decimal point
			}
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
					int cursor = printNote(cvValPrint, &displayStr[0], module->showSharp);// given str pointer must be 4 chars (3 display and one end of string)
					// here cursor is <= 3
					float cvValQuant = std::round(cvValPrint * 12.0f) / 12.0f;
					if (module->showPlusMinus && cvValPrint != cvValQuant) {
						if (cvValPrint > cvValQuant) {
							displayStr[cursor] = '+';
						}
						else {
							displayStr[cursor] = '-';
						}
						cursor++;
						displayStr[cursor] = 0;
					}
					// here cursor is <= 4
					
					// insert space because decimal point not used
					displayStr[5] = displayStr[4];
					displayStr[4] = displayStr[3];
					displayStr[3] = displayStr[2];
					displayStr[2] = ' ';
				}
			}
		}
	};

	void appendContextMenu(Menu *menu) override {
		Part *module = dynamic_cast<Part*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());
		
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createBoolPtrMenuItem("Sharp (unchecked is flat)", "", &module->showSharp));
		
		menu->addChild(createBoolPtrMenuItem("Show +/- for notes", "", &module->showPlusMinus));
		
		menu->addChild(createBoolPtrMenuItem("Apply -1mV epsilon to split point", "", &module->applyEpsilonForSplit));
	}	
	
	
	PartWidget(Part *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Part.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));
		
		
		const float colM = box.size.x / 2;
		static const int colL = 25;
		static const int colR = 65;
		
		static const int row0 = 56;// mode switch
		static const int row1 = 106;// display
		static const int row2 = 152;// split knob
		static const int row5 = 326;// split in, low gate out
		static const int row4 = row5 - 54;// gate in, high gate out
		static const int row3 = row4 - 54;// CV in and thru
		

		// Display mode switch
		addParam(createDynamicSwitchCentered<IMSwitch2H>(VecPx(colM, row0), module, Part::MODE_PARAM, mode, svgPanel));		
		
		// Display
		SplitDisplayWidget* splitDisplayWidget = new SplitDisplayWidget(VecPx(colM, row1), VecPx(65, 24), module);// 4 characters + decimal point
		addChild(splitDisplayWidget);
		svgPanel->fb->addChild(new DisplayBackground(splitDisplayWidget->box.pos, splitDisplayWidget->box.size, mode));
		
		// Split knob 
		addParam(createDynamicParamCentered<IMBigKnob>(VecPx(colM, row2), module, Part::SPLIT_PARAM, mode));


		// CV input
		addInput(createDynamicPortCentered<IMPort>(VecPx(colL, row3), true, module, Part::CV_INPUT, mode));		
		// Thru output
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colR, row3), false, module, Part::CVTHRU_OUTPUT, mode));
		
		// Gate input
		addInput(createDynamicPortCentered<IMPort>(VecPx(colL, row4), true, module, Part::GATE_INPUT, mode));		
		// Gate high output
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colR, row4), false, module, Part::HIGH_OUTPUT, mode));
		
		// Split CV
		addInput(createDynamicPortCentered<IMPort>(VecPx(colL, row5), true, module, Part::SPLIT_INPUT, mode));		
		// Gate low output
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colR, row5), false, module, Part::LOW_OUTPUT, mode));
	}
};


Model *modelPart = createModel<Part, PartWidget>("Part-Gate-Split");
