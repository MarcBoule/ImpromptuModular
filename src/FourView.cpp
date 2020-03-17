//***********************************************************************************************
//Four channel note viewer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"

// Intervals (two notes)
// // https://en.wikipedia.org/wiki/Interval_(music)#Main_intervals
static const std::string intervalNames[13] = {"PER","MIN","MAJ","MIN","MAJ","PER","DIM","PER","MIN","MAJ","MIN","MAJ","PER"};
static const int       intervalNumbers[13] = {  1,    2,    2,    3,    3,    4,   	5, 	  5,	6,	  6,	7,	  7,	8};
// short										P1	  m2	M2	  m3	M3	  P4	d5	  P5	m6	  M6	m7	  M7	P8
// semitone distance							0	  1		2	  3		4	  5		6	  7		8	  9		10	  11	12

// Triads (three notes)
// https://en.wikipedia.org/wiki/Chord_(music)#Examples
// https://en.wikipedia.org/wiki/Interval_(music)#Intervals_in_chords
static const int NUM_TRIADS = 6;
static const int triadIntervals[NUM_TRIADS][2] =  {{4, 7}, {4, 8}, {3, 7}, {3, 6}, {2, 7}, {5, 7}};
static const std::string triadNames[NUM_TRIADS] = {"MAJ",  "AUG",  "MIN",  "DIM",  "SUS",  "SUS"};
static const int       triadNumbers[NUM_TRIADS] = { -1,     -1,     -1,     -1,     2,      4};



struct FourView : Module {
	enum ParamIds {
		MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(CV_INPUTS, 4),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Constants
	const float unusedValue = -100.0f;

	// Expander
	float leftMessages[2][5] = {};// messages from mother (CvPad or ChordKey): 4 CV values, panelTheme

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	bool showSharp;

	// No need to save, with reset
	float displayValues[4];
	char displayChord[16];// 4 displays of 3-char strings each having a fourth null termination char

	// No need to save, no reset
	RefreshCounter refresh;


	
	inline float quantize(float cv, bool enable) {
		return enable ? (std::round(cv * 12.0f) / 12.0f) : cv;
	}
	
	
	FourView() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		onReset();
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		configParam(MODE_PARAM, 0.0, 1.0, 0.0, "Display mode");// 0.0 is left, notes by default left, chord right
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}
	

	void onReset() override {
		showSharp = true;
		resetNonJson();
	}
	void resetNonJson() {
		for (int i = 0; i < 4; i++) {
			displayValues[i] = unusedValue;
		}
		memset(displayChord, 0, 16);
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
		
		if (refresh.processInputs()) {
			bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelCvPad ||
														  leftExpander.module->model == modelChordKey ||
														  leftExpander.module->model == modelChordKeyExpander));
			if (motherPresent) {
				// From Mother
				float *messagesFromMother = (float*)leftExpander.consumerMessage;
				for (int i = 0; i < 4; i++) {
					displayValues[i] = inputs[CV_INPUTS + i].isConnected() ? inputs[CV_INPUTS + i].getVoltage() : messagesFromMother[i];// input cable has higher priority than mother
				}
				panelTheme = clamp((int)(messagesFromMother[4] + 0.5f), 0, 1);
			}	
			else {
				for (int i = 0; i < 4; i++) {
					displayValues[i] = inputs[CV_INPUTS + i].isConnected() ? inputs[CV_INPUTS + i].getVoltage() : unusedValue;
				}
			}
		}// userInputs refresh
		
		
		// outputs
		for (int i = 0; i < 4; i++)
			outputs[CV_OUTPUTS + i].setVoltage(inputs[CV_INPUTS + i].getVoltage());
		
		
		if (refresh.processLights()) {
			if (params[MODE_PARAM].getValue() >= 0.5f) {
				calcDisplayChord();
			}
		}// lightRefreshCounter
		
	}
	
	void calcDisplayChord() {
		// count notes and prepare sorted (will be sorted just in time later)
		int numNotes = 0;
		float packedNotes[4];
		for (int i = 0; i < 4; i++) {
			if (displayValues[i] != unusedValue) {
				packedNotes[numNotes] = displayValues[i];
				numNotes ++;
			}
		}		
		
		if (numNotes == 0) {
			printDashes();
		}
		else { 
			// Single note
			if (numNotes == 1) {
				printNote(packedNotes[0], &displayChord[0], showSharp, false);
				displayChord[4] = 0;
				displayChord[8] = 0;
				displayChord[12] = 0;				
			}
			// Interval
			else if (numNotes == 2) {
				int interval = (int)std::round(std::abs(packedNotes[0] - packedNotes[1]) * 12.0f);
				if (interval > 12) {
					printDashes();
				}
				else {
					printNote(std::min(packedNotes[0], packedNotes[1]), &displayChord[0], showSharp, false);
					printInterval(interval); // prints to displayChord[4..7] and displayChord[8..11] 
					displayChord[12] = 0;		
				}					
			}
			// Triad
			else if (numNotes == 3) {
				int interval1 = (int)std::round((packedNotes[1] - packedNotes[0]) * 12.0f);			
				int interval2 = (int)std::round((packedNotes[2] - packedNotes[0]) * 12.0f);
				if (!printTriad(packedNotes, interval1, interval2)) {
					printDashes();
				}
			}
			// 4-note chord
			else {
				// float sortedNotes[4];
				snprintf(&displayChord[0 ], 4, "C  ");
				snprintf(&displayChord[4 ], 4, "MAJ");
				snprintf(&displayChord[8 ], 4, "7  ");
				snprintf(&displayChord[12], 4, "/E ");				
			}
		}
	}
	
	
	void printDashes() {
		snprintf(&displayChord[0 ], 4, " - ");
		snprintf(&displayChord[4 ], 4, " - ");
		snprintf(&displayChord[8 ], 4, " - ");
		snprintf(&displayChord[12], 4, " - ");				
	}
	
	
	void printInterval(int interval) {// prints to &displayChord[4] and &displayChord[8] by default 
		assert(interval >= 0 && interval <=12);
		snprintf(&displayChord[4], 4, "%s", intervalNames[interval].c_str());
		snprintf(&displayChord[8], 4, "%i", intervalNumbers[interval]);
	}
	
	
	bool printTriad(float *packedNotes, int interval1, int interval2) {// prints to all of displayChord if match found 
		// No inversion
		for (int t = 0; t < 6; t++) {
			if (triadIntervals[t][0] == interval1 && triadIntervals[t][1] == interval2) {
				printNote(packedNotes[0], &displayChord[0], showSharp, false);
				snprintf(&displayChord[4], 4, "%s", triadNames[t].c_str());
				if (triadNumbers[t] != -1) {
					snprintf(&displayChord[8], 4, "%i", triadNumbers[t]);
				}
				else {
					displayChord[8] = 0;
				}
				displayChord[12] = 0;
				return true;
			}
		}
		
		// First inversion
		for (int t = 0; t < 6; t++) {
			int firstBase = -1 * (triadIntervals[t][1] - 12);
			if (firstBase == interval1 && (triadIntervals[t][0] + firstBase) == interval2) {
				printNote(packedNotes[1], &displayChord[0], showSharp, false);
				snprintf(&displayChord[4], 4, "%s", triadNames[t].c_str());
				int inversionCursor = 8;
				if (triadNumbers[t] != -1) {
					inversionCursor += 4;
					snprintf(&displayChord[8], 4, "%i", triadNumbers[t]);
				}
				else {
					displayChord[12] = 0;
				}
				printNote(packedNotes[0], &displayChord[inversionCursor + 1], showSharp, false);
				displayChord[inversionCursor] = '/';
				return true;
			}
		}
		
		// Second inversion
		for (int t = 0; t < 6; t++) {
			if ((triadIntervals[t][1] - triadIntervals[t][0]) == interval1 && (12 - triadIntervals[t][0]) == interval2) {
				printNote(packedNotes[2], &displayChord[0], showSharp, false);
				snprintf(&displayChord[4], 4, "%s", triadNames[t].c_str());
				int inversionCursor = 8;
				if (triadNumbers[t] != -1) {
					inversionCursor += 4;
					snprintf(&displayChord[8], 4, "%i", triadNumbers[t]);
				}
				else {
					displayChord[12] = 0;
				}
				printNote(packedNotes[0], &displayChord[inversionCursor + 1], showSharp, false);
				displayChord[inversionCursor] = '/';
				return true;
			}
		}
		
		return false;
	}
};// module


