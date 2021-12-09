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


#include "ClockedCommon.hpp"


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
	bool *trigOut;
	
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
	void construct(Clock* clkGiven, bool *resetClockOutputsHighPtr, bool *trigOutPtr) {
		syncSrc = clkGiven;
		resetClockOutputsHigh = resetClockOutputsHighPtr;
		trigOut = trigOutPtr;
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
			if (*trigOut)
				return (step <= 0.001f) ? 1 : 0;
			return (step < (length * 0.5)) ? 1 : 0;
		}
		return (*resetClockOutputsHigh) ? 1 : 0;
	}	
};


//*****************************************************************************


struct Clkd : Module {
	
	struct BpmParam : ParamQuantity {
		std::string getDisplayValueString() override {
			return module->inputs[BPM_INPUT].isConnected() ? "Ext." : ParamQuantity::getDisplayValueString();
		}
	};

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
		RESET_OUTPUT,// must be at index 4 because autopatch has this hardcoded
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
	static constexpr float masterLengthMax = 60.0f / bpmMin;// a length is a period
	static constexpr float masterLengthMin = 60.0f / bpmMax;// a length is a period
	
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool running;
	bool bpmDetectionMode;
	unsigned int resetOnStartStop;// see bit masks ON_STOP_INT_RST_MSK, ON_START_EXT_RST_MSK, etc
	int ppqn;
	bool resetClockOutputsHigh;
	bool momentaryRunInput;// true = trigger (original rising edge only version), false = level sensitive (emulated with rising and falling detection)
	bool forceCvOnBpmOut;
	int displayIndex;
	bool trigOuts[4];// output triggers when true, one for each clock output, master is index 0. 

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
		if (i >= numRatios) {
			i = numRatios - 1;
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
			snprintf(strBuf, 32, "Clk %i ratio", i + 1);
			configParam<RatioParam>(RATIO_PARAMS + i, ((float)numRatios - 1.0f)*-1.0f, float(numRatios) - 1.0f, 0.0f, strBuf);
			paramQuantities[RATIO_PARAMS + i]->snapEnabled = true;
		}
		configParam<BpmParam>(BPM_PARAM, (float)(bpmMin), (float)(bpmMax), 120.0f, "Master clock", " BPM");// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		paramQuantities[BPM_PARAM]->snapEnabled = true;
		
		configInput(RESET_INPUT, "Reset");
		configInput(RUN_INPUT, "Run");
		configInput(BPM_INPUT, "BPM CV / Ext clock");

		configOutput(CLK_OUTPUTS + 0, "Master clock");
		for (int i = 1; i < 4; i++) {
			configOutput(CLK_OUTPUTS + i, string::f("Clock %i", i));
		}
		configOutput(RESET_OUTPUT, "Reset");
		configOutput(RUN_OUTPUT, "Run");
		configOutput(BPM_OUTPUT, "BPM CV / Ext clock thru");
		
		configBypass(RESET_INPUT, RESET_OUTPUT);
		configBypass(RUN_INPUT, RUN_OUTPUT);
		configBypass(BPM_INPUT, BPM_OUTPUT);

