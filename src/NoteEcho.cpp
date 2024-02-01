//***********************************************************************************************
//CV-Gate shift register (a.k.a. note delay) for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Pyer. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct NoteEcho : Module {	

	static const int NUM_TAPS = 4;
	static const int MAX_POLY = 4;

	enum ParamIds {
		ENUMS(TAP_PARAMS, NUM_TAPS),
		ENUMS(ST_PARAMS, NUM_TAPS),
		ENUMS(CV2_PARAMS, NUM_TAPS),
		ENUMS(PROB_PARAMS, NUM_TAPS),
		POLY_PARAM,
		CV2MODE_PARAM,
		PMODE_PARAM,
		// SD_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		GATE_INPUT,
		CV2_INPUT,
		CLK_INPUT,
		CLEAR_INPUT,
		// TAPCV_INPUT,
		// STCV_INPUT,
		// VELCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE_OUTPUT,
		CV2_OUTPUT,
		CLK_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(GATE_LIGHTS, (NUM_TAPS + 1) * MAX_POLY),
		// SD_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Expander
	// none

	// Constants
	static const int LENGTH = 32;
	static constexpr float delayInfoTime = 3.0f;// seconds
	
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	// int sampDelayClk;
	float cv[LENGTH][MAX_POLY];
	float cv2[LENGTH][MAX_POLY];
	bool gate[LENGTH][MAX_POLY];
	bool gateEn[NUM_TAPS][MAX_POLY];
	int head;// points the next register to be written when next clock occurs

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	int notifySource[4] = {0, 0, 0, 0};// 0 (not really used), 1 (semi) 2 (CV2), 3 (prob)
	long notifyInfo[4] = {0l, 0l, 0l, 0l};// downward step counter when semi, cv2, prob to be displayed, 0 when normal tap display
	long notifyPoly = 0l;// downward step counter when notify poly size in leds, 0 when normal leds
	RefreshCounter refresh;
	Trigger clkTrigger;
	Trigger clearTrigger;
	// Trigger sdTrigger;


	int getPolyKnob() {
		return (int)(params[POLY_PARAM].getValue() + 0.5f);
	}
	int getTapValue(int tapNum) {
		return (int)(params[TAP_PARAMS + tapNum].getValue() + 0.5f);
	}
	bool isTapActive(int tapNum) {
		return params[TAP_PARAMS + tapNum].getValue() >= 0.5f;
	}
	int countActiveTaps() {
		int count = 0;
		for (int i = 0; i < NUM_TAPS; i++) {
			if (isTapActive(i)) {
				count++;
			}			
		}
		return count;
	}
	bool isLastTapAllowed() {
		return getPolyKnob() < MAX_POLY || countActiveTaps() < NUM_TAPS;
	}
	int getSemiValue(int tapNum) {
		return (int)(std::round(params[ST_PARAMS + tapNum].getValue()));
	}
	bool isCv2Offset() {
		return params[CV2MODE_PARAM].getValue() > 0.5f;
	}
	
	NoteEcho() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(POLY_PARAM, 1, MAX_POLY, 1, "Input polyphony");
		paramQuantities[POLY_PARAM]->snapEnabled = true;
		// configParam(SD_PARAM, 0.0f, 1.0f, 0.0f, "1 sample delay clk");
		configSwitch(CV2MODE_PARAM, 0.0f, 1.0f, 1.0f, "CV2 mode", {"Scale", "Offset"});
		configSwitch(PMODE_PARAM, 0.0f, 1.0f, 1.0f, "Random mode", {"Separate", "Chord"});
		
		char strBuf[32];
		for (int i = 0; i < NUM_TAPS; i++) {
			// tap knobs
			snprintf(strBuf, 32, "Tap %i delay", i + 1);
			configParam(TAP_PARAMS + i, 0, (float)LENGTH, ((float)i) + 1.0f, strBuf);		
			paramQuantities[TAP_PARAMS + i]->snapEnabled = true;
			// tap knobs
			snprintf(strBuf, 32, "Tap %i semitone offset", i + 1);
			configParam(ST_PARAMS + i, -48.0f, 48.0f, 0.0f, strBuf);		
			paramQuantities[ST_PARAMS + i]->snapEnabled = true;
			// CV2 knobs
			snprintf(strBuf, 32, "Tap %i CV2 offset or scale", i + 1);
			configParam(CV2_PARAMS + i, -1.0f, 1.0f, 0.0f, strBuf);		
			// CV2 knobs
			snprintf(strBuf, 32, "Tap %i probability", i + 1);
			configParam(PROB_PARAMS + i, 0.0f, 1.0f, 1.0f, strBuf);		
		}
		
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(CV2_INPUT, "CV2/Velocity");
		configInput(CLK_INPUT, "Clock");
		configInput(CLEAR_INPUT, "Clear (reset)");
		// configInput(TAPCV_INPUT, "Tap CV");
		// configInput(STCV_INPUT, "Semitone offset CV");
		// configInput(CV2_INPUT, "Velocity percent CV");

		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV2_OUTPUT, "CV2/Velocity");
		configOutput(CLK_OUTPUT, "Clock");
		
		configBypass(CV_INPUT, CV_OUTPUT);
		configBypass(GATE_INPUT, GATE_OUTPUT);
		configBypass(CV2_INPUT, CV2_OUTPUT);
		configBypass(CLK_INPUT, CLK_OUTPUT);

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	void clear() {
		for (int j = 0; j < LENGTH; j++) {
			for (int i = 0; i < MAX_POLY; i++) {
				cv[j][i] = 0.0f;
				cv2[j][i] = 0.0f;
				gate[j][i] = false;
			}
		}
		for (int j = 0; j < NUM_TAPS; j++) {
			for (int i = 0; i < MAX_POLY; i++) {
				gateEn[j][i] = false;
			}
		}
		head = 0;
	}

	void onReset() override final {
		// sampDelayClk = 0;
		clear();
		resetNonJson();
	}
	void resetNonJson() {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// sampDelayClk
		// json_object_set_new(rootJ, "sampDelayClk", json_integer(sampDelayClk));

		// cv, cv2, gate
		json_t *cvJ = json_array();
		json_t *cv2J = json_array();
		json_t *gateJ = json_array();
		for (int j = 0; j < LENGTH; j++) {
			for (int i = 0; i < MAX_POLY; i++) {
				json_array_insert_new(cvJ, j * MAX_POLY + i, json_real(cv[j][i]));
				json_array_insert_new(cv2J, j * MAX_POLY + i, json_real(cv2[j][i]));
				json_array_insert_new(gateJ, j * MAX_POLY + i, json_boolean(gate[j][i]));
			}
		}
		json_object_set_new(rootJ, "cv", cvJ);
		json_object_set_new(rootJ, "cv2", cv2J);
		json_object_set_new(rootJ, "gate", gateJ);
		
		// gateEn
		json_t *gateEnJ = json_array();
		for (int j = 0; j < NUM_TAPS; j++) {
			for (int i = 0; i < MAX_POLY; i++) {
				json_array_insert_new(gateEnJ, j * MAX_POLY + i, json_boolean(gateEn[j][i]));
			}
		}
		json_object_set_new(rootJ, "gateEn", gateEnJ);

		// head
		json_object_set_new(rootJ, "head", json_integer(head));
		
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

		// sampDelayClk
		// json_t *sampDelayClkJ = json_object_get(rootJ, "sampDelayClk");
		// if (sampDelayClkJ)
			// sampDelayClk = json_integer_value(sampDelayClkJ);

		// cv, cv2, gate
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int j = 0; j < LENGTH; j++) {
				for (int i = 0; i < MAX_POLY; i++) {
					json_t *cvArrayJ = json_array_get(cvJ, j * MAX_POLY + i);
					if (cvArrayJ)
						cv[j][i] = json_number_value(cvArrayJ);
				}
			}
		}
		json_t *cv2J = json_object_get(rootJ, "cv2");
		if (cv2J) {
			for (int j = 0; j < LENGTH; j++) {
				for (int i = 0; i < MAX_POLY; i++) {
					json_t *cv2ArrayJ = json_array_get(cv2J, j * MAX_POLY + i);
					if (cv2ArrayJ)
						cv2[j][i] = json_number_value(cv2ArrayJ);
				}
			}
		}
		json_t *gateJ = json_object_get(rootJ, "gate");
		if (gateJ) {
			for (int j = 0; j < LENGTH; j++) {
				for (int i = 0; i < MAX_POLY; i++) {
					json_t *gateArrayJ = json_array_get(gateJ, j * MAX_POLY + i);
					if (gateArrayJ)
						gate[j][i] = json_is_true(gateArrayJ);
				}
			}
		}
		
		// gateEn
		json_t *gateEnJ = json_object_get(rootJ, "gateEn");
		if (gateEnJ) {
			for (int j = 0; j < NUM_TAPS; j++) {
				for (int i = 0; i < MAX_POLY; i++) {
					json_t *gateEnArrayJ = json_array_get(gateEnJ, j * MAX_POLY + i);
					if (gateEnArrayJ)
						gate[j][i] = json_is_true(gateEnArrayJ);
				}
			}
		}

		// head
		json_t *headJ = json_object_get(rootJ, "head");
		if (headJ)
			head = json_integer_value(headJ);

		resetNonJson();
	}


	void process(const ProcessArgs &args) override {
		int poly = getPolyKnob();
		int activeTaps = countActiveTaps();
		bool lastTapAllowed = poly < MAX_POLY || activeTaps < NUM_TAPS;
		int chans = std::min(poly * (1 + activeTaps), 16);
		if (outputs[CV_OUTPUT].getChannels() != chans) {
			outputs[CV_OUTPUT].setChannels(chans);
			outputs[GATE_OUTPUT].setChannels(chans);
			outputs[CV2_OUTPUT].setChannels(chans);
		}

		if (refresh.processInputs()) {
			// none
		}// userInputs refresh
	
	
	
		// main code
		if (clearTrigger.process(inputs[CLEAR_INPUT].getVoltage())) {
			clear();
		}
		if (clkTrigger.process(inputs[CLK_INPUT].getVoltage())) {
			// sample the inputs
			for (int i = 0; i < MAX_POLY; i++) {
				cv[head][i] = inputs[CV_INPUT].getChannels() > i ? inputs[CV_INPUT].getVoltage(i) : 0.0f;
				cv2[head][i] = inputs[CV2_INPUT].getChannels() > i ? inputs[CV2_INPUT].getVoltage(i) : 0.0f;// normalling to 10V is not really useful here, since even if we also normal the passthrough (tap0) when CV2 input unconnected, it's value can't be controlled, so even if we can apply CV2 mod to the 4 true taps, a 10V value on the passthrough tap would have to coincide with that is desired and useful. So given this, forget normalling CV input to 10V.
				gate[head][i] = inputs[GATE_INPUT].getChannels() > i ? (inputs[GATE_INPUT].getVoltage(i) > 1.0f) : false;
			}
			// step the head pointer
			head = (head + 1) % LENGTH;
			// refill gateEn array
			for (int j = 0; j < NUM_TAPS; j++) {
				for (int i = 0; i < MAX_POLY; i++) {
					if (i > 0 && params[PMODE_PARAM].getValue() > 0.5f) {
						// single prob for all poly chans
						gateEn[j][i] = gateEn[j][0];
					}
					else {
						// separate probs for each poly chan
						gateEn[j][i] = (random::uniform() < params[PROB_PARAMS + j].getValue()); // random::uniform is [0.0, 1.0), see include/util/common.hpp
					}
				}
			}
		}
		
		
		// outputs
		// do passthrough outputs first since automatic
		outputs[CLK_OUTPUT].setVoltage(inputs[CLK_INPUT].getVoltage());
		int c = 0;// running index for all poly cable writes
		for (; c < poly; c++) {
			outputs[CV_OUTPUT].setVoltage(inputs[CV_INPUT].getVoltage(c),c);
			outputs[GATE_OUTPUT].setVoltage(inputs[GATE_INPUT].getVoltage(c),c);
			outputs[CV2_OUTPUT].setVoltage(inputs[CV2_INPUT].getVoltage(c),c);
		}
		// now do tap outputs
		for (int j = 0; j < NUM_TAPS; j++) {
			if ( !isTapActive(j) || (j == (NUM_TAPS - 1) && !lastTapAllowed) ) {
				continue;
			}
			int srIndex = ((head - getTapValue(j) - 1) + 2 * LENGTH) % LENGTH;
			for (int i = 0; i < poly; i++, c++) {
				// cv
				float cvWithSemi = cv[srIndex][i] + params[ST_PARAMS + j].getValue() / 12.0f;
				outputs[CV_OUTPUT].setVoltage(cvWithSemi, c);
				
				// gate
				bool gateWithProb = gate[srIndex][i] && gateEn[j][i] && inputs[CLK_INPUT].getVoltage() > 1.0f;
				outputs[GATE_OUTPUT].setVoltage(gateWithProb ? 10.0f : 0.0f, c);
				
				// cv2
				float cv2WithMod = cv2[srIndex][i];
				if (isCv2Offset()) {
					cv2WithMod += params[CV2_PARAMS + j].getValue() * 10.0f;
				}
				else {
					cv2WithMod *= params[CV2_PARAMS + j].getValue();
				}
				outputs[CV2_OUTPUT].setVoltage(cv2WithMod, c);
			}
		}
		// assert(c == chans);
			
		
		// lights
		if (refresh.processLights()) {
			// sampDelayClk light
			// lights[SD_LIGHT].setBrightness(sampDelayClk != 0 ? 1.0f : 0.0f);

			// gate lights
			if (notifyPoly > 0) {
				for (int i = 0; i < MAX_POLY; i++) {
					lights[GATE_LIGHTS + i].setBrightness(i < poly ? 1.0f : 0.0f);
				}
				for (int j = 0; j < NUM_TAPS; j++) {
					for (int i = 0; i < MAX_POLY; i++) {
						bool lstate = i < poly && isTapActive(j);
						if (j == (NUM_TAPS - 1) && !isLastTapAllowed()) {
							lstate = false;
						}
						lights[GATE_LIGHTS + 4 + j * MAX_POLY + i].setBrightness(lstate ? 1.0f : 0.0f);
					}
				}
			}
			else {
				int c = 0;
				for (int i = 0; i < MAX_POLY; i++) {
					bool lstate = i < poly;
					lights[GATE_LIGHTS + i].setBrightness(lstate && outputs[GATE_OUTPUT].getVoltage(c) ? 1.0f : 0.0f);
					if (lstate) c++;
				}
				for (int j = 0; j < NUM_TAPS; j++) {
					for (int i = 0; i < MAX_POLY; i++) {
						bool lstate = i < poly && isTapActive(j);
						if (j == (NUM_TAPS - 1) && !isLastTapAllowed()) {
							lstate = false;
						}
						lights[GATE_LIGHTS + 4 + j * MAX_POLY + i].setBrightness(lstate && outputs[GATE_OUTPUT].getVoltage(c) ? 1.0f : 0.0f);
						if (lstate) c++;
					}
				}
			}

			// info notification counters
			for (int i = 0; i < 4; i++) {
				notifyInfo[i]--;
				if (notifyInfo[i] < 0l) {
					notifyInfo[i] = 0l;
				}
			}
			notifyPoly--;
			if (notifyPoly < 0l) {
				notifyPoly = 0l;
			}

		}// lightRefreshCounter
	}// process()
};


