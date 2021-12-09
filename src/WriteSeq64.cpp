//***********************************************************************************************
//Four channel 64-step writable sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "WriteSeqUtil.hpp"


struct WriteSeq64 : Module {
	enum ParamIds {
		SHARP_PARAM,
		QUANTIZE_PARAM,
		GATE_PARAM,
		CHANNEL_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RUN_PARAM,
		WRITE_PARAM,
		STEPL_PARAM,
		MONITOR_PARAM,
		STEPR_PARAM,
		STEPS_PARAM,
		STEP_PARAM,
		AUTOSTEP_PARAM,
		RESET_PARAM,
		PASTESYNC_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CHANNEL_INPUT,// no longer used
		CV_INPUT,
		GATE_INPUT,
		WRITE_INPUT,
		STEPL_INPUT,
		STEPR_INPUT,
		CLOCK12_INPUT,
		CLOCK34_INPUT,
		RESET_INPUT,
		RUNCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(GATE_LIGHT, 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(WRITE_LIGHT, 2),// room for GreenRed
		PENDING_LIGHT,
		NUM_LIGHTS
	};

	// Constants
	// none

	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool running;
	int indexStep[5];// [0;63] each
	int indexSteps[5];// [1;64] each
	float cv[5][64];
	int gates[5][64];
	bool resetOnRun;
	int stepRotates;

	// No need to save, with reset
	long clockIgnoreOnReset;
	float cvCPbuffer[64];// copy paste buffer for CVs
	int gateCPbuffer[64];// copy paste buffer for gates
	int stepsCPbuffer;
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	int pendingPaste;// 0 = nothing to paste, 1 = paste on clk, 2 = paste on seq, destination channel in next msbits
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate

	// No need to save, no reset
	RefreshCounter refresh;	
	float editingGateCV;// no need to initialize, this goes with editingGate (output this only when editingGate > 0)
	int stepKnob = 0;
	int stepsKnob = 0;
	float resetLight = 0.0f;
	Trigger clock12Trigger;
	Trigger clock34Trigger;
	Trigger resetTrigger;
	Trigger runningTrigger;
	Trigger stepLTrigger;
	Trigger stepRTrigger;
	Trigger copyTrigger;
	Trigger pasteTrigger;
	Trigger writeTrigger;
	Trigger gateTrigger;

	
	inline int calcChan() {
		return clamp((int)(params[CHANNEL_PARAM].getValue() + 0.5f), 0, 4);
	}


