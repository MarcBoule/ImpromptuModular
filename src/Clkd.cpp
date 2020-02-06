//***********************************************************************************************
//Half-sized version of Clocked without PW, Swing and Delay for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé
//***********************************************************************************************


#include "ImpromptuModular.hpp"


class Clock {
	// The -1.0 step is used as a reset state every period so that 
	//   lengths can be re-computed; it will stay at -1.0 when a clock is inactive.
	// a clock frame is defined as "length * iterations + syncWait", and
	//   for master, syncWait does not apply and iterations = 1

	
	double step;// -1.0 when stopped, [0 to period[ for clock steps 
	double length;// period
	double sampleTime;
	int iterations;// run this many periods before going into sync if sub-clock
	Clock* syncSrc = nullptr; // only subclocks will have this set to master clock
	static constexpr double guard = 0.0005;// in seconds, region for sync to occur right before end of length of last iteration; sub clocks must be low during this period
	bool *resetClockOutputsHigh;
	
	public:
	
	Clock() {
		reset();
	}
	
	void reset() {
		step = -1.0;
	}
	bool isReset() {
		return step == -1.0;
	}
	double getStep() {
		return step;
	}
	void construct(Clock* clkGiven, bool *resetClockOutputsHighPtr) {
		syncSrc = clkGiven;
		resetClockOutputsHigh = resetClockOutputsHighPtr;
	}
	void start() {
		step = 0.0;
	}
	
	void setup(double lengthGiven, int iterationsGiven, double sampleTimeGiven) {
		length = lengthGiven;
		iterations = iterationsGiven;
		sampleTime = sampleTimeGiven;
	}

	void stepClock() {// here the clock was output on step "step", this function is called near end of module::process()
		if (step >= 0.0) {// if active clock
			step += sampleTime;
			if ( (syncSrc != nullptr) && (iterations == 1) && (step > (length - guard)) ) {// if in sync region
				if (syncSrc->isReset()) {
					reset();
				}// else nothing needs to be done, just wait and step stays the same
			}
			else {
				if (step >= length) {// reached end iteration
					iterations--;
					step -= length;
					if (iterations <= 0) 
						reset();// frame done
				}
			}
		}
	}
	
	void applyNewLength(double lengthStretchFactor) {
		if (step != -1.0)
			step *= lengthStretchFactor;
		length *= lengthStretchFactor;
	}
	
	int isHigh() {
		if (step >= 0.0) {
			return (step < (length * 0.5)) ? 1 : 0;
		}
		return (*resetClockOutputsHigh) ? 1 : 0;
	}	
};


//*****************************************************************************


