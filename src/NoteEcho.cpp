//***********************************************************************************************
//CV-Gate shift register / delay module for VCV Rack by Marc Boulé
//Follows the set-on-write paradigm for both modes (regarding Delay, Semi, CV2+-, p knobs);
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Pyer. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************

#include "ImpromptuModular.hpp"
#include <queue>


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
		PMODE_PARAM,// end of 1st version of module
		// ----
		DEL_MODE_PARAM,// start of 2nd version of module with two modes
		CV2NORM_PARAM,
		FREEZE_PARAM,
		FRZLEN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		GATE_INPUT,
		CV2_INPUT,
		CLK_INPUT,
		CLEAR_INPUT,
		// ----
		FREEZE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE_OUTPUT,
		CV2_OUTPUT,
		// CLK_OUTPUT,// unused
		// ----
		NUM_OUTPUTS
	};
	enum LightIds {
		SD_LIGHT,
		FILTER_LIGHT,
		WET_LIGHT,
		ENUMS(GATE_LIGHTS, (NUM_TAPS + 1) * MAX_POLY),
		// ----
		FREEZE_LIGHT,
		NUM_LIGHTS
	};
	
	
	struct NoteEvent {
		// all info here has semitone, cv2offset and prob applied 
		// an event is considered pending when gateOffFrame==0 (waiting for falling edge of a gate input), and ready when >0; when pending, rest of struct is populated
		int64_t capturedFrame;
		int64_t gateOnFrame;
		int64_t gateOffFrame;// 0 when pending (DEL MODE)
		float cv;
		float cv2;
		bool muted;// this is used to enter events that fail the probability gen so as to properly queue things for falling gate edges
		
		NoteEvent(float _cv, float _cv2, int64_t _capturedFrame, int64_t _gateOnFrame, int64_t _gateOffFrame, bool _muted) {
			cv = _cv;
			cv2 = _cv2;
			capturedFrame = _capturedFrame;
			gateOnFrame = _gateOnFrame;
			gateOffFrame = _gateOffFrame;
			muted = _muted;
		}
	};

	
	// Expander
	// none

	// Constants
	static const int MAX_DEL = 32;
	static const int SR_LENGTH = MAX_DEL + 1;
	static const int DEL_MAX_QUEUE = MAX_DEL * 4;// assume quarter clocked note is finest resolution 
	static constexpr float delayInfoTime = 3.0f;// seconds
	static constexpr float cv2NormMin = -10.0f;
	static constexpr float cv2NormMax = 10.0f;
	static constexpr float cv2NormDefault = 0.0f;
	static constexpr float groupedProbsEpsilon = 0.02f;// time in seconds within which gates are considered grouped across polys, for when random mode is set to chord, for DEL Mode
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool noteFilter;
	bool wetOnly;// excludes tap0 from outputs, when freeze, this note will be missing in the loop, but all echoes as properly timed in the loop, will play)
	// float cv2NormalledVoltage;
	int clkDelay;// SR Mode
	float clkDelReg[2];// SR Mode
	int64_t clockPeriod;// DEL Mode

	// No need to save, with reset
	bool freeze;
	std::queue<NoteEvent> channel[NUM_TAPS + 1][MAX_POLY];// all info here has semitone, cv2offset and prob applied, last tap is dry (tap 0)
	int64_t lastRisingClkFrame;// -1 when none
	int64_t clkCount;
	int lastPoly;
	bool lastDelMode;
	
	// No need to save, no reset
	int notifySource[NUM_TAPS] = {};// 0 (normal), 1 (semi) 2 (CV2), 3 (prob), 4 (freeze length)
	long notifyInfo[NUM_TAPS] = {};// downward step counter when semi, cv2, prob to be displayed, 0 when normal tap display
	long notifyPoly = 0l;// downward step counter when notify poly size in leds, 0 when normal leds
	RefreshCounter refresh;
	TriggerRiseFall clkTrigger;
	TriggerRiseFall gateTriggers[NUM_TAPS];
	Trigger freezeButtonTrigger;
	Trigger clearTrigger;


	bool getIsDelMode() {
		return params[DEL_MODE_PARAM].getValue() >= 0.5f;
	}
	int getPolyKnob() {
		return (int)(params[POLY_PARAM].getValue() + 0.5f);
	}
	int getFreezeLengthKnob() {
		return (int)(params[FRZLEN_PARAM].getValue() + 0.5f);
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
	float cv2NormalledVoltage() {
		return params[CV2NORM_PARAM].getValue();
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
	
	
	void sampleCvs(int t, int p, float* cv, float* cv2) {
		// this method supports t == NUM_TAPS in case it is called for tap0, in which case the cv semi knob and the cv2 mod knobs are not probed since they do not exist
		// cv
		*cv = inputs[CV_INPUT].getChannels() > p ? inputs[CV_INPUT].getVoltage(p) : 0.0f;
		if (t < NUM_TAPS) {
			*cv += getSemiVolts(t);
		}
		
		// cv2
		if (inputs[CV2_INPUT].getChannels() < 1) {
			*cv2 = cv2NormalledVoltage();
		}
		else {
			*cv2 = inputs[CV2_INPUT].getVoltage(std::min(p, inputs[CV2_INPUT].getChannels() - 1));
		}
		if (t < NUM_TAPS) {
			if (isCv2Offset()) {
				*cv2 += params[CV2_PARAMS + t].getValue() * 10.0f;
			}
			else {
				*cv2 *= params[CV2_PARAMS + t].getValue();
			}
		}
	}
	
	
	void finishPending(int t, int p, int64_t argsFrame, bool isDelMode) {
		if (!channel[t][p].empty()) {
			if (channel[t][p].back().muted) {
				channel[t][p].pop();
			}
			else {
				if (channel[t][p].back().gateOffFrame == 0) {
					// we have a pending event
					if (isDelMode) {
						int64_t gateFrames = argsFrame - channel[t][p].back().capturedFrame;
						channel[t][p].back().gateOffFrame = channel[t][p].back().gateOnFrame + gateFrames;
					}
					else {
						channel[t][p].back().gateOffFrame = channel[t][p].back().gateOnFrame + 1;// not really relevant, but for good form
					}
				}
				else {
					// error, back event already has a gateOffFrame, flush channel
					clearChannel(t, p);
				}
			}
		}
	}	
	
	
	
	NoteEcho() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(POLY_PARAM, 1, MAX_POLY, 1, "Input polyphony");
		paramQuantities[POLY_PARAM]->snapEnabled = true;
		configSwitch(CV2MODE_PARAM, 0.0f, 1.0f, 1.0f, "CV2 mode", {"Scale", "Offset"});
		configSwitch(PMODE_PARAM, 0.0f, 1.0f, 1.0f, "Random mode", {"Separate", "Chord"});
		configSwitch(DEL_MODE_PARAM, 0.0f, 1.0f, 1.0f, "Main mode", {"Shift register", "Delay"});
		configParam(CV2NORM_PARAM, -10.0f, 10.0f, 0.0f, "CV2 input normalization", "");
		configParam(FREEZE_PARAM, 0.0f, 1.0f, 0.0f, "Freeze (loop)");
		configParam(FRZLEN_PARAM, 1.0f, (float)(MAX_DEL), 4.0f, "Freeze length");
		paramQuantities[FRZLEN_PARAM]->snapEnabled = true;
		
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
		configInput(FREEZE_INPUT, "Freeze (loop)");

		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV2_OUTPUT, "CV2/Velocity");
		// configOutput(CLK_OUTPUT, "Clock");
		
		configLight(SD_LIGHT, "Sample delay active");
		// configLight(FILTER_LIGHT, "Filter identical notes");
		// configLight(WET_LIGHT, "Echoes only");
		
		configBypass(CV_INPUT, CV_OUTPUT);
		configBypass(GATE_INPUT, GATE_OUTPUT);
		configBypass(CV2_INPUT, CV2_OUTPUT);
		// configBypass(CLK_INPUT, CLK_OUTPUT);

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	
	void clearChannel(int t, int p) {
		while (!channel[t][p].empty()) {
			channel[t][p].pop();
		}
	}
	void clear() {
		for (int t = 0; t < NUM_TAPS + 1; t++) {
			for (int p = 0; p < MAX_POLY; p++) {
				// clear all including tap0 which is last tap
				clearChannel(t, p);
			}
		}
	}

	void onReset() override final {
		noteFilter = false;
		wetOnly = false;
		// cv2NormalledVoltage = 0.0f;
		clkDelay = 0;
		clkDelReg[0] = 0.0f;
		clkDelReg[1] = 0.0f;
		clockPeriod = (int64_t)(APP->engine->getSampleRate());// 60 BPM until detected
		resetNonJson();
	}
	void resetNonJson() {
		freeze = false;
		clear();
		lastRisingClkFrame = -1;// none
		clkCount = 0;
		lastPoly = getPolyKnob();
		lastDelMode = getIsDelMode();
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
		// json_object_set_new(rootJ, "cv2NormalledVoltage", json_real(cv2NormalledVoltage));

		// clkDelay
		json_object_set_new(rootJ, "clkDelay", json_integer(clkDelay));
		
		// clkDelReg[]
		json_object_set_new(rootJ, "clkDelReg0", json_real(clkDelReg[0]));
		json_object_set_new(rootJ, "clkDelReg1", json_real(clkDelReg[1]));
		
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

		// cv2NormalledVoltage (also the flag for 1st version)
		json_t *cv2NormalledVoltageJ = json_object_get(rootJ, "cv2NormalledVoltage");
		if (cv2NormalledVoltageJ) {
			// old version
			params[DEL_MODE_PARAM].setValue(0.0f);// force SR Mode since this is a 1st version load
			params[CV2NORM_PARAM].setValue(json_number_value(cv2NormalledVoltageJ));
		}
		else {
			// new version with two modes
			// nothing to do (DEL_MODE_PARAM and CV2NORM_PARAM already loaded or init properly)
		}

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

		// clockPeriod
		json_t *clockPeriodJ = json_object_get(rootJ, "clockPeriod");
		if (clockPeriodJ)
			clockPeriod = (int64_t)json_integer_value(clockPeriodJ);

		resetNonJson();
	}


	void onSampleRateChange() override {
		clear();
	}		


	void process(const ProcessArgs &args) override {
		int poly = getPolyKnob();
		if (poly != lastPoly) {
			clear();
			lastPoly = poly;
		}
		bool isDelMode = getIsDelMode();
		if (isDelMode != lastDelMode) {
			clear();
			lastDelMode = isDelMode;
		}
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
		}// userInputs refresh
	
		// Freeze
		if (freezeButtonTrigger.process(inputs[FREEZE_INPUT].getVoltage() + params[FREEZE_PARAM].getValue())) {
			freeze = !freeze;
		}
	
	
		// clear and clock
		if (clearTrigger.process(inputs[CLEAR_INPUT].getVoltage())) {
			clear();
		}
		float clockSignal = (clkDelay <= 0 || isDelMode) ? inputs[CLK_INPUT].getVoltage() : clkDelReg[clkDelay - 1];
		int clkEdge = clkTrigger.process(clockSignal);
		if (clkEdge == 1) {
			if (!isDelMode) {
				// ** SR MODE
				clkCount++;
			}
			else {
				// ** DEL MODE
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
		}
		
		// sample the inputs on main clock (SR Mode) or poly gates (DEL Mode)
		int gateEdges[MAX_POLY];
		int64_t currFrameOrClk = isDelMode ? args.frame : clkCount;
		for (int p = 0; p < MAX_POLY; p++) {
			// keep triggers continually processed, just like clock/tempo, irrespective of mode
			gateEdges[p] = gateTriggers[p].process(inputs[GATE_INPUT].getVoltage(p));
		}
		for (int p = 0; p < poly; p++) {
			// ** SR MODE : sample the inputs on rising clock edge (finish job on falling)
			// ** DEL MODE : sample the inputs on rising poly gate edges (finish job on falling)
			int eventEdge = isDelMode ? gateEdges[p] : clkEdge;				
			if (eventEdge == 1) {
				// here we have a rising gate on poly p, or a rising clk
				
				// do dry first (tap0):
				float cv, cv2;
				sampleCvs(NUM_TAPS, p, &cv, &cv2);
				int64_t capturedFrame = currFrameOrClk;
				int64_t gateOnFrame = capturedFrame;
				int64_t gateOffFrame = 0;// will be completed when gate/clk falls
				bool muted = !wetOnly;
				if (channel[NUM_TAPS][p].size() >= DEL_MAX_QUEUE) {
					continue;// skip event if queue too full
				}
				NoteEvent newEvent(cv, cv2, capturedFrame, gateOnFrame, gateOffFrame, muted);
				channel[NUM_TAPS][p].push(newEvent);// pushed at the end, get end using .back()

				// now scan taps:	
				for (int t = 0; t < NUM_TAPS; t++) {
					if ( !isTapActive(t) || (t == (NUM_TAPS - 1) && !lastTapAllowed) ) {
						continue;
					}
					if (channel[t][p].size() >= DEL_MAX_QUEUE) {
						continue;// skip event if queue too full
					}
					sampleCvs(t, p, &cv, &cv2);

					// frames
					capturedFrame = currFrameOrClk + (isDelMode ? 0 : 1);// 1 is since dry is clkCount
					gateOnFrame = capturedFrame + (isDelMode ? clockPeriod : 1) * (int64_t)getTapValue(t);
					gateOffFrame = 0;// will be completed when gate/clk falls
					
					// muted
					muted = false;
					bool gotMuted = false;
					if (isSingleProbs()) {
						// try to get muted prob from another poly if close enough time-wise
						int64_t closenessFrames = (int64_t)(groupedProbsEpsilon * args.sampleRate);
						for (int p2 = 0; p2 < poly; p2++) {
							if (p2 == p || channel[t][p2].empty()) continue;
							int64_t delta = channel[t][p2].back().capturedFrame - capturedFrame;
							int64_t delta2 = channel[t][p2].back().gateOnFrame - gateOnFrame;
							bool closeEnough;
							if (isDelMode) {
								closeEnough = llabs(delta) < closenessFrames && llabs(delta2) < closenessFrames;
							}
							else {
								closeEnough = (delta == 0 && delta2 == 0);
							}
							if (closeEnough) {
								muted = channel[t][p2].back().muted;
								gotMuted = true;
								break;
							}
						}
					}
					if (!gotMuted) {
						// generate new muted according to prob
						muted = !getGateProbEnableForTap(t);
					}

					NoteEvent newEvent(cv, cv2, capturedFrame, gateOnFrame, gateOffFrame, muted);
					channel[t][p].push(newEvent);// pushed at the end, get end using .back()
				}// tap t
			}
			else if (eventEdge == -1) {
				// here we have a falling gate on poly p, or falling clk
				
				// do dry first (tap0):	
				finishPending(NUM_TAPS, p, args.frame, isDelMode);
				// now scan taps:
				for (int t = 0; t < NUM_TAPS; t++) {
					// scan the taps to set the gateOffFrame
					if ( !isTapActive(t) || (t == (NUM_TAPS - 1) && !lastTapAllowed) ) {
						continue;
					}					
					finishPending(t, p, args.frame, isDelMode);
				}// tap t					
			}
		}// poly p

		
		// outputs
		// outputs[CLK_OUTPUT].setVoltage(clockSignal);
		// do tap0 outputs first
		int c = 0;// running index for all poly cable writes
		// do tap0
		for (int p = 0; p < poly; p++, c++) {
			bool gate = false;
			if (!channel[NUM_TAPS][p].empty()) {
				NoteEvent e = channel[NUM_TAPS][p].front();
				if (currFrameOrClk >= e.gateOnFrame) {
					gate = true;
					if (e.gateOffFrame != 0 && currFrameOrClk >= e.gateOffFrame) {
						// note is now done, remove it (CVs will stay held in the ports though)
						gate = false;
						channel[NUM_TAPS][p].pop();
					}
					else if (noteFilter) {
						// don't allow the gate to turn on if the same CV is already on another channel that has its gate high
						for (int c2 = 0; c2 < c; c2++) {
							if (outputs[GATE_OUTPUT].getVoltage(c2) != 0.0f &&
								outputs[CV_OUTPUT].getVoltage(c2) == e.cv) {
								gate = false;
								channel[NUM_TAPS][p].pop();
								break;
							}
						}
					}
					if (gate && !wetOnly) {
						outputs[CV_OUTPUT].setVoltage(e.cv, c);
						outputs[CV2_OUTPUT].setVoltage(e.cv2, c);							
					}
				}
			}
			if (!wetOnly) {
				if (!isDelMode) {
					gate &= clockSignal > 1.0f;
				}
				outputs[GATE_OUTPUT].setVoltage(gate ? 10.0f : 0.0f, c);
			}
		}
		if (wetOnly) {
			c = 0;
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
					if (currFrameOrClk >= e.gateOnFrame) {
						gate = true;
						if (e.gateOffFrame != 0 && currFrameOrClk >= e.gateOffFrame) {
							// note is now done, remove it (CVs will stay held in the ports though)
							gate = false;
							channel[t][p].pop();
						}
						else if (noteFilter) {
							// don't allow the gate to turn on if the same CV is already on another channel that has its gate high
							for (int c2 = 0; c2 < c; c2++) {
								if (outputs[GATE_OUTPUT].getVoltage(c2) != 0.0f &&
									outputs[CV_OUTPUT].getVoltage(c2) == e.cv) {
									gate = false;
									channel[t][p].pop();
									break;
								}
							}
						}
						if (gate) {
							outputs[CV_OUTPUT].setVoltage(e.cv, c);
							outputs[CV2_OUTPUT].setVoltage(e.cv2, c);							
						}
					}
				}
				if (!isDelMode) {
					gate &= clockSignal > 1.0f;
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
				for (int p = 0; p < MAX_POLY; p++) {
					lights[GATE_LIGHTS + p].setBrightness(p < poly && !wetOnly ? 1.0f : 0.0f);
				}
				// now do main tap outputs
				for (int t = 0; t < NUM_TAPS; t++) {
					for (int p = 0; p < MAX_POLY; p++) {
						bool lstate = p < poly && isTapActive(t);
						if (t == (NUM_TAPS - 1) && !isLastTapAllowed()) {
							lstate = false;
						}
						lights[GATE_LIGHTS + 4 + t * MAX_POLY + p].setBrightness(lstate ? 1.0f : 0.0f);
					}
				}
			}
			else {
				int c = 0;
				// do tap0 outputs first
				for (int p = 0; p < MAX_POLY; p++) {
					bool lstate = p < poly && !wetOnly;
					lights[GATE_LIGHTS + p].setBrightness(lstate && outputs[GATE_OUTPUT].getVoltage(c) ? 1.0f : 0.0f);
					if (lstate) c++;
				}
				// now do main tap outputs
				for (int t = 0; t < NUM_TAPS; t++) {
					for (int p = 0; p < MAX_POLY; p++) {
						bool lstate = p < poly && isTapActive(t);
						if (t == (NUM_TAPS - 1) && !isLastTapAllowed()) {
							lstate = false;
						}
						lights[GATE_LIGHTS + 4 + t * MAX_POLY + p].setBrightness(lstate && outputs[GATE_OUTPUT].getVoltage(c) ? 1.0f : 0.0f);
						if (lstate) c++;
					}
				}
			}

			// info notification counters
			for (int t = 0; t < NUM_TAPS; t++) {
				notifyInfo[t]--;
				if (notifyInfo[t] < 0l) {
					notifyInfo[t] = 0l;
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
	struct FreezeKnob : IMMediumKnob {
		NoteEcho *module = nullptr;
		void onDragMove(const event::DragMove &e) override {
			if (module) {
				for (int t = 0; t < NoteEcho::NUM_TAPS; t++) {
					module->notifySource[t] = 4l;
					module->notifyInfo[t] = (long) (NoteEcho::delayInfoTime * APP->engine->getSampleRate() / RefreshCounter::displayRefreshStepSkips);
				}
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
		std::string dispStr;
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
						float cv2 = module->params[NoteEcho::CV2_PARAMS + tapNum].getValue();
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
					else if (module->notifySource[tapNum] == 3) {
						// Prob
						float prob = module->params[NoteEcho::PROB_PARAMS + tapNum].getValue();
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
					else {
						// Freeze length
						if (tapNum > 0) {
							dispStr = "";
						}
						else {
							dispStr = string::f("F %2u", (unsigned)(module->getFreezeLengthKnob()));	
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
	

	void appendContextMenu(Menu *menu) override {
		NoteEcho *module = static_cast<NoteEcho*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), static_cast<SvgPanel*>(getPanel()));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));

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
		
		menu->addChild(createBoolPtrMenuItem("Filter out identical notes", "", &module->noteFilter));
		
		menu->addChild(createBoolPtrMenuItem("Wet (echoes) only", "", &module->wetOnly));
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
		static const float col57 = col56 + col5d + 2.0f;
		
		static const float col54d = 2.0f;
		static const float col41 = col51 + col54d;// Tap
		static const float col43 = col54 - col54d;// Semitone
		static const float col42 = (col41 + col43) / 2.0f;// Displays
		
		static const float row0 = 21.0f;// Clk, CV, Gate, CV2 inputs, and sampDelayClk
		static const float row1 = row0 + 22.0f;// tap 1
		static const float row24d = 16.0f;// taps 2-4
		static const float row5 = 110.5f;// Clk, CV, Gate, CV2 outputs
		// static const float row4 = row5 - 20.0f;// CV inputs and poly knob
		
		static const float displayWidths = 48; // 43 for 14pt, 46 for 15pt
		static const float displayHeights = 24; // 22 for 14pt, 24 for 15pt

		
		// row 0
		PolyKnob* polyKnobP;
		addParam(polyKnobP = createDynamicParamCentered<PolyKnob>(mm2px(Vec(col51, row0)), module, NoteEcho::POLY_PARAM, mode));
		polyKnobP->module = module;

		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col52, row0)), true, module, NoteEcho::CV_INPUT, mode));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col53, row0)), true, module, NoteEcho::GATE_INPUT, mode));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col54, row0)), true, module, NoteEcho::CV2_INPUT, mode));
		
		addParam(createDynamicParamCentered<IMSmallKnob>(mm2px(Vec(col55, row0)), module, NoteEcho::CV2NORM_PARAM, mode));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col56, row0)), true, module, NoteEcho::CLK_INPUT, mode));
		addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(col56 + 4.3f, row0 + 6.6f)), module, NoteEcho::SD_LIGHT));
				
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col57, row0)), module, NoteEcho::DEL_MODE_PARAM, mode, svgPanel));

		
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
		// addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col51, row5)), false, module, NoteEcho::CLK_OUTPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col51, row5)), true, module, NoteEcho::CLEAR_INPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col52, row5)), false, module, NoteEcho::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col53, row5)), false, module, NoteEcho::GATE_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col54, row5)), false, module, NoteEcho::CV2_OUTPUT, mode));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col55, row5 - 1.5f)), module, NoteEcho::CV2MODE_PARAM, mode, svgPanel));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col56, row5 - 1.5f)), module, NoteEcho::PMODE_PARAM, mode, svgPanel));
		
		// right side column (excl. first row)
		addParam(createParamCentered<LEDBezel>(mm2px(VecPx(col57, row1)), module, NoteEcho::FREEZE_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(mm2px(VecPx(col57, row1)), module, NoteEcho::FREEZE_LIGHT));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col57, row1 + row24d - 2.0f)), true, module, NoteEcho::FREEZE_INPUT, mode));
		
		FreezeKnob* frzLenKnobP;
		addParam(frzLenKnobP = createDynamicParamCentered<FreezeKnob>(mm2px(Vec(col57, row1 + 2 * row24d)), module, NoteEcho::FRZLEN_PARAM, mode));	
		frzLenKnobP->module = module;

		addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(col57, row5 - 16.6f)), module, NoteEcho::FILTER_LIGHT));
		
		addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(col57, row5)), module, NoteEcho::WET_LIGHT));


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
			
			// gate lights done in process() since they look at output[], and when connecting/disconnecting cables the cable sizes are reset (and buffers cleared), which makes the gate lights flicker
			
			// sample delay light
			m->lights[NoteEcho::SD_LIGHT].setBrightness( ((float)(m->clkDelay)) / 2.0f );
			
			// filter light
			m->lights[NoteEcho::FILTER_LIGHT].setBrightness( m->noteFilter ? 1.0f : 0.0f );
			
			// wet light
			m->lights[NoteEcho::WET_LIGHT].setBrightness( m->wetOnly ? 1.0f : 0.0f );
			
			// CV2 knobs' labels
			m->refreshCv2ParamQuantities();
			
			// Freeze light
			m->lights[NoteEcho::FREEZE_LIGHT].setBrightness(m->freeze ? 1.0f : 0.0f);

		}
		ModuleWidget::step();
	}
};

Model *modelNoteEcho = createModel<NoteEcho, NoteEchoWidget>("NoteEcho");
