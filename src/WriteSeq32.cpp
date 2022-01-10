//***********************************************************************************************
//Three channel 32-step writable sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "WriteSeqUtil.hpp"


struct WriteSeq32 : Module {
	enum ParamIds {
		SHARP_PARAM,
		ENUMS(WINDOW_PARAM, 4),
		QUANTIZE_PARAM,
		ENUMS(GATE_PARAM, 8),
		CHANNEL_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RUN_PARAM,
		WRITE_PARAM,
		STEPL_PARAM,
		MONITOR_PARAM,
		STEPR_PARAM,
		STEPS_PARAM,
		AUTOSTEP_PARAM,
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
		CLOCK_INPUT,
		RESET_INPUT,
		RUNCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 3),
		ENUMS(GATE_OUTPUTS, 3),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(WINDOW_LIGHTS, 4),
		ENUMS(STEP_LIGHTS, 8),
		ENUMS(GATE_LIGHTS, 8 * 2),// room for GreenRed
		ENUMS(CHANNEL_LIGHTS, 4),
		RUN_LIGHT,
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
	int indexStep;
	int indexStepStage;
	int indexChannel;
	float cv[4][32];
	int gates[4][32];
	bool resetOnRun;
	int stepRotates;

	// No need to save, with reset
	long clockIgnoreOnReset;
	float cvCPbuffer[32];// copy paste buffer for CVs
	int gateCPbuffer[32];// copy paste buffer for gates
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	int pendingPaste;// 0 = nothing to paste, 1 = paste on clk, 2 = paste on seq, destination channel in next msbits
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate

	// No need to save, no reset
	RefreshCounter refresh;	
	float editingGateCV;// no need to initialize, this goes with editingGate (output this only when editingGate > 0)
	Trigger clockTrigger;
	Trigger resetTrigger;
	Trigger runningTrigger;
	Trigger channelTrigger;
	Trigger stepLTrigger;
	Trigger stepRTrigger;
	Trigger copyTrigger;
	Trigger pasteTrigger;
	Trigger writeTrigger;
	Trigger gateTriggers[8];
	Trigger windowTriggers[4];
	
	
	WriteSeq32() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configSwitch(AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f, "Autostep when write", {"No", "Yes"});
		configSwitch(SHARP_PARAM, 0.0f, 1.0f, 1.0f, "Display mode", {"Flat", "Sharp"});
		configSwitch(QUANTIZE_PARAM, 0.0f, 1.0f, 1.0f, "Quantize", {"No", "Yes"}); 
		