struct Clkd : Module {
	enum ParamIds {
		ENUMS(RATIO_PARAMS, 3),
		BPM_PARAM,// must be contiguous with RATIO_PARAMS
		RESET_PARAM,
		RUN_PARAM,
		BPMMODE_DOWN_PARAM,
		BPMMODE_UP_PARAM,
		DISPLAY_DOWN_PARAM,
		DISPLAY_UP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RESET_INPUT,
		RUN_INPUT,
		BPM_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CLK_OUTPUTS, 4),// master is index 0
		RESET_OUTPUT,
		RUN_OUTPUT,
		BPM_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RESET_LIGHT,
		RUN_LIGHT,
		ENUMS(CLK_LIGHTS, 4 * 2),// master is index 0, room for GreenRed
		ENUMS(BPMSYNC_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	
	// Constants
	const float ratioValues[34] = {1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 23, 24, 29, 31, 32, 37, 41, 43, 47, 48, 53, 59, 61, 64};
	static const int bpmMax = 300;
	static const int bpmMin = 30;
	static constexpr float masterLengthMax = 60.0f / bpmMin;// a length is a period
	static constexpr float masterLengthMin = 60.0f / bpmMax;// a length is a period
	
	
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	bool running;
	bool bpmDetectionMode;
	int restartOnStopStartRun;// 0 = nothing, 1 = restart on stop run, 2 = restart on start run
	bool sendResetOnRestart;
	int ppqn;
	bool resetClockOutputsHigh;
	bool momentaryRunInput;// true = trigger (original rising edge only version), false = level sensitive (emulated with rising and falling detection)
	int displayIndex;

	// No need to save, with reset
	long editingBpmMode;// 0 when no edit bpmMode, downward step counter timer when edit, negative upward when show can't edit ("--") 
	double sampleRate;
	double sampleTime;
	Clock clk[4];
	float bufferedKnobs[4];// must be before ratiosDoubled, master is index 3, ratio knobs are 0 to 2
	bool syncRatios[3];
	int ratiosDoubled[3];
	int extPulseNumber;// 0 to ppqn - 1
	double extIntervalTime;
	double timeoutTime;
	float newMasterLength;
	float masterLength;
	float clkOutputs[4];
	
	// No need to save, no reset
	bool scheduledReset = false;
	long cantRunWarning = 0l;// 0 when no warning, positive downward step counter timer when warning
	RefreshCounter refresh;
	float resetLight = 0.0f;
	Trigger resetTrigger;
	Trigger runButtonTrigger;
	TriggerRiseFall runInputTrigger;
	Trigger bpmDetectTrigger;
	Trigger bpmModeUpTrigger;
	Trigger bpmModeDownTrigger;
	Trigger displayUpTrigger;
	Trigger displayDownTrigger;
	dsp::PulseGenerator resetPulse;
	dsp::PulseGenerator runPulse;

	
	int getRatioDoubled(int ratioKnobIndex) {
		// ratioKnobIndex is 0 to 2 for ratio knobs
		// returns a positive ratio for mult, negative ratio for div (0 never returned)
		bool isDivision = false;
		int i = (int) std::round( bufferedKnobs[ratioKnobIndex] );// [ -(numRatios-1) ; (numRatios-1) ]
		if (i < 0) {
			i *= -1;
			isDivision = true;
		}
		if (i >= 34) {
			i = 34 - 1;
		}
		int ret = (int) (ratioValues[i] * 2.0f + 0.5f);
		if (isDivision) 
			return -1l * ret;
		return ret;
	}
	
	
	Clkd() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(BPMMODE_DOWN_PARAM, 0.0f, 1.0f, 0.0f, "Bpm mode prev");
		configParam(BPMMODE_UP_PARAM, 0.0f, 1.0f, 0.0f,  "Bpm mode next");
		configParam(DISPLAY_DOWN_PARAM, 0.0f, 1.0f, 0.0f, "Display prev");
		configParam(DISPLAY_UP_PARAM, 0.0f, 1.0f, 0.0f,  "Display next");
		char strBuf[32];
		for (int i = 0; i < 3; i++) {// Row 2-4 (sub clocks)
			// Ratio1 knob
			snprintf(strBuf, 32, "Ratio clk %i", i + 1);
			configParam(RATIO_PARAMS + i, (34.0f - 1.0f)*-1.0f, 34.0f - 1.0f, 0.0f, strBuf);		
		}
		configParam(BPM_PARAM, (float)(bpmMin), (float)(bpmMax), 120.0f, "Master clock", " BPM");// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		
		for (int i = 1; i < 4; i++) {
			clk[i].construct(&clk[0], &resetClockOutputsHigh);		
		}
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}
	

	void onReset() override {
		running = true;
		bpmDetectionMode = false;
		restartOnStopStartRun = 0;
		sendResetOnRestart = false;
		ppqn = 4;
		resetClockOutputsHigh = true;
		momentaryRunInput = true;
		displayIndex = 0;// show BPM (knob 0) by default
		resetNonJson(false);
	}
	void resetNonJson(bool delayed) {// delay thread sensitive parts (i.e. schedule them so that process() will do them)
		editingBpmMode = 0l;
		if (delayed) {
			scheduledReset = true;// will be a soft reset
		}
		else {
			resetClkd(true);// hard reset
		}			
	}
	
