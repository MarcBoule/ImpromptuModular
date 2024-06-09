//***********************************************************************************************
//CV-Gate delay (a.k.a. note delay) for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Pyer. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include <queue>



struct NoteEcho2 : Module {	

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
		FILTER_LIGHT,
		WET_LIGHT,
		ENUMS(CV2NORM_LIGHTS, 2),// reg green
		ENUMS(GATE_LIGHTS, (NUM_TAPS + 1) * MAX_POLY),
		NUM_LIGHTS
	};
	
	
	struct NoteEvent {
		// all info here has semitone, cv2offset and prob applied 
		// an event is considered pending when gateOffFrame<=0 (waiting for falling edge of a gate input), and ready when >0
		int64_t gateOnFrame;
		int64_t gateOffFrame;// can be -x when waiting for falling gate, all the while rest of struct will be populated, where
		// x is number of frames of delay that were added to the rising gate's frame to create the gateOnFrame
		float cv;
		float cv2;
		bool muted;// this is used to enter events that fail the probability gen so as to properly queue things for falling gate edges
		
		NoteEvent(float _cv, float _cv2, int64_t _gateOnFrame, int64_t _gateOffFrame, bool _muted) {
			cv = _cv;
			cv2 = _cv2;
			gateOnFrame = _gateOnFrame;
			gateOffFrame = _gateOffFrame;
			muted = _muted;
		}
	};

	
	// Expander
	// none

	// Constants
	static const int MAX_DEL = 32;
	static const int MAX_QUEUE = MAX_DEL * 4;// assume quarter clocked note is finest resolution 
	static constexpr float delayInfoTime = 3.0f;// seconds
	static constexpr float cv2NormMin = -10.0f;
	static constexpr float cv2NormMax = 10.0f;
	static constexpr float cv2NormDefault = 0.0f;
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool noteFilter;
	bool wetOnly;
	float cv2NormalledVoltage;
	int64_t clockPeriod;

	// No need to save, with reset
	std::queue<NoteEvent> channel[NUM_TAPS][MAX_POLY];// all info here has semitone, cv2offset and prob applied 
	int64_t lastRisingClkFrame;// -1 when none
	int lastPoly;
	
	// No need to save, no reset
	int notifySource[NUM_TAPS] = {};// 0 (normal), 1 (semi) 2 (CV2), 3 (prob)
	long notifyInfo[NUM_TAPS] = {};// downward step counter when semi, cv2, prob to be displayed, 0 when normal tap display
	long notifyPoly = 0l;// downward step counter when notify poly size in leds, 0 when normal leds
	RefreshCounter refresh;
	Trigger clkTrigger;
	Trigger clearTrigger;
	TriggerRiseFall gateTriggers[NUM_TAPS];


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
		return getPolyKnob() < MAX_POLY || countActiveTaps() < NUM_TAPS || wetOnly;
	}
	int getSemiValue(int tapNum) {
		return (int)(std::round(params[ST_PARAMS + tapNum].getValue()));
	}
	float getSemiVolts(int tapNum) {
		return params[ST_PARAMS + tapNum].getValue() / 12.0f;
	}
	bool isCv2Offset() {
		return params[CV2MODE_PARAM].getValue() > 0.5f;
	}
	bool isSingleProbs() {
		return params[PMODE_PARAM].getValue() > 0.5f;
	}
	bool getGateProbEnableForTap(int tapNum) {
		return (random::uniform() < params[PROB_PARAMS + tapNum].getValue()); // random::uniform is [0.0, 1.0), see include/util/common.hpp
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
	
	
	
	NoteEcho2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(POLY_PARAM, 1, MAX_POLY, 1, "Input polyphony");
		paramQuantities[POLY_PARAM]->snapEnabled = true;
		configSwitch(CV2MODE_PARAM, 0.0f, 1.0f, 1.0f, "CV2 mode", {"Scale", "Offset"});
		configSwitch(PMODE_PARAM, 0.0f, 1.0f, 1.0f, "Random mode", {"Separate", "Chord"});
		
		for (int i = 0; i < NUM_TAPS; i++) {
			// tap knobs
			configParam(TAP_PARAMS + i, 0, (float)(MAX_DEL), ((float)i) + 1.0f, string::f("Tap %i delay", i + 1));
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
		
		configLight(FILTER_LIGHT, "Filter identical notes");
		configLight(WET_LIGHT, "Echoes only");
		configLight(CV2NORM_LIGHTS, "CV2 normalization voltage");
		
		configBypass(CV_INPUT, CV_OUTPUT);
		configBypass(GATE_INPUT, GATE_OUTPUT);
		configBypass(CV2_INPUT, CV2_OUTPUT);
		configBypass(CLK_INPUT, CLK_OUTPUT);

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	void clearChannel(int t, int p) {
		while (!channel[t][p].empty()) {
			channel[t][p].pop();
		}
	}
	void clear() {
		for (int t = 0; t < NUM_TAPS; t++) {
			for (int p = 0; p < MAX_POLY; p++) {
				clearChannel(t, p);
			}
		}
	}


	void onReset() override final {
		noteFilter = false;
		wetOnly = false;
		cv2NormalledVoltage = 0.0f;
		clockPeriod = (int64_t)(APP->engine->getSampleRate());// 60 BPM until detected
		resetNonJson();
	}
	void resetNonJson() {
		clear();
		lastRisingClkFrame = -1;// none
		lastPoly = getPolyKnob();
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));
		
		// noteFilter
		json_object_set_new(rootJ, "noteFilter", json_boolean(noteFilter));

		// wetOnly
		json_object_set_new(rootJ, "wetOnly", json_boolean(wetOnly));

		// cv2NormalledVoltage
		json_object_set_new(rootJ, "cv2NormalledVoltage", json_real(cv2NormalledVoltage));

		// clockPeriod
		json_object_set_new(rootJ, "clockPeriod", json_integer((long)clockPeriod));

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

		// noteFilter
		json_t *noteFilterJ = json_object_get(rootJ, "noteFilter");
		if (noteFilterJ)
			noteFilter = json_is_true(noteFilterJ);

		// wetOnly
		json_t *wetOnlyJ = json_object_get(rootJ, "wetOnly");
		if (wetOnlyJ)
			wetOnly = json_is_true(wetOnlyJ);

		// cv2NormalledVoltage
		json_t *cv2NormalledVoltageJ = json_object_get(rootJ, "cv2NormalledVoltage");
		if (cv2NormalledVoltageJ)
			cv2NormalledVoltage = json_number_value(cv2NormalledVoltageJ);

		// clockPeriod
		json_t *clockPeriodJ = json_object_get(rootJ, "clockPeriod");
		if (clockPeriodJ)
			clockPeriod = (int64_t)json_integer_value(clockPeriodJ);

		resetNonJson();
	}


	void filterOutputsForIdentNotes(int chans, int poly) {
		// TODO
	}


	void onSampleRateChange() override {
		clear();
	}		


	void process(const ProcessArgs &args) override {
		int poly = getPolyKnob();
		int numActiveTaps = countActiveTaps();
		bool lastTapAllowed = isLastTapAllowed();
		int chans = std::min( poly * ( (wetOnly ? 0 : 1) + numActiveTaps ) , 16 );
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
			if (poly != lastPoly) {
				clear();
				lastPoly = poly;
			}
		}// userInputs refresh
	
	
	
		// clear and clock
		if (clearTrigger.process(inputs[CLEAR_INPUT].getVoltage())) {
			clear();
		}
		float clockSignal = inputs[CLK_INPUT].getVoltage();
		bool clkEdge = clkTrigger.process(clockSignal);
		if (clkEdge) {
			// update clockPeriod
			if (lastRisingClkFrame != -1) {
				int64_t deltaFrames = args.frame - lastRisingClkFrame;
				float deltaFramesF = (float)deltaFrames;
				if (deltaFramesF <= (args.sampleRate * 6.0f) && 
				    deltaFramesF >= (args.sampleRate / 5.0f)) {// 10-300 BPM tempo range
					clockPeriod = deltaFrames;
				}
				// else, remember the previous clockPeriod so nothing to do
			}
			lastRisingClkFrame = args.frame;
		}	
		
		// check the poly gate input for rising/falling gates, 
		//   and enter proper events according to taps
		for (int p = 0; p < MAX_POLY; p++) {
			int gateEdge = gateTriggers[p].process(inputs[GATE_INPUT].getVoltage(p));
			if (p >= std::min(poly, inputs[GATE_INPUT].getChannels())) {
				break;
			}
			if (gateEdge == 1) {
				// here we have a rising gate on poly p
				// now scan taps and prepare events for this gate on poly p 
				for (int t = 0; t < NUM_TAPS; t++) {
					if ( !isTapActive(t) || (t == (NUM_TAPS - 1) && !lastTapAllowed) ) {
						continue;
					}
					if (channel[t][p].size() < MAX_QUEUE) {
						// cv
						float cv = inputs[CV_INPUT].getChannels() > p ? inputs[CV_INPUT].getVoltage(p) : 0.0f;
						cv += getSemiVolts(t);
						
						// cv2
						float cv2;
						if (inputs[CV2_INPUT].getChannels() < 1) {
							cv2 = cv2NormalledVoltage;
						}
						else {
							cv2 = inputs[CV2_INPUT].getVoltage(std::min(p, inputs[CV2_INPUT].getChannels() - 1));
						}
						if (isCv2Offset()) {
							cv2 += params[CV2_PARAMS + t].getValue() * 10.0f;
						}
						else {
							cv2 *= params[CV2_PARAMS + t].getValue();
						}

						// frames
						int64_t delayFrames = clockPeriod * (int64_t)getTapValue(t);
						int64_t gateOnFrame = delayFrames + (int64_t)args.frame;
						int64_t gateOffFrame = -delayFrames;// will be completed when gate falls
						
						// muted
						bool muted;
						if (p == 0 || !isSingleProbs() || channel[t][0].empty()) {
							// generate new muted according to prob
							muted = !getGateProbEnableForTap(t);
						}
						else {
							// get muted prob from first poly
							muted = channel[t][0].back().muted;
						}

						NoteEvent newEvent(cv, cv2, gateOnFrame, gateOffFrame, muted);
						channel[t][p].push(newEvent);// pushed at the end, get end using .back()
					}
					else {
						// MAX_QUEUE reached, flush
						clearChannel(t, p);
					}
				}// tap t
			}
			else if (gateEdge == -1) {
				// here we have a falling gate on poly p
				for (int t = 0; t < NUM_TAPS; t++) {
					// scan the taps to add the gateOffFrame
					if ( !isTapActive(t) || (t == (NUM_TAPS - 1) && !lastTapAllowed) ) {
						continue;
					}					
					if (!channel[t][p].empty()) {
						NoteEvent backEvent = channel[t][p].back();
						if (backEvent.muted) {
							channel[t][p].pop();
						}
						else {
							if (backEvent.gateOffFrame <= 0) {
								// we have a pending event
								int64_t gateOffFrame = -backEvent.gateOffFrame + (double)args.frame;
								if (gateOffFrame > backEvent.gateOnFrame) {
									// well-formed event
									channel[t][p].back().gateOffFrame = gateOffFrame;
								}
								else {
									// error, back event would have a gateOffFrame <= gateOnFrame, flush
									clearChannel(t, p);
								}
							}
							else {
								// error, back event already has a gateOffFrame, flush
								clearChannel(t, p);
							}
						}
					}
				}// tap t					
			}
		}// poly p
		
		
		// outputs
		outputs[CLK_OUTPUT].setVoltage(clockSignal);
		// do tap0 outputs first
		int c = 0;// running index for all poly cable writes
		if (!wetOnly) {
			for (; c < poly; c++) {
				outputs[CV_OUTPUT].setVoltage(inputs[CV_INPUT].getVoltage(c), c);
				outputs[GATE_OUTPUT].setVoltage(inputs[GATE_INPUT].getVoltage(c), c);
				outputs[CV2_OUTPUT].setVoltage(inputs[CV2_INPUT].getVoltage(c),c);
			}
		}
		// now do main tap outputs
		for (int t = 0; t < NUM_TAPS; t++) {
			if ( !isTapActive(t) || (t == (NUM_TAPS - 1) && !lastTapAllowed) ) {
				continue;
			}
			for (int p = 0; p < poly; p++, c++) {
				bool gate = false;
				if (!channel[t][p].empty()) {
					NoteEvent e = channel[t][p].front();
					if (args.frame >= e.gateOnFrame) {
						outputs[CV_OUTPUT].setVoltage(e.cv, c);
						outputs[CV2_OUTPUT].setVoltage(e.cv2, c);
						gate = true;
						if (e.gateOffFrame > 0 && args.frame >= e.gateOffFrame) {
							gate = false;
							channel[t][p].pop();
						}
					}
				}
				outputs[GATE_OUTPUT].setVoltage(gate ? 10.0f : 0.0f, c);
			}
		}
		
		
		// lights
		if (refresh.processLights()) {
			// lights
			// simple ones done in NoteEchoWidget::step(), gate lights must be here (see detailed comment in step():
			
			// gate lights
			if (notifyPoly > 0) {
				// do tap0 outputs first
				for (int i = 0; i < MAX_POLY; i++) {
					lights[GATE_LIGHTS + i].setBrightness(i < poly && !wetOnly ? 1.0f : 0.0f);
				}
				// now do main tap outputs
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
				// do tap0 outputs first
				for (int i = 0; i < MAX_POLY; i++) {
					bool lstate = i < poly && !wetOnly;
					lights[GATE_LIGHTS + i].setBrightness(lstate && outputs[GATE_OUTPUT].getVoltage(c) ? 1.0f : 0.0f);
					if (lstate) c++;
				}
				// now do main tap outputs
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
			for (int i = 0; i < NUM_TAPS; i++) {
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


struct NoteEcho2Widget : ModuleWidget {
	struct PolyKnob : IMFourPosSmallKnob {
		NoteEcho2 *module = nullptr;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifyPoly = (long) (NoteEcho2::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
			}
			IMFourPosSmallKnob::onDragMove(e);
		}
	};
	struct TapKnob : IMMediumKnob {
		NoteEcho2 *module = nullptr;
		int tapNum = 0;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifyInfo[tapNum] = 0l;
			}
			IMMediumKnob::onDragMove(e);
		}
	};
	struct SemitoneKnob : IMMediumKnob {
		NoteEcho2 *module = nullptr;
		int tapNum = 0;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifySource[tapNum] = 1l;
				module->notifyInfo[tapNum] = (long) (NoteEcho2::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
			}
			IMMediumKnob::onDragMove(e);
		}
	};
	struct Cv2Knob : IMSmallKnob {
		NoteEcho2 *module = nullptr;
		int tapNum = 0;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifySource[tapNum] = 2l;
				module->notifyInfo[tapNum] = (long) (NoteEcho2::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
			}
			IMSmallKnob::onDragMove(e);
		}
	};
	struct ProbKnob : IMSmallKnob {
		NoteEcho2 *module = nullptr;
		int tapNum = 0;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				module->notifySource[tapNum] = 3l;
				module->notifyInfo[tapNum] = (long) (NoteEcho2::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
			}
			IMSmallKnob::onDragMove(e);
		}
	};
	
	struct TapDisplayWidget : TransparentWidget {// a centered display, must derive from this
		NoteEcho2 *module = nullptr;
		int tapNum = 0;
		std::shared_ptr<Font> font;
		std::string fontPath;
		std::string dispStr;
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f; // 18.2f for 14 pt, 19.7f for 15pt
		
		TapDisplayWidget(int _tapNum, Vec _pos, Vec _size, NoteEcho2 *_module) {
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
					dispStr = "  - ";
				}
				else if (tapNum == 3 && !module->isLastTapAllowed()) {
					dispStr = "O VF";
				}
				else if (module->notifyInfo[tapNum] > 0l) {
					if (module->notifySource[tapNum] == 1) {
						// semitone
						int ist = module->getSemiValue(tapNum);
						dispStr = string::f("  %2u", (unsigned)(std::abs(ist)));
						if (ist != 0) {
							dispStr[0] = ist > 0 ? '+' : '-';
						}
					}
					else if (module->notifySource[tapNum] == 2) {
						// CV2
						float cv2 = module->params[NoteEcho2::CV2_PARAMS + tapNum].getValue();
						if (module->isCv2Offset()) {
							// CV2 mode is: offset
							float cvValPrint = std::fabs(cv2) * 10.0f;
							if (cvValPrint > 9.975f) {
								dispStr = "  10";
							}
							else if (cvValPrint < 0.025f) {
								dispStr = "   0";
							}
							else {
								dispStr = string::f("%3.2f", cvValPrint);// Three-wide, two positions after the decimal, left-justified
								dispStr[1] = '.';// in case locals in printf
							}								
						}
						else {
							// CV2 mode is: scale
							unsigned int iscale100 = (unsigned)(std::round(std::fabs(cv2) * 100.0f));
							if ( iscale100 >= 100) {
								dispStr = "   1";
							}
							else if (iscale100 >= 1) {
								dispStr = string::f("0.%02u", (unsigned) iscale100);
							}	
							else {
								dispStr = "   0";
							}
						}
					}
					else {
						// Prob
						float prob = module->params[NoteEcho2::PROB_PARAMS + tapNum].getValue();
						unsigned int iprob100 = (unsigned)(std::round(prob * 100.0f));
						if ( iprob100 >= 100) {
							dispStr = "   1";
						}
						else if (iprob100 >= 1) {
							dispStr = string::f("0.%02u", (unsigned) iprob100);
						}	
						else {
							dispStr = "   0";
						}
					}
				}
				else {
					dispStr = string::f("D %2u", (unsigned)(module->getTapValue(tapNum)));	
				}
				nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, &dispStr[1], NULL);
				dispStr[1] = 0;
				nvgText(args.vg, textPos.x, textPos.y, dispStr.c_str(), NULL);
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
					float getMinValue() override {return NoteEcho2::cv2NormMin;}
					float getMaxValue() override {return NoteEcho2::cv2NormMax;}
					float getDefaultValue() override {return NoteEcho2::cv2NormDefault;}
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
		NoteEcho2 *module = static_cast<NoteEcho2*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), static_cast<SvgPanel*>(getPanel()));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createBoolPtrMenuItem("Filter out identical notes", "", &module->noteFilter));
		
		menu->addChild(createBoolPtrMenuItem("Echoes only", "", &module->wetOnly));

		createCv2NormalizationMenu(menu, &(module->cv2NormalledVoltage));
	}	

	
	
	NoteEcho2Widget(NoteEcho2 *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/NoteEcho2.svg")));
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
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col51, row0)), true, module, NoteEcho2::CLK_INPUT, mode));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col52, row0)), true, module, NoteEcho2::CV_INPUT, mode));
		addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(col52 + 3.3f, row0 - 6.6f)), module, NoteEcho2::FILTER_LIGHT));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col53, row0)), true, module, NoteEcho2::GATE_INPUT, mode));
		addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(col53 + 5.6f, row0 - 6.6f)), module, NoteEcho2::WET_LIGHT));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col54, row0)), true, module, NoteEcho2::CV2_INPUT, mode));
		addChild(createLightCentered<TinyLight<GreenRedLight>>(mm2px(Vec(col54 + 4.7f, row0 - 6.6f)), module, NoteEcho2::CV2NORM_LIGHTS));
		
		PolyKnob* polyKnobP;
		addParam(polyKnobP = createDynamicParamCentered<PolyKnob>(mm2px(Vec(col55, row0)), module, NoteEcho2::POLY_PARAM, mode));
		polyKnobP->module = module;
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col56, row0)), true, module, NoteEcho2::CLEAR_INPUT, mode));

		
		// rows 1-4
		for (int i = 0; i < NoteEcho2::NUM_TAPS; i++) {
			TapKnob* tapKnobP;
			addParam(tapKnobP = createDynamicParamCentered<TapKnob>(mm2px(Vec(col41, row1 + i * row24d)), module, NoteEcho2::TAP_PARAMS + i, mode));	
			tapKnobP->module = module;
			tapKnobP->tapNum = i;
			
			// displays
			TapDisplayWidget* tapDisplayWidget = new TapDisplayWidget(i, mm2px(Vec(col42, row1 + i * row24d)), VecPx(displayWidths + 4, displayHeights), module);
			addChild(tapDisplayWidget);
			svgPanel->fb->addChild(new DisplayBackground(tapDisplayWidget->box.pos, tapDisplayWidget->box.size, mode));
			
			SemitoneKnob* stKnobP;
			addParam(stKnobP = createDynamicParamCentered<SemitoneKnob>(mm2px(Vec(col43, row1 + i * row24d)), module, NoteEcho2::ST_PARAMS + i, mode));
			stKnobP->module = module;
			stKnobP->tapNum = i;
			
			Cv2Knob* cv2KnobP;			
			addParam(cv2KnobP = createDynamicParamCentered<Cv2Knob>(mm2px(Vec(col55, row1 + i * row24d)), module, NoteEcho2::CV2_PARAMS + i, mode));
			cv2KnobP->module = module;
			cv2KnobP->tapNum = i;
			
			ProbKnob* probKnobP;			
			addParam(probKnobP = createDynamicParamCentered<ProbKnob>(mm2px(Vec(col56, row1 + i * row24d)), module, NoteEcho2::PROB_PARAMS + i, mode));
			probKnobP->module = module;
			probKnobP->tapNum = i;
		}
		
		// row 5
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col51, row5)), false, module, NoteEcho2::CLK_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col52, row5)), false, module, NoteEcho2::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col53, row5)), false, module, NoteEcho2::GATE_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col54, row5)), false, module, NoteEcho2::CV2_OUTPUT, mode));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col55, row5 - 1.5f)), module, NoteEcho2::CV2MODE_PARAM, mode, svgPanel));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col56, row5 - 1.5f)), module, NoteEcho2::PMODE_PARAM, mode, svgPanel));

		// gate lights
		static const float gldx = 3.0f;
		static const float glto = -7.0f;
		static const float posy[5] = {28.0f, row1 + glto, row1 + glto + row24d, row1 + glto + 2 * row24d, row1 + glto + 3 * row24d};
		
		for (int j = 0; j < 5; j++) {
			float posx = col42 - gldx * 1.5f;
			for (int i = 0; i < NoteEcho2::MAX_POLY; i++) {
				addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(posx, posy[j])), module, NoteEcho2::GATE_LIGHTS + j * NoteEcho2::MAX_POLY + i));
				posx += gldx;
			}
		}
	}
	
	void step() override {
		if (module) {
			NoteEcho2* m = static_cast<NoteEcho2*>(module);
			
			// gate lights done in process() since they look at output[], and when connecting/disconnecting cables the cable sizes are reset (and buffers cleared), which makes the gate lights flicker
			
			// filter light
			m->lights[NoteEcho2::FILTER_LIGHT].setBrightness( m->noteFilter ? 1.0f : 0.0f );
			
			// wet light
			m->lights[NoteEcho2::WET_LIGHT].setBrightness( m->wetOnly ? 1.0f : 0.0f );
			
			// CV2 norm light
			bool cv2uncon = !(m->inputs[NoteEcho2::CV2_INPUT].isConnected());
			float green = cv2uncon ? (((float)(m->cv2NormalledVoltage)) / NoteEcho2::cv2NormMax) : 0.0f;
			float red =   cv2uncon ? (((float)(m->cv2NormalledVoltage)) / NoteEcho2::cv2NormMin) : 0.0f;
			m->lights[NoteEcho2::CV2NORM_LIGHTS + 0].setBrightness(green);
			m->lights[NoteEcho2::CV2NORM_LIGHTS + 1].setBrightness(red);

			// CV2 knobs' labels
			m->refreshCv2ParamQuantities();
		}
		ModuleWidget::step();
	}
};

Model *modelNoteEcho2 = createModel<NoteEcho2, NoteEcho2Widget>("NoteEcho2");
