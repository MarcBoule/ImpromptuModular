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

		configParam(AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f, "Autostep");
		configParam(SHARP_PARAM, 0.0f, 1.0f, 1.0f, "Sharp notation");
		configParam(QUANTIZE_PARAM, 0.0f, 1.0f, 1.0f, "Quantize"); 
		
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
		configParam(PASTESYNC_PARAM, 0.0f, 2.0f, 0.0f, "Paste Sync");	
		configParam(STEPL_PARAM, 0.0f, 1.0f, 0.0f, "Step left");
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(STEPR_PARAM, 0.0f, 1.0f, 0.0f, "Step right");	
		configParam(WRITE_PARAM, 0.0f, 1.0f, 0.0f, "Write");
		configParam(STEPS_PARAM, 1.0f, 32.0f, 32.0f, "Number of steps");		
		configParam(MONITOR_PARAM, 0.0f, 1.0f, 1.0f, "Monitor");		
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
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

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

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

	
	void process(const ProcessArgs &args) override {
		static const float copyPasteInfoTime = 0.7f;// seconds
		static const float gateTime = 0.15f;// seconds
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		int numSteps = (int) clamp(std::round(params[STEPS_PARAM].getValue()), 1.0f, 32.0f);	
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
	SvgPanel* darkPanel;
	int notesPos[8]; // used for rendering notes in LCD_24, 8 gate and 8 step LEDs 

	struct NotesDisplayWidget : LightWidget {//TransparentWidget {
		WriteSeq32 *module;
		std::shared_ptr<Font> font;
		char text[4];
		int* notesPosLocal;

		NotesDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
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
				if ( ( (index&0x18) |index8) >= (int) clamp(std::round(module->params[WriteSeq32::STEPS_PARAM].getValue()), 1.0f, 32.0f) ) {
					text[0] = 0;
				}
				else {
					float cvVal = module->cv[module->indexChannel][index8|(index&0x18)];
					printNote(cvVal, text, module->params[WriteSeq32::SHARP_PARAM].getValue() > 0.5f);
				}
			}
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1.5);

			for (int i = 0; i < 8; i++) {
				Vec textPos = VecPx(notesPosLocal[i], 24);
				nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
				nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
				nvgFillColor(args.vg, textColor);
				cvToStr(i);
				nvgText(args.vg, textPos.x, textPos.y, text, NULL);
			}
		}
	};


	struct StepsDisplayWidget : LightWidget {//TransparentWidget {
		WriteSeq32 *module;
		std::shared_ptr<Font> font;
		
		StepsDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			//nvgTextLetterSpacing(args.vg, 2.5);

			Vec textPos = VecPx(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~~", NULL);
			nvgFillColor(args.vg, textColor);
			char displayStr[3];
			float valueKnob = (module ? module->params[WriteSeq32::STEPS_PARAM].getValue() : 32.0f);
			snprintf(displayStr, 3, "%2u", (unsigned) clamp(std::round(valueKnob), 1.0f, 32.0f) );
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};

	
	struct PanelThemeItem : MenuItem {
		WriteSeq32 *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	struct ResetOnRunItem : MenuItem {
		WriteSeq32 *module;
		void onAction(const event::Action &e) override {
			module->resetOnRun = !module->resetOnRun;
		}
	};
	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		WriteSeq32 *module = dynamic_cast<WriteSeq32*>(this->module);
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
		
		ArrowModeItem *arrowItem = createMenuItem<ArrowModeItem>("Arrow controls", RIGHT_ARROW);
		arrowItem->stepRotatesSrc = &(module->stepRotates);
		menu->addChild(arrowItem);

		ResetOnRunItem *rorItem = createMenuItem<ResetOnRunItem>("Reset on run", CHECKMARK(module->resetOnRun));
		rorItem->module = module;
		menu->addChild(rorItem);
	}	
	
	
	WriteSeq32Widget(WriteSeq32 *module) {
		setModule(module);
		
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/WriteSeq32.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/WriteSeq32_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), module ? &module->panelTheme : NULL));

		// Column rulers (horizontal positions)
		static const int columnRuler0 = 25;
		static const int columnnRulerStep = 69;
		static const int columnRuler1 = columnRuler0 + columnnRulerStep;
		static const int columnRuler2 = columnRuler1 + columnnRulerStep;
		static const int columnRuler3 = columnRuler2 + columnnRulerStep;
		static const int columnRuler4 = columnRuler3 + columnnRulerStep;
		static const int columnRuler5 = columnRuler4 + columnnRulerStep - 15;
		
		// Row rulers (vertical positions)
		static const int rowRuler0 = 172;
		static const int rowRulerStep = 49;
		static const int rowRuler1 = rowRuler0 + rowRulerStep;
		static const int rowRuler2 = rowRuler1 + rowRulerStep;
		static const int rowRuler3 = rowRuler2 + rowRulerStep + 4;


		// ****** Top portion ******
		
		static const int yRulerTopLEDs = 42;
		static const int yRulerTopSwitches = yRulerTopLEDs-11;
		
		// Autostep, sharp/flat and quantize switches
		// Autostep	
		addParam(createParam<CKSSNoRandom>(VecPx(columnRuler0+3+hOffsetCKSS, yRulerTopSwitches+vOffsetCKSS), module, WriteSeq32::AUTOSTEP_PARAM));
		// Sharp/flat
		addParam(createParam<CKSSNoRandom>(VecPx(columnRuler4+hOffsetCKSS, yRulerTopSwitches+vOffsetCKSS), module, WriteSeq32::SHARP_PARAM));
		// Quantize
		addParam(createParam<CKSSNoRandom>(VecPx(columnRuler5+hOffsetCKSS, yRulerTopSwitches+vOffsetCKSS), module, WriteSeq32::QUANTIZE_PARAM));

		// Window LED buttons
		static const float wLightsPosX = 140.0f;
		static const float wLightsIntX = 35.0f;
		for (int i = 0; i < 4; i++) {
			addParam(createParam<LEDButton>(VecPx(wLightsPosX + i * wLightsIntX, yRulerTopLEDs - 4.4f), module, WriteSeq32::WINDOW_PARAM + i));
			addChild(createLight<MediumLight<GreenLight>>(VecPx(wLightsPosX + 4.4f + i * wLightsIntX, yRulerTopLEDs), module, WriteSeq32::WINDOW_LIGHTS + i));
		}
		
		// Prepare 8 positions for step lights, gate lights and notes display
		notesPos[0] = 9;// this is also used to help line up LCD digits with LEDbuttons and avoid bad horizontal scaling with long str in display  
		for (int i = 1; i < 8; i++) {
			notesPos[i] = notesPos[i-1] + 46;
		}

		// Notes display
		NotesDisplayWidget *displayNotes = new NotesDisplayWidget();
		displayNotes->box.pos = VecPx(12, 76);
		displayNotes->box.size = VecPx(381, 30);
		displayNotes->module = module;
		displayNotes->notesPosLocal = notesPos;
		addChild(displayNotes);

		// Step LEDs (must be done after Notes display such that LED glow will overlay the notes display
		static const int yRulerStepLEDs = 65;
		for (int i = 0; i < 8; i++) {
			addChild(createLight<SmallLight<GreenLight>>(VecPx(notesPos[i]+25.0f+1.5f, yRulerStepLEDs), module, WriteSeq32::STEP_LIGHTS + i));
		}

		// Gates LED buttons
		static const int yRulerT2 = 119.0f;
		for (int i = 0; i < 8; i++) {
			addParam(createParam<LEDButton>(VecPx(notesPos[i]+25.0f-4.4f, yRulerT2-4.4f), module, WriteSeq32::GATE_PARAM + i));
			addChild(createLight<MediumLight<GreenRedLight>>(VecPx(notesPos[i]+25.0f, yRulerT2), module, WriteSeq32::GATE_LIGHTS + i * 2));
		}
		
		
		// ****** Bottom portion ******
		
		// Column 0
		// Channel button
		addParam(createDynamicParam<IMBigPushButton>(VecPx(columnRuler0+offsetCKD6b, rowRuler0+offsetCKD6b), module, WriteSeq32::CHANNEL_PARAM, module ? &module->panelTheme : NULL));
		// Channel LEDS
		static const int chanLEDoffsetX = 25;
		static const int chanLEDoffsetY[4] = {-20, -8, 4, 16};
		addChild(createLight<MediumLight<GreenLight>>(VecPx(columnRuler0 + chanLEDoffsetX + offsetMediumLight, rowRuler0 - 4 + chanLEDoffsetY[0] + offsetMediumLight), module, WriteSeq32::CHANNEL_LIGHTS + 0));
		addChild(createLight<MediumLight<YellowLight>>(VecPx(columnRuler0 + chanLEDoffsetX + offsetMediumLight, rowRuler0 - 4 + chanLEDoffsetY[1] + offsetMediumLight), module, WriteSeq32::CHANNEL_LIGHTS + 1));
		addChild(createLight<MediumLight<OrangeLight>>(VecPx(columnRuler0 + chanLEDoffsetX + offsetMediumLight, rowRuler0 - 4 + chanLEDoffsetY[2] + offsetMediumLight), module, WriteSeq32::CHANNEL_LIGHTS + 2));
		addChild(createLight<MediumLight<BlueLight>>(VecPx(columnRuler0 + chanLEDoffsetX + offsetMediumLight, rowRuler0 - 4 + chanLEDoffsetY[3] + offsetMediumLight), module, WriteSeq32::CHANNEL_LIGHTS + 3));
		// Copy/paste switches
		addParam(createDynamicParam<IMPushButton>(VecPx(columnRuler0-10, rowRuler1+offsetTL1105), module, WriteSeq32::COPY_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<IMPushButton>(VecPx(columnRuler0+20, rowRuler1+offsetTL1105), module, WriteSeq32::PASTE_PARAM, module ? &module->panelTheme : NULL));
		// Paste sync (and light)
		addParam(createParam<CKSSThreeInvNoRandom>(VecPx(columnRuler0+hOffsetCKSS, rowRuler2+vOffsetCKSSThree), module, WriteSeq32::PASTESYNC_PARAM));	
		addChild(createLight<SmallLight<RedLight>>(VecPx(columnRuler0 + 41, rowRuler2 + 14), module, WriteSeq32::PENDING_LIGHT));		
		// Run CV input
		addInput(createDynamicPort<IMPort>(VecPx(columnRuler0, rowRuler3), true, module, WriteSeq32::RUNCV_INPUT, module ? &module->panelTheme : NULL));
		
		
		// Column 1
		// Step L button
		addParam(createDynamicParam<IMBigPushButton>(VecPx(columnRuler1+offsetCKD6b, rowRuler0+offsetCKD6b), module, WriteSeq32::STEPL_PARAM, module ? &module->panelTheme : NULL));
		// Run LED bezel and light
		addParam(createParam<LEDBezel>(VecPx(columnRuler1+offsetLEDbezel, rowRuler1+offsetLEDbezel), module, WriteSeq32::RUN_PARAM));
		addChild(createLight<MuteLight<GreenLight>>(VecPx(columnRuler1+offsetLEDbezel+offsetLEDbezelLight, rowRuler1+offsetLEDbezel+offsetLEDbezelLight), module, WriteSeq32::RUN_LIGHT));
		// Gate input
		addInput(createDynamicPort<IMPort>(VecPx(columnRuler1, rowRuler2), true, module, WriteSeq32::GATE_INPUT, module ? &module->panelTheme : NULL));		
		// Step L input
		addInput(createDynamicPort<IMPort>(VecPx(columnRuler1, rowRuler3), true, module, WriteSeq32::STEPL_INPUT, module ? &module->panelTheme : NULL));
		
		
		// Column 2
		// Step R button
		addParam(createDynamicParam<IMBigPushButton>(VecPx(columnRuler2+offsetCKD6b, rowRuler0+offsetCKD6b), module, WriteSeq32::STEPR_PARAM, module ? &module->panelTheme : NULL));	
		// Write button and light
		addParam(createDynamicParam<IMBigPushButton>(VecPx(columnRuler2+offsetCKD6b, rowRuler1+offsetCKD6b), module, WriteSeq32::WRITE_PARAM, module ? &module->panelTheme : NULL));
		addChild(createLight<SmallLight<GreenRedLight>>(VecPx(columnRuler2 -12, rowRuler1 - 12), module, WriteSeq32::WRITE_LIGHT));
		// CV input
		addInput(createDynamicPort<IMPort>(VecPx(columnRuler2, rowRuler2), true, module, WriteSeq32::CV_INPUT, module ? &module->panelTheme : NULL));		
		// Step R input
		addInput(createDynamicPort<IMPort>(VecPx(columnRuler2, rowRuler3), true, module, WriteSeq32::STEPR_INPUT, module ? &module->panelTheme : NULL));
		
		
		// Column 3
		// Steps display
		StepsDisplayWidget *displaySteps = new StepsDisplayWidget();
		displaySteps->box.pos = VecPx(columnRuler3-7, rowRuler0+vOffsetDisplay);
		displaySteps->box.size = VecPx(40, 30);// 2 characters
		displaySteps->module = module;
		addChild(displaySteps);
		// Steps knob
		addParam(createDynamicParam<IMBigKnob<false, true>>(VecPx(columnRuler3+offsetIMBigKnob, rowRuler1+offsetIMBigKnob), module, WriteSeq32::STEPS_PARAM, module ? &module->panelTheme : NULL));		
		// Monitor
		addParam(createParam<CKSSHNoRandom>(VecPx(columnRuler3+hOffsetCKSSH, rowRuler2+vOffsetCKSSH), module, WriteSeq32::MONITOR_PARAM));		
		// Write input
		addInput(createDynamicPort<IMPort>(VecPx(columnRuler3, rowRuler3), true, module, WriteSeq32::WRITE_INPUT, module ? &module->panelTheme : NULL));
		
		
		// Column 4
		// Outputs
		addOutput(createDynamicPort<IMPort>(VecPx(columnRuler4, rowRuler0), false, module, WriteSeq32::CV_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(columnRuler4, rowRuler1), false, module, WriteSeq32::CV_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(columnRuler4, rowRuler2), false, module, WriteSeq32::CV_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		// Reset
		addInput(createDynamicPort<IMPort>(VecPx(columnRuler4, rowRuler3), true, module, WriteSeq32::RESET_INPUT, module ? &module->panelTheme : NULL));		

		
		// Column 5
		// Gates
		addOutput(createDynamicPort<IMPort>(VecPx(columnRuler5, rowRuler0), false, module, WriteSeq32::GATE_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(columnRuler5, rowRuler1), false, module, WriteSeq32::GATE_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(columnRuler5, rowRuler2), false, module, WriteSeq32::GATE_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		// Clock
		addInput(createDynamicPort<IMPort>(VecPx(columnRuler5, rowRuler3), true, module, WriteSeq32::CLOCK_INPUT, module ? &module->panelTheme : NULL));			
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((WriteSeq32*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((WriteSeq32*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelWriteSeq32 = createModel<WriteSeq32, WriteSeq32Widget>("Write-Seq-32");
