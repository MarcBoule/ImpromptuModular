//***********************************************************************************************
//Six channel 128-step sequencer module for VCV Rack by Marc Boulé 
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Based on the BigButton sequencer by Look-Mum-No-Computer, with modifications 
//  by Marc Boulé and Jean-Sébastien Monzani
//https://www.youtube.com/watch?v=6ArDGcUqiWM
//https://www.lookmumnocomputer.com/projects/#/big-button/
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "Interop.hpp"


struct BigButtonSeq2 : Module {
	enum ParamIds {
		CHAN_PARAM,
		LEN_PARAM,
		RND_PARAM,
		RESET_PARAM,
		CLEAR_PARAM,
		BANK_PARAM,
		DEL_PARAM,
		FILL_PARAM,
		BIG_PARAM,
		WRITEFILL_PARAM,
		QUANTIZEBIG_PARAM,
		SAMPLEHOLD_PARAM,
		CLOCK_PARAM,
		DISPMODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLK_INPUT,
		CHAN_INPUT,
		BIG_INPUT,
		LEN_INPUT,
		RND_INPUT,
		RESET_INPUT,
		CLEAR_INPUT,
		BANK_INPUT,
		DEL_INPUT,
		FILL_INPUT,
		CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CHAN_OUTPUTS, 6),
		ENUMS(CV_OUTPUTS, 6),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHAN_LIGHTS, 6 * 2),// Room for GreenRed
		BIG_LIGHT,
		BIGC_LIGHT,
		ENUMS(METRONOME_LIGHT, 2),// Room for GreenRed
		WRITEFILL_LIGHT,
		QUANTIZEBIG_LIGHT,
		SAMPLEHOLD_LIGHT,		
		NUM_LIGHTS
	};
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;

	
	// Need to save, with reset
	int indexStep;
	int bank[6];
	uint64_t gates[6][2][2];// channel , bank , 64x2 page for 128
	float cv[6][2][128];// channel , bank , indexStep
	int metronomeDiv = 4;
	bool writeFillsToMemory;
	bool quantizeBig;
	bool nextStepHits;
	bool sampleAndHold;
	
	// No need to save, with reset
	long clockIgnoreOnReset;
	double lastPeriod;//2.0 when not seen yet (init or stopped clock and went greater than 2s, which is max period supported for time-snap)
	double clockTime;//clock time counter (time since last clock)
	int pendingOp;// 0 means nothing pending, +1 means pending big button push, -1 means pending del
	float pendingCV;// 
	bool fillPressed;

	// No need to save, no reset
	RefreshCounter refresh;	
	float bigLight = 0.0f;
	float metronomeLightStart = 0.0f;
	float metronomeLightDiv = 0.0f;
	int channel = 0;
	int length = 0; 
	float sampleHoldBuf[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	Trigger clockTrigger;
	Trigger resetTrigger;
	Trigger bankTrigger;
	Trigger bigTrigger;
	Trigger clearTrigger;
	Trigger writeFillTrigger;
	Trigger quantizeBigTrigger;
	Trigger sampleHoldTrigger;
	Trigger internalSHTriggers[6];
	dsp::PulseGenerator outLightPulse;
	dsp::PulseGenerator bigPulse;
	dsp::PulseGenerator bigLightPulse;

	
	inline bool getGate(int _chan, int _step) {return !((gates[_chan][bank[_chan]][_step >> 6] & (((uint64_t)1) << (uint64_t)(_step & 0x3F))) == 0);}
	inline void setGate(int _chan, int _step) {gates[_chan][bank[_chan]][_step >> 6] |= (((uint64_t)1) << (uint64_t)(_step & 0x3F));}
	inline void clearGate(int _chan, int _step) {gates[_chan][bank[_chan]][_step >> 6] &= ~(((uint64_t)1) << (uint64_t)(_step & 0x3F));}
	inline void toggleGate(int _chan, int _step) {gates[_chan][bank[_chan]][_step >> 6] ^= (((uint64_t)1) << (uint64_t)(_step & 0x3F));}
	inline void clearGates(int _chan, int bnk) {gates[_chan][bnk][0] = 0; gates[_chan][bnk][1] = 0;}
	inline void randomizeGates(int _chan, int bnk) {gates[_chan][bnk][0] = random::u64(); gates[_chan][bnk][1] = random::u64();}
	inline void writeCV(int _chan, int _step, float cvValue) {cv[_chan][bank[_chan]][_step] = cvValue;}
	inline void writeCV(int _chan, int bnk, int _step, float cvValue) {cv[_chan][bnk][_step] = cvValue;}
	inline void sampleOutput(int _chan) {sampleHoldBuf[_chan] = cv[_chan][bank[_chan]][indexStep];}
	inline int calcChan() {
		float chanInputValue = inputs[CHAN_INPUT].getVoltage() / 10.0f * (6.0f - 1.0f);
		return (int) clamp(std::round(params[CHAN_PARAM].getValue() + chanInputValue), 0.0f, (6.0f - 1.0f));		
	}

	
	BigButtonSeq2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		configParam(RND_PARAM, 0.0f, 100.0f, 0.0f, "Random");
		paramQuantities[RND_PARAM]->snapEnabled = true;
		configParam(CHAN_PARAM, 0.0f, 6.0f - 1.0f, 0.0f, "Channel", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset	
		paramQuantities[CHAN_PARAM]->snapEnabled = true;
		configParam(LEN_PARAM, 0.0f, 128.0f - 1.0f, 32.0f - 1.0f, "Length", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset
		paramQuantities[LEN_PARAM]->snapEnabled = true;
		configSwitch(DISPMODE_PARAM, 0.0, 1.0f, 0.0f, "Display mode", {"Length", "Current step"});		
		configParam(WRITEFILL_PARAM, 0.0f, 1.0f, 0.0f, "Write fill");
		configParam(BANK_PARAM, 0.0f, 1.0f, 0.0f, "Bank");	
		configParam(CLOCK_PARAM, 0.0f, 1.0f, 0.0f, "Clock step");	
		configParam(DEL_PARAM, 0.0f, 1.0f, 0.0f, "Delete");	
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");	
		configParam(FILL_PARAM, 0.0f, 1.0f, 0.0f, "Fill");	
		configParam(BIG_PARAM, 0.0f, 1.0f, 0.0f, "Big button");
		configParam(QUANTIZEBIG_PARAM, 0.0f, 1.0f, 0.0f, "Quantize big button");
		configParam(CLEAR_PARAM, 0.0f, 1.0f, 0.0f, "Clear");	
		configParam(SAMPLEHOLD_PARAM, 0.0f, 1.0f, 0.0f, "Sample & hold");
		
		getParamQuantity(DISPMODE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(LEN_PARAM)->randomizeEnabled = false;		
		getParamQuantity(CHAN_PARAM)->randomizeEnabled = false;	

		configInput(CLK_INPUT, "Clock");
		configInput(CHAN_INPUT, "Channel select");
		configInput(BIG_INPUT, "Big button");
		configInput(LEN_INPUT, "Length");
		configInput(RND_INPUT, "Random");
		configInput(RESET_INPUT, "Reset");
		configInput(CLEAR_INPUT, "Clear");
		configInput(BANK_INPUT, "Bank select");
		configInput(DEL_INPUT, "Delete");
		configInput(FILL_INPUT, "Fill");
		configInput(CV_INPUT, "CV");

		for (int i = 0; i < 6; i++) {
			configOutput(CHAN_OUTPUTS + i, string::f("Channel %i gate", i + 1));
			configOutput(CV_OUTPUTS + i, string::f("Channel %i CV", i + 1));
		}

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	
	void onReset() override {
		indexStep = 0;
		for (int c = 0; c < 6; c++) {
			bank[c] = 0;
			for (int b = 0; b < 2; b++) {
				clearGates(c, b);
				for (int s = 0; s < 128; s++)
					writeCV(c, b, s, 0.0f);
			}
		}
		metronomeDiv = 4;
		writeFillsToMemory = false;
		quantizeBig = true;
		nextStepHits = false;
		sampleAndHold = false;
		resetNonJson();
	}
	void resetNonJson() {
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * APP->engine->getSampleRate());
		lastPeriod = 2.0;
		clockTime = 0.0;
		pendingOp = 0;
		pendingCV = 0.0f;
		fillPressed = false;
	}


	void onRandomize() override {
		int chanRnd = calcChan();
		randomizeGates(chanRnd, bank[chanRnd]);
		for (int s = 0; s < 128; s++)
			writeCV(chanRnd, bank[chanRnd], s, ((float)(random::u32() % 5)) + ((float)(random::u32() % 12)) / 12.0f - 2.0f);
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// indexStep
		json_object_set_new(rootJ, "indexStep", json_integer(indexStep));

		// bank
		json_t *bankJ = json_array();
		for (int c = 0; c < 6; c++)
			json_array_insert_new(bankJ, c, json_integer(bank[c]));
		json_object_set_new(rootJ, "bank", bankJ);

		// gates LS64
		json_t *gatesLJ = json_array();
		for (int c = 0; c < 6; c++)
			for (int b = 0; b < 8; b++) {// bank to store is like uint64_t to store, so go to 8
				// first to get stored is 16 lsbits of bank 0, then next 16 bits,... to 16 msbits of bank 1
				unsigned int intValue = (unsigned int) ( (uint64_t)0xFFFF & (gates[c][b/4][0] >> (uint64_t)(16 * (b % 4))) );
				json_array_insert_new(gatesLJ, b + (c << 3) , json_integer(intValue));
			}
		json_object_set_new(rootJ, "gatesL", gatesLJ);
		// gates MS64
		json_t *gatesMJ = json_array();
		for (int c = 0; c < 6; c++)
			for (int b = 0; b < 8; b++) {// bank to store is like uint64_t to store, so go to 8
				// first to get stored is 16 lsbits of bank 0, then next 16 bits,... to 16 msbits of bank 1
				unsigned int intValue = (unsigned int) ( (uint64_t)0xFFFF & (gates[c][b/4][1] >> (uint64_t)(16 * (b % 4))) );
				json_array_insert_new(gatesMJ, b + (c << 3) , json_integer(intValue));
			}
		json_object_set_new(rootJ, "gatesM", gatesMJ);

		// CV bank 0
		json_t *cv0J = json_array();
		for (int c = 0; c < 6; c++) {
			for (int s = 0; s < 128; s++) {
				json_array_insert_new(cv0J, s + c * 128, json_real(cv[c][0][s]));
			}
		}
		json_object_set_new(rootJ, "cv0", cv0J);
		// CV bank 1
		json_t *cv1J = json_array();
		for (int c = 0; c < 6; c++) {
			for (int s = 0; s < 128; s++) {
				json_array_insert_new(cv1J, s + c * 128, json_real(cv[c][1][s]));
			}
		}
		json_object_set_new(rootJ, "cv1", cv1J);

		// metronomeDiv
		json_object_set_new(rootJ, "metronomeDiv", json_integer(metronomeDiv));

		// writeFillsToMemory
		json_object_set_new(rootJ, "writeFillsToMemory", json_boolean(writeFillsToMemory));

		// quantizeBig
		json_object_set_new(rootJ, "quantizeBig", json_boolean(quantizeBig));

		// nextStepHits
		json_object_set_new(rootJ, "nextStepHits", json_boolean(nextStepHits));

		// sampleAndHold
		json_object_set_new(rootJ, "sampleAndHold", json_boolean(sampleAndHold));

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

		// indexStep
		json_t *indexStepJ = json_object_get(rootJ, "indexStep");
		if (indexStepJ)
			indexStep = json_integer_value(indexStepJ);

		// bank
		json_t *bankJ = json_object_get(rootJ, "bank");
		if (bankJ)
			for (int c = 0; c < 6; c++)
			{
				json_t *bankArrayJ = json_array_get(bankJ, c);
				if (bankArrayJ)
					bank[c] = json_integer_value(bankArrayJ);
			}

		// gates LS64
		json_t *gatesLJ = json_object_get(rootJ, "gatesL");
		uint64_t bank8intsL[8] = {0,0,0,0,0,0,0,0};
		if (gatesLJ) {
			for (int c = 0; c < 6; c++) {
				for (int b = 0; b < 8; b++) {// bank to store is like to uint64_t to store, so go to 8
					// first to get read is 16 lsbits of bank 0, then next 16 bits,... to 16 msbits of bank 1
					json_t *gateLJ = json_array_get(gatesLJ, b + (c << 3));
					if (gateLJ)
						bank8intsL[b] = (uint64_t) json_integer_value(gateLJ);
				}
				gates[c][0][0] = bank8intsL[0] | (bank8intsL[1] << (uint64_t)16) | (bank8intsL[2] << (uint64_t)32) | (bank8intsL[3] << (uint64_t)48);
				gates[c][1][0] = bank8intsL[4] | (bank8intsL[5] << (uint64_t)16) | (bank8intsL[6] << (uint64_t)32) | (bank8intsL[7] << (uint64_t)48);
			}
		}
		// gates MS64
		json_t *gatesMJ = json_object_get(rootJ, "gatesM");
		uint64_t bank8intsM[8] = {0,0,0,0,0,0,0,0};
		if (gatesMJ) {
			for (int c = 0; c < 6; c++) {
				for (int b = 0; b < 8; b++) {// bank to store is like to uint64_t to store, so go to 8
					// first to get read is 16 lsbits of bank 0, then next 16 bits,... to 16 msbits of bank 1
					json_t *gateMJ = json_array_get(gatesMJ, b + (c << 3));
					if (gateMJ)
						bank8intsM[b] = (uint64_t) json_integer_value(gateMJ);
				}
				gates[c][0][1] = bank8intsM[0] | (bank8intsM[1] << (uint64_t)16) | (bank8intsM[2] << (uint64_t)32) | (bank8intsM[3] << (uint64_t)48);
				gates[c][1][1] = bank8intsM[4] | (bank8intsM[5] << (uint64_t)16) | (bank8intsM[6] << (uint64_t)32) | (bank8intsM[7] << (uint64_t)48);
			}
		}
		
		// CV bank 0
		json_t *cv0J = json_object_get(rootJ, "cv0");
		if (cv0J) {
			for (int c = 0; c < 6; c++)
				for (int s = 0; s < 128; s++) {
					json_t *cv0ArrayJ = json_array_get(cv0J, s + c * 128);
					if (cv0ArrayJ)
						cv[c][0][s] = json_number_value(cv0ArrayJ);
				}
		}
		// CV bank 1
		json_t *cv1J = json_object_get(rootJ, "cv1");
		if (cv1J) {
			for (int c = 0; c < 6; c++)
				for (int s = 0; s < 128; s++) {
					json_t *cv1ArrayJ = json_array_get(cv1J, s + c * 128);
					if (cv1ArrayJ)
						cv[c][1][s] = json_number_value(cv1ArrayJ);
				}
		}
		
		// metronomeDiv
		json_t *metronomeDivJ = json_object_get(rootJ, "metronomeDiv");
		if (metronomeDivJ)
			metronomeDiv = json_integer_value(metronomeDivJ);

		// writeFillsToMemory
		json_t *writeFillsToMemoryJ = json_object_get(rootJ, "writeFillsToMemory");
		if (writeFillsToMemoryJ)
			writeFillsToMemory = json_is_true(writeFillsToMemoryJ);
		
		// quantizeBig
		json_t *quantizeBigJ = json_object_get(rootJ, "quantizeBig");
		if (quantizeBigJ)
			quantizeBig = json_is_true(quantizeBigJ);

		// nextStepHits
		json_t *nextStepHitsJ = json_object_get(rootJ, "nextStepHits");
		if (nextStepHitsJ)
			nextStepHits = json_is_true(nextStepHitsJ);

		// sampleAndHold
		json_t *sampleAndHoldJ = json_object_get(rootJ, "sampleAndHold");
		if (sampleAndHoldJ)
			sampleAndHold = json_is_true(sampleAndHoldJ);
		
		resetNonJson();
	}

	
	IoStep* fillIoSteps(int *seqLenPtr) {// caller must delete return array
		int seqLen = length;
		IoStep* ioSteps = new IoStep[seqLen];
		
		// populate ioSteps array
		for (int i = 0; i < seqLen; i++) {
			ioSteps[i].pitch = cv[channel][bank[channel]][i];
			ioSteps[i].gate = getGate(channel, i);
			ioSteps[i].tied = false;
			ioSteps[i].vel = -1.0f;// no concept of velocity in BigButton2
			ioSteps[i].prob = -1.0f;// no concept of probability in BigButton2
		}
		
		// return values 
		*seqLenPtr = seqLen;
		return ioSteps;
	}
	
	
	void emptyIoSteps(IoStep* ioSteps, int seqLen) {
		// params[LEN_PARAM].setValue(seqLen); length not modified since only one length knob for all 6 channels and don't want to change that
		// so instead we will pad empty gates up to length if pasted seq is shorter than length
		
		// populate steps in the sequencer
		int i = 0;
		for (; i < seqLen; i++) {
			cv[channel][bank[channel]][i] = ioSteps[i].pitch;
			if (ioSteps[i].gate) {
				setGate(channel, i);
			}
			else {
				clearGate(channel, i);
			}
		}
		// pad the rest
		for (; i < length; i++) {
			cv[channel][bank[channel]][i] = 0.0f;
			clearGate(channel, i);
		}		
	}	
	
	
	void process(const ProcessArgs &args) override {
		double sampleTime = 1.0 / args.sampleRate;
		static const float lightTime = 0.1f;
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		channel = calcChan();		
		length = (int) clamp(std::round( params[LEN_PARAM].getValue() + ( inputs[LEN_INPUT].isConnected() ? (inputs[LEN_INPUT].getVoltage() / 10.0f * (128.0f - 1.0f)) : 0.0f ) ), 0.0f, (128.0f - 1.0f)) + 1;	

		
		if (refresh.processInputs()) {
			// Big button
			if (bigTrigger.process(params[BIG_PARAM].getValue() + inputs[BIG_INPUT].getVoltage())) {
				bigLight = 1.0f;
				if (nextStepHits) {
					int nextStep = (indexStep + 1) % length;
					setGate(channel, nextStep);// bank is global
					if (inputs[CV_INPUT].isConnected()) {
						writeCV(channel, nextStep, inputs[CV_INPUT].getVoltage());
					}
				}
				else if (quantizeBig && (clockTime > (lastPeriod / 2.0)) && (clockTime <= (lastPeriod * 1.01))) {// allow for 1% clock jitter
					pendingOp = 1;
					pendingCV = inputs[CV_INPUT].getVoltage();
				}
				else {
					if (!getGate(channel, indexStep)) {
						setGate(channel, indexStep);// bank is global
						bigPulse.trigger(0.001f);
					}
					if (inputs[CV_INPUT].isConnected()) {
						writeCV(channel, indexStep, inputs[CV_INPUT].getVoltage());
					}
					bigLightPulse.trigger(lightTime);
				}
			}

			// Bank button
			if (bankTrigger.process(params[BANK_PARAM].getValue() + inputs[BANK_INPUT].getVoltage()))
				bank[channel] = 1 - bank[channel];
			
			// Clear button
			if (clearTrigger.process(params[CLEAR_PARAM].getValue() + inputs[CLEAR_INPUT].getVoltage())) {
				clearGates(channel, bank[channel]);
				for (int s = 0; s < 128; s++)
					cv[channel][bank[channel]][s] = 0.0f;
			}
			
			// Write fill to memory
			if (writeFillTrigger.process(params[WRITEFILL_PARAM].getValue()))
				writeFillsToMemory = !writeFillsToMemory;

			// Quantize big button (aka snap)
			if (quantizeBigTrigger.process(params[QUANTIZEBIG_PARAM].getValue()))
				quantizeBig = !quantizeBig;

			// Sample and hold
			if (sampleHoldTrigger.process(params[SAMPLEHOLD_PARAM].getValue()))
				sampleAndHold = !sampleAndHold;
			
			// Del button
			if (params[DEL_PARAM].getValue() + inputs[DEL_INPUT].getVoltage() > 0.5f) {
				if (nextStepHits) {
					int nextStep = (indexStep + 1) % length;
					clearGate(channel, nextStep);// bank is global
					cv[channel][bank[channel]][nextStep] = 0.0f;
				}
				else if (quantizeBig && (clockTime > (lastPeriod / 2.0)) && (clockTime <= (lastPeriod * 1.01))) {// allow for 1% clock jitter
					pendingOp = -1;// overrides the pending write if it exists
				}
				else {
					clearGate(channel, indexStep);// bank is global
					cv[channel][bank[channel]][indexStep] = 0.0f;
				}
			}

			// Pending timeout (write/del current step)
			if (pendingOp != 0 && clockTime > (lastPeriod * 1.01) ) 
				performPending(channel, lightTime);
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (clockIgnoreOnReset == 0l) {			
			if (clockTrigger.process(inputs[CLK_INPUT].getVoltage() + params[CLOCK_PARAM].getValue())) {
				if ((++indexStep) >= length) indexStep = 0;
				
				// Fill button
				fillPressed = (params[FILL_PARAM].getValue() + inputs[FILL_INPUT].getVoltage()) > 0.5f;// used in clock block and others
				if (fillPressed && writeFillsToMemory) {
					setGate(channel, indexStep);// bank is global
					if (inputs[CV_INPUT].isConnected()) {
						writeCV(channel, indexStep, inputs[CV_INPUT].getVoltage());//sampleHoldBuf[channel]);
					}
				}
				
				//outPulse.trigger(0.001f);
				outLightPulse.trigger(lightTime);
				
				if (pendingOp != 0)
					performPending(channel, lightTime);// Proper pending write/del to next step which is now reached
				
				if (indexStep == 0)
					metronomeLightStart = 1.0f;
				metronomeLightDiv = ((indexStep % metronomeDiv) == 0 && indexStep != 0) ? 1.0f : 0.0f;
				
				// Random (toggle gate according to probability knob)
				float rnd01 = params[RND_PARAM].getValue() / 100.0f + inputs[RND_INPUT].getVoltage() / 10.0f;
				if (rnd01 > 0.0f) {
					if (random::uniform() < rnd01)// random::uniform is [0.0, 1.0), see include/util/common.hpp
						toggleGate(channel, indexStep);
				}
				lastPeriod = clockTime > 2.0 ? 2.0 : clockTime;
				clockTime = 0.0;
			}
		}
			
		
		// Reset
		if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * args.sampleRate);
			indexStep = 0;
			//outPulse.trigger(0.001f);
			outLightPulse.trigger(0.02f);
			metronomeLightStart = 1.0f;
			metronomeLightDiv = 0.0f;
			clockTrigger.reset();
		}		
		
		
		
		//********** Outputs and lights **********
		
		
		// Gate outputs
		bool bigPulseState = bigPulse.process((float)sampleTime);
		bool outPulseState = clockTrigger.isHigh();
		bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
		for (int i = 0; i < 6; i++) {
			bool gate = getGate(i, indexStep);
			bool outSignal = ( ((gate || (i == channel && fillPressed)) && outPulseState) || (gate && bigPulseState && i == channel) );
			float outGateValue = outSignal ? 10.0f : 0.0f;
			if (internalSHTriggers[i].process(outGateValue))
				sampleOutput(i);
			outputs[CHAN_OUTPUTS + i].setVoltage((retriggingOnReset ? 0.0f : outGateValue));
			float cvOut = (i == channel && fillPressed && !writeFillsToMemory && inputs[CV_INPUT].isConnected()) ? inputs[CV_INPUT].getVoltage() : 
							(sampleAndHold ? sampleHoldBuf[i] : cv[i][bank[i]][indexStep]);
			outputs[CV_OUTPUTS + i].setVoltage(cvOut);
		}

		
		// lights
		if (refresh.processLights()) {
			float deltaTime = (float)sampleTime * RefreshCounter::displayRefreshStepSkips;

			// Gate light outputs
			bool bigLightPulseState = bigLightPulse.process(deltaTime);
			bool outLightPulseState = outLightPulse.process(deltaTime);
			for (int i = 0; i < 6; i++) {
				bool gate = getGate(i, indexStep);
				bool outLight  = (((gate || (i == channel && fillPressed)) && outLightPulseState) || (gate && bigLightPulseState && i == channel));
				lights[(CHAN_LIGHTS + i) * 2 + 1].setSmoothBrightness(outLight ? 1.0f : 0.0f, deltaTime);
				lights[(CHAN_LIGHTS + i) * 2 + 0].setBrightness(i == channel ? (1.0f - lights[(CHAN_LIGHTS + i) * 2 + 1].getBrightness()) : 0.0f);
			}
			
			deltaTime = (float)sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2);

			// Big button lights
			lights[BIG_LIGHT].setBrightness(bank[channel] == 1 ? 1.0f : 0.0f);
			lights[BIGC_LIGHT].setSmoothBrightness(bigLight, deltaTime);
			bigLight = 0.0f;	
			
			// Metronome light
			lights[METRONOME_LIGHT + 1].setSmoothBrightness(metronomeLightStart, deltaTime);
			lights[METRONOME_LIGHT + 0].setSmoothBrightness(metronomeLightDiv, deltaTime);
			metronomeLightStart = 0.0f;	
			metronomeLightDiv = 0.0f;
		
			// Other push button lights
			lights[WRITEFILL_LIGHT].setBrightness(writeFillsToMemory ? 1.0f : 0.0f);
			lights[QUANTIZEBIG_LIGHT].setBrightness(quantizeBig ? 1.0f : 0.0f);
			lights[SAMPLEHOLD_LIGHT].setBrightness(sampleAndHold ? 1.0f : 0.0f);
		}
		
		clockTime += sampleTime;
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// process()
	
	
	inline void performPending(int chan, float lightTime) {
		if (pendingOp == 1) {
			if (!getGate(chan, indexStep)) {
				setGate(chan, indexStep);// bank is global
				bigPulse.trigger(0.001f);
			}
			if (inputs[CV_INPUT].isConnected()) {
				writeCV(chan, indexStep, pendingCV);
			}
			bigLightPulse.trigger(lightTime);
		}
		else {
			clearGate(chan, indexStep);// bank is global
		}
		pendingOp = 0;
	}
};


struct BigButtonSeq2Widget : ModuleWidget {
	struct ChanDisplayWidget : TransparentWidget {
		BigButtonSeq2 *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		
		ChanDisplayWidget() {
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
				nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				char displayStr[2];
				unsigned int channel = (unsigned)(module ? module->channel : 0);
				snprintf(displayStr, 2, "%1u", (unsigned) (channel + 1) );
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};

	struct StepsDisplayWidget : TransparentWidget {
		BigButtonSeq2 *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		
		StepsDisplayWidget() {
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
				char displayStr[4];
				unsigned dispVal = 128;
				if (module)
					dispVal = (unsigned)(module->params[BigButtonSeq2::DISPMODE_PARAM].getValue() < 0.5f ?  module->length : module->indexStep + 1);
				snprintf(displayStr, 4, "%3u",  dispVal);
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};
	
	struct InteropSeqItem : MenuItem {
		struct InteropCopySeqItem : MenuItem {
			BigButtonSeq2 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = module->fillIoSteps(&seqLen);
				interopCopySequence(seqLen, ioSteps);
				delete[] ioSteps;
			}
		};
		struct InteropPasteSeqItem : MenuItem {
			BigButtonSeq2 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = interopPasteSequence(128, &seqLen);
				if (ioSteps != nullptr) {
					module->emptyIoSteps(ioSteps, seqLen);
					delete[] ioSteps;
				}
			}
		};
		BigButtonSeq2 *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			InteropCopySeqItem *interopCopySeqItem = createMenuItem<InteropCopySeqItem>(portableSequenceCopyID, "");
			interopCopySeqItem->module = module;
			menu->addChild(interopCopySeqItem);		
			
			InteropPasteSeqItem *interopPasteSeqItem = createMenuItem<InteropPasteSeqItem>(portableSequencePasteID, "");
			interopPasteSeqItem->module = module;
			menu->addChild(interopPasteSeqItem);		

			return menu;
		}
	};	
	
	void appendContextMenu(Menu *menu) override {
		BigButtonSeq2 *module = dynamic_cast<BigButtonSeq2*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());
		
		InteropSeqItem *interopSeqItem = createMenuItem<InteropSeqItem>(portableSequenceID, RIGHT_ARROW);
		interopSeqItem->module = module;
		menu->addChild(interopSeqItem);		

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createBoolPtrMenuItem("Big and Del on next step", "", &module->nextStepHits));

		menu->addChild(createSubmenuItem("Metronome light", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("Every clock", "",
				[=]() {return module->metronomeDiv == 1;},
				[=]() {module->metronomeDiv = 1;}
			));
			menu->addChild(createCheckMenuItem("/2", "",
				[=]() {return module->metronomeDiv == 2;},
				[=]() {module->metronomeDiv = 2;}
			));
			menu->addChild(createCheckMenuItem("/4", "",
				[=]() {return module->metronomeDiv == 4;},
				[=]() {module->metronomeDiv = 4;}
			));
			menu->addChild(createCheckMenuItem("/8", "",
				[=]() {return module->metronomeDiv == 8;},
				[=]() {module->metronomeDiv = 8;}
			));
			menu->addChild(createCheckMenuItem("Full length", "",
				[=]() {return module->metronomeDiv == 1000;},
				[=]() {module->metronomeDiv = 1000;}
			));
		}));	
	}	
	
	
	BigButtonSeq2Widget(BigButtonSeq2 *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/BigButtonSeq2.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		
		
		// Column rulers (horizontal positions)
		static const int colRulerCenter = 129;// not real center, center of big button
		static const int offsetChanOutX = 20;
		static const int rowRuler0 = 52;// chan and len knobs+displays
		static const int rowRuler1 = rowRuler0 + 48;// chan and len CVs, display switch
		static const int rowRuler2 = rowRuler1 + 40;// fillmem led button and CV in
		static const int rowRuler3 = rowRuler1 + 21;// bank
		static const int rowRuler4 = rowRuler3 + 29;// clear and del
		static const int rowRuler5 = rowRuler4 + 45;// reset and fill
		static const int colRulerT0 = colRulerCenter - offsetChanOutX * 5;
		static const int colRulerT1 = colRulerCenter - offsetChanOutX * 3;
		static const int colRulerT2 = colRulerCenter - offsetChanOutX * 1;
		static const int colRulerT3 = colRulerCenter + offsetChanOutX * 1;
		static const int colRulerT4 = colRulerCenter + offsetChanOutX * 3;
		static const int colRulerT5 = colRulerCenter + offsetChanOutX * 5;
		static const int colRulerT6 = colRulerT5 + 42;
		static const int clearAndDelButtonOffsetX = (colRulerCenter - colRulerT0) / 2 + 4;
		static const int knobCVjackOffsetY = 36;
		static const int lengthDisplayOffsetX = 20;
		
		
		// Rnd knob
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colRulerT0, rowRuler0), module, BigButtonSeq2::RND_PARAM, mode));
		// Channel knob
		addParam(createDynamicParamCentered<IMSixPosBigKnob>(VecPx(colRulerCenter - clearAndDelButtonOffsetX, rowRuler0), module, BigButtonSeq2::CHAN_PARAM, mode));	
		// Channel display
		ChanDisplayWidget *displayChan = new ChanDisplayWidget();
		displayChan->box.pos = VecPx(colRulerCenter - 12, rowRuler0 - 15);
		displayChan->box.size = VecPx(24, 30);// 1 character
		displayChan->module = module;
		addChild(displayChan);
		svgPanel->fb->addChild(new DisplayBackground(displayChan->box.pos, displayChan->box.size, mode));
		// Len knob
		addParam(createDynamicParamCentered<IMBigKnob>(VecPx(colRulerCenter + clearAndDelButtonOffsetX, rowRuler0), module, BigButtonSeq2::LEN_PARAM, mode));
		// Length display
		StepsDisplayWidget *displaySteps = new StepsDisplayWidget();
		displaySteps->box.pos = VecPx(colRulerT5 - 27 + lengthDisplayOffsetX, rowRuler0 - 15);
		displaySteps->box.size = VecPx(55, 30);// 3 characters
		displaySteps->module = module;
		addChild(displaySteps);
		svgPanel->fb->addChild(new DisplayBackground(displaySteps->box.pos, displaySteps->box.size, mode));
		
		// Rnd jack
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerT0, rowRuler1), true, module, BigButtonSeq2::RND_INPUT, mode));
		// Channel jack
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerCenter - clearAndDelButtonOffsetX, rowRuler1), true, module, BigButtonSeq2::CHAN_INPUT, mode));
		// Length jack
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerCenter + clearAndDelButtonOffsetX, rowRuler1), true, module, BigButtonSeq2::LEN_INPUT, mode));
		// Display switch
		addParam(createDynamicSwitchCentered<IMSwitch2H>(VecPx(colRulerT5 + lengthDisplayOffsetX, rowRuler1 - 12), module, BigButtonSeq2::DISPMODE_PARAM, mode, svgPanel));		


		// Metronome light
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colRulerT0, rowRuler2), module, BigButtonSeq2::METRONOME_LIGHT + 0));
		// Mem fill LED button
		addParam(createParamCentered<LEDButton>(VecPx(colRulerT5, rowRuler2), module, BigButtonSeq2::WRITEFILL_PARAM));
		addChild(createLightCentered<MediumLight<GreenLightIM>>(VecPx(colRulerT5, rowRuler2), module, BigButtonSeq2::WRITEFILL_LIGHT));
		// CV Input
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerT6, rowRuler2), true, module, BigButtonSeq2::CV_INPUT, mode));		

		
		// Bank button and jack
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colRulerCenter, rowRuler3), module, BigButtonSeq2::BANK_PARAM, mode));	
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerCenter, rowRuler3 + knobCVjackOffsetY), true, module, BigButtonSeq2::BANK_INPUT, mode));
		// Clock button and jack
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colRulerCenter - clearAndDelButtonOffsetX, rowRuler4), module, BigButtonSeq2::CLOCK_PARAM, mode));	
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerCenter - clearAndDelButtonOffsetX, rowRuler4 + knobCVjackOffsetY), true, module, BigButtonSeq2::CLK_INPUT, mode));
		// Del button and jack
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colRulerCenter + clearAndDelButtonOffsetX, rowRuler4), module, BigButtonSeq2::DEL_PARAM, mode));	
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerCenter + clearAndDelButtonOffsetX, rowRuler4 + knobCVjackOffsetY), true, module, BigButtonSeq2::DEL_INPUT, mode));
		// Reset button and jack
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colRulerT0, rowRuler5), module, BigButtonSeq2::RESET_PARAM, mode));	
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerT0, rowRuler5 + knobCVjackOffsetY), true, module, BigButtonSeq2::RESET_INPUT, mode));
		// Fill button and jack
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colRulerT5, rowRuler5), module, BigButtonSeq2::FILL_PARAM,  mode));	
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerT5, rowRuler5 + knobCVjackOffsetY), true, module, BigButtonSeq2::FILL_INPUT, mode));

		// And now time for... BIG BUTTON!
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(colRulerCenter, rowRuler5 - 11), module, BigButtonSeq2::BIG_LIGHT));
		addParam(createLightParamCentered<LEDLightBezelBig>(VecPx(colRulerCenter, rowRuler5 + 26), module, BigButtonSeq2::BIG_PARAM, BigButtonSeq2::BIGC_LIGHT));
		// Big CV input
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerCenter - clearAndDelButtonOffsetX, rowRuler5 + knobCVjackOffsetY), true, module, BigButtonSeq2::BIG_INPUT, mode));
		// Big snap
		addParam(createParamCentered<LEDButton>(VecPx(colRulerCenter + clearAndDelButtonOffsetX, rowRuler5 + knobCVjackOffsetY), module, BigButtonSeq2::QUANTIZEBIG_PARAM));
		addChild(createLightCentered<MediumLight<GreenLightIM>>(VecPx(colRulerCenter + clearAndDelButtonOffsetX, rowRuler5 + knobCVjackOffsetY), module, BigButtonSeq2::QUANTIZEBIG_LIGHT));

		// Clear button and jack
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colRulerT6, rowRuler5), module, BigButtonSeq2::CLEAR_PARAM, mode));	
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerT6, rowRuler5 + knobCVjackOffsetY), true, module, BigButtonSeq2::CLEAR_INPUT, mode));

		
		// LEDs
		static const int rowRuler10 = 288;
		static const int ledOffsetY = -28;
		static const int gateOffsetY = 42;
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colRulerT0, rowRuler10 + ledOffsetY), module, BigButtonSeq2::CHAN_LIGHTS + 0));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colRulerT1, rowRuler10 + ledOffsetY), module, BigButtonSeq2::CHAN_LIGHTS + 2));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colRulerT2, rowRuler10 + ledOffsetY), module, BigButtonSeq2::CHAN_LIGHTS + 4));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colRulerT3, rowRuler10 + ledOffsetY), module, BigButtonSeq2::CHAN_LIGHTS + 6));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colRulerT4, rowRuler10 + ledOffsetY), module, BigButtonSeq2::CHAN_LIGHTS + 8));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colRulerT5, rowRuler10 + ledOffsetY), module, BigButtonSeq2::CHAN_LIGHTS + 10));
		// CV Outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT0, rowRuler10), false, module, BigButtonSeq2::CV_OUTPUTS + 0, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT1, rowRuler10), false, module, BigButtonSeq2::CV_OUTPUTS + 1, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT2, rowRuler10), false, module, BigButtonSeq2::CV_OUTPUTS + 2, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT3, rowRuler10), false, module, BigButtonSeq2::CV_OUTPUTS + 3, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT4, rowRuler10), false, module, BigButtonSeq2::CV_OUTPUTS + 4, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT5, rowRuler10), false, module, BigButtonSeq2::CV_OUTPUTS + 5, mode));
		// Gate outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT0, rowRuler10 + gateOffsetY), false, module, BigButtonSeq2::CHAN_OUTPUTS + 0, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT1, rowRuler10 + gateOffsetY), false, module, BigButtonSeq2::CHAN_OUTPUTS + 1, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT2, rowRuler10 + gateOffsetY), false, module, BigButtonSeq2::CHAN_OUTPUTS + 2, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT3, rowRuler10 + gateOffsetY), false, module, BigButtonSeq2::CHAN_OUTPUTS + 3, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT4, rowRuler10 + gateOffsetY), false, module, BigButtonSeq2::CHAN_OUTPUTS + 4, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colRulerT5, rowRuler10 + gateOffsetY), false, module, BigButtonSeq2::CHAN_OUTPUTS + 5, mode));
		
		// S&H LED button
		addParam(createParamCentered<LEDButton>(VecPx(colRulerT6 + 2, rowRuler10 + gateOffsetY / 2), module, BigButtonSeq2::SAMPLEHOLD_PARAM));
		addChild(createLightCentered<MediumLight<GreenLightIM>>(VecPx(colRulerT6 + 2, rowRuler10 + gateOffsetY / 2), module, BigButtonSeq2::SAMPLEHOLD_LIGHT));
	}
};

Model *modelBigButtonSeq2 = createModel<BigButtonSeq2, BigButtonSeq2Widget>("Big-Button-Seq2");