	void onRandomize() override {
		resetClkd(false);
	}

	
	void resetClkd(bool hardReset) {// set hardReset to true to revert learned BPM to 120 in sync mode, or else when false, learned bmp will stay persistent
		sampleRate = (double)(APP->engine->getSampleRate());
		sampleTime = 1.0 / sampleRate;
		for (int i = 0; i < 4; i++) {
			clk[i].reset();
			clkOutputs[i] = resetClockOutputsHigh ? 10.0f : 0.0f;
			bufferedKnobs[i] = params[RATIO_PARAMS + i].getValue();// must be done before the getRatioDoubled() a few lines down
		}
		for (int i = 0; i < 3; i++) {
			syncRatios[i] = false;
			ratiosDoubled[i] = getRatioDoubled(i);
		}
		extPulseNumber = -1;
		extIntervalTime = 0.0;
		timeoutTime = 2.0 / ppqn + 0.1;// worst case. This is a double period at 30 BPM (4s), divided by the expected number of edges in the double period 
									   //   which is 2*ppqn, plus epsilon. This timeoutTime is only used for timingout the 2nd clock edge
		if (inputs[BPM_INPUT].isConnected()) {
			if (bpmDetectionMode) {
				if (hardReset)
					newMasterLength = 0.5f;// 120 BPM
			}
			else
				newMasterLength = 0.5f / std::pow(2.0f, inputs[BPM_INPUT].getVoltage());// bpm = 120*2^V, T = 60/bpm = 60/(120*2^V) = 0.5/2^V
		}
		else
			newMasterLength = 60.0f / bufferedKnobs[3];
		newMasterLength = clamp(newMasterLength, masterLengthMin, masterLengthMax);
		masterLength = newMasterLength;
	}	
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// bpmDetectionMode
		json_object_set_new(rootJ, "bpmDetectionMode", json_boolean(bpmDetectionMode));
		
		// restartOnStopStartRun
		json_object_set_new(rootJ, "restartOnStopStartRun", json_integer(restartOnStopStartRun));
		
		// sendResetOnRestart
		json_object_set_new(rootJ, "sendResetOnRestart", json_boolean(sendResetOnRestart));
		
		// ppqn
		json_object_set_new(rootJ, "ppqn", json_integer(ppqn));
		
		// resetClockOutputsHigh
		json_object_set_new(rootJ, "resetClockOutputsHigh", json_boolean(resetClockOutputsHigh));
		
		// momentaryRunInput
		json_object_set_new(rootJ, "momentaryRunInput", json_boolean(momentaryRunInput));
		
		// displayIndex
		json_object_set_new(rootJ, "displayIndex", json_integer(displayIndex));
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// bpmDetectionMode
		json_t *bpmDetectionModeJ = json_object_get(rootJ, "bpmDetectionMode");
		if (bpmDetectionModeJ)
			bpmDetectionMode = json_is_true(bpmDetectionModeJ);

		// restartOnStopStartRun
		json_t *restartOnStopStartRunJ = json_object_get(rootJ, "restartOnStopStartRun");
		if (restartOnStopStartRunJ) 
			restartOnStopStartRun = json_integer_value(restartOnStopStartRunJ);

		// sendResetOnRestart
		json_t *sendResetOnRestartJ = json_object_get(rootJ, "sendResetOnRestart");
		if (sendResetOnRestartJ)
			sendResetOnRestart = json_is_true(sendResetOnRestartJ);

		// ppqn
		json_t *ppqnJ = json_object_get(rootJ, "ppqn");
		if (ppqnJ)
			ppqn = json_integer_value(ppqnJ);

		// resetClockOutputsHigh
		json_t *resetClockOutputsHighJ = json_object_get(rootJ, "resetClockOutputsHigh");
		if (resetClockOutputsHighJ)
			resetClockOutputsHigh = json_is_true(resetClockOutputsHighJ);

		// momentaryRunInput
		json_t *momentaryRunInputJ = json_object_get(rootJ, "momentaryRunInput");
		if (momentaryRunInputJ)
			momentaryRunInput = json_is_true(momentaryRunInputJ);

		// displayIndex
		json_t *displayIndexJ = json_object_get(rootJ, "displayIndex");
		if (displayIndexJ)
			displayIndex = json_integer_value(displayIndexJ);

