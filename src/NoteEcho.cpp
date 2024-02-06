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
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		GATE_INPUT,
		CV2_INPUT,
		CLK_INPUT,
		CLEAR_INPUT,
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
		SD_LIGHT,
		ENUMS(CV2NORM_LIGHTS, 2),// reg green
		NUM_LIGHTS
	};
	
	
	// Expander
	// none

	// Constants
	static const int LENGTH = 32 + 1;
	static constexpr float delayInfoTime = 3.0f;// seconds
	static constexpr float cv2NormMin = -10.0f;
	static constexpr float cv2NormMax = 10.0f;
	static constexpr float cv2NormDefault = 0.0f;
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	float cv[LENGTH][MAX_POLY];
	float cv2[LENGTH][MAX_POLY];
	bool gate[LENGTH][MAX_POLY];
	bool gateEn[NUM_TAPS][MAX_POLY];
	int head;// points the next register to be written when next clock occurs
	bool noteFilter;
	int clkDelay;
	float clkDelReg[2];
	float cv2NormalledVoltage;

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	int notifySource[4] = {};// 0 (normal), 1 (semi) 2 (CV2), 3 (prob)
	long notifyInfo[4] = {};// downward step counter when semi, cv2, prob to be displayed, 0 when normal tap display
	long notifyPoly = 0l;// downward step counter when notify poly size in leds, 0 when normal leds
	RefreshCounter refresh;
	Trigger clkTrigger;
	Trigger clearTrigger;


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
	
	void refreshCv2ParamQuantities() {
		// set the CV2 param units to either "V" or "", depending on the value of CV2MODE_PARAM (which must be setup when this method is called)
		for (int i = 0; i < NUM_TAPS; i++) {
			ParamQuantity* pq = paramQuantities[CV2_PARAMS + i];
			if (!pq) continue;

			if (isCv2Offset() && pq->unit != " V") {
				pq->name = string::f("Tap %i CV2 offset", i + 1);
				pq->unit = " V";
				pq->displayMultiplier = 10.f;
			}
			else if (!isCv2Offset() && pq->unit != "") {
				pq->name = string::f("Tap %i CV2 scale", i + 1);
				pq->unit = "";
				pq->displayMultiplier = 1.f;
			}
		}
	}
	
	
	
	NoteEcho() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(POLY_PARAM, 1, MAX_POLY, 1, "Input polyphony");
		paramQuantities[POLY_PARAM]->snapEnabled = true;
		configSwitch(CV2MODE_PARAM, 0.0f, 1.0f, 1.0f, "CV2 mode", {"Scale", "Offset"});
		configSwitch(PMODE_PARAM, 0.0f, 1.0f, 1.0f, "Random mode", {"Separate", "Chord"});
		
		for (int i = 0; i < NUM_TAPS; i++) {
			// tap knobs
			configParam(TAP_PARAMS + i, 0, (float)(LENGTH - 1), ((float)i) + 1.0f, string::f("Tap %i delay", i + 1));
			paramQuantities[TAP_PARAMS + i]->snapEnabled = true;
			// tap knobs
			configParam(ST_PARAMS + i, -48.0f, 48.0f, 0.0f, string::f("Tap %i semitone offset", i + 1));		
			paramQuantities[ST_PARAMS + i]->snapEnabled = true;
			// CV2 knobs
			configParam(CV2_PARAMS + i, -1.0f, 1.0f, 0.0f, "CV2", "");// temporary labels, good values done in refreshCv2ParamQuantities() below (CV2MODE_PARAM must be setup at this point)
			// CV2 knobs
			configParam(PROB_PARAMS + i, 0.0f, 1.0f, 1.0f, string::f("Tap %i probability", i + 1));		
		}
		refreshCv2ParamQuantities();
		
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(CV2_INPUT, "CV2/Velocity");
		configInput(CLK_INPUT, "Clock");
		configInput(CLEAR_INPUT, "Clear (reset)");

		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV2_OUTPUT, "CV2/Velocity");
		configOutput(CLK_OUTPUT, "Clock");
		
		configLight(SD_LIGHT, "Sample delay active");
		configLight(CV2NORM_LIGHTS, "CV2 normalization voltage");
		
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
		clear();
		noteFilter = false;
		clkDelay = 0;
		clkDelReg[0] = 0.0f;
		clkDelReg[1] = 0.0f;
		cv2NormalledVoltage = 0.0f;
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
		
		// noteFilter
		json_object_set_new(rootJ, "noteFilter", json_boolean(noteFilter));

		// clkDelay
		json_object_set_new(rootJ, "clkDelay", json_integer(clkDelay));
		
		// clkDelReg[]
		json_object_set_new(rootJ, "clkDelReg0", json_real(clkDelReg[0]));
		json_object_set_new(rootJ, "clkDelReg1", json_real(clkDelReg[1]));
		
		// cv2NormalledVoltage
		json_object_set_new(rootJ, "cv2NormalledVoltage", json_real(cv2NormalledVoltage));

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
					if (gateArrayJ) {
						gate[j][i] = json_is_true(gateArrayJ);
					}
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
						gateEn[j][i] = json_is_true(gateEnArrayJ);
				}
			}
		}

		// head
		json_t *headJ = json_object_get(rootJ, "head");
		if (headJ)
			head = json_integer_value(headJ);

		// noteFilter
		json_t *noteFilterJ = json_object_get(rootJ, "noteFilter");
		if (noteFilterJ)
			noteFilter = json_is_true(noteFilterJ);

		// clkDelay
		json_t *clkDelayJ = json_object_get(rootJ, "clkDelay");
		if (clkDelayJ)
			clkDelay = json_integer_value(clkDelayJ);

		// clkDelReg[]
		json_t *clkDelRegJ = json_object_get(rootJ, "clkDelReg0");
		if (clkDelRegJ)
			clkDelReg[0] = json_number_value(clkDelRegJ);
		clkDelRegJ = json_object_get(rootJ, "clkDelReg1");
		if (clkDelRegJ)
			clkDelReg[1] = json_number_value(clkDelRegJ);

		// cv2NormalledVoltage
		json_t *cv2NormalledVoltageJ = json_object_get(rootJ, "cv2NormalledVoltage");
		if (cv2NormalledVoltageJ)
			cv2NormalledVoltage = json_number_value(cv2NormalledVoltageJ);

		resetNonJson();
	}


	void filterOutputsForIdentNotes(int chans, int poly) {
		// kill gates of redundant notes, first priority to lowest channel
		// will kill redundant notes using gateEn, so that it does not need to be called on each sample, rather only on each clkEdge, after the outputs have been set for that edge
		for (int c = 0; c < chans - 1; c++) {
			if (outputs[GATE_OUTPUT].getVoltage(c) == 0.0f) continue;
			int c2 = c - (c % poly) + poly;// check only with downstream taps
			for (; c2 < chans; c2++) {
				if (outputs[GATE_OUTPUT].getVoltage(c2) == 0.0f) continue;
				if (outputs[CV_OUTPUT].getVoltage(c) == outputs[CV_OUTPUT].getVoltage(c2)) {
					// kill redundant note
					outputs[GATE_OUTPUT].setVoltage(0.0f, c2);
					// and use its gateEn[][] to keep this persistent after the clock edge
					int j = (c2 - poly) / poly;
					gateEn[j][c2 % poly] = false;
				}
			}
		}
	}


	void process(const ProcessArgs &args) override {
		int poly = getPolyKnob();
		int activeTaps = countActiveTaps();
		bool lastTapAllowed = poly < MAX_POLY || activeTaps < NUM_TAPS;
		int chans = std::min(poly * (1 + activeTaps), 16);
		if (outputs[CV_OUTPUT].getChannels() != chans) {
			outputs[CV_OUTPUT].setChannels(chans);
		}
		if (outputs[GATE_OUTPUT].getChannels() != chans) {
			outputs[GATE_OUTPUT].setChannels(chans);
		}
		if (outputs[CV2_OUTPUT].getChannels() != chans) {
			outputs[CV2_OUTPUT].setChannels(chans);
		}

		if (refresh.processInputs()) {
			// none
		}// userInputs refresh
	
	
	
		// clear and clock
		if (clearTrigger.process(inputs[CLEAR_INPUT].getVoltage())) {
			clear();
		}
		float clockSignal = clkDelay <= 0 ? inputs[CLK_INPUT].getVoltage() : clkDelReg[clkDelay - 1];
		bool clkEdge = clkTrigger.process(clockSignal);
		if (clkEdge) {
			// sample the inputs
			for (int i = 0; i < MAX_POLY; i++) {
				cv[head][i] = inputs[CV_INPUT].getChannels() > i ? inputs[CV_INPUT].getVoltage(i) : 0.0f;
				if (inputs[CV2_INPUT].getChannels() < 1) {
					cv2[head][i] = cv2NormalledVoltage;
				}
				else {
					cv2[head][i] = inputs[CV2_INPUT].getVoltage(std::min(i, inputs[CV2_INPUT].getChannels() - 1));
				}
				// normaling CV2 to 10V is not really useful here, since even if we also normal the passthrough (tap0) when CV2 input unconnected, it's value can't be controlled, so even if we can apply CV2 mod to the 4 true taps, a 10V value on the passthrough tap would have to coincide with that is desired and useful. So given this, forget normaling CV input to 10V.
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
		// do tap0 outputs first
		outputs[CLK_OUTPUT].setVoltage(clockSignal);
		int c = 0;// running index for all poly cable writes
		for (; c < poly; c++) {
			int srIndex = (head - 1 + 2 * LENGTH) % LENGTH;
			outputs[CV_OUTPUT].setVoltage(cv[srIndex][c],c);
			outputs[GATE_OUTPUT].setVoltage(gate[srIndex][c] && clockSignal > 1.0f ? 10.0f : 0.0f,c);
			outputs[CV2_OUTPUT].setVoltage(cv2[srIndex][c],c);
		}
		// now do main tap outputs
		for (int j = 0; j < NUM_TAPS; j++) {
			if ( !isTapActive(j) || (j == (NUM_TAPS - 1) && !lastTapAllowed) ) {
				continue;
			}
			int srIndex = (head - getTapValue(j) - 1 + 2 * LENGTH) % LENGTH;
			for (int i = 0; i < poly; i++, c++) {
				// cv
				float cvWithSemi = cv[srIndex][i] + params[ST_PARAMS + j].getValue() / 12.0f;
				outputs[CV_OUTPUT].setVoltage(cvWithSemi, c);
				
				// gate
				bool gateWithProb = gate[srIndex][i] && gateEn[j][i] && clockSignal > 1.0f;
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
		if (clkEdge && noteFilter) {
			filterOutputsForIdentNotes(chans, poly); 
		}
			
		
		// lights
		if (refresh.processLights()) {
			// gate lights
			// see NoteEchoWidget::step()

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
		
		clkDelReg[1] = clkDelReg[0];
		clkDelReg[0] = inputs[CLK_INPUT].getVoltage();	
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
	

	void createCv2NormalizationMenu(ui::Menu* menu, float* cv2Normalled) {
			
		struct Cv2NormItem : MenuItem {
			float* cv2Normalled = NULL;
			
			Menu *createChildMenu() override {
				struct Cv2NormQuantity : Quantity {
					float* cv2Normalled;
					
					Cv2NormQuantity(float* _cv2Normalled) {
						cv2Normalled = _cv2Normalled;
					}
					void setValue(float value) override {
						*cv2Normalled = math::clamp(value, getMinValue(), getMaxValue());
					}
					float getValue() override {
						return *cv2Normalled;
					}
					float getMinValue() override {return NoteEcho::cv2NormMin;}
					float getMaxValue() override {return NoteEcho::cv2NormMax;}
					float getDefaultValue() override {return NoteEcho::cv2NormDefault;}
					float getDisplayValue() override {return *cv2Normalled;}
					std::string getDisplayValueString() override {
						return string::f("%.2f", clamp(*cv2Normalled, getMinValue(), getMaxValue()));
					}
					void setDisplayValue(float displayValue) override {setValue(displayValue);}
					std::string getLabel() override {return "CV2";}
					std::string getUnit() override {return " V";}
				};
				struct Cv2NormSlider : ui::Slider {
					Cv2NormSlider(float* cv2Normalled) {
						quantity = new Cv2NormQuantity(cv2Normalled);
					}
					~Cv2NormSlider() {
						delete quantity;
					}
				};
			
				Menu *menu = new Menu;

				Cv2NormSlider *cSlider = new Cv2NormSlider(cv2Normalled);
				cSlider->box.size.x = 200.0f;
				menu->addChild(cSlider);
			
				return menu;
			}
		};
		
		Cv2NormItem *c2nItem = createMenuItem<Cv2NormItem>("Input normalization", RIGHT_ARROW);
		c2nItem->cv2Normalled = cv2Normalled;
		menu->addChild(c2nItem);
	}	
	

	void appendContextMenu(Menu *menu) override {
		NoteEcho *module = static_cast<NoteEcho*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), static_cast<SvgPanel*>(getPanel()));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));

		menu->addChild(createBoolPtrMenuItem("Filter out identical notes", "", &module->noteFilter));

		menu->addChild(createSubmenuItem("Sample delay on clock input", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("0", "",
				[=]() {return module->clkDelay == 0;},
				[=]() {module->clkDelay = 0;}
			));
			menu->addChild(createCheckMenuItem("1", "",
				[=]() {return module->clkDelay == 1;},
				[=]() {module->clkDelay = 1;}
			));
			menu->addChild(createCheckMenuItem("2", "",
				[=]() {return module->clkDelay == 2;},
				[=]() {module->clkDelay = 2;}
			));
		}));	
		
		createCv2NormalizationMenu(menu, &(module->cv2NormalledVoltage));
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
		addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(col51 + 4.3f, row0 - 6.6f)), module, NoteEcho::SD_LIGHT));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col52, row0)), true, module, NoteEcho::CV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col53, row0)), true, module, NoteEcho::GATE_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col54, row0)), true, module, NoteEcho::CV2_INPUT, mode));
		addChild(createLightCentered<TinyLight<GreenRedLight>>(mm2px(Vec(col54 + 4.7f, row0 - 6.6f)), module, NoteEcho::CV2NORM_LIGHTS));
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
		
		// row 5
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col51, row5)), false, module, NoteEcho::CLK_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col52, row5)), false, module, NoteEcho::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col53, row5)), false, module, NoteEcho::GATE_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col54, row5)), false, module, NoteEcho::CV2_OUTPUT, mode));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col55, row5 - 1.5f)), module, NoteEcho::CV2MODE_PARAM, mode, svgPanel));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col56, row5 - 1.5f)), module, NoteEcho::PMODE_PARAM, mode, svgPanel));

		// gate lights
		static const float gldx = 3.0f;
		static const float glto = -7.0f;
		static const float posy[5] = {28.0f, row1 + glto, row1 + glto + row24d, row1 + glto + 2 * row24d, row1 + glto + 3 * row24d};
		
		for (int j = 0; j < 5; j++) {
			float posx = col42 - gldx * 1.5f;
			for (int i = 0; i < NoteEcho::MAX_POLY; i++) {
				addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(posx, posy[j])), module, NoteEcho::GATE_LIGHTS + j * NoteEcho::MAX_POLY + i));
				posx += gldx;
			}
		}
	}
	
	void step() override {
		if (module) {
			NoteEcho* m = static_cast<NoteEcho*>(module);
			
			// gate lights
			int poly = m->getPolyKnob();
			if (m->notifyPoly > 0) {
				for (int i = 0; i < NoteEcho::MAX_POLY; i++) {
					m->lights[NoteEcho::GATE_LIGHTS + i].setBrightness(i < poly ? 1.0f : 0.0f);
				}
				for (int j = 0; j < NoteEcho::NUM_TAPS; j++) {
					for (int i = 0; i < NoteEcho::MAX_POLY; i++) {
						bool lstate = i < poly && m->isTapActive(j);
						if (j == (NoteEcho::NUM_TAPS - 1) && !m->isLastTapAllowed()) {
							lstate = false;
						}
						m->lights[NoteEcho::GATE_LIGHTS + 4 + j * NoteEcho::MAX_POLY + i].setBrightness(lstate ? 1.0f : 0.0f);
					}
				}
			}
			else {
				int c = 0;
				for (int i = 0; i < NoteEcho::MAX_POLY; i++) {
					bool lstate = i < poly;
					m->lights[NoteEcho::GATE_LIGHTS + i].setBrightness(lstate && m->outputs[NoteEcho::GATE_OUTPUT].getVoltage(c) ? 1.0f : 0.0f);
					if (lstate) c++;
				}
				for (int j = 0; j < NoteEcho::NUM_TAPS; j++) {
					for (int i = 0; i < NoteEcho::MAX_POLY; i++) {
						bool lstate = i < poly && m->isTapActive(j);
						if (j == (NoteEcho::NUM_TAPS - 1) && !m->isLastTapAllowed()) {
							lstate = false;
						}
						m->lights[NoteEcho::GATE_LIGHTS + 4 + j * NoteEcho::MAX_POLY + i].setBrightness(lstate && m->outputs[NoteEcho::GATE_OUTPUT].getVoltage(c) ? 1.0f : 0.0f);
						if (lstate) c++;
					}
				}
			}	
			
			// sample delay light
			m->lights[NoteEcho::SD_LIGHT].setBrightness( ((float)(m->clkDelay)) / 2.0f);
			
			// CV2 norm light
			m->lights[NoteEcho::CV2NORM_LIGHTS + 0].setBrightness( ((float)(m->cv2NormalledVoltage)) / NoteEcho::cv2NormMax);
			m->lights[NoteEcho::CV2NORM_LIGHTS + 1].setBrightness( ((float)(m->cv2NormalledVoltage)) / NoteEcho::cv2NormMin);

			// CV2 knobs' labels
			m->refreshCv2ParamQuantities();
		}
		ModuleWidget::step();
	}
};

Model *modelNoteEcho = createModel<NoteEcho, NoteEchoWidget>("NoteEcho");