struct FourViewWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct NotesDisplayWidget : TransparentWidget {
		FourView* module;
		int baseIndex;
		std::shared_ptr<Font> font;
		char text[4];

		NotesDisplayWidget(Vec _pos, Vec _size, FourView* _module, int _baseIndex) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			baseIndex = _baseIndex;
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void cvToStr() {
			if (module == NULL) {
				snprintf(text, 4, " - ");
			}	
			else if (module->params[FourView::MODE_PARAM].getValue() >= 0.5f) {// chord mode
				snprintf(text, 4, "%s", &module->displayChord[baseIndex<<2]);
			}
			else {// note mode
				if (module->displayValues[baseIndex] != module->unusedValue) {
					float cvVal = module->displayValues[baseIndex];
					printNote(cvVal, text, module->showSharp);
				}
				else {
					snprintf(text, 4, " - ");
				}
			}
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 17);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1.5);

			Vec textPos = Vec(7.0f, 23.4f);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(args.vg, textColor);
			cvToStr();
			nvgText(args.vg, textPos.x, textPos.y, text, NULL);
		}
	};


	struct PanelThemeItem : MenuItem {
		FourView *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	struct SharpItem : MenuItem {
		FourView *module;
		void onAction(const event::Action &e) override {
			module->showSharp = !module->showSharp;
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		FourView *module = dynamic_cast<FourView*>(this->module);
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
	
	
	FourViewWidget(FourView *module) {
		setModule(module);
		
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/FourView.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/FourView_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}

		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));

		const float centerX = box.size.x / 2;
		static const int rowRulerTop = 66;
		static const int spacingY = 48;
		static const float offsetXL = 30;
		static const float offsetXR = 18;
		
		// Notes displays and inputs
		NotesDisplayWidget* displayNotes[4];
		for (int i = 0; i < 4; i++) {
			displayNotes[i] = new NotesDisplayWidget(Vec(centerX + offsetXR, rowRulerTop + i * spacingY), Vec(52, 29), module, i);
			addChild(displayNotes[i]);

			addInput(createDynamicPortCentered<IMPort>(Vec(centerX - offsetXL, rowRulerTop + i * spacingY), true, module, FourView::CV_INPUTS + i, module ? &module->panelTheme : NULL));	
		}


		// Display mode switch
		addParam(createParamCentered<CKSSH>(Vec(centerX, 240), module, FourView::MODE_PARAM));		


		static const int spacingY2 = 46;
		static const float offsetX = 20;
		static const int posY2 = 285;

		// Thru outputs
		addOutput(createDynamicPortCentered<IMPort>(Vec(centerX - offsetX, posY2), false, module, FourView::CV_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(centerX + offsetX, posY2), false, module, FourView::CV_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(centerX - offsetX, posY2 + spacingY2), false, module, FourView::CV_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(centerX + offsetX, posY2 + spacingY2), false, module, FourView::CV_OUTPUTS + 3, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((FourView*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((FourView*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelFourView = createModel<FourView, FourViewWidget>("Four-View");