		resetNonJson(true);
	}

	void toggleRun(void) {
		if (!(bpmDetectionMode && inputs[BPM_INPUT].isConnected()) || running) {// toggle when not BPM detect, turn off only when BPM detect (allows turn off faster than timeout if don't want any trailing beats after stoppage). If allow manually start in bpmDetectionMode   the clock will not know which pulse is the 1st of a ppqn set, so only allow stop
			running = !running;
			runPulse.trigger(0.001f);
			if (!running && restartOnStopStartRun == 1) {
				resetClkd(false);
				if (sendResetOnRestart) {
					resetPulse.trigger(0.001f);
					resetLight = 1.0f;
				}
			}
			if (running && restartOnStopStartRun == 2) {
				resetClkd(false);
				if (sendResetOnRestart) {
					resetPulse.trigger(0.001f);
					resetLight = 1.0f;
				}
			}
		}
		else {
			cantRunWarning = (long) (0.7 * sampleRate / RefreshCounter::displayRefreshStepSkips);
		}
	}

	
	void onSampleRateChange() override {
		resetClkd(false);
	}		
	

	void process(const ProcessArgs &args) override {
		// Scheduled reset
		if (scheduledReset) {
			resetClkd(false);		
			scheduledReset = false;
		}
		
		// Run button
		if (runButtonTrigger.process(params[RUN_PARAM].getValue())) {
			toggleRun();
		}
		// Run input
		if (inputs[RUN_INPUT].isConnected()) {
			int state = runInputTrigger.process(inputs[RUN_INPUT].getVoltage());
			if (state != 0) {
				if (momentaryRunInput) {
					if (state == 1) {
						toggleRun();
					}
				}
				else {
					if ( (running && state == -1) || (!running && state == 1) ) {
						toggleRun();
					}
				}
			}
		}


		// Reset (has to be near top because it sets steps to 0, and 0 not a real step (clock section will move to 1 before reaching outputs)
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue())) {
			resetLight = 1.0f;
			resetPulse.trigger(0.001f);
			resetClkd(false);	
		}	

		if (refresh.processInputs()) {
			// Detect bpm and ratio knob changes and update bufferedKnobs
			for (int i = 0; i < 4; i++) {
				if (bufferedKnobs[i] != params[RATIO_PARAMS + i].getValue()) {
					bufferedKnobs[i] = params[RATIO_PARAMS + i].getValue();
					if (i < 3) {
						syncRatios[i] = true;
					}
					displayIndex = (i + 1) % 4;
				}
			}
			
			// Display mode
			if (displayUpTrigger.process(params[DISPLAY_UP_PARAM].getValue())) {
				if (displayIndex < 3) displayIndex++;
				else displayIndex = 0;
			}
			if (displayDownTrigger.process(params[DISPLAY_DOWN_PARAM].getValue())) {
				if (displayIndex > 0) displayIndex--;
				else displayIndex = 3;
			}
			
			// BPM mode
			bool trigUp = bpmModeUpTrigger.process(params[BPMMODE_UP_PARAM].getValue());
			bool trigDown = bpmModeDownTrigger.process(params[BPMMODE_DOWN_PARAM].getValue());
			if (trigUp || trigDown) {
				if (editingBpmMode != 0ul) {// force active before allow change
					if (bpmDetectionMode == false) {
						bpmDetectionMode = true;
						ppqn = (trigUp ? 2 : 24);
					}
					else {
						if (ppqn == 2) {
							if (trigUp) ppqn = 4;
							else bpmDetectionMode = false;
						}
						else if (ppqn == 4)
							ppqn = (trigUp ? 8 : 2);
						else if (ppqn == 8)
							ppqn = (trigUp ? 12 : 4);
						else if (ppqn == 12)
							ppqn = (trigUp ? 16 : 8);
						else if (ppqn == 16)
							ppqn = (trigUp ? 24 : 12);
						else {// 24
							if (trigUp) bpmDetectionMode = false;
							else ppqn = 16;
						}
					}
				}
				editingBpmMode = (long) (3.0 * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
		}// userInputs refresh
	
		// BPM input and knob
		newMasterLength = masterLength;
		if (inputs[BPM_INPUT].isConnected()) { 
			bool trigBpmInValue = bpmDetectTrigger.process(inputs[BPM_INPUT].getVoltage());
			
			// BPM Detection method
			if (bpmDetectionMode) {
				// rising edge detect
				if (trigBpmInValue) {
					if (!running) {
						// this must be the only way to start runnning when in bpmDetectionMode or else
						//   when manually starting, the clock will not know which pulse is the 1st of a ppqn set
						running = true;
						runPulse.trigger(0.001f);
						resetClkd(false);
						if (restartOnStopStartRun == 2) {
							// resetClkd(false); implicit above
							if (sendResetOnRestart) {
								resetPulse.trigger(0.001f);
								resetLight = 1.0f;
							}
						}
					}
					if (running) {
						extPulseNumber++;
						if (extPulseNumber >= ppqn)
							extPulseNumber = 0;
						if (extPulseNumber == 0)// if first pulse, start interval timer
							extIntervalTime = 0.0;
						else {
							// all other ppqn pulses except the first one. now we have an interval upon which to plan a strecth 
							double timeLeft = extIntervalTime * (double)(ppqn - extPulseNumber) / ((double)extPulseNumber);
							newMasterLength = clamp(clk[0].getStep() + timeLeft, masterLengthMin / 1.5f, masterLengthMax * 1.5f);// extended range for better sync ability (20-450 BPM)
							timeoutTime = extIntervalTime * ((double)(1 + extPulseNumber) / ((double)extPulseNumber)) + 0.1; // when a second or higher clock edge is received, 
							//  the timeout is the predicted next edge (whici is extIntervalTime + extIntervalTime / extPulseNumber) plus epsilon
						}
					}
				}
				if (running) {
					extIntervalTime += sampleTime;
					if (extIntervalTime > timeoutTime) {
						running = false;
						runPulse.trigger(0.001f);
						if (restartOnStopStartRun == 1) {
							resetClkd(false);
							if (sendResetOnRestart) {
								resetPulse.trigger(0.001f);
								resetLight = 1.0f;
							}
						}
					}
				}
			}
			// BPM CV method
			else {// bpmDetectionMode not active
				newMasterLength = clamp(0.5f / std::pow(2.0f, inputs[BPM_INPUT].getVoltage()), masterLengthMin, masterLengthMax);// bpm = 120*2^V, T = 60/bpm = 60/(120*2^V) = 0.5/2^V
				// no need to round since this clocked's master's BPM knob is a snap knob thus already rounded, and with passthru approach, no cumul error
			}
		}
		else {// BPM_INPUT not active
			newMasterLength = clamp(60.0f / bufferedKnobs[3], masterLengthMin, masterLengthMax);
		}
		if (newMasterLength != masterLength) {
			double lengthStretchFactor = ((double)newMasterLength) / ((double)masterLength);
			for (int i = 0; i < 4; i++) {
				clk[i].applyNewLength(lengthStretchFactor);
			}
			masterLength = newMasterLength;
		}
		
		
		// main clock engine
		if (running) {
			// See if clocks finished their prescribed number of iteratios of double periods (and syncWait for sub) or 
			//    if they were forced reset and if so, recalc and restart them
			
			// Master clock
			if (clk[0].isReset()) {
				// See if ratio knobs changed (or unitinialized)
				for (int i = 0; i < 3; i++) {
					if (syncRatios[i]) {// unused (undetermined state) for master
						clk[i + 1].reset();// force reset (thus refresh) of that sub-clock
						ratiosDoubled[i] = getRatioDoubled(i);
						syncRatios[i] = false;
					}
				}
				clk[0].setup(masterLength, 1, sampleTime);// must call setup before start. length = double_period
				clk[0].start();
			}
			clkOutputs[0] = clk[0].isHigh() ? 10.0f : 0.0f;		
			
			// Sub clocks
			for (int i = 1; i < 4; i++) {
				if (clk[i].isReset()) {
					double length;
					int iterations;
					int ratioDoubled = ratiosDoubled[i - 1];
					if (ratioDoubled < 0) { // if div 
						ratioDoubled *= -1;
						length = masterLength * ((double)ratioDoubled) / 2.0;
						iterations = 1l + (ratioDoubled % 2);		
					}
					else {// mult 
						length = (2.0f * masterLength) / ((double)ratioDoubled);
						iterations = ratioDoubled / (2l - (ratioDoubled % 2l));							
					}
					clk[i].setup(length, iterations, sampleTime);
					clk[i].start();
				}
				clkOutputs[i] = clk[i].isHigh() ? 10.0f : 0.0f;
			}

			// Step clocks
			for (int i = 0; i < 4; i++)
				clk[i].stepClock();
		}
		
		// outputs
		for (int i = 0; i < 4; i++) {
			outputs[CLK_OUTPUTS + i].setVoltage(clkOutputs[i]);
		}
		outputs[RESET_OUTPUT].setVoltage((resetPulse.process((float)sampleTime) ? 10.0f : 0.0f));
		outputs[RUN_OUTPUT].setVoltage((runPulse.process((float)sampleTime) ? 10.0f : 0.0f));
		outputs[BPM_OUTPUT].setVoltage( inputs[BPM_INPUT].isConnected() ? inputs[BPM_INPUT].getVoltage() : log2f(0.5f / masterLength));
			
		
		// lights
		if (refresh.processLights()) {
			// Reset light
			lights[RESET_LIGHT].setSmoothBrightness(resetLight, (float)sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));	
			resetLight = 0.0f;
			
			// Run light
			lights[RUN_LIGHT].setBrightness(running ? 1.0f : 0.0f);
			
			// BPM light
			bool warningFlashState = true;
			if (cantRunWarning > 0l) 
				warningFlashState = calcWarningFlash(cantRunWarning, (long) (0.7 * sampleRate / RefreshCounter::displayRefreshStepSkips));
			lights[BPMSYNC_LIGHT + 0].setBrightness((bpmDetectionMode && warningFlashState) ? 1.0f : 0.0f);
			lights[BPMSYNC_LIGHT + 1].setBrightness((bpmDetectionMode && warningFlashState) ? (float)((ppqn - 2)*(ppqn - 2))/440.0f : 0.0f);			
			
			// ratios synched lights
			for (int i = 0; i < 4; i++) {
				if (i > 0 && running && syncRatios[i - 1]) {// red
					lights[CLK_LIGHTS + i * 2 + 0].setBrightness(0.0f);
					lights[CLK_LIGHTS + i * 2 + 1].setBrightness(1.0f);
				}
				else if (i == displayIndex) {// green
					lights[CLK_LIGHTS + i * 2 + 0].setBrightness(1.0f);
					lights[CLK_LIGHTS + i * 2 + 1].setBrightness(0.0f);
				}
				else {// off
					lights[CLK_LIGHTS + i * 2 + 0].setBrightness(0.0f);
					lights[CLK_LIGHTS + i * 2 + 1].setBrightness(0.0f);
				}
			}

			if (cantRunWarning > 0l)
				cantRunWarning--;
			
			editingBpmMode--;
			if (editingBpmMode < 0l)
				editingBpmMode = 0l;
			
		}// lightRefreshCounter
	}// process()
};


