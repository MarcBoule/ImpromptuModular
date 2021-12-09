//***********************************************************************************************
//Chain-able clock module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé, Nigel Sixsmith, Xavier Belmont and Steve Baker
//***********************************************************************************************


#include "ClockedCommon.hpp"


class Clock {
	// The -1.0 step is used as a reset state every double-period so that 
	//   lengths can be re-computed; it will stay at -1.0 when a clock is inactive.
	// a clock frame is defined as "length * iterations + syncWait", and
	//   for master, syncWait does not apply and iterations = 1

	
	double step;// -1.0 when stopped, [0 to 2*period[ for clock steps (*2 is because of swing, so we do groups of 2 periods)
	double length;// double period
	double sampleTime;
	int iterations;// run this many double periods before going into sync if sub-clock
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
	
	int isHigh(float swingParam, float pulseWidth) {
		// last 0.5ms (guard time) must be low so that sync mechanism will work properly (i.e. no missed pulses)
		//   this will automatically be the case, since code below disallows any pulses or inter-pulse times less than 1ms
		int high = 0;
		if (step >= 0.0) {
			// all following values are in seconds
			float onems = 0.001f;
			float period = (float)length / 2.0f;
			float swing = (period - 2.0f * onems) * swingParam;// swingParam is [-1 : 1]
			float p2min = onems;
			float p2max = period - onems - std::fabs(swing);
			if (p2max < p2min) {
				p2max = p2min;
			}
			
			//double p1 = 0.0;// implicit, no need 
			double p2 = (double)((p2max - p2min) * pulseWidth + p2min);// pulseWidth is [0 : 1]
			double p3 = (double)(period + swing);
			double p4 = ((double)(period + swing)) + p2;
			
			if (step <= p2)
				high = 1;
			else if ((step >= p3) && (step <= p4))
				high = 2;
		}
		else if (*resetClockOutputsHigh)
			high = 1;
		return high;
	}	
};


//*****************************************************************************


class ClockDelay {
	long stepCounter;
	int lastWriteValue;
	bool readState;
	long stepRise1;
	long stepFall1;
	long stepRise2;
	long stepFall2;
	
	public:
	
	ClockDelay() {
		reset(true);
	}
	
	void setup() {
	}
	
	void reset(bool resetClockOutputsHigh) {
		stepCounter = 0l;
		lastWriteValue = 0;
		readState = resetClockOutputsHigh;
		stepRise1 = 0l;
		stepFall1 = 0l;
		stepRise2 = 0l;
		stepFall2 = 0l;
	}
	
	void write(int value) {
		if (value == 1) {// first pulse is high
			if (lastWriteValue == 0) // if got rise 1
				stepRise1 = stepCounter;
		}
		else if (value == 2) {// second pulse is high
			if (lastWriteValue == 0) // if got rise 2
				stepRise2 = stepCounter;
		}
		else {// value = 0 (pulse is low)
			if (lastWriteValue == 1) // if got fall 1
				stepFall1 = stepCounter;
			else if (lastWriteValue == 2) // if got fall 2
				stepFall2 = stepCounter;
		}
		
		lastWriteValue = value;
	}
	
	bool read(long delaySamples) {
		long delayedStepCounter = stepCounter - delaySamples;
		if (delayedStepCounter == stepRise1 || delayedStepCounter == stepRise2)
			readState = true;
		else if (delayedStepCounter == stepFall1 || delayedStepCounter == stepFall2)
			readState = false;
		stepCounter++;
		if (stepCounter > 1e8) {// keep within long's bounds (could go higher or could allow negative)
			stepCounter -= 1e8;// 192000 samp/s * 2s * 64 * (3/4) = 18.4 Msamp
			stepRise1 -= 1e8;
			stepFall1 -= 1e8;
			stepRise2 -= 1e8;
			stepFall2 -= 1e8;
		}
		return readState;
	}
};


//*****************************************************************************


struct Clocked : Module {
	
	struct BpmParam : ParamQuantity {
		std::string getDisplayValueString() override {
			return module->inputs[BPM_INPUT].isConnected() ? "Ext." : ParamQuantity::getDisplayValueString();
		}
	};
	