struct NoteEchoWidget : ModuleWidget {
	struct PolyKnob : IMFourPosSmallKnob {
		NoteEcho *module = nullptr;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifyPoly = (long) (NoteEcho::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
			}
			IMFourPosSmallKnob::onDragMove(e);
		}
	};
	struct TapKnob : IMMediumKnob {
		NoteEcho *module = nullptr;
		int tapNum = 0;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifyInfo[tapNum] = 0l;
			}
			IMMediumKnob::onDragMove(e);
		}
	};
	struct SemitoneKnob : IMMediumKnob {
		NoteEcho *module = nullptr;
		int tapNum = 0;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifySource[tapNum] = 1l;
				module->notifyInfo[tapNum] = (long) (NoteEcho::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
			}
			IMMediumKnob::onDragMove(e);
		}
	};
	struct Cv2Knob : IMSmallKnob {
		NoteEcho *module = nullptr;
		int tapNum = 0;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifySource[tapNum] = 2l;
				module->notifyInfo[tapNum] = (long) (NoteEcho::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
			}
			IMSmallKnob::onDragMove(e);
		}
	};
	struct ProbKnob : IMSmallKnob {
		NoteEcho *module = nullptr;
		int tapNum = 0;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifySource[tapNum] = 3l;
				module->notifyInfo[tapNum] = (long) (NoteEcho::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
			}
			IMSmallKnob::onDragMove(e);
		}
	};
	
	struct TapDisplayWidget : TransparentWidget {// a centered display, must derive from this
		NoteEcho *module = nullptr;
		int tapNum = 0;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char displayStr[16] = {};
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f; // 18.2f for 14 pt, 19.7f for 15pt
		
		TapDisplayWidget(int _tapNum, Vec _pos, Vec _size, NoteEcho *_module) {
			tapNum = _tapNum;
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
				static const float offsetXfrac = 3.5f;
				nvgFontSize(args.vg, textFontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, -0.4);

				Vec textPos = VecPx(6.3f, textOffsetY);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
				std::string initString(".~~");
				nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, initString.c_str(), NULL);
				
				nvgFillColor(args.vg, displayColOn);
				if (!module || module->getTapValue(tapNum) < 1) {
					snprintf(displayStr, 5, "  - ");
				}
				else if (tapNum == 3 && !module->isLastTapAllowed()) {
					snprintf(displayStr, 5, "O VF");
				}
				else if (module->notifyInfo[tapNum] > 0l) {
					if (module->notifySource[tapNum] == 1) {
						// semitone
						int ist = module->getSemiValue(tapNum);
						snprintf(displayStr, 5, "  %2u", (unsigned)(std::abs(ist)));
						if (ist != 0) {
							displayStr[0] = ist > 0 ? '+' : '-';
						}
					}
					else if (module->notifySource[tapNum] == 2) {
						// CV2
						float cv2 = module->params[NoteEcho::CV2_PARAMS + tapNum].getValue();
						if (module->isCv2Offset()) {
							// CV2 mode is: offset
							float cvValPrint = std::fabs(cv2) * 10.0f;
							if (cvValPrint > 9.975f) {
								snprintf(displayStr, 5, "  10");
							}
							else if (cvValPrint < 0.025f) {
								snprintf(displayStr, 5, "   0");
							}
							else {
								snprintf(displayStr, 5, "%3.2f", cvValPrint);// Three-wide, two positions after the decimal, left-justified
								displayStr[1] = '.';// in case locals in printf
							}								
						}
						else {
							// CV2 mode is: scale
							unsigned int iscale100 = (unsigned)(std::round(std::fabs(cv2) * 100.0f));
							if ( iscale100 >= 100) {
								snprintf(displayStr, 5, "   1");
							}
							else if (iscale100 >= 1) {
								snprintf(displayStr, 5, "0.%02u", (unsigned) iscale100);
							}	
							else {
								snprintf(displayStr, 5, "   0");
							}
						}
					}
					else {
						// Prob
						float prob = module->params[NoteEcho::PROB_PARAMS + tapNum].getValue();
						unsigned int iprob100 = (unsigned)(std::round(prob * 100.0f));
						if ( iprob100 >= 100) {
							snprintf(displayStr, 5, "   1");
						}
						else if (iprob100 >= 1) {
							snprintf(displayStr, 5, "0.%02u", (unsigned) iprob100);
						}	
						else {
							snprintf(displayStr, 5, "   0");
						}
					}
				}
				else {
					snprintf(displayStr, 5, "D %2u", (unsigned)(module->getTapValue(tapNum)));	
				}
				nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, &displayStr[1], NULL);
				displayStr[1] = 0;
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};
	
	

	void appendContextMenu(Menu *menu) override {
		NoteEcho *module = static_cast<NoteEcho*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), static_cast<SvgPanel*>(getPanel()));

		// menu->addChild(new MenuSeparator());
		// menu->addChild(createMenuLabel("Settings"));
	}	
	
	
	NoteEchoWidget(NoteEcho *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/NoteEcho.svg")));
		SvgPanel* svgPanel = static_cast<SvgPanel*>(getPanel());
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));


		static const float col5d = 14.5f;
		static const float col51 = 9.47f;
		static const float col52 = col51 + col5d;
		static const float col53 = col52 + col5d;
		static const float col54 = col53 + col5d;
		static const float col55 = col54 + col5d;
		static const float col56 = col55 + col5d;
		
		static const float col54d = 2.0f;
		static const float col41 = col51 + col54d;// Tap
		static const float col43 = col54 - col54d;// Semitone
		static const float col42 = (col41 + col43) / 2.0f;// Displays
		
		static const float row0 = 21.0f;// Clk, CV, Gate, CV2 inputs, and sampDelayClk
		static const float row1 = row0 + 20.0f;// tap 1
		static const float row24d = 16.0f;// taps 2-4
		static const float row5 = 110.5f;// Clk, CV, Gate, CV2 outputs
		// static const float row4 = row5 - 20.0f;// CV inputs and poly knob
		
		static const float displayWidths = 48; // 43 for 14pt, 46 for 15pt
		static const float displayHeights = 24; // 22 for 14pt, 24 for 15pt

		
		// row 0
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col51, row0)), true, module, NoteEcho::CLK_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col52, row0)), true, module, NoteEcho::CV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col53, row0)), true, module, NoteEcho::GATE_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col54, row0)), true, module, NoteEcho::CV2_INPUT, mode));
		PolyKnob* polyKnobP;
		addParam(polyKnobP = createDynamicParamCentered<PolyKnob>(mm2px(Vec(col55, row0)), module, NoteEcho::POLY_PARAM, mode));
		polyKnobP->module = module;
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col56, row0)), true, module, NoteEcho::CLEAR_INPUT, mode));

		
		// rows 1-4
		for (int i = 0; i < NoteEcho::NUM_TAPS; i++) {
			TapKnob* tapKnobP;
			addParam(tapKnobP = createDynamicParamCentered<TapKnob>(mm2px(Vec(col41, row1 + i * row24d)), module, NoteEcho::TAP_PARAMS + i, mode));	
			tapKnobP->module = module;
			tapKnobP->tapNum = i;
			
			// displays
			TapDisplayWidget* tapDisplayWidget = new TapDisplayWidget(i, mm2px(Vec(col42, row1 + i * row24d)), VecPx(displayWidths + 4, displayHeights), module);
			addChild(tapDisplayWidget);
			svgPanel->fb->addChild(new DisplayBackground(tapDisplayWidget->box.pos, tapDisplayWidget->box.size, mode));
			
			SemitoneKnob* stKnobP;
			addParam(stKnobP = createDynamicParamCentered<SemitoneKnob>(mm2px(Vec(col43, row1 + i * row24d)), module, NoteEcho::ST_PARAMS + i, mode));	
			stKnobP->module = module;
			stKnobP->tapNum = i;
			
			Cv2Knob* cv2KnobP;			
			addParam(cv2KnobP = createDynamicParamCentered<Cv2Knob>(mm2px(Vec(col55, row1 + i * row24d)), module, NoteEcho::CV2_PARAMS + i, mode));	
			cv2KnobP->module = module;
			cv2KnobP->tapNum = i;
			
			ProbKnob* probKnobP;			
			addParam(probKnobP = createDynamicParamCentered<ProbKnob>(mm2px(Vec(col56, row1 + i * row24d)), module, NoteEcho::PROB_PARAMS + i, mode));	
			probKnobP->module = module;
			probKnobP->tapNum = i;
		}
		
		// row 4
		// addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col41, row4)), true, module, NoteEcho::TAPCV_INPUT, mode));
		// addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col43, row4)), true, module, NoteEcho::STCV_INPUT, mode));
		// addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col44, row4)), true, module, NoteEcho::VELCV_INPUT, mode));
		
		// row 5
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col51, row5)), false, module, NoteEcho::CLK_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col52, row5)), false, module, NoteEcho::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col53, row5)), false, module, NoteEcho::GATE_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col54, row5)), false, module, NoteEcho::CV2_OUTPUT, mode));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col55, row5)), module, NoteEcho::CV2MODE_PARAM, mode, svgPanel));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col56, row5)), module, NoteEcho::PMODE_PARAM, mode, svgPanel));

		// gate lights
		static const float gldx = 2.0f;
		static const float glto = -6.0f;
		static const float posy[5] = {30.0f, row1 + glto, row1 + glto + row24d, row1 + glto + 2 * row24d, row1 + glto + 3 * row24d};
		
		for (int j = 0; j < 5; j++) {
			float posx = col42 - gldx * 1.5f;
			for (int i = 0; i < NoteEcho::MAX_POLY; i++) {
				// if (j == 4 && i == (NoteEcho::MAX_POLY - 1)) continue;
				addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(posx, posy[j])), module, NoteEcho::GATE_LIGHTS + j * NoteEcho::MAX_POLY + i));
				posx += gldx;
			}
		}
	}
};

Model *modelNoteEcho = createModel<NoteEcho, NoteEchoWidget>("NoteEcho");