struct ClkdWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct BpmRatioDisplayWidget : TransparentWidget {
		Clkd *module;
		std::shared_ptr<Font> font;
		char displayStr[4];

		
		BpmRatioDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			//nvgTextLetterSpacing(args.vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(args.vg, textColor);
			if (module == NULL) {
				snprintf(displayStr, 4, "120");
			}
			else if (module->editingBpmMode != 0l) {// BPM mode to display
				if (!module->bpmDetectionMode)
					snprintf(displayStr, 4, " CV");
				else
					snprintf(displayStr, 4, "P%2u", (unsigned) module->ppqn);
			}
			else if (module->displayIndex > 0) {// Ratio to display
				bool isDivision = false;
				int ratioDoubled = module->getRatioDoubled(module->displayIndex - 1);
				if (ratioDoubled < 0) {
					ratioDoubled = -1 * ratioDoubled;
					isDivision = true;
				}
				if ( (ratioDoubled % 2) == 1 )
					snprintf(displayStr, 4, "%c,5", 0x30 + (char)(ratioDoubled / 2));
				else {
					snprintf(displayStr, 4, "X%2u", (unsigned)(ratioDoubled / 2));
					if (isDivision)
						displayStr[0] = '/';
				}
			}
			else {// BPM to display
				snprintf(displayStr, 4, "%3u", (unsigned)((60.0f / module->masterLength) + 0.5f));
			}
			displayStr[3] = 0;// more safety
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	struct PanelThemeItem : MenuItem {
		Clkd *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	struct MomentaryRunInputItem : MenuItem {
		Clkd *module;
		void onAction(const event::Action &e) override {
			module->momentaryRunInput = !module->momentaryRunInput;
		}
	};
	struct RestartOnStopStartItem : MenuItem {
		Clkd *module;
		
		struct RestartOnStopStartSubItem : MenuItem {
			Clkd *module;
			int setVal = 0;
			void onAction(const event::Action &e) override {
				module->restartOnStopStartRun = setVal;
			}
		};
	
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			RestartOnStopStartSubItem *re0Item = createMenuItem<RestartOnStopStartSubItem>("turned off", CHECKMARK(module->restartOnStopStartRun == 1));
			re0Item->module = module;
			re0Item->setVal = 1;
			menu->addChild(re0Item);

			RestartOnStopStartSubItem *re1Item = createMenuItem<RestartOnStopStartSubItem>("turned on", CHECKMARK(module->restartOnStopStartRun == 2));
			re1Item->module = module;
			re1Item->setVal = 2;
			menu->addChild(re1Item);

			RestartOnStopStartSubItem *re2Item = createMenuItem<RestartOnStopStartSubItem>("neither", CHECKMARK(module->restartOnStopStartRun == 0));
			re2Item->module = module;
			menu->addChild(re2Item);

			return menu;
		}
	};	
	struct SendResetOnRestartItem : MenuItem {
		Clkd *module;
		void onAction(const event::Action &e) override {
			module->sendResetOnRestart = !module->sendResetOnRestart;
		}
	};	
	struct ResetHighItem : MenuItem {
		Clkd *module;
		void onAction(const event::Action &e) override {
			module->resetClockOutputsHigh = !module->resetClockOutputsHigh;
			module->resetClkd(true);
		}
	};	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Clkd *module = dynamic_cast<Clkd*>(this->module);
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
		