		clk[0].construct(nullptr, &resetClockOutputsHigh, &trigOuts[0]);
		for (int i = 1; i < 4; i++) {
			clk[i].construct(&clk[0], &resetClockOutputsHigh, &trigOuts[i]);		
		}
		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}
	

	void onReset() override {
		running = true;
		bpmDetectionMode = false;
		resetOnStartStop = 0;
		ppqn = 4;
		resetClockOutputsHigh = true;
		momentaryRunInput = true;
		forceCvOnBpmOut = false;
		displayIndex = 0;// show BPM (knob 0) by default
		for (int i = 0; i < 4; i++) {
			trigOuts[i] = false;
		}
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

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// bpmDetectionMode
		json_object_set_new(rootJ, "bpmDetectionMode", json_boolean(bpmDetectionMode));
		
		// resetOnStartStop
		json_object_set_new(rootJ, "resetOnStartStop", json_integer(resetOnStartStop));
		
		// ppqn
		json_object_set_new(rootJ, "ppqn", json_integer(ppqn));
		
		// resetClockOutputsHigh
		json_object_set_new(rootJ, "resetClockOutputsHigh", json_boolean(resetClockOutputsHigh));
		
		// momentaryRunInput
		json_object_set_new(rootJ, "momentaryRunInput", json_boolean(momentaryRunInput));
		
		// forceCvOnBpmOut
		json_object_set_new(rootJ, "forceCvOnBpmOut", json_boolean(forceCvOnBpmOut));
		
		// displayIndex
		json_object_set_new(rootJ, "displayIndex", json_integer(displayIndex));
		
		// trigOuts
		json_t *trigOutsJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(trigOutsJ, i, json_boolean(trigOuts[i]));
		json_object_set_new(rootJ, "trigOuts", trigOutsJ);
		
		// clockMaster (stores a valid (non-negative) id when we are the master, -1 when we are not)
		json_object_set_new(rootJ, "clockMaster", json_integer(clockMaster.id == id ? id : -1));
		
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

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// bpmDetectionMode
		json_t *bpmDetectionModeJ = json_object_get(rootJ, "bpmDetectionMode");
		if (bpmDetectionModeJ)
			bpmDetectionMode = json_is_true(bpmDetectionModeJ);

		// resetOnStartStop
		json_t *resetOnStartStopJ = json_object_get(rootJ, "resetOnStartStop");
		if (resetOnStartStopJ) {
			resetOnStartStop = json_integer_value(resetOnStartStopJ);
		}
		else {// legacy
			int restartOnStopStartRun = 0;// 0 = nothing, 1 = restart on stop run, 2 = restart on start run
			bool sendResetOnRestart = false;
			
			// restartOnStopStartRun
			json_t *restartOnStopStartRunJ = json_object_get(rootJ, "restartOnStopStartRun");
			if (restartOnStopStartRunJ) 
				restartOnStopStartRun = json_integer_value(restartOnStopStartRunJ);

			// sendResetOnRestart
			json_t *sendResetOnRestartJ = json_object_get(rootJ, "sendResetOnRestart");
			if (sendResetOnRestartJ)
				sendResetOnRestart = json_is_true(sendResetOnRestartJ);
				
			resetOnStartStop = 0;
			if (restartOnStopStartRun == 1) 
				resetOnStartStop |= ON_STOP_INT_RST_MSK;
			else if (restartOnStopStartRun == 2) 
				resetOnStartStop |= ON_START_INT_RST_MSK;
			if (sendResetOnRestart)
				resetOnStartStop |= (ON_START_EXT_RST_MSK | ON_STOP_EXT_RST_MSK);
		}

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

		// forceCvOnBpmOut
		json_t *forceCvOnBpmOutJ = json_object_get(rootJ, "forceCvOnBpmOut");
		if (forceCvOnBpmOutJ)
			forceCvOnBpmOut = json_is_true(forceCvOnBpmOutJ);

		// displayIndex
		json_t *displayIndexJ = json_object_get(rootJ, "displayIndex");
		if (displayIndexJ)
			displayIndex = json_integer_value(displayIndexJ);
		
		// trigOuts
		json_t *trigOutsJ = json_object_get(rootJ, "trigOuts");
		if (trigOutsJ)
			for (int i = 0; i < 4; i++)
			{
				json_t *trigOutsArrayJ = json_array_get(trigOutsJ, i);
				if (trigOutsArrayJ)
					trigOuts[i] = json_is_true(trigOutsArrayJ);
			}

		resetNonJson(true);

		// clockMaster
		json_t *clockMasterJ = json_object_get(rootJ, "clockMaster");
		if (clockMasterJ) {
			int clkId = json_integer_value(clockMasterJ);
			if (clkId == id) { // set this as the master only when reload the id of the master and that previous master has same id as this id
				clockMaster.setAsMaster(id, resetClockOutputsHigh);
			}
		}
	}
	

	void toggleRun(void) {
		if (!(bpmDetectionMode && inputs[BPM_INPUT].isConnected()) || running) {// toggle when not BPM detect, turn off only when BPM detect (allows turn off faster than timeout if don't want any trailing beats after stoppage). If allow manually start in bpmDetectionMode   the clock will not know which pulse is the 1st of a ppqn set, so only allow stop
			running = !running;
			runPulse.trigger(0.001f);
			if (running) {
				if (resetOnStartStop & ON_START_INT_RST_MSK) {
					resetClkd(false);
				}
				if (resetOnStartStop & ON_START_EXT_RST_MSK) {
					resetPulse.trigger(0.001f);
					resetLight = 1.0f;
				}
			}
			else {
				if (resetOnStartStop & ON_STOP_INT_RST_MSK) {
					resetClkd(false);
				}
				if (resetOnStartStop & ON_STOP_EXT_RST_MSK) {
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
						if (resetOnStartStop & ON_START_INT_RST_MSK) {
							// resetClkd(false); implicit above
						}
						if (resetOnStartStop & ON_START_EXT_RST_MSK) {
							resetPulse.trigger(0.001f);
							resetLight = 1.0f;
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
						if (resetOnStartStop & ON_STOP_INT_RST_MSK) {
							resetClkd(false);
						}
						if (resetOnStartStop & ON_STOP_EXT_RST_MSK) {
							resetPulse.trigger(0.001f);
							resetLight = 1.0f;
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
		outputs[BPM_OUTPUT].setVoltage( (inputs[BPM_INPUT].isConnected() && !forceCvOnBpmOut) ? inputs[BPM_INPUT].getVoltage() : log2f(0.5f / masterLength));
			
		
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
	PortWidget* slaveResetRunBpmInputs[3];

	struct BpmRatioDisplayWidget : TransparentWidget {
		Clkd *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char displayStr[16];

		
		BpmRatioDisplayWidget() {
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, 18);
				nvgFontFaceId(args.vg, font->handle);
				//nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = VecPx(6, 24);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
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
						snprintf(displayStr, 16, "X%2u", (unsigned)(ratioDoubled / 2));
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
		}
	};		
	
	void appendContextMenu(Menu *menu) override {
		Clkd *module = dynamic_cast<Clkd*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createSubmenuItem("On Start", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("Do internal reset", "",
				[=]() {return module->resetOnStartStop & ON_START_INT_RST_MSK;},
				[=]() {module->resetOnStartStop ^= ON_START_INT_RST_MSK;}
			));
			menu->addChild(createCheckMenuItem("Send reset pulse", "",
				[=]() {return module->resetOnStartStop & ON_START_EXT_RST_MSK;},
				[=]() {module->resetOnStartStop ^= ON_START_EXT_RST_MSK;}
			));
		}));	
		
		menu->addChild(createSubmenuItem("On Stop", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("Do internal reset", "",
				[=]() {return module->resetOnStartStop & ON_STOP_INT_RST_MSK;},
				[=]() {module->resetOnStartStop ^= ON_STOP_INT_RST_MSK;}
			));
			menu->addChild(createCheckMenuItem("Send reset pulse", "",
				[=]() {return module->resetOnStartStop & ON_STOP_EXT_RST_MSK;},
				[=]() {module->resetOnStartStop ^= ON_STOP_EXT_RST_MSK;}
			));
		}));	

		menu->addChild(createCheckMenuItem("Outputs high on reset when not running", "",
			[=]() {return module->resetClockOutputsHigh;},
			[=]() {module->resetClockOutputsHigh = !module->resetClockOutputsHigh;
				   module->resetClkd(true);}
		));
		
		menu->addChild(createBoolMenuItem("Run CV input is level sensitive", "",
			[=]() {return !module->momentaryRunInput;},
			[=](bool loop) {module->momentaryRunInput = !module->momentaryRunInput;}
		));

		menu->addChild(createBoolPtrMenuItem("BPM out is CV when ext sync", "", &module->forceCvOnBpmOut));

		menu->addChild(createSubmenuItem("Send triggers (instead of gates)", "", [=](Menu* menu) {
			menu->addChild(createBoolPtrMenuItem("Master clk", "", &(module->trigOuts[0])));
			menu->addChild(createBoolPtrMenuItem("Clock 1", "", &(module->trigOuts[1])));
			menu->addChild(createBoolPtrMenuItem("Clock 2", "", &(module->trigOuts[2])));
			menu->addChild(createBoolPtrMenuItem("Clock 3", "", &(module->trigOuts[3])));
		}));		

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Actions"));
		
		AutopatchItem *apItem = createMenuItem<AutopatchItem>("Auto-patch", RIGHT_ARROW);
		apItem->idPtr = &module->id;
		apItem->resetClockOutputsHighPtr = &module->resetClockOutputsHigh;
		apItem->slaveResetRunBpmInputs = slaveResetRunBpmInputs;
		menu->addChild(apItem);
	}
	
	struct BpmKnob : IMMediumKnob {
		int *displayIndexPtr = NULL;
		void onButton(const event::Button& e) override {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity && displayIndexPtr) {
				*displayIndexPtr = 0;
			}
			IMMediumKnob::onButton(e);
		}
	};
	struct RatioKnob : IMSmallKnob {
		int *displayIndexPtr = NULL;
		void onButton(const event::Button& e) override {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity && displayIndexPtr) {
				int ratioNum = paramQuantity->paramId - Clkd::RATIO_PARAMS;
				if (ratioNum >= 0 && ratioNum < 3) {
					*displayIndexPtr = 1 + ratioNum;
				}
			}
			IMSmallKnob::onButton(e);
		}
	};

	ClkdWidget(Clkd *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Clkd.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));


		static const int colL = 30;
		static const int colC = 75;
		static const int colR = 120;
		
		static const int row0 = 58;// reset, run, bpm inputs
		static const int row1 = 95;// reset and run switches, bpm knob
		static const int row2 = 148;// bpm display, display index lights, master clk out
		static const int row3 = 198;// display and mode buttons
		static const int row4 = 227;// sub clock ratio knobs
		static const int row5 = 281;// sub clock outs
		static const int row6 = 328;// reset, run, bpm outputs


		// Row 0
		// Reset input
		addInput(slaveResetRunBpmInputs[0] = createDynamicPortCentered<IMPort>(VecPx(colL, row0), true, module, Clkd::RESET_INPUT, mode));
		// Run input
		addInput(slaveResetRunBpmInputs[1] = createDynamicPortCentered<IMPort>(VecPx(colC, row0), true, module, Clkd::RUN_INPUT, mode));
		// Bpm input
		addInput(slaveResetRunBpmInputs[2] = createDynamicPortCentered<IMPort>(VecPx(colR, row0), true, module, Clkd::BPM_INPUT, mode));
		

		// Row 1
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(colL, row1), module, Clkd::RESET_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(colL, row1), module, Clkd::RESET_LIGHT));
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(colC, row1), module, Clkd::RUN_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(colC, row1), module, Clkd::RUN_LIGHT));
		// Master BPM knob
		BpmKnob *bpmKnob;
		addParam(bpmKnob = createDynamicParamCentered<BpmKnob>(VecPx(colR, row1), module, Clkd::BPM_PARAM, mode));// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		// bpmKnob->displayIndexPtr = (done below with ratio knobs)

		// Row 2
		// Clock master out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colL, row2), false, module, Clkd::CLK_OUTPUTS + 0, mode));
		// Display index lights
		static const int delY = 10;
		addChild(createLightCentered<SmallLight<GreenRedLightIM>>(VecPx(colC - 10.5f, row2  -2 * delY - 4 ), module, Clkd::CLK_LIGHTS + 0 * 2));		
		for (int y = 1; y < 4; y++) {
			addChild(createLightCentered<SmallLight<GreenRedLightIM>>(VecPx(colC - 10.5f, row2 + delY * (y - 2) ), module, Clkd::CLK_LIGHTS + y * 2));		
		}
		// BPM display
		BpmRatioDisplayWidget *bpmRatioDisplay = new BpmRatioDisplayWidget();
		bpmRatioDisplay->box.size = VecPx(55, 30);// 3 characters
		bpmRatioDisplay->box.pos = VecPx((colR + colC) / 2.0f + 9, row2).minus(bpmRatioDisplay->box.size.div(2));
		bpmRatioDisplay->module = module;
		addChild(bpmRatioDisplay);
		svgPanel->fb->addChild(new DisplayBackground(bpmRatioDisplay->box.pos, bpmRatioDisplay->box.size, mode));
		

		// Row 3
		static const int bspaceX = 24;
		// Display mode buttons
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colL - 4, row3), module, Clkd::DISPLAY_DOWN_PARAM, mode));
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colL + bspaceX - 4, row3), module, Clkd::DISPLAY_UP_PARAM, mode));
		// BPM mode light
		addChild(createLightCentered<SmallLight<GreenRedLightIM>>(VecPx(colC, row3), module, Clkd::BPMSYNC_LIGHT));		
		// BPM mode buttons
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colR - bspaceX + 4, row3), module, Clkd::BPMMODE_DOWN_PARAM, mode));
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colR + 4, row3), module, Clkd::BPMMODE_UP_PARAM, mode));

		
		// Row 4 		
		// Ratio knobs
		RatioKnob *ratioKnobs[3];
		addParam(ratioKnobs[0] = createDynamicParamCentered<RatioKnob>(VecPx(colL, row4), module, Clkd::RATIO_PARAMS + 0, mode));	
		addParam(ratioKnobs[1] = createDynamicParamCentered<RatioKnob>(VecPx(colC, row4), module, Clkd::RATIO_PARAMS + 1, mode));		
		addParam(ratioKnobs[2] = createDynamicParamCentered<RatioKnob>(VecPx(colR, row4), module, Clkd::RATIO_PARAMS + 2, mode));	
		if (module) {
			for (int i = 0; i < 3; i++) {
				ratioKnobs[i]->displayIndexPtr = &(module->displayIndex);
			}
			bpmKnob->displayIndexPtr = &(module->displayIndex);
		}


		// Row 5
		// Sub-clock outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colL, row5), false, module, Clkd::CLK_OUTPUTS + 1, mode));	
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colC, row5), false, module, Clkd::CLK_OUTPUTS + 2, mode));	
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colR, row5), false, module, Clkd::CLK_OUTPUTS + 3, mode));	


		// Row 6 (last row)
		// Reset out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colL, row6), false, module, Clkd::RESET_OUTPUT, mode));
		// Run out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colC, row6), false, module, Clkd::RUN_OUTPUT, mode));
		// Out out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colR, row6), false, module, Clkd::BPM_OUTPUT, mode));

	}
	
	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			if ( e.key == GLFW_KEY_SPACE && ((e.mods & RACK_MOD_MASK) == 0) ) {
				Clkd *module = dynamic_cast<Clkd*>(this->module);
				module->toggleRun();
				e.consume(this);
				return;
			}
			else if ( e.key == GLFW_KEY_M && ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) ) {
				Clkd *module = dynamic_cast<Clkd*>(this->module);
				if (clockMaster.id != module->id && clockMaster.validateClockModule()) {
					autopatch(slaveResetRunBpmInputs, &module->resetClockOutputsHigh);
				}
				e.consume(this);
				return;
			}
		}
		ModuleWidget::onHoverKey(e); 
	}
};

Model *modelClkd = createModel<Clkd, ClkdWidget>("Clocked-Clkd");