		char strBuf[32];
		for (int i = 0; i < 4; i++) {
			snprintf(strBuf, 32, "Window #%i of 4", i + 1);
			configParam(WINDOW_PARAM + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		for (int i = 0; i < 8; i++) {
			snprintf(strBuf, 32, "Gate #%i of 8", i + 1);
			configParam(GATE_PARAM + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		configParam(CHANNEL_PARAM, 0.0f, 1.0f, 0.0f, "Channel");
		configParam(COPY_PARAM, 0.0f, 1.0f, 0.0f, "Copy");
		configParam(PASTE_PARAM, 0.0f, 1.0f, 0.0f, "Paste");
		configSwitch(PASTESYNC_PARAM, 0.0f, 2.0f, 0.0f, "Paste sync", {"Real-time (immediate)", "On next clock", "On end of sequence"});
		configParam(STEPL_PARAM, 0.0f, 1.0f, 0.0f, "Step left");
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(STEPR_PARAM, 0.0f, 1.0f, 0.0f, "Step right");	
		configParam(WRITE_PARAM, 0.0f, 1.0f, 0.0f, "Write");
		configParam(STEPS_PARAM, 1.0f, 32.0f, 32.0f, "Number of steps");
		paramQuantities[STEPS_PARAM]->snapEnabled = true;		
		configSwitch(MONITOR_PARAM, 0.0f, 1.0f, 1.0f, "Monitor", {"CV input", "Sequencer"});		
		
		getParamQuantity(PASTESYNC_PARAM)->randomizeEnabled = false;		
		getParamQuantity(MONITOR_PARAM)->randomizeEnabled = false;		
		getParamQuantity(AUTOSTEP_PARAM)->randomizeEnabled = false;		
		getParamQuantity(SHARP_PARAM)->randomizeEnabled = false;		
		getParamQuantity(QUANTIZE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(STEPS_PARAM)->randomizeEnabled = false;		

		configInput(CHANNEL_INPUT, "Unused");
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(WRITE_INPUT, "Write");
		configInput(STEPL_INPUT, "Step left");
		configInput(STEPR_INPUT, "Step right");
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(RUNCV_INPUT, "Run");

		for (int i = 0; i < 3; i++) {
			configOutput(CV_OUTPUTS + i, string::f("Channel %i CV", i + 1));
			configOutput(GATE_OUTPUTS + i, string::f("Channel %i Gate", i + 1));
		}

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}
	

	void onReset() override {
		running = true;
		indexStep = 0;
		indexStepStage = 0;
		indexChannel = 0;
		for (int s = 0; s < 32; s++) {
			for (int c = 0; c < 4; c++) {
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
		for (int s = 0; s < 32; s++) {
			cvCPbuffer[s] = 0.0f;
			gateCPbuffer[s] = 1;
		}
		infoCopyPaste = 0l;
		pendingPaste = 0;
		editingGate = 0ul;
	}

	
	void onRandomize() override {
		for (int s = 0; s < 32; s++) {
			cv[indexChannel][s] = quantize((random::uniform() * 5.0f) - 2.0f, params[QUANTIZE_PARAM].getValue() > 0.5f);
			gates[indexChannel][s] = (random::uniform() > 0.5f) ? 1 : 0;
		}
		pendingPaste = 0;
	}
	
	int calcSteps() {
		return (int) clamp(std::round(params[STEPS_PARAM].getValue()), 1.0f, 32.0f);
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
		json_object_set_new(rootJ, "indexStep", json_integer(indexStep));

		// indexStepStage
		json_object_set_new(rootJ, "indexStepStage", json_integer(indexStepStage));

		// indexChannel
		json_object_set_new(rootJ, "indexChannel", json_integer(indexChannel));

		// CV
		json_t *cvJ = json_array();
		for (int c = 0; c < 4; c++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(cvJ, s + (c<<5), json_real(cv[c][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// Gates
		json_t *gatesJ = json_array();
		for (int c = 0; c < 4; c++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(gatesJ, s + (c<<5), json_integer(gates[c][s]));
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
			indexStep = json_integer_value(indexStepJ);

		// indexStepStage
		json_t *indexStepStageJ = json_object_get(rootJ, "indexStepStage");
		if (indexStepStageJ)
			indexStepStage = json_integer_value(indexStepStageJ);

		// indexChannel
		json_t *indexChannelJ = json_object_get(rootJ, "indexChannel");
		if (indexChannelJ)
			indexChannel = json_integer_value(indexChannelJ);

		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int c = 0; c < 4; c++)
				for (int s = 0; s < 32; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (c<<5));
					if (cvArrayJ)
						cv[c][s] = json_number_value(cvArrayJ);
				}
		}
		
		// Gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int c = 0; c < 4; c++)
				for (int s = 0; s < 32; s++) {
					json_t *gateJ = json_array_get(gatesJ, s + (c<<5));
					if (gateJ)
						gates[c][s] = json_integer_value(gateJ);
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
		int seqLen = calcSteps();
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
		params[STEPS_PARAM].setValue(clamp((float)seqLen, 1.0f, 32.0f));// clamp not really needed here, < 1 tested above, 32 max done elsewhere
		
		// clear everything first
		for (int i = 0; i < seqLen; i++) {
			cv[indexChannel][i] = 0.0f;
			gates[indexChannel][i] = 0;
		}

		// Scan notes and write into steps
		for (unsigned int ni = 0; ni < ioNotes->size(); ni++) {
			int si = std::max((int)0, (int)(*ioNotes)[ni].start);
			if (si >= 32) continue;
			float noteLen = (*ioNotes)[ni].length;
			int numFull = (int)std::floor(noteLen);// number of steps with full gate
			int numNormal = (std::floor(noteLen) == noteLen ? 0 : 1);
			for (; numFull > 0 && si < 32; si++, numFull--) {
				cv[indexChannel][si] = (*ioNotes)[ni].pitch;
				gates[indexChannel][si] = 2;// full gate
			}
			if (numNormal != 0 && si < 32) {
				cv[indexChannel][si] = (*ioNotes)[ni].pitch;
				gates[indexChannel][si] = 1;// normal gate
			}
		}
	}	
	
	
	void process(const ProcessArgs &args) override {
		static const float copyPasteInfoTime = 0.7f;// seconds
		static const float gateTime = 0.15f;// seconds
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		int numSteps = calcSteps();	
		bool canEdit = !running || (indexChannel == 3);
		
		// Run state button
		if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage())) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			//pendingPaste = 0;// no pending pastes across run state toggles
			if (running) {
				if (resetOnRun) {
					clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * args.sampleRate);
					indexStep = 0;
					indexStepStage = 0;
				}
			}
		}
		
		if (refresh.processInputs()) {
			// Copy button
			if (copyTrigger.process(params[COPY_PARAM].getValue())) {
				infoCopyPaste = (long) (copyPasteInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
				for (int s = 0; s < 32; s++) {
					cvCPbuffer[s] = cv[indexChannel][s];
					gateCPbuffer[s] = gates[indexChannel][s];
				}
				pendingPaste = 0;
			}
			// Paste button
			if (pasteTrigger.process(params[PASTE_PARAM].getValue())) {
				if (params[PASTESYNC_PARAM].getValue() < 0.5f || indexChannel == 3) {
					// Paste realtime, no pending to schedule
					infoCopyPaste = (long) (-1 * copyPasteInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					for (int s = 0; s < 32; s++) {
						cv[indexChannel][s] = cvCPbuffer[s];
						gates[indexChannel][s] = gateCPbuffer[s];
					}
					pendingPaste = 0;
				}
				else {
					pendingPaste = params[PASTESYNC_PARAM].getValue() > 1.5f ? 2 : 1;
					pendingPaste |= indexChannel<<2; // add paste destination channel into pendingPaste
				}
			}
			
			// Channel selection button
			if (channelTrigger.process(params[CHANNEL_PARAM].getValue())) {
				indexChannel++;
				if (indexChannel >= 4)
					indexChannel = 0;
			}
			
			// Gate buttons
			for (int index8 = 0, iGate = 0; index8 < 8; index8++) {
				if (gateTriggers[index8].process(params[GATE_PARAM + index8].getValue())) {
					iGate = ( (indexChannel == 3 ? indexStepStage : indexStep) & 0x18) | index8;
					if (iGate < numSteps) {// don't toggle gates beyond steps
						gates[indexChannel][iGate]++;
						if (gates[indexChannel][iGate] > 2)
							gates[indexChannel][iGate] = 0;
					}
				}
			}
				
			// Steps knob will not trigger anything in step(), and if user goes lower than current step, lower the index accordingly
			if (indexStep >= numSteps)
				indexStep = numSteps - 1;
			if (indexStepStage >= numSteps)
				indexStepStage = numSteps - 1;
			
			// Write button (must be before StepL and StepR in case route gate simultaneously to Step R and Write for example)
			//  (write must be to correct step)
			if (writeTrigger.process(params[WRITE_PARAM].getValue() + inputs[WRITE_INPUT].getVoltage())) {
				if (canEdit) {		
					int index = (indexChannel == 3 ? indexStepStage : indexStep);
					// CV
					cv[indexChannel][index] = quantize(inputs[CV_INPUT].getVoltage(), params[QUANTIZE_PARAM].getValue() > 0.5f);
					// Gate
					if (inputs[GATE_INPUT].isConnected())
						gates[indexChannel][index] = (inputs[GATE_INPUT].getVoltage() >= 1.0f) ? 1 : 0;
					// Editing gate
					editingGate = (unsigned long) (gateTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					editingGateCV = cv[indexChannel][index];
					// Autostep
					if (params[AUTOSTEP_PARAM].getValue() > 0.5f) {
						if (indexChannel == 3)
							indexStepStage = moveIndex(indexStepStage, indexStepStage + 1, numSteps);
						else 
							indexStep = moveIndex(indexStep, indexStep + 1, numSteps);
					}
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
					if (indexChannel == 3) {
						indexStepStage = moveIndex(indexStepStage, indexStepStage + delta, numSteps);
					}
					else {
						indexStep = moveIndex(indexStep, indexStep + delta, numSteps);
					}
					// Editing gate
					int index = (indexChannel == 3 ? indexStepStage : indexStep);
					editingGate = (unsigned long) (gateTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					editingGateCV = cv[indexChannel][index];
				}
				else {
					// rotate mode
					rotateSeq(delta, numSteps, cv[indexChannel], gates[indexChannel]);
				}
			}
			
			// Window buttons
			for (int i = 0; i < 4; i++) {
				if (windowTriggers[i].process(params[WINDOW_PARAM+i].getValue())) {
					if (canEdit) {		
						if (indexChannel == 3)
							indexStepStage = (i<<3) | (indexStepStage&0x7);
						else 
							indexStep = (i<<3) | (indexStep&0x7);
					}
				}
			}
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (running && clockIgnoreOnReset == 0l) {
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				indexStep = moveIndex(indexStep, indexStep + 1, numSteps);
				
				// Pending paste on clock or end of seq
				if ( ((pendingPaste&0x3) == 1) || ((pendingPaste&0x3) == 2 && indexStep == 0) ) {
					int pasteChannel = pendingPaste>>2;
					infoCopyPaste = (long) (-1 * copyPasteInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
					for (int s = 0; s < 32; s++) {
						cv[pasteChannel][s] = cvCPbuffer[s];
						gates[pasteChannel][s] = gateCPbuffer[s];
					}
					pendingPaste = 0;
				}
			}
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * args.sampleRate);
			indexStep = 0;
			indexStepStage = 0;	
			pendingPaste = 0;
			clockTrigger.reset();
		}		
		
		
		//********** Outputs and lights **********
		
		// CV and gate outputs (staging area not used)
		if (running) {
			bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
			for (int i = 0; i < 3; i++) {
				outputs[CV_OUTPUTS + i].setVoltage(cv[i][indexStep]);
				outputs[GATE_OUTPUTS + i].setVoltage(( (((gates[i][indexStep] == 1) && clockTrigger.isHigh()) || gates[i][indexStep] == 2) && !retriggingOnReset ) ? 10.0f : 0.0f);
			}
		}
		else {			
			for (int i = 0; i < 3; i++) {
				// CV
				if (params[MONITOR_PARAM].getValue() > 0.5f) // if monitor switch is set to SEQ
					outputs[CV_OUTPUTS + i].setVoltage((editingGate > 0ul) ? editingGateCV : cv[i][indexStep]);// each CV out monitors the current step CV of that channel
				else
					outputs[CV_OUTPUTS + i].setVoltage(quantize(inputs[CV_INPUT].getVoltage(), params[QUANTIZE_PARAM].getValue() > 0.5f));// all CV outs monitor the CV in (only current channel will have a gate though)
				
				// Gate
				outputs[GATE_OUTPUTS + i].setVoltage(((i == indexChannel) && (editingGate > 0ul)) ? 10.0f : 0.0f);
			}
		}

		// lights
		if (refresh.processLights()) {
			int index = (indexChannel == 3 ? indexStepStage : indexStep);
			// Window lights
			for (int i = 0; i < 4; i++) {
				lights[WINDOW_LIGHTS + i].setBrightness((i == (index >> 3)) ? 1.0f : 0.0f);
			}
			// Step and gate lights
			for (int index8 = 0, iGate = 0; index8 < 8; index8++) {
				lights[STEP_LIGHTS + index8].setBrightness((index8 == (index&0x7)) ? 1.0f : 0.0f);
				iGate = (index&0x18) | index8;
				float green = 0.0f;
				float red = 0.0f;
				if ( (iGate < numSteps) && (gates[indexChannel][iGate] != 0) ) {
					if (gates[indexChannel][iGate] == 1) 	green = 1.0f;
					else {									green = 0.45f; red = 1.0f;}
				}	
				lights[GATE_LIGHTS + index8 * 2 + 0].setBrightness(green);
				lights[GATE_LIGHTS + index8 * 2 + 1].setBrightness(red);
			}
				
			// Channel lights		
			lights[CHANNEL_LIGHTS + 0].setBrightness((indexChannel == 0) ? 1.0f : 0.0f);// green
			lights[CHANNEL_LIGHTS + 1].setBrightness((indexChannel == 1) ? 1.0f : 0.0f);// yellow
			lights[CHANNEL_LIGHTS + 2].setBrightness((indexChannel == 2) ? 1.0f : 0.0f);// orange
			lights[CHANNEL_LIGHTS + 3].setBrightness((indexChannel == 3) ? 1.0f : 0.0f);// blue
			
			// Run light
			lights[RUN_LIGHT].setBrightness(running ? 1.0f : 0.0f);
			
			// Write allowed light
			lights[WRITE_LIGHT + 0].setBrightness(canEdit ? 1.0f : 0.0f);
			lights[WRITE_LIGHT + 1].setBrightness(canEdit ? 0.0f : 1.0f);
			
			// Pending paste light
			lights[PENDING_LIGHT].setBrightness((pendingPaste == 0 ? 0.0f : 1.0f));
			
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


struct WriteSeq32Widget : ModuleWidget {
	int notesPos[8]; // used for rendering notes in LCD_24, 8 gate and 8 step LEDs 

	struct NotesDisplayWidget : TransparentWidget {
		WriteSeq32 *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char text[8];
		int* notesPosLocal;

		NotesDisplayWidget() {
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void cvToStr(int index8) {
			if (module == NULL) {
				snprintf(text, 4, "C4 ");
			}
			else if (module->infoCopyPaste != 0l) {
				if (index8 == 0) {
					if (module->infoCopyPaste > 0l)
						snprintf(text, 4, "COP");			
					else 
						snprintf(text, 4, "PAS");
				}
				else if (index8 == 1) {
					if (module->infoCopyPaste > 0l)
						snprintf(text, 4, "Y  ");			
					else 
						snprintf(text, 4, "TE ");
				}
				else {
					snprintf(text, 4, "   ");
				}
			}
			else {
				int index = (module->indexChannel == 3 ? module->indexStepStage : module->indexStep);
				if ( ( (index&0x18) |index8) >= module->calcSteps() ) {
					text[0] = 0;
				}
				else {
					float cvVal = module->cv[module->indexChannel][index8|(index&0x18)];
					printNoteOrig(cvVal, text, module->params[WriteSeq32::SHARP_PARAM].getValue() > 0.5f);
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

				for (int i = 0; i < 8; i++) {
					Vec textPos = VecPx(notesPosLocal[i], 24);
					nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
					nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
					
					nvgFillColor(args.vg, displayColOn);
					cvToStr(i);
					nvgText(args.vg, textPos.x, textPos.y, text, NULL);
				}
			}
		}
	};


	struct StepsDisplayWidget : TransparentWidget {
		WriteSeq32 *module;
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
				char displayStr[8];
				unsigned int numSteps = (module ? (unsigned int)(module->calcSteps()) : 32);
				snprintf(displayStr, 3, "%2u", (unsigned int)numSteps);
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};

	
	struct InteropSeqItem : MenuItem {
		struct InteropCopySeqItem : MenuItem {
			WriteSeq32 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				std::vector<IoNote>* ioNotes = module->fillIoNotes(&seqLen);
				interopCopySequenceNotes(seqLen, ioNotes);
				delete ioNotes;
			}
		};
		struct InteropPasteSeqItem : MenuItem {
			WriteSeq32 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				std::vector<IoNote>* ioNotes = interopPasteSequenceNotes(32, &seqLen);
				if (ioNotes != nullptr) {
					module->emptyIoNotes(ioNotes, seqLen);
					delete ioNotes;
				}
			}
		};
		WriteSeq32 *module;
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
		WriteSeq32 *module = dynamic_cast<WriteSeq32*>(this->module);
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
	
	

	
	WriteSeq32Widget(WriteSeq32 *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/WriteSeq32.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
			
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

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
		static const int row3 = row2 + rowStep + 4;
		


		// ****** Top portion ******
		
		static const float yTopLEDs = 46.4f;
		static const float yTopSwitches = yTopLEDs - 3.6f;
		
		// Autostep, sharp/flat and quantize switches
		// Autostep	
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(col0 + 3, yTopSwitches), module, WriteSeq32::AUTOSTEP_PARAM, mode, svgPanel));
		// Sharp/flat
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(col4, yTopSwitches), module, WriteSeq32::SHARP_PARAM, mode, svgPanel));
		// Quantize
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(col5, yTopSwitches), module, WriteSeq32::QUANTIZE_PARAM, mode, svgPanel));

		// Window LED buttons
		static const float wLightsPosX = 149.0f;
		static const float wLightsIntX = 35.0f;
		for (int i = 0; i < 4; i++) {
			addParam(createParamCentered<LEDButton>(VecPx(wLightsPosX + i * wLightsIntX, yTopLEDs), module, WriteSeq32::WINDOW_PARAM + i));

			addChild(createLightCentered<MediumLight<GreenLightIM>>(VecPx(wLightsPosX + i * wLightsIntX, yTopLEDs), module, WriteSeq32::WINDOW_LIGHTS + i));
		}
		
		// Prepare 8 positions for step lights, gate lights and notes display
		notesPos[0] = 9;// this is also used to help line up LCD digits with LEDbuttons and avoid bad horizontal scaling with long str in display  
		for (int i = 1; i < 8; i++) {
			notesPos[i] = notesPos[i-1] + 46;
		}

		// Notes display
		NotesDisplayWidget *displayNotes = new NotesDisplayWidget();
		displayNotes->box.size = VecPx(381, 30);
		displayNotes->box.pos = VecPx(202.5f, 91.0f).minus(displayNotes->box.size.div(2));
		displayNotes->module = module;
		displayNotes->notesPosLocal = notesPos;
		addChild(displayNotes);
		svgPanel->fb->addChild(new DisplayBackground(displayNotes->box.pos, displayNotes->box.size, mode));

		// Step LEDs (must be done after Notes display such that LED glow will overlay the notes display
		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<SmallLight<GreenLightIM>>(VecPx(notesPos[i] + 29.6f, 68.0f), module, WriteSeq32::STEP_LIGHTS + i));
		}

		// Gates LED buttons
		static const float yRulerT2 = 123.6f;
		for (int i = 0; i < 8; i++) {
			addParam(createParamCentered<LEDButton>(VecPx(notesPos[i] + 29.6f, yRulerT2), module, WriteSeq32::GATE_PARAM + i));
			addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(notesPos[i] + 29.6f, yRulerT2), module, WriteSeq32::GATE_LIGHTS + i * 2));
		}
		
		
		// ****** Bottom portion ******
		
		// Column 0
		// Channel button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col0, row0), module, WriteSeq32::CHANNEL_PARAM, mode));
		// Channel LEDS
		static const float chanLEDoffsetX = 25 - 12 + 4.6f + 9;
		static const int chanLEDoffsetY[4] = {-20, -8, 4, 16};
		addChild(createLightCentered<MediumLight<GreenLightIM>>(VecPx(col0 + chanLEDoffsetX, row0 - 2.4f + chanLEDoffsetY[0]), module, WriteSeq32::CHANNEL_LIGHTS + 0));
		addChild(createLightCentered<MediumLight<YellowLight>>(VecPx(col0 + chanLEDoffsetX, row0 - 2.4f + chanLEDoffsetY[1]), module, WriteSeq32::CHANNEL_LIGHTS + 1));
		addChild(createLightCentered<MediumLight<OrangeLightIM>>(VecPx(col0 + chanLEDoffsetX, row0 - 2.4f + chanLEDoffsetY[2]), module, WriteSeq32::CHANNEL_LIGHTS + 2));
		addChild(createLightCentered<MediumLight<BlueLight>>(VecPx(col0 + chanLEDoffsetX, row0 - 2.4f + chanLEDoffsetY[3]), module, WriteSeq32::CHANNEL_LIGHTS + 3));
		// Copy/paste switches
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(col0 - 15, row1), module, WriteSeq32::COPY_PARAM, mode));
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(col0 + 15, row1), module, WriteSeq32::PASTE_PARAM, mode));
		// Paste sync (and light)
		addParam(createDynamicSwitchCentered<IMSwitch3VInv>(VecPx(col0, row2), module, WriteSeq32::PASTESYNC_PARAM, mode, svgPanel));	
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(col0 + 32, row2 + 5), module, WriteSeq32::PENDING_LIGHT));		
		// Run CV input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col0, row3), true, module, WriteSeq32::RUNCV_INPUT, mode));
		
		
		// Column 1
		// Step L button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col1, row0), module, WriteSeq32::STEPL_PARAM, mode));
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(col1, row1), module, WriteSeq32::RUN_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(col1, row1), module, WriteSeq32::RUN_LIGHT));
		// Gate input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col1, row2), true, module, WriteSeq32::GATE_INPUT, mode));		
		// Step L input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col1, row3), true, module, WriteSeq32::STEPL_INPUT, mode));
		
		
		// Column 2
		// Step R button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col2, row0), module, WriteSeq32::STEPR_PARAM, mode));	
		// Write button and light
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(col2, row1), module, WriteSeq32::WRITE_PARAM, mode));
		addChild(createLightCentered<SmallLight<GreenRedLightIM>>(VecPx(col2 - 21, row1 - 21), module, WriteSeq32::WRITE_LIGHT));
		// CV input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col2, row2), true, module, WriteSeq32::CV_INPUT, mode));		
		// Step R input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col2, row3), true, module, WriteSeq32::STEPR_INPUT, mode));
		
		
		// Column 3
		// Steps display
		StepsDisplayWidget *displaySteps = new StepsDisplayWidget();
		displaySteps->box.size = VecPx(40, 30);// 2 characters
		displaySteps->box.pos = VecPx(col3, row0).minus(displaySteps->box.size.div(2));
		displaySteps->module = module;
		addChild(displaySteps);
		svgPanel->fb->addChild(new DisplayBackground(displaySteps->box.pos, displaySteps->box.size, mode));
		// Steps knob
		addParam(createDynamicParamCentered<IMBigKnob>(VecPx(col3, row1), module, WriteSeq32::STEPS_PARAM, mode));		
		// Monitor
		addParam(createDynamicSwitchCentered<IMSwitch2H>(VecPx(col3, row2), module, WriteSeq32::MONITOR_PARAM, mode, svgPanel));		
		// Write input
		addInput(createDynamicPortCentered<IMPort>(VecPx(col3, row3), true, module, WriteSeq32::WRITE_INPUT, mode));
		
		
		// Column 4
		// Outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col4, row0), false, module, WriteSeq32::CV_OUTPUTS + 0, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col4, row1), false, module, WriteSeq32::CV_OUTPUTS + 1, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col4, row2), false, module, WriteSeq32::CV_OUTPUTS + 2, mode));
		// Reset
		addInput(createDynamicPortCentered<IMPort>(VecPx(col4, row3), true, module, WriteSeq32::RESET_INPUT, mode));		

		
		// Column 5
		// Gates
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row0), false, module, WriteSeq32::GATE_OUTPUTS + 0, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row1), false, module, WriteSeq32::GATE_OUTPUTS + 1, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(col5, row2), false, module, WriteSeq32::GATE_OUTPUTS + 2, mode));
		// Clock
		addInput(createDynamicPortCentered<IMPort>(VecPx(col5, row3), true, module, WriteSeq32::CLOCK_INPUT, mode));		
	}
};

Model *modelWriteSeq32 = createModel<WriteSeq32, WriteSeq32Widget>("Write-Seq-32");