		RestartOnStopStartItem *erItem = createMenuItem<RestartOnStopStartItem>("Restart when run is:", RIGHT_ARROW);
		erItem->module = module;
		menu->addChild(erItem);

		SendResetOnRestartItem *sendItem = createMenuItem<SendResetOnRestartItem>("Send reset pulse when restart", module->restartOnStopStartRun != 0 ? CHECKMARK(module->sendResetOnRestart) : "");
		sendItem->module = module;
		sendItem->disabled = module->restartOnStopStartRun == 0;
		menu->addChild(sendItem);

		ResetHighItem *rhItem = createMenuItem<ResetHighItem>("Outputs reset high when not running", CHECKMARK(module->resetClockOutputsHigh));
		rhItem->module = module;
		menu->addChild(rhItem);
		
		MomentaryRunInputItem *runInItem = createMenuItem<MomentaryRunInputItem>("Run CV input is level sensitive", CHECKMARK(!module->momentaryRunInput));
		runInItem->module = module;
		menu->addChild(runInItem);
	}	
	

	ClkdWidget(Clkd *module) {
		setModule(module);
		
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/Clkd.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/Clkd_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));


		static const int colL = 30;
		static const int colC = 75;
		static const int colR = 120;
		
		static const int row0 = 58;// reset, run, bpm inputs
		static const int row1 = 94;// reset and run switches, bpm knob
		static const int row2 = 144;// bpm display, display index lights, master clk out
		static const int row3 = 194;// display and mode buttons
		static const int row4 = 227;// sub clock ratio knobs
		static const int row5 = 281;// sub clock outs
		static const int row6 = 328;// reset, run, bpm outputs


		// Row 0
		// Reset input
		addInput(createDynamicPortCentered<IMPort>(Vec(colL, row0), true, module, Clkd::RESET_INPUT, module ? &module->panelTheme : NULL));
		// Run input
		addInput(createDynamicPortCentered<IMPort>(Vec(colC, row0), true, module, Clkd::RUN_INPUT, module ? &module->panelTheme : NULL));
		// Bpm input
		addInput(createDynamicPortCentered<IMPort>(Vec(colR, row0), true, module, Clkd::BPM_INPUT, module ? &module->panelTheme : NULL));
		

		// Row 1
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colL, row1), module, Clkd::RESET_PARAM));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colL, row1), module, Clkd::RESET_LIGHT));
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colC, row1), module, Clkd::RUN_PARAM));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colC, row1), module, Clkd::RUN_LIGHT));
		// Master BPM knob
		addParam(createDynamicParamCentered<IMSmallKnob<true, true>>(Vec(colR, row1), module, Clkd::BPM_PARAM, module ? &module->panelTheme : NULL));// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		

		// Row 2
		// BPM display
		BpmRatioDisplayWidget *bpmRatioDisplay = new BpmRatioDisplayWidget();
		bpmRatioDisplay->box.size = Vec(55, 30);// 3 characters
		bpmRatioDisplay->box.pos = Vec((colL + colC) / 2 - bpmRatioDisplay->box.size.x / 2 - 8, row2 - bpmRatioDisplay->box.size.y / 2);
		bpmRatioDisplay->module = module;
		addChild(bpmRatioDisplay);
		// Display index lights
		static const int delY = 10;
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colC + 11, row2  -2 * delY - 4 ), module, Clkd::CLK_LIGHTS + 0 * 2));		
		for (int y = 1; y < 4; y++) {
			addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colC + 11, row2 + delY * (y - 2) ), module, Clkd::CLK_LIGHTS + y * 2));		
		}
		// Clock master out
		addOutput(createDynamicPortCentered<IMPort>(Vec(colR, row2), false, module, Clkd::CLK_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		

		// Row 3
		static const int bspaceX = 24;
		// Display mode buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colL - 4, row3), module, Clkd::DISPLAY_DOWN_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colL + bspaceX - 4, row3), module, Clkd::DISPLAY_UP_PARAM, module ? &module->panelTheme : NULL));
		// BPM mode light
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colC, row3), module, Clkd::BPMSYNC_LIGHT));		
		// BPM mode buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colR - bspaceX + 4, row3), module, Clkd::BPMMODE_DOWN_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colR + 4, row3), module, Clkd::BPMMODE_UP_PARAM, module ? &module->panelTheme : NULL));

		
		// Row 4 		
		// Ratio knobs
		addParam(createDynamicParamCentered<IMSmallKnob<true, true>>(Vec(colL, row4), module, Clkd::RATIO_PARAMS + 0, module ? &module->panelTheme : NULL));		
		addParam(createDynamicParamCentered<IMSmallKnob<true, true>>(Vec(colC, row4), module, Clkd::RATIO_PARAMS + 1, module ? &module->panelTheme : NULL));		
		addParam(createDynamicParamCentered<IMSmallKnob<true, true>>(Vec(colR, row4), module, Clkd::RATIO_PARAMS + 2, module ? &module->panelTheme : NULL));		


		// Row 5
		// Sub-clock outputs
		addOutput(createDynamicPortCentered<IMPort>(Vec(colL, row5), false, module, Clkd::CLK_OUTPUTS + 1, module ? &module->panelTheme : NULL));	
		addOutput(createDynamicPortCentered<IMPort>(Vec(colC, row5), false, module, Clkd::CLK_OUTPUTS + 2, module ? &module->panelTheme : NULL));	
		addOutput(createDynamicPortCentered<IMPort>(Vec(colR, row5), false, module, Clkd::CLK_OUTPUTS + 3, module ? &module->panelTheme : NULL));	


		// Row 6 (last row)
		// Reset out
		addOutput(createDynamicPortCentered<IMPort>(Vec(colL, row6), false, module, Clkd::RESET_OUTPUT, module ? &module->panelTheme : NULL));
		// Run out
		addOutput(createDynamicPortCentered<IMPort>(Vec(colC, row6), false, module, Clkd::RUN_OUTPUT, module ? &module->panelTheme : NULL));
		// Out out
		addOutput(createDynamicPortCentered<IMPort>(Vec(colR, row6), false, module, Clkd::BPM_OUTPUT, module ? &module->panelTheme : NULL));

	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Clkd*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Clkd*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
	
	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			if ( e.key == GLFW_KEY_SPACE && ((e.mods & RACK_MOD_MASK) == 0) ) {
				Clkd *module = dynamic_cast<Clkd*>(this->module);
				module->toggleRun();
				e.consume(this);
				return;
			}
		}
		ModuleWidget::onHoverKey(e); 
	}
};

Model *modelClkd = createModel<Clkd, ClkdWidget>("Clocked-Clkd");