	WriteSeq64() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configSwitch(SHARP_PARAM, 0.0f, 2.0f, 1.0f, "Display mode", {"Volts", "Sharp", "Flat"});// 0 is top position
		configParam(CHANNEL_PARAM, 0.0f, 4.0f, 0.0f, "Channel", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset
		configParam(STEP_PARAM, -INFINITY, INFINITY, 0.0f, "Step");		
		configParam(GATE_PARAM, 0.0f, 1.0f, 0.0f, "Gate");
		configSwitch(AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f, "Autostep when write", {"No", "Yes"});
		configSwitch(QUANTIZE_PARAM, 0.0f, 1.0f, 1.0f, "Quantize", {"No", "Yes"});
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
		configParam(STEPS_PARAM, -INFINITY, INFINITY, 0.0f, "Number of steps");		
		configParam(COPY_PARAM, 0.0f, 1.0f, 0.0f, "Copy");
		configParam(PASTE_PARAM, 0.0f, 1.0f, 0.0f, "Paste");
		configSwitch(PASTESYNC_PARAM, 0.0f, 2.0f, 0.0f, "Paste sync", {"Real-time (immediate)", "On next clock", "On end of sequence"});
		configParam(STEPL_PARAM, 0.0f, 1.0f, 0.0f, "Step left");
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(STEPR_PARAM, 0.0f, 1.0f, 0.0f, "Step right");	
		configParam(WRITE_PARAM, 0.0f, 1.0f, 0.0f, "Write");
		configSwitch(MONITOR_PARAM, 0.0f, 1.0f, 1.0f, "Monitor", {"CV input", "Sequencer"});
		
		getParamQuantity(SHARP_PARAM)->randomizeEnabled = false;		
		getParamQuantity(PASTESYNC_PARAM)->randomizeEnabled = false;		
		getParamQuantity(MONITOR_PARAM)->randomizeEnabled = false;		
		getParamQuantity(AUTOSTEP_PARAM)->randomizeEnabled = false;		
		getParamQuantity(QUANTIZE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(CHANNEL_PARAM)->randomizeEnabled = false;		

		configInput(CHANNEL_INPUT, "Unused");
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(WRITE_INPUT, "Write");
		configInput(STEPL_INPUT, "Step left");
		configInput(STEPR_INPUT, "Step right");
		configInput(CLOCK12_INPUT, "Clock 1 and 2");
		configInput(CLOCK34_INPUT, "Clock 3 and 4");
		configInput(RESET_INPUT, "Reset");
		configInput(RUNCV_INPUT, "Run");

		for (int i = 0; i < 4; i++) {
			configOutput(CV_OUTPUTS + i, string::f("Channel %i CV", i + 1));
			configOutput(GATE_OUTPUTS + i, string::f("Channel %i Gate", i + 1));
		}

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	
	void onReset() override {
		running = true;
		for (int c = 0; c < 5; c++) {
			indexStep[c] = 0;
			indexSteps[c] = 64;
			for (int s = 0; s < 64; s++) {
				cv[c][s] = 0.0f;
				gates[c][s] = 1;
			}
		}
		resetOnRun = false;
		stepRotates = 0;
		resetNonJson();
	}
	void resetNonJson() {
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * APP->engine->getSampleRate());
		for (int s = 0; s < 64; s++) {
			cvCPbuffer[s] = 0.0f;
			gateCPbuffer[s] = 1;
		}
		stepsCPbuffer = 64;
		infoCopyPaste = 0l;
		pendingPaste = 0;
		editingGate = 0ul;
	}

	
	void onRandomize() override {
		int indexChannel = calcChan();
		for (int s = 0; s < 64; s++) {
			cv[indexChannel][s] = quantize((random::uniform() * 5.0f) - 2.0f, params[QUANTIZE_PARAM].getValue() > 0.5f);
			gates[indexChannel][s] = (random::uniform() > 0.5f) ? 1 : 0;
		}		
		pendingPaste = 0;
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// indexStep
		json_t *indexStepJ = json_array();
		for (int c = 0; c < 5; c++)
			json_array_insert_new(indexStepJ, c, json_integer(indexStep[c]));
		json_object_set_new(rootJ, "indexStep", indexStepJ);

		// indexSteps 
		json_t *indexStepsJ = json_array();
		for (int c = 0; c < 5; c++)
			json_array_insert_new(indexStepsJ, c, json_integer(indexSteps[c]));
		json_object_set_new(rootJ, "indexSteps", indexStepsJ);

		// CV
		json_t *cvJ = json_array();
		for (int c = 0; c < 5; c++)
			for (int s = 0; s < 64; s++) {
				json_array_insert_new(cvJ, s + (c<<6), json_real(cv[c][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// gates
		json_t *gatesJ = json_array();
		for (int c = 0; c < 5; c++)
			for (int s = 0; s < 64; s++) {
				json_array_insert_new(gatesJ, s + (c<<6), json_integer(gates[c][s]));
			}
		json_object_set_new(rootJ, "gates", gatesJ);

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// stepRotates
		json_object_set_new(rootJ, "stepRotates", json_integer(stepRotates));

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
		
		// indexStep
		json_t *indexStepJ = json_object_get(rootJ, "indexStep");
		if (indexStepJ)
			for (int c = 0; c < 5; c++)
			{
				json_t *indexStepArrayJ = json_array_get(indexStepJ, c);
				if (indexStepArrayJ)
					indexStep[c] = json_integer_value(indexStepArrayJ);
			}

		// indexSteps
		json_t *indexStepsJ = json_object_get(rootJ, "indexSteps");
		if (indexStepsJ)
			for (int c = 0; c < 5; c++)
			{
				json_t *indexStepsArrayJ = json_array_get(indexStepsJ, c);
				if (indexStepsArrayJ)
					indexSteps[c] = json_integer_value(indexStepsArrayJ);
			}

		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int c = 0; c < 5; c++)
				for (int i = 0; i < 64; i++) {
					json_t *cvArrayJ = json_array_get(cvJ, i + (c<<6));
					if (cvArrayJ)
						cv[c][i] = json_number_value(cvArrayJ);
				}
		}
		
		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int c = 0; c < 5; c++)
				for (int i = 0; i < 64; i++) {
					json_t *gateJ = json_array_get(gatesJ, i + (c<<6));
					if (gateJ)
						gates[c][i] = json_integer_value(gateJ);
				}
		}
		
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// stepRotates
		json_t *stepRotatesJ = json_object_get(rootJ, "stepRotates");
		if (stepRotatesJ)
			stepRotates = json_integer_value(stepRotatesJ);

		resetNonJson();
	}
	
	
	std::vector<IoNote>* fillIoNotes(int *seqLenPtr) {// caller must delete return array
		int indexChannel = calcChan();
		int seqLen = indexSteps[indexChannel];
		std::vector<IoNote>* ioNotes = new std::vector<IoNote>;
		
		// populate ioNotes array
		for (int i = 0; i < seqLen; ) {
			if (gates[indexChannel][i] == 0) {
				i++;
				continue;
			}
			IoNote ioNote;
			ioNote.start = (float)i;
			int j = i + 1;
			if (gates[indexChannel][i] == 2) {
				// if full gate, check for consecutive full gates with same cv in order to make one long note
				while (j < seqLen && cv[indexChannel][i] == cv[indexChannel][j] && gates[indexChannel][j] == 2) {j++;}
				ioNote.length = (float)(j - i);
			}
			else {
				ioNote.length = 0.5f;
			}
			ioNote.pitch = cv[indexChannel][i];
			ioNote.vel = -1.0f;// no concept of velocity in WriteSequencers
			ioNote.prob = -1.0f;// no concept of probability in WriteSequencers	
			ioNotes->push_back(ioNote);
			i = j;
		}

		// return values 
		*seqLenPtr = seqLen;
		return ioNotes;
	}


	void emptyIoNotes(std::vector<IoNote>* ioNotes, int seqLen) {
		if (seqLen < 1) {
			return;
		}
		int indexChannel = calcChan();
		indexSteps[indexChannel] = clamp(seqLen, 1, 64);// clamp not really needed here, < 1 tested above, 64 max done elsewhere
		
		// clear everything first
		for (int i = 0; i < seqLen; i++) {
			cv[indexChannel][i] = 0.0f;
			gates[indexChannel][i] = 0;
		}

		// Scan notes and write into steps
		for (unsigned int ni = 0; ni < ioNotes->size(); ni++) {
			int si = std::max((int)0, (int)(*ioNotes)[ni].start);
			if (si >= 64) continue;
			float noteLen = (*ioNotes)[ni].length;
			int numFull = (int)std::floor(noteLen);// number of steps with full gate
			int numNormal = (std::floor(noteLen) == noteLen ? 0 : 1);
			for (; numFull > 0 && si < 64; si++, numFull--) {
				cv[indexChannel][si] = (*ioNotes)[ni].pitch;
				gates[indexChannel][si] = 2;// full gate
			}
			if (numNormal != 0 && si < 64) {
				cv[indexChannel][si] = (*ioNotes)[ni].pitch;
				gates[indexChannel][si] = 1;// normal gate
			}
		}
	}	


	void process(const ProcessArgs &args) override {
		static const float copyPasteInfoTime = 0.7f;// seconds
		static const float gateTime = 0.15f;// seconds
		
		
		//********** Buttons, knobs, switches and inputs **********
		int indexChannel = calcChan();
		bool canEdit = !running || (indexChannel == 4);
		
		// Run state button
		if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage())) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			//pendingPaste = 0;// no pending pastes across run state toggles
			if (running) {
				if (resetOnRun) {
					clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * args.sampleRate);
					for (int c = 0; c < 5; c++) 
						indexStep[c] = 0;
				}
			}
		}
	
		if (refresh.processInputs()) {
			// Copy button
			if (copyTrigger.process(params[COPY_PARAM].getValue())) {
				infoCopyPaste = (long) (copyPasteInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
				for (int s = 0; s < 64; s++) {
					cvCPbuffer[s] = cv[indexChannel][s];
					gateCPbuffer[s] = gates[indexChannel][s];
				}
				stepsCPbuffer = indexSteps[indexChannel];
				pendingPaste = 0;
			}
			// Paste button
			if (pasteTrigger.process(params[PASTE_PARAM].getValue())) {
				if (params[PASTESYNC_PARAM].getValue() < 0.5f || indexChannel == 4) {
					// Paste realtime, no pending to schedule
					infoCopyPaste = (long) (-1 * copyPasteInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					for (int s = 0; s < 64; s++) {
						cv[indexChannel][s] = cvCPbuffer[s];
						gates[indexChannel][s] = gateCPbuffer[s];
					}
					indexSteps[indexChannel] = stepsCPbuffer;
					if (indexStep[indexChannel] >= stepsCPbuffer)
						indexStep[indexChannel] = stepsCPbuffer - 1;
					pendingPaste = 0;
				}
				else {
					pendingPaste = params[PASTESYNC_PARAM].getValue() > 1.5f ? 2 : 1;
					pendingPaste |= indexChannel<<2; // add paste destination channel into pendingPaste				
				}
			}
				
			// Gate button
			if (gateTrigger.process(params[GATE_PARAM].getValue())) {
				gates[indexChannel][indexStep[indexChannel]]++;
				if (gates[indexChannel][indexStep[indexChannel]] > 2)
					gates[indexChannel][indexStep[indexChannel]] = 0;
			}
			
			// Steps knob
			float stepsParamValue = params[STEPS_PARAM].getValue();
			int newStepsKnob = (int)std::round(stepsParamValue * 10.0f);
			if (stepsParamValue == 0.0f)// true when constructor or dataFromJson() occured
				stepsKnob = newStepsKnob;
			if (newStepsKnob != stepsKnob) {
				if (abs(newStepsKnob - stepsKnob) <= 3) // avoid discontinuous step (initialize for example)
					indexSteps[indexChannel] = clamp( indexSteps[indexChannel] + newStepsKnob - stepsKnob, 1, 64); 
				stepsKnob = newStepsKnob;
			}	
			// Step knob
			float stepParamValue = params[STEP_PARAM].getValue();
			int newStepKnob = (int)std::round(stepParamValue * 10.0f);
			if (stepParamValue == 0.0f)// true when constructor or dataFromJson() occured
				stepKnob = newStepKnob;
			if (newStepKnob != stepKnob) {
				if (canEdit && (abs(newStepKnob - stepKnob) <= 3) ) // avoid discontinuous step (initialize for example)
					indexStep[indexChannel] = moveIndex(indexStep[indexChannel], indexStep[indexChannel] + newStepKnob - stepKnob, indexSteps[indexChannel]);
				stepKnob = newStepKnob;// must do this step whether running or not
			}	
			// If steps knob goes down past step, step knob will not get triggered above, so reduce accordingly
			for (int c = 0; c < 5; c++)
				if (indexStep[c] >= indexSteps[c])
					indexStep[c] = indexSteps[c] - 1;
			
			// Write button and input (must be before StepL and StepR in case route gate simultaneously to Step R and Write for example)
			//  (write must be to correct step)
			if (writeTrigger.process(params[WRITE_PARAM].getValue() + inputs[WRITE_INPUT].getVoltage())) {
				if (canEdit) {		
					// CV
					cv[indexChannel][indexStep[indexChannel]] = quantize(inputs[CV_INPUT].getVoltage(), params[QUANTIZE_PARAM].getValue() > 0.5f);
					// Gate
					if (inputs[GATE_INPUT].isConnected())
						gates[indexChannel][indexStep[indexChannel]] = (inputs[GATE_INPUT].getVoltage() >= 1.0f) ? 1 : 0;
					// Editing gate
					editingGate = (unsigned long) (gateTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					editingGateCV = cv[indexChannel][indexStep[indexChannel]];
					// Autostep
					if (params[AUTOSTEP_PARAM].getValue() > 0.5f)
						indexStep[indexChannel] = moveIndex(indexStep[indexChannel], indexStep[indexChannel] + 1, indexSteps[indexChannel]);
				}
			}
			// Step L and R buttons
			int delta = 0;
			if (stepLTrigger.process(params[STEPL_PARAM].getValue() + inputs[STEPL_INPUT].getVoltage())) {
				delta = -1;
			}
			if (stepRTrigger.process(params[STEPR_PARAM].getValue() + inputs[STEPR_INPUT].getVoltage())) {
				delta = +1;
			}
			if (delta != 0 && canEdit) {		
				if (stepRotates == 0) {
					// step mode
					indexStep[indexChannel] = moveIndex(indexStep[indexChannel], indexStep[indexChannel] + delta, indexSteps[indexChannel]); 
					// Editing gate
					editingGate = (unsigned long) (gateTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					editingGateCV = cv[indexChannel][indexStep[indexChannel]];
				}
				else {
					// rotate mode
					rotateSeq(delta, indexSteps[indexChannel], cv[indexChannel], gates[indexChannel]);
				}
			}
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (running && clockIgnoreOnReset == 0l) {
			bool clk12step = clock12Trigger.process(inputs[CLOCK12_INPUT].getVoltage());
			bool clk34step = ((!inputs[CLOCK34_INPUT].isConnected()) && clk12step) || 
							  clock34Trigger.process(inputs[CLOCK34_INPUT].getVoltage());
			if (clk12step) {
				indexStep[0] = moveIndex(indexStep[0], indexStep[0] + 1, indexSteps[0]);
				indexStep[1] = moveIndex(indexStep[1], indexStep[1] + 1, indexSteps[1]);
			}
			if (clk34step) {
				indexStep[2] = moveIndex(indexStep[2], indexStep[2] + 1, indexSteps[2]);
				indexStep[3] = moveIndex(indexStep[3], indexStep[3] + 1, indexSteps[3]);
			}	

			// Pending paste on clock or end of seq
			if ( ((pendingPaste&0x3) == 1) || ((pendingPaste&0x3) == 2 && indexStep[indexChannel] == 0) ) {
				if ( (clk12step && (indexChannel == 0 || indexChannel == 1)) ||
					 (clk34step && (indexChannel == 2 || indexChannel == 3)) ) {
					infoCopyPaste = (long) (-1 * copyPasteInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					int pasteChannel = pendingPaste>>2;
					for (int s = 0; s < 64; s++) {
						cv[pasteChannel][s] = cvCPbuffer[s];
						gates[pasteChannel][s] = gateCPbuffer[s];
					}
					indexSteps[pasteChannel] = stepsCPbuffer;
					if (indexStep[pasteChannel] >= stepsCPbuffer)
						indexStep[pasteChannel] = stepsCPbuffer - 1;
					pendingPaste = 0;
				}
			}
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue())) {
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * args.sampleRate);
			for (int t = 0; t < 5; t++)
				indexStep[t] = 0;
			resetLight = 1.0f;
			pendingPaste = 0;
			clock12Trigger.reset();
			clock34Trigger.reset();
		}
		
		
		//********** Outputs and lights **********
		
		// CV and gate outputs (staging area not used)
		if (running) {
			bool clockHigh = false;
			bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
			for (int i = 0; i < 4; i++) {
				outputs[CV_OUTPUTS + i].setVoltage(cv[i][indexStep[i]]);
				clockHigh = i < 2 ? clock12Trigger.isHigh() : clock34Trigger.isHigh();
				outputs[GATE_OUTPUTS + i].setVoltage(( (((gates[i][indexStep[i]] == 1) && clockHigh) || gates[i][indexStep[i]] == 2) && !retriggingOnReset ) ? 10.0f : 0.0f);
			}
		}
		else {
			for (int i = 0; i < 4; i++) {
				// CV
				if (params[MONITOR_PARAM].getValue() > 0.5f) // if monitor switch is set to SEQ
					outputs[CV_OUTPUTS + i].setVoltage((editingGate > 0ul) ? editingGateCV : cv[i][indexStep[i]]);// each CV out monitors the current step CV of that channel
				else
					outputs[CV_OUTPUTS + i].setVoltage(quantize(inputs[CV_INPUT].getVoltage(), params[QUANTIZE_PARAM].getValue() > 0.5f));// all CV outs monitor the CV in (only current channel will have a gate though)
				
				// Gate
				outputs[GATE_OUTPUTS + i].setVoltage(((i == indexChannel) && (editingGate > 0ul)) ? 10.0f : 0.0f);
			}
		}
		
		// lights
		if (refresh.processLights()) {
			// Gate light
			float green = 0.0f;
			float red = 0.0f;
			if (gates[indexChannel][indexStep[indexChannel]] != 0) {
				if (gates[indexChannel][indexStep[indexChannel]] == 1) 	green = 1.0f;
				else {													green = 0.45f; red = 1.0f;}
			}	
			lights[GATE_LIGHT + 0].setBrightness(green);
			lights[GATE_LIGHT + 1].setBrightness(red);
			
			// Reset light
			lights[RESET_LIGHT].setSmoothBrightness(resetLight, args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));	
			resetLight = 0.0f;

			// Run light
			lights[RUN_LIGHT].setBrightness(running ? 1.0f : 0.0f);
			
			// Write allowed light
			lights[WRITE_LIGHT + 0].setBrightness(canEdit ? 1.0f : 0.0f);
			lights[WRITE_LIGHT + 1].setBrightness(canEdit ? 0.0f : 1.0f);
			
			// Pending paste light
			lights[PENDING_LIGHT].setBrightness(pendingPaste == 0 ? 0.0f : 1.0f);
			
			if (infoCopyPaste != 0l) {
				if (infoCopyPaste > 0l)
					infoCopyPaste --;
				if (infoCopyPaste < 0l)
					infoCopyPaste ++;
			}
			if (editingGate > 0ul) {
				editingGate--;
				if (editingGate == 0ul) {
					if (stepRTrigger.isHigh() || stepLTrigger.isHigh() || writeTrigger.isHigh()) {
						editingGate = 1ul;
					}
				}
			}
		}// lightRefreshCounter
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}
};


struct WriteSeq64Widget : ModuleWidget {
	struct NoteDisplayWidget : TransparentWidget {
		WriteSeq64 *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char text[7];

		NoteDisplayWidget() {
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void cvToStr(void) {
			if (module == NULL) {
				snprintf(text, 7, " C4");
			} 
			else {
				int indexChannel = module->calcChan();
				float cvVal = module->cv[indexChannel][module->indexStep[indexChannel]];
				if (module->infoCopyPaste != 0l) {
					if (module->infoCopyPaste > 0l) {// if copy then display "Copy"
						snprintf(text, 7, "COPY");
					}
					else {// paste then display "Paste"
						snprintf(text, 7, "PASTE");
					}
				}
				else {			
					if (module->params[WriteSeq64::SHARP_PARAM].getValue() > 0.5f) {// show notes
						text[0] = ' ';
						printNote(cvVal, &text[1], module->params[WriteSeq64::SHARP_PARAM].getValue() < 1.5f);
					}
					else  {// show volts
						float cvValPrint = std::fabs(cvVal);
						cvValPrint = (cvValPrint > 9.999f) ? 9.999f : cvValPrint;
						snprintf(text, 7, " %4.3f", cvValPrint);// Four-wide, three positions after the decimal, left-justified
						text[0] = (cvVal<0.0f) ? '-' : ' ';
						text[2] = ',';
					}
				}
			}
		}

		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, 18);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, -1.5);

				Vec textPos = VecPx(6, 24);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~~~~~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				cvToStr();
				nvgText(args.vg, textPos.x, textPos.y, text, NULL);
			}
		}
	};


	struct StepsDisplayWidget : TransparentWidget {
		WriteSeq64 *module;
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
				nvgText(args.vg, textPos.x, textPos.y, "~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				char displayStr[3];
				unsigned int numSteps = (module ? (unsigned)(module->indexSteps[module->calcChan()]) : 64);
				snprintf(displayStr, 3, "%2u", numSteps);
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};
	
	
	struct StepDisplayWidget : TransparentWidget {
		WriteSeq64 *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		
		StepDisplayWidget() {
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
				nvgText(args.vg, textPos.x, textPos.y, "~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				char displayStr[3];
				unsigned int stepNum = (module ? (unsigned) module->indexStep[module->calcChan()] : 0);
				snprintf(displayStr, 3, "%2u", stepNum + 1);
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};
	
	
	struct ChannelDisplayWidget : TransparentWidget {
		WriteSeq64 *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		
		ChannelDisplayWidget() {
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
				char chanNum = (module ? module->calcChan() : 0);
				displayStr[0] = 0x30 + (char) (chanNum + 1);
				displayStr[1] = 0;
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};

		
	struct InteropSeqItem : MenuItem {
		struct InteropCopySeqItem : MenuItem {
			WriteSeq64 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				std::vector<IoNote>* ioNotes = module->fillIoNotes(&seqLen);
				interopCopySequenceNotes(seqLen, ioNotes);
				delete ioNotes;
			}
		};
		struct InteropPasteSeqItem : MenuItem {
			WriteSeq64 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				std::vector<IoNote>* ioNotes = interopPasteSequenceNotes(64, &seqLen);
				if (ioNotes != nullptr) {
					module->emptyIoNotes(ioNotes, seqLen);
					delete ioNotes;
				}
			}
		};
		WriteSeq64 *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			InteropCopySeqItem *interopCopySeqItem = createMenuItem<InteropCopySeqItem>(portableSequenceCopyID, "");
			interopCopySeqItem->module = module;
			interopCopySeqItem->disabled = disabled;
			menu->addChild(interopCopySeqItem);		
			
			InteropPasteSeqItem *interopPasteSeqItem = createMenuItem<InteropPasteSeqItem>(portableSequencePasteID, "");
			interopPasteSeqItem->module = module;
			interopPasteSeqItem->disabled = disabled;
			menu->addChild(interopPasteSeqItem);		

			return menu;
		}
	};	

	void appendContextMenu(Menu *menu) override {
		WriteSeq64 *module = dynamic_cast<WriteSeq64*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());

		InteropSeqItem *interopSeqItem = createMenuItem<InteropSeqItem>(portableSequenceID, RIGHT_ARROW);
		interopSeqItem->module = module;
		menu->addChild(interopSeqItem);		

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createSubmenuItem("Arrow controls", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("Step", "",
				[=]() {return module->stepRotates == 0;},
				[=]() {module->stepRotates = 0x0;}
			));
			menu->addChild(createCheckMenuItem("Rotate", "",
				[=]() {return module->stepRotates != 0;},
				[=]() {module->stepRotates = 0x1;}
			));
		}));	

		menu->addChild(createBoolPtrMenuItem("Reset on run", "", &module->resetOnRun));
	}	
	
	
	WriteSeq64Widget(WriteSeq64 *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/WriteSeq64.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		
		// ****** Top portion ******
		
		static const int rowT0 = 68;
		static const int colT0 = 35;
		static const int colT1 = colT0 + 57;
		static const int colT2 = colT1 + 50;
		static const int colT3 = colT2 + 43;
		static const int colT4 = colT3 + 175;
		
		
		// Channel display
		ChannelDisplayWidget *channelTrack = new ChannelDisplayWidget();
		channelTrack->box.size = VecPx(24, 30);// 1 character
		channelTrack->box.pos = VecPx(colT0, rowT0).minus(channelTrack->box.size.div(2));
		channelTrack->module = module;
		addChild(channelTrack);
		svgPanel->fb->addChild(new DisplayBackground(channelTrack->box.pos, channelTrack->box.size, mode));
		// Step display
		StepDisplayWidget *displayStep = new StepDisplayWidget();
		displayStep->box.size = VecPx(40, 30);// 2 characters
		displayStep->box.pos = VecPx(colT1, rowT0).minus(displayStep->box.size.div(2));
		displayStep->module = module;
		addChild(displayStep);
		svgPanel->fb->addChild(new DisplayBackground(displayStep->box.pos, displayStep->box.size, mode));
		// Gate LED
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colT2, rowT0), module, WriteSeq64::GATE_LIGHT));
		// Note display
		NoteDisplayWidget *displayNote = new NoteDisplayWidget();
		displayNote->box.size = VecPx(98, 30);// 6 characters (ex.: "-1,234")
		displayNote->box.pos = VecPx(colT3 + 37, rowT0).minus(displayNote->box.size.div(2));
		displayNote->module = module;
		addChild(displayNote);
		svgPanel->fb->addChild(new DisplayBackground(displayNote->box.pos, displayNote->box.size, mode));
		// Volt/sharp/flat switch
		addParam(createDynamicSwitchCentered<IMSwitch3VInv>(VecPx(colT3 + 114, rowT0), module, WriteSeq64::SHARP_PARAM, mode, svgPanel));
		// Steps display
		StepsDisplayWidget *displaySteps = new StepsDisplayWidget();
		displaySteps->box.size = VecPx(40, 30);// 2 characters
		displaySteps->box.pos = VecPx(colT4, rowT0).minus(displaySteps->box.size.div(2));
		displaySteps->module = module;
		addChild(displaySteps);
		svgPanel->fb->addChild(new DisplayBackground(displaySteps->box.pos, displaySteps->box.size, mode));

		static const int rowT1 = 117;
		
		// Channel knob
		addParam(createDynamicParamCentered<IMFivePosMediumKnob>(VecPx(colT0, rowT1), module, WriteSeq64::CHANNEL_PARAM, mode));
		// Step knob
		addParam(createDynamicParamCentered<IMBigKnobInf>(VecPx(colT1, rowT1), module, WriteSeq64::STEP_PARAM, mode));		
		// Gate button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colT2, rowT1), module, WriteSeq64::GATE_PARAM, mode));
		// Autostep	
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(colT2 + 53, rowT1 + 6), module, WriteSeq64::AUTOSTEP_PARAM, mode, svgPanel));
		// Quantize switch
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(colT2 + 110, rowT1 + 6), module, WriteSeq64::QUANTIZE_PARAM, mode, svgPanel));
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(colT2 + 164, rowT1 + 6), module, WriteSeq64::RESET_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(colT2 + 164, rowT1 + 6), module, WriteSeq64::RESET_LIGHT));
		// Steps knob
		addParam(createDynamicParamCentered<IMBigKnobInf>(VecPx(colT4, rowT1), module, WriteSeq64::STEPS_PARAM, mode));		
	
		
		// ****** Bottom portion ******
		
		// Column rulers (horizontal positions)
		static const int col0 = 37;
		static const int colStep = 69;
		static const int col1 = col0 + colStep;
		static const int col2 = col1 + colStep;
		static const int col3 = col2 + colStep;
		static const int col4 = col3 + colStep;
		static const int col5 = col4 + colStep - 15;
		
		// Row rulers (vertical positions)
		static const int row0 = 184;
		static const int rowStep = 49;
		static const int row1 = row0 + rowStep;
		static const int row2 = row1 + rowStep;
		static const int row3 = row2 + rowStep;
		
		
		// Column 0 
		// Copy/paste switches
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(col0 - 15, row0), module, WriteSeq64::COPY_PARAM, mode));
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(col0 + 15, row0), module, WriteSeq64::PASTE_PARAM, mode));
		// Paste sync (and light)
		addParam(createDynamicSwitchCentered<IMSwitch3VInv>(VecPx(col0, row1), module, WriteSeq64::PASTESYNC_PARAM, mode, svgPanel));	
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(col0 + 32, row1 + 5), module, WriteSeq64::PENDING_LIGHT));
		// Gate input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col0, row2), true, module, WriteSeq64::GATE_INPUT, mode));				
		// Run CV input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col0, row3), true, module, WriteSeq64::RUNCV_INPUT, mode));
		
		
		// Column 1
		// Step L button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col1, row0), module, WriteSeq64::STEPL_PARAM, mode));
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(col1, row1), module, WriteSeq64::RUN_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(col1, row1), module, WriteSeq64::RUN_LIGHT));
		// CV input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col1, row2), true, module, WriteSeq64::CV_INPUT, mode));
		// Step L input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col1, row3), true, module, WriteSeq64::STEPL_INPUT, mode));
		
		
		// Column 2
		// Step R button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col2, row0), module, WriteSeq64::STEPR_PARAM, mode));	
		// Write button and light
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col2, row1), module, WriteSeq64::WRITE_PARAM, mode));
		addChild(createLightCentered<SmallLight<GreenRedLightIM>>(VecPx(col2 - 21, row1 - 21), module, WriteSeq64::WRITE_LIGHT));
		// Monitor
		addParam(createDynamicSwitchCentered<IMSwitch2H>(VecPx(col2, row2), module, WriteSeq64::MONITOR_PARAM, mode, svgPanel));
		// Step R input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col2, row3), true, module, WriteSeq64::STEPR_INPUT, mode));
		
		
		// Column 3
		// Clocks
		addInput(createDynamicPortCentered<IMPort>(VecPx(col3, row0), true, module, WriteSeq64::CLOCK12_INPUT, mode));		
		addInput(createDynamicPortCentered<IMPort>(VecPx(col3, row1), true, module, WriteSeq64::CLOCK34_INPUT, mode));		
		// Reset
		addInput(createDynamicPortCentered<IMPort>(VecPx(col3, row2), true, module, WriteSeq64::RESET_INPUT, mode));		
		// Write input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col3, row3), true, module, WriteSeq64::WRITE_INPUT, mode));
		
					
		// Column 4 (CVs)
		// Outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col4, row0), false, module, WriteSeq64::CV_OUTPUTS + 0, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col4, row1), false, module, WriteSeq64::CV_OUTPUTS + 1, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col4, row2), false, module, WriteSeq64::CV_OUTPUTS + 2, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col4, row3), false, module, WriteSeq64::CV_OUTPUTS + 3, mode));
		
		
		// Column 5 (Gates)
		// Gates
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row0), false, module, WriteSeq64::GATE_OUTPUTS + 0, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row1), false, module, WriteSeq64::GATE_OUTPUTS + 1, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row2), false, module, WriteSeq64::GATE_OUTPUTS + 2, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row3), false, module, WriteSeq64::GATE_OUTPUTS + 3, mode));
	}
};

Model *modelWriteSeq64 = createModel<WriteSeq64, WriteSeq64Widget>("Write-Seq-64");