	enum ParamIds {
		ENUMS(RATIO_PARAMS, 4),// master is index 0
		ENUMS(SWING_PARAMS, 4),// master is index 0
		ENUMS(PW_PARAMS, 4),// master is index 0
		RESET_PARAM,
		RUN_PARAM,
		ENUMS(DELAY_PARAMS, 4),// index 0 is unused
		BPMMODE_DOWN_PARAM,
		BPMMODE_UP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(PW_INPUTS, 4),// unused
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
		ENUMS(CLK_LIGHTS, 4),// master is index 0 (not used)
		ENUMS(BPMSYNC_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	
	// Expander
	float rightMessages[2][8] = {};// messages from expander
		

	// Constants
	const float delayValues[8] = {0.0f,  0.0625f, 0.125f, 0.25f, 1.0f/3.0f, 0.5f , 2.0f/3.0f, 0.75f};
	static constexpr float masterLengthMax = 120.0f / bpmMin;// a length is a double period
	static constexpr float masterLengthMin = 120.0f / bpmMax;// a length is a double period
	static constexpr float delayInfoTime = 3.0f;// seconds
	static constexpr float swingInfoTime = 2.0f;// seconds
	
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool running;
	bool displayDelayNoteMode;
	bool bpmDetectionMode;
	unsigned int resetOnStartStop;// see bit masks ON_STOP_INT_RST_MSK, ON_START_EXT_RST_MSK, etc
	int ppqn;
	bool resetClockOutputsHigh;
	bool momentaryRunInput;// true = trigger (original rising edge only version), false = level sensitive (emulated with rising and falling detection)
	bool forceCvOnBpmOut;

	// No need to save, with reset
	long editingBpmMode;// 0 when no edit bpmMode, downward step counter timer when edit, negative upward when show can't edit ("--") 
	double sampleRate;
	double sampleTime;
	Clock clk[4];
	ClockDelay delay[3];// only channels 1 to 3 have delay
	float bufferedRatioKnobs[4];// 0 = mast bpm knob, 1..3 is ratio knobs
	bool syncRatios[4];// 0 index unused
	int ratiosDoubled[4];
	int extPulseNumber;// 0 to ppqn * 2 - 1
	double extIntervalTime;
	double timeoutTime;
	float pulseWidth[4];
	float swingAmount[4];
	long delaySamples[4];
	float newMasterLength;
	float masterLength;
	float clkOutputs[4];
	
	// No need to save, no reset
	bool scheduledReset = false;
	int notifyingSource[4] = {-1, -1, -1, -1};
	long notifyInfo[4] = {0l, 0l, 0l, 0l};// downward step counter when swing to be displayed, 0 when normal display
	long cantRunWarning = 0l;// 0 when no warning, positive downward step counter timer when warning
	RefreshCounter refresh;
	float resetLight = 0.0f;
	Trigger resetTrigger;
	Trigger runButtonTrigger;
	TriggerRiseFall runInputTrigger;
	Trigger bpmDetectTrigger;
	Trigger bpmModeUpTrigger;
	Trigger bpmModeDownTrigger;
	dsp::PulseGenerator resetPulse;
	dsp::PulseGenerator runPulse;

	
	int getRatioDoubled(int ratioKnobIndex) {
		// ratioKnobIndex is 0 for master BPM's ratio (1 is implicitly returned), and 1 to 3 for other ratio knobs
		// returns a positive ratio for mult, negative ratio for div (0 never returned)
		if (ratioKnobIndex < 1) 
			return 1;
		bool isDivision = false;
		int i = (int) std::round( bufferedRatioKnobs[ratioKnobIndex] );// [ -(numRatios-1) ; (numRatios-1) ]
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
	
	void updatePulseSwingDelay() {
		bool expanderPresent = (rightExpander.module && rightExpander.module->model == modelClockedExpander);
		float *messagesFromExpander = (float*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
		for (int i = 0; i < 4; i++) {
			// Pulse Width
			pulseWidth[i] = params[PW_PARAMS + i].getValue();
			if (i < 3 && expanderPresent) {
				pulseWidth[i] += (messagesFromExpander[i] / 10.0f);
				pulseWidth[i] = clamp(pulseWidth[i], 0.0f, 1.0f);
			}
			
			// Swing
			swingAmount[i] = params[SWING_PARAMS + i].getValue();
			if (i < 3 && expanderPresent) {
				swingAmount[i] += (messagesFromExpander[i + 4] / 5.0f);
				swingAmount[i] = clamp(swingAmount[i], -1.0f, 1.0f);
			}
		}

		// Delay
		delaySamples[0] = 0ul;
		for (int i = 1; i < 4; i++) {	
			int delayKnobIndex = (int)(params[DELAY_PARAMS + i].getValue() + 0.5f);
			float delayFraction = delayValues[delayKnobIndex];
			float ratioValue = ((float)ratiosDoubled[i]) / 2.0f;
			if (ratioValue < 0)
				ratioValue = 1.0f / (-1.0f * ratioValue);
			delaySamples[i] = (long)(masterLength * delayFraction * sampleRate / (ratioValue * 2.0));
		}				
	}

	
	Clocked() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		configParam<BpmParam>(RATIO_PARAMS + 0, (float)(bpmMin), (float)(bpmMax), 120.0f, "Master clock", " BPM");// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		paramQuantities[RATIO_PARAMS + 0]->snapEnabled = true;
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(BPMMODE_DOWN_PARAM, 0.0f, 1.0f, 0.0f, "Bpm mode prev");
		configParam(BPMMODE_UP_PARAM, 0.0f, 1.0f, 0.0f,  "Bpm mode next");
		configParam(SWING_PARAMS + 0, -1.0f, 1.0f, 0.0f, "Swing clk");
		configParam(PW_PARAMS + 0, 0.0f, 1.0f, 0.5f, "Pulse width clk");			
		char strBuf[32];
		for (int i = 0; i < 3; i++) {// Row 2-4 (sub clocks)
			// Ratio knobs
			snprintf(strBuf, 32, "Clk %i ratio", i + 1);
			configParam<RatioParam>(RATIO_PARAMS + 1 + i, ((float)numRatios - 1.0f)*-1.0f, (float)numRatios - 1.0f, 0.0f, strBuf);		
			paramQuantities[RATIO_PARAMS + 1 + i]->snapEnabled = true;
			// Swing knobs
			snprintf(strBuf, 32, "Swing clk %i", i + 1);
			configParam(SWING_PARAMS + 1 + i, -1.0f, 1.0f, 0.0f, strBuf);
			// PW knobs
			snprintf(strBuf, 32, "Pulse width clk %i", i + 1);
			configParam(PW_PARAMS + 1 + i, 0.0f, 1.0f, 0.5f,strBuf);
			// Delay knobs
			snprintf(strBuf, 32, "Delay clk %i", i + 1);
			configParam(DELAY_PARAMS + 1 + i, 0.0f, 8.0f - 1.0f, 0.0f, strBuf);
			paramQuantities[DELAY_PARAMS + 1 + i]->snapEnabled = true;
		}
		
		for (int i = 0; i < 4; i++) {
			configInput(PW_INPUTS + i, string::f("Unused %i", i + 1));
		}
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

		clk[0].construct(nullptr, &resetClockOutputsHigh);
		for (int i = 1; i < 4; i++) {
			clk[i].construct(&clk[0], &resetClockOutputsHigh);		
		}
		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	void onReset() override {
		running = true;
		displayDelayNoteMode = true;
		bpmDetectionMode = false;
		resetOnStartStop = 0;
		ppqn = 4;
		resetClockOutputsHigh = true;
		momentaryRunInput = true;
		forceCvOnBpmOut = false;
		resetNonJson(false);
	}
	void resetNonJson(bool delayed) {// delay thread sensitive parts (i.e. schedule them so that process() will do them)
		editingBpmMode = 0l;
		if (delayed) {
			scheduledReset = true;// will be a soft reset
		}
		else {
			resetClocked(true);// hard reset
		}			
	}
	
	void onRandomize() override {
		resetClocked(false);
	}

	
	void resetClocked(bool hardReset) {// set hardReset to true to revert learned BPM to 120 in sync mode, or else when false, learned bmp will stay persistent
		sampleRate = (double)(APP->engine->getSampleRate());
		sampleTime = 1.0 / sampleRate;
		for (int i = 0; i < 4; i++) {
			clk[i].reset();
			if (i < 3) 
				delay[i].reset(resetClockOutputsHigh);
			bufferedRatioKnobs[i] = params[RATIO_PARAMS + i].getValue();// must be done before the getRatioDoubled() a few lines down
			syncRatios[i] = false;
			ratiosDoubled[i] = getRatioDoubled(i);
			clkOutputs[i] = resetClockOutputsHigh ? 10.0f : 0.0f;
		}
		updatePulseSwingDelay();
		extPulseNumber = -1;
		extIntervalTime = 0.0;
		timeoutTime = 2.0 / ppqn + 0.1;// worst case. This is a double period at 30 BPM (4s), divided by the expected number of edges in the double period 
									   //   which is 2*ppqn, plus epsilon. This timeoutTime is only used for timingout the 2nd clock edge
		if (inputs[BPM_INPUT].isConnected()) {
			if (bpmDetectionMode) {
				if (hardReset)
					newMasterLength = 1.0f;// 120 BPM
			}
			else
				newMasterLength = 1.0f / std::pow(2.0f, inputs[BPM_INPUT].getVoltage());// bpm = 120*2^V, 2T = 120/bpm = 120/(120*2^V) = 1/2^V
		}
		else
			newMasterLength = 120.0f / bufferedRatioKnobs[0];
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
		
		// displayDelayNoteMode
		json_object_set_new(rootJ, "displayDelayNoteMode", json_boolean(displayDelayNoteMode));
		
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
		
		// clockMaster
		json_object_set_new(rootJ, "clockMaster", json_boolean(clockMaster.id == id));
		
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

		// displayDelayNoteMode
		json_t *displayDelayNoteModeJ = json_object_get(rootJ, "displayDelayNoteMode");
		if (displayDelayNoteModeJ)
			displayDelayNoteMode = json_is_true(displayDelayNoteModeJ);

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
			if (restartOnStopStartRunJ) {
				restartOnStopStartRun = json_integer_value(restartOnStopStartRunJ);
			}
			else {// legacy level 2
				// emitResetOnStopRun
				json_t *emitResetOnStopRunJ = json_object_get(rootJ, "emitResetOnStopRun");
				if (emitResetOnStopRunJ) {
					restartOnStopStartRun = json_is_true(emitResetOnStopRunJ) ? 1 : 0;
				}
			}

			// sendResetOnRestart
			json_t *sendResetOnRestartJ = json_object_get(rootJ, "sendResetOnRestart");
			if (sendResetOnRestartJ)
				sendResetOnRestart = json_is_true(sendResetOnRestartJ);
			else// legacy level 2
				sendResetOnRestart = (restartOnStopStartRun == 1);
				
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

		resetNonJson(true);
		
		// clockMaster
		json_t *clockMasterJ = json_object_get(rootJ, "clockMaster");
		if (clockMasterJ) {
			if (json_is_true(clockMasterJ)) {
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
					resetClocked(false);
				}
				if (resetOnStartStop & ON_START_EXT_RST_MSK) {
					resetPulse.trigger(0.001f);
					resetLight = 1.0f;
				}
			}
			else {
				if (resetOnStartStop & ON_STOP_INT_RST_MSK) {
					resetClocked(false);
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
		resetClocked(false);
	}		
	

	void process(const ProcessArgs &args) override {
		// Scheduled reset
		if (scheduledReset) {
			resetClocked(false);		
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
			resetClocked(false);	
		}	

		if (refresh.processInputs()) {
			updatePulseSwingDelay();
		
			// Detect bpm and ratio knob changes and update bufferedRatioKnobs
			for (int i = 0; i < 4; i++) {				
				if (bufferedRatioKnobs[i] != params[RATIO_PARAMS + i].getValue()) {
					bufferedRatioKnobs[i] = params[RATIO_PARAMS + i].getValue();
					if (i > 0) {
						syncRatios[i] = true;
					}
					notifyInfo[i] = 0l;
				}
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
						resetClocked(false);
						if (resetOnStartStop & ON_START_INT_RST_MSK) {
							// resetClocked(false); implicit above
						}
						if (resetOnStartStop & ON_START_EXT_RST_MSK) {
							resetPulse.trigger(0.001f);
							resetLight = 1.0f;
						}
					}
					if (running) {
						extPulseNumber++;
						if (extPulseNumber >= ppqn * 2)// *2 because working with double_periods
							extPulseNumber = 0;
						if (extPulseNumber == 0)// if first pulse, start interval timer
							extIntervalTime = 0.0;
						else {
							// all other ppqn pulses except the first one. now we have an interval upon which to plan a strecth 
							double timeLeft = extIntervalTime * (double)(ppqn * 2 - extPulseNumber) / ((double)extPulseNumber);
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
							resetClocked(false);
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
				newMasterLength = clamp(1.0f / std::pow(2.0f, inputs[BPM_INPUT].getVoltage()), masterLengthMin, masterLengthMax);// bpm = 120*2^V, 2T = 120/bpm = 120/(120*2^V) = 1/2^V
				// no need to round since this clocked's master's BPM knob is a snap knob thus already rounded, and with passthru approach, no cumul error
			}
		}
		else {// BPM_INPUT not active
			newMasterLength = clamp(120.0f / bufferedRatioKnobs[0], masterLengthMin, masterLengthMax);
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
				for (int i = 1; i < 4; i++) {
					if (syncRatios[i]) {// unused (undetermined state) for master
						clk[i].reset();// force reset (thus refresh) of that sub-clock
						ratiosDoubled[i] = getRatioDoubled(i);
						syncRatios[i] = false;
					}
				}
				clk[0].setup(masterLength, 1, sampleTime);// must call setup before start. length = double_period
				clk[0].start();
			}
			clkOutputs[0] = clk[0].isHigh(swingAmount[0], pulseWidth[0]) ? 10.0f : 0.0f;		
			
			// Sub clocks
			for (int i = 1; i < 4; i++) {
				if (clk[i].isReset()) {
					double length;
					int iterations;
					int ratioDoubled = ratiosDoubled[i];
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
				delay[i - 1].write(clk[i].isHigh(swingAmount[i], pulseWidth[i]));
				clkOutputs[i] = delay[i - 1].read(delaySamples[i]) ? 10.0f : 0.0f;
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
		outputs[BPM_OUTPUT].setVoltage( (inputs[BPM_INPUT].isConnected() && !forceCvOnBpmOut) ? inputs[BPM_INPUT].getVoltage() : log2f(1.0f / masterLength));
			
		
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
			for (int i = 1; i < 4; i++)
				lights[CLK_LIGHTS + i].setBrightness((syncRatios[i] && running) ? 1.0f: 0.0f);

			// info notification counters
			for (int i = 0; i < 4; i++) {
				notifyInfo[i]--;
				if (notifyInfo[i] < 0l)
					notifyInfo[i] = 0l;
			}
			if (cantRunWarning > 0l)
				cantRunWarning--;
			editingBpmMode--;
			if (editingBpmMode < 0l)
				editingBpmMode = 0l;
			
			// To Expander
			if (rightExpander.module && rightExpander.module->model == modelClockedExpander) {
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				messageToExpander[0] = (float)panelTheme;
				messageToExpander[1] = panelContrast;
				rightExpander.module->leftExpander.messageFlipRequested = true;
			}
		}// lightRefreshCounter
	}// process()
};


struct ClockedWidget : ModuleWidget {
	PortWidget* slaveResetRunBpmInputs[3];

	struct RatioDisplayWidget : TransparentWidget {
		Clocked *module;
		int knobIndex;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char displayStr[16];
		const std::string delayLabelsClock[8] = {"D 0", "/16",   "1/8",  "1/4", "1/3",     "1/2", "2/3",     "3/4"};
		const std::string delayLabelsNote[8]  = {"D 0", "/64",   "/32",  "/16", "/8t",     "1/8", "/4t",     "/8d"};

		
		RatioDisplayWidget() {
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
					if (knobIndex == 0)
						snprintf(displayStr, 4, "120");
					else
						snprintf(displayStr, 4, "X 1");
				}
				else if (module->notifyInfo[knobIndex] > 0l)
				{
					int srcParam = module->notifyingSource[knobIndex];
					if ( (srcParam >= Clocked::SWING_PARAMS + 0) && (srcParam <= Clocked::SWING_PARAMS + 3) ) {
						float swValue = module->swingAmount[knobIndex];//module->params[Clocked::SWING_PARAMS + knobIndex].getValue();
						int swInt = (int)std::round(swValue * 99.0f);
						snprintf(displayStr, 16, " %2u", (unsigned) abs(swInt));
						if (swInt < 0)
							displayStr[0] = '-';
						if (swInt >= 0)
							displayStr[0] = '+';
					}
					else if ( (srcParam >= Clocked::DELAY_PARAMS + 1) && (srcParam <= Clocked::DELAY_PARAMS + 3) ) {				
						int delayKnobIndex = (int)(module->params[Clocked::DELAY_PARAMS + knobIndex].getValue() + 0.5f);
						if (module->displayDelayNoteMode)
							snprintf(displayStr, 4, "%s", (delayLabelsNote[delayKnobIndex]).c_str());
						else
							snprintf(displayStr, 4, "%s", (delayLabelsClock[delayKnobIndex]).c_str());
					}					
					else if ( (srcParam >= Clocked::PW_PARAMS + 0) && (srcParam <= Clocked::PW_PARAMS + 3) ) {				
						float pwValue = module->pulseWidth[knobIndex];//module->params[Clocked::PW_PARAMS + knobIndex].getValue();
						int pwInt = ((int)std::round(pwValue * 98.0f)) + 1;
						snprintf(displayStr, 16, "_%2u", (unsigned) abs(pwInt));
					}					
				}
				else {
					if (knobIndex > 0) {// ratio to display
						bool isDivision = false;
						int ratioDoubled = module->getRatioDoubled(knobIndex);
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
						if (module->editingBpmMode != 0l) {
							if (!module->bpmDetectionMode)
								snprintf(displayStr, 4, " CV");
							else
								snprintf(displayStr, 16, "P%2u", (unsigned) module->ppqn);
						}
						else
							snprintf(displayStr, 16, "%3u", (unsigned)((120.0f / module->masterLength) + 0.5f));
					}
				}
				displayStr[3] = 0;// more safety
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};		
	
	void appendContextMenu(Menu *menu) override {
		Clocked *module = dynamic_cast<Clocked*>(this->module);
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
				   module->resetClocked(true);}
		));
		
		menu->addChild(createBoolPtrMenuItem("Display delay values in notes", "", &module->displayDelayNoteMode));

		menu->addChild(createBoolMenuItem("Run CV input is level sensitive", "",
			[=]() {return !module->momentaryRunInput;},
			[=](bool loop) {module->momentaryRunInput = !module->momentaryRunInput;}
		));
		
		menu->addChild(createBoolPtrMenuItem("BPM out is CV when ext sync", "", &module->forceCvOnBpmOut));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Actions"));
		
		AutopatchItem *apItem = createMenuItem<AutopatchItem>("Auto-patch", RIGHT_ARROW);
		apItem->idPtr = &module->id;
		apItem->resetClockOutputsHighPtr = &module->resetClockOutputsHigh;
		apItem->slaveResetRunBpmInputs = slaveResetRunBpmInputs;
		menu->addChild(apItem);
		
		InstantiateExpanderItem *expItem = createMenuItem<InstantiateExpanderItem>("Add expander (4HP right side)", "");
		expItem->module = module;
		expItem->model = modelClockedExpander;
		expItem->posit = box.pos.plus(math::Vec(box.size.x,0));
		menu->addChild(expItem);	
	}	
	
	struct IMSmallKnobNotify : IMSmallKnob {// for Swing and PW
		void onDragMove(const event::DragMove &e) override {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity) {
				Clocked *module = dynamic_cast<Clocked*>(paramQuantity->module);
				int dispIndex = 0;
				int paramId = paramQuantity->paramId;
				if ( (paramId >= Clocked::SWING_PARAMS + 0) && (paramId <= Clocked::SWING_PARAMS + 3) )
					dispIndex = paramId - Clocked::SWING_PARAMS;
				else if ( (paramId >= Clocked::PW_PARAMS + 0) && (paramId <= Clocked::PW_PARAMS + 3) )
					dispIndex = paramId - Clocked::PW_PARAMS;
				module->notifyingSource[dispIndex] = paramId;
				module->notifyInfo[dispIndex] = (long) (Clocked::delayInfoTime * module->sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			Knob::onDragMove(e);
		}
	};
	struct IMSmallSnapKnobNotify : IMSmallKnob {// used for Delay
		void onDragMove(const event::DragMove &e) override {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity) {
				Clocked *module = dynamic_cast<Clocked*>(paramQuantity->module);
				int dispIndex = 0;
				int paramId = paramQuantity->paramId;
				if ( (paramId >= Clocked::DELAY_PARAMS + 1) && (paramId <= Clocked::DELAY_PARAMS + 3) )
					dispIndex = paramId - Clocked::DELAY_PARAMS;
				module->notifyingSource[dispIndex] = paramId;
				module->notifyInfo[dispIndex] = (long) (Clocked::delayInfoTime * module->sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			Knob::onDragMove(e);
		}
	};

	
	ClockedWidget(Clocked *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Clocked.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));


		static const int col0 = 30;// reset input and button, ratio knobs
		static const int colRulerSpacingT = 47;
		static const int col1 = col0 + colRulerSpacingT;// run input and button
		static const int col2 = col1 + colRulerSpacingT;// in and pwMaster inputs
		static const int col3 = col2 + colRulerSpacingT + 5;// swingMaster knob
		static const int col4 = col3 + colRulerSpacingT;// pwMaster knob
		static const int col5 = col4 + colRulerSpacingT;// clkMaster output
		static const int colM0 = col0 + 5;// ratio knobs
		static const int colM1 = col0 + 60;// ratio displays
		static const int colM2 = col3;// swingX knobs
		static const int colM3 = col4;// pwX knobs
		static const int colM4 = col5;// clkX outputs
		
		static const int row0 = 62;//reset, run inputs, master knob and bpm display
		static const int row1 = row0 + 55;// reset, run switches
		static const int row2 = row1 + 55;// clock 1
		static const int rowSpacingClks = 50;
		static const int row5 = row2 + rowSpacingClks * 2 + 55;// reset,run outputs, pw inputs
		
		
		RatioDisplayWidget *displayRatios[4];
		
		// Row 0
		// Reset input
		addInput(slaveResetRunBpmInputs[0] = createDynamicPortCentered<IMPort>(VecPx(col0, row0), true, module, Clocked::RESET_INPUT, mode));
		// Run input
		addInput(slaveResetRunBpmInputs[1] = createDynamicPortCentered<IMPort>(VecPx(col1, row0), true, module, Clocked::RUN_INPUT, mode));
		// In input
		addInput(slaveResetRunBpmInputs[2] = createDynamicPortCentered<IMPort>(VecPx(col2, row0), true, module, Clocked::BPM_INPUT, mode));
		// Master BPM knob
		addParam(createDynamicParamCentered<IMBigKnob>(VecPx(col3 + 1, row0), module, Clocked::RATIO_PARAMS + 0, mode));// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		// BPM display
		displayRatios[0] = new RatioDisplayWidget();
		displayRatios[0]->box.size = VecPx(55, 30);// 3 characters
		displayRatios[0]->box.pos = VecPx(col4 + 26.5f, row0).minus(displayRatios[0]->box.size.div(2));
		displayRatios[0]->module = module;
		displayRatios[0]->knobIndex = 0;
		addChild(displayRatios[0]);
		svgPanel->fb->addChild(new DisplayBackground(displayRatios[0]->box.pos, displayRatios[0]->box.size, mode));
		
		// Row 1
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(col0, row1), module, Clocked::RESET_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(col0, row1), module, Clocked::RESET_LIGHT));
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(col1, row1), module, Clocked::RUN_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(col1, row1), module, Clocked::RUN_LIGHT));
		// BPM mode buttons
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(col2 - 12, row1), module, Clocked::BPMMODE_DOWN_PARAM, mode));
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(col2 + 12, row1), module, Clocked::BPMMODE_UP_PARAM, mode));
		// BPM mode light
		addChild(createLightCentered<SmallLight<GreenRedLightIM>>(VecPx(col2, row1 + 15), module, Clocked::BPMSYNC_LIGHT));		
		// Swing master knob
		addParam(createDynamicParamCentered<IMSmallKnobNotify>(VecPx(col3, row1), module, Clocked::SWING_PARAMS + 0, mode));
		// PW master knob
		addParam(createDynamicParamCentered<IMSmallKnobNotify>(VecPx(col4, row1), module, Clocked::PW_PARAMS + 0, mode));
		// Clock master out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row1), false, module, Clocked::CLK_OUTPUTS + 0, mode));
		
		
		// Row 2-4 (sub clocks)		
		for (int i = 0; i < 3; i++) {
			// Ratio1 knob
			addParam(createDynamicParamCentered<IMBigKnob>(VecPx(colM0, row2 + i * rowSpacingClks), module, Clocked::RATIO_PARAMS + 1 + i, mode));		
			// Ratio display
			displayRatios[i + 1] = new RatioDisplayWidget();
			displayRatios[i + 1]->box.size = VecPx(55, 30);// 3 characters
			displayRatios[i + 1]->box.pos = VecPx(colM1 + 15.4f, row2 + i * rowSpacingClks).minus(displayRatios[i + 1]->box.size.div(2));
			displayRatios[i + 1]->module = module;
			displayRatios[i + 1]->knobIndex = i + 1;
			addChild(displayRatios[i + 1]);
			svgPanel->fb->addChild(new DisplayBackground(displayRatios[i + 1]->box.pos, displayRatios[i + 1]->box.size, mode));
			// Sync light
			addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(colM1 + 54, row2 + i * rowSpacingClks), module, Clocked::CLK_LIGHTS + i + 1));		
			// Swing knobs
			addParam(createDynamicParamCentered<IMSmallKnobNotify>(VecPx(colM2, row2 + i * rowSpacingClks), module, Clocked::SWING_PARAMS + 1 + i, mode));
			// PW knobs
			addParam(createDynamicParamCentered<IMSmallKnobNotify>(VecPx(colM3, row2 + i * rowSpacingClks), module, Clocked::PW_PARAMS + 1 + i, mode));
			// Delay knobs
			addParam(createDynamicParamCentered<IMSmallSnapKnobNotify>(VecPx(colM4, row2 + i * rowSpacingClks), module, Clocked::DELAY_PARAMS + 1 + i, mode));
		}

		// Last row
		// Reset out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col0, row5), false, module, Clocked::RESET_OUTPUT, mode));
		// Run out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col1, row5), false, module, Clocked::RUN_OUTPUT, mode));
		// Out out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col2, row5), false, module, Clocked::BPM_OUTPUT, mode));
		// Sub-clock outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col3, row5), false, module, Clocked::CLK_OUTPUTS + 1, mode));	
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col4, row5), false, module, Clocked::CLK_OUTPUTS + 2, mode));	
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row5), false, module, Clocked::CLK_OUTPUTS + 3, mode));	
	}
	
	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			if ( e.key == GLFW_KEY_SPACE && ((e.mods & RACK_MOD_MASK) == 0) ) {
				Clocked *module = dynamic_cast<Clocked*>(this->module);
				module->toggleRun();
				e.consume(this);
				return;
			}
			if ( e.key == GLFW_KEY_M && ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) ) {
				Clocked *module = dynamic_cast<Clocked*>(this->module);
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

Model *modelClocked = createModel<Clocked, ClockedWidget>("Clocked");
