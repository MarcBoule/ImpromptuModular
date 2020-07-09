//***********************************************************************************************
//Six channel 64-step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Based on the BigButton sequencer by Look-Mum-No-Computer
//https://www.youtube.com/watch?v=6ArDGcUqiWM
//https://www.lookmumnocomputer.com/projects/#/big-button/
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct BigButtonSeq : Module {
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
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CHAN_OUTPUTS, 6),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHAN_LIGHTS, 6 * 2),// Room for GreenRed
		BIG_LIGHT,
		BIGC_LIGHT,
		ENUMS(METRONOME_LIGHT, 2),// Room for GreenRed
		WRITEFILL_LIGHT,
		QUANTIZEBIG_LIGHT,
		NUM_LIGHTS
	};
	
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int indexStep;
	int bank[6];
	uint64_t gates[6][2];// channel , bank
	int metronomeDiv;
	bool writeFillsToMemory;
	bool quantizeBig;
	bool nextStepHits;
	
	// No need to save, with reset
	long clockIgnoreOnReset;
	double lastPeriod;//2.0 when not seen yet (init or stopped clock and went greater than 2s, which is max period supported for time-snap)
	double clockTime;//clock time counter (time since last clock)
	int pendingOp;// 0 means nothing pending, +1 means pending big button push, -1 means pending del
	bool fillPressed;
	
	// No need to save, no reset
	RefreshCounter refresh;	
	float bigLight = 0.0f;
	float metronomeLightStart = 0.0f;
	float metronomeLightDiv = 0.0f;
	int channel = 0;
	int length = 0; 
	Trigger clockTrigger;
	Trigger resetTrigger;
	Trigger bankTrigger;
	Trigger bigTrigger;
	Trigger writeFillTrigger;
	Trigger quantizeBigTrigger;
	dsp::PulseGenerator outPulse;
	dsp::PulseGenerator outLightPulse;
	dsp::PulseGenerator bigPulse;
	dsp::PulseGenerator bigLightPulse;

	
	inline void toggleGate(int _chan) {gates[_chan][bank[_chan]] ^= (((uint64_t)1) << (uint64_t)indexStep);}
	inline void setGate(int _chan, int _step) {gates[_chan][bank[_chan]] |= (((uint64_t)1) << (uint64_t)_step);}
	inline void clearGate(int _chan, int _step) {gates[_chan][bank[_chan]] &= ~(((uint64_t)1) << (uint64_t)_step);}
	inline bool getGate(int _chan, int _step) {return !((gates[_chan][bank[_chan]] & (((uint64_t)1) << (uint64_t)_step)) == 0);}
	inline int calcChan() {
		float chanInputValue = inputs[CHAN_INPUT].getVoltage() / 10.0f * (6.0f - 1.0f);
		return (int) clamp(std::round(params[CHAN_PARAM].getValue() + chanInputValue), 0.0f, (6.0f - 1.0f));		
	}
	
	BigButtonSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		configParam(CHAN_PARAM, 0.0f, 6.0f - 1.0f, 0.0f, "Channel", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset		
		configParam(LEN_PARAM, 0.0f, 64.0f - 1.0f, 32.0f - 1.0f, "Length", "", 0.0f, 1.0f, 1.0f);// diplay params are: base, mult, offset
		configParam(RND_PARAM, 0.0f, 100.0f, 0.0f, "Random");		
		configParam(BANK_PARAM, 0.0f, 1.0f, 0.0f, "Bank");	
		configParam(CLEAR_PARAM, 0.0f, 1.0f, 0.0f, "Clear");	
		configParam(DEL_PARAM, 0.0f, 1.0f, 0.0f, "Delete");	
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");	
		configParam(FILL_PARAM, 0.0f, 1.0f, 0.0f, "Fill");	
		configParam(BIG_PARAM, 0.0f, 1.0f, 0.0f, "Big button");
		configParam(QUANTIZEBIG_PARAM, 0.0f, 1.0f, 0.0f, "Quantize big button");
		configParam(WRITEFILL_PARAM, 0.0f, 1.0f, 0.0f, "Write fill");		
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		indexStep = 0;
		for (int c = 0; c < 6; c++) {
			bank[c] = 0;
			gates[c][0] = 0;
			gates[c][1] = 0;
		}
		metronomeDiv = 4;
		writeFillsToMemory = false;
		quantizeBig = true;
		nextStepHits = false;
		resetNonJson();
	}
	void resetNonJson() {
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * APP->engine->getSampleRate());
		lastPeriod = 2.0;
		clockTime = 0.0;
		pendingOp = 0;
		fillPressed = false;
	}


	void onRandomize() override {
		int chanRnd = calcChan();
		gates[chanRnd][bank[chanRnd]] = random::u64();
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// indexStep
		json_object_set_new(rootJ, "indexStep", json_integer(indexStep));

		// bank
		json_t *bankJ = json_array();
		for (int c = 0; c < 6; c++)
			json_array_insert_new(bankJ, c, json_integer(bank[c]));
		json_object_set_new(rootJ, "bank", bankJ);

		// gates
		json_t *gatesJ = json_array();
		for (int c = 0; c < 6; c++)
			for (int b = 0; b < 8; b++) {// bank to store is like to uint64_t to store, so go to 8
				// first to get stored is 16 lsbits of bank 0, then next 16 bits,... to 16 msbits of bank 1
				unsigned int intValue = (unsigned int) ( (uint64_t)0xFFFF & (gates[c][b/4] >> (uint64_t)(16 * (b % 4))) );
				json_array_insert_new(gatesJ, b + (c << 3) , json_integer(intValue));
			}
		json_object_set_new(rootJ, "gates", gatesJ);

		// metronomeDiv
		json_object_set_new(rootJ, "metronomeDiv", json_integer(metronomeDiv));

		// writeFillsToMemory
		json_object_set_new(rootJ, "writeFillsToMemory", json_boolean(writeFillsToMemory));

		// quantizeBig
		json_object_set_new(rootJ, "quantizeBig", json_boolean(quantizeBig));

		// nextStepHits
		json_object_set_new(rootJ, "nextStepHits", json_boolean(nextStepHits));

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

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

		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		uint64_t bank8ints[8] = {0,0,0,0,0,0,0,0};
		if (gatesJ) {
			for (int c = 0; c < 6; c++) {
				for (int b = 0; b < 8; b++) {// bank to store is like to uint64_t to store, so go to 8
					// first to get read is 16 lsbits of bank 0, then next 16 bits,... to 16 msbits of bank 1
					json_t *gateJ = json_array_get(gatesJ, b + (c << 3));
					if (gateJ)
						bank8ints[b] = (uint64_t) json_integer_value(gateJ);
				}
				gates[c][0] = bank8ints[0] | (bank8ints[1] << (uint64_t)16) | (bank8ints[2] << (uint64_t)32) | (bank8ints[3] << (uint64_t)48);
				gates[c][1] = bank8ints[4] | (bank8ints[5] << (uint64_t)16) | (bank8ints[6] << (uint64_t)32) | (bank8ints[7] << (uint64_t)48);
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

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		double sampleTime = 1.0 / args.sampleRate;
		static const float lightTime = 0.1f;
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		channel = calcChan();	
		length = (int) clamp(std::round( params[LEN_PARAM].getValue() + ( inputs[LEN_INPUT].isConnected() ? (inputs[LEN_INPUT].getVoltage() / 10.0f * (64.0f - 1.0f)) : 0.0f ) ), 0.0f, (64.0f - 1.0f)) + 1;	
		
		if (refresh.processInputs()) {
			// Big button
			if (bigTrigger.process(params[BIG_PARAM].getValue() + inputs[BIG_INPUT].getVoltage())) {
				bigLight = 1.0f;
				if (nextStepHits) {
					setGate(channel, (indexStep + 1) % length);// bank is global
				}
				else if (quantizeBig && (clockTime > (lastPeriod / 2.0)) && (clockTime <= (lastPeriod * 1.01))) {// allow for 1% clock jitter
					pendingOp = 1;
				}
				else {
					if (!getGate(channel, indexStep)) {
						setGate(channel, indexStep);// bank is global
						bigPulse.trigger(0.001f);
						bigLightPulse.trigger(lightTime);
					}
				}
			}

			// Bank button
			if (bankTrigger.process(params[BANK_PARAM].getValue() + inputs[BANK_INPUT].getVoltage()))
				bank[channel] = 1 - bank[channel];
			
			// Clear button
			if (params[CLEAR_PARAM].getValue() + inputs[CLEAR_INPUT].getVoltage() > 0.5f)
				gates[channel][bank[channel]] = 0;
			
			// Del button
			if (params[DEL_PARAM].getValue() + inputs[DEL_INPUT].getVoltage() > 0.5f) {
				if (nextStepHits) 
					clearGate(channel, (indexStep + 1) % length);// bank is global
				else if (quantizeBig && (clockTime > (lastPeriod / 2.0)) && (clockTime <= (lastPeriod * 1.01)))// allow for 1% clock jitter
					pendingOp = -1;// overrides the pending write if it exists
				else 
					clearGate(channel, indexStep);// bank is global
			}

			// Pending timeout (write/del current step)
			if (pendingOp != 0 && clockTime > (lastPeriod * 1.01) ) 
				performPending(channel, lightTime);

			// Write fill to memory
			if (writeFillTrigger.process(params[WRITEFILL_PARAM].getValue()))
				writeFillsToMemory = !writeFillsToMemory;

			// Quantize big button (aka snap)
			if (quantizeBigTrigger.process(params[QUANTIZEBIG_PARAM].getValue()))
				quantizeBig = !quantizeBig;
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (clockIgnoreOnReset == 0l) {
			if (clockTrigger.process(inputs[CLK_INPUT].getVoltage())) {
				if ((++indexStep) >= length) indexStep = 0;
				
				// Fill button
				fillPressed = (params[FILL_PARAM].getValue() + inputs[FILL_INPUT].getVoltage()) > 0.5f;
				if (fillPressed && writeFillsToMemory)
					setGate(channel, indexStep);// bank is global
				
				outPulse.trigger(0.001f);
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
						toggleGate(channel);
				}
				lastPeriod = clockTime > 2.0 ? 2.0 : clockTime;
				clockTime = 0.0;
			}
		}
			
		
		// Reset
		if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * args.sampleRate);
			indexStep = 0;
			outPulse.trigger(0.001f);
			outLightPulse.trigger(0.02f);
			metronomeLightStart = 1.0f;
			metronomeLightDiv = 0.0f;
			clockTrigger.reset();
		}		
		
		
		
		//********** Outputs and lights **********
		
		
		// Gate outputs
		bool bigPulseState = bigPulse.process((float)sampleTime);
		bool outPulseState = outPulse.process((float)sampleTime);
		bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
		for (int i = 0; i < 6; i++) {
			bool gate = getGate(i, indexStep);
			bool outSignal = (((gate || (i == channel && fillPressed)) && outPulseState) || (gate && bigPulseState && i == channel));
			outputs[CHAN_OUTPUTS + i].setVoltage(((outSignal && !retriggingOnReset) ? 10.0f : 0.0f));
		}

		
		// lights
		if (refresh.processLights()) {
			float deltaTime = (float)sampleTime * (RefreshCounter::displayRefreshStepSkips);

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
			metronomeLightDiv =0.0f;
		
			// Other push button lights
			lights[WRITEFILL_LIGHT].setBrightness(writeFillsToMemory ? 1.0f : 0.0f);
			lights[QUANTIZEBIG_LIGHT].setBrightness(quantizeBig ? 1.0f : 0.0f);
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
				bigLightPulse.trigger(lightTime);
			}
		}
		else {
			clearGate(chan, indexStep);// bank is global
		}
		pendingOp = 0;
	}
};


struct BigButtonSeqWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct ChanDisplayWidget : LightWidget {//TransparentWidget {
		BigButtonSeq *module;
		std::shared_ptr<Font> font;
		
		ChanDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			//nvgTextLetterSpacing(args.vg, 2.5);

			Vec textPos = VecPx(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
			nvgFillColor(args.vg, textColor);
			char displayStr[2];
			unsigned int chan = (unsigned)(module ? module->channel : 0);
			snprintf(displayStr, 2, "%1u", (unsigned) (chan + 1) );
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};

	struct StepsDisplayWidget : LightWidget {//TransparentWidget {
		BigButtonSeq *module;
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
			unsigned int len = (unsigned)(module ? module->length : 64);
			snprintf(displayStr, 3, "%2u", (unsigned) len );
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};
	
	struct PanelThemeItem : MenuItem {
		BigButtonSeq *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	struct NextStepHitsItem : MenuItem {
		BigButtonSeq *module;
		void onAction(const event::Action &e) override {
			module->nextStepHits = !module->nextStepHits;
		}
	};
	struct MetronomeItem : MenuItem {
		struct MetronomeSubItem : MenuItem {
			BigButtonSeq *module;
			int setVal = 1000;
			void onAction(const event::Action &e) override {
				module->metronomeDiv = setVal;
			}
		};
		BigButtonSeq *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			MetronomeSubItem *metro1Item = createMenuItem<MetronomeSubItem>("Every clock", CHECKMARK(module->metronomeDiv == 1));
			metro1Item->module = this->module;
			metro1Item->setVal = 1;
			menu->addChild(metro1Item);

			MetronomeSubItem *metro2Item = createMenuItem<MetronomeSubItem>("/2", CHECKMARK(module->metronomeDiv == 2));
			metro2Item->module = this->module;
			metro2Item->setVal = 2;
			menu->addChild(metro2Item);

			MetronomeSubItem *metro4Item = createMenuItem<MetronomeSubItem>("/4", CHECKMARK(module->metronomeDiv == 4));
			metro4Item->module = this->module;
			metro4Item->setVal = 4;
			menu->addChild(metro4Item);

			MetronomeSubItem *metro8Item = createMenuItem<MetronomeSubItem>("/8", CHECKMARK(module->metronomeDiv == 8));
			metro8Item->module = this->module;
			metro8Item->setVal = 8;
			menu->addChild(metro8Item);

			MetronomeSubItem *metroFItem = createMenuItem<MetronomeSubItem>("Full length", CHECKMARK(module->metronomeDiv == 1000));
			metroFItem->module = this->module;
			menu->addChild(metroFItem);

			return menu;
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		BigButtonSeq *module = dynamic_cast<BigButtonSeq*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		std::string hotRodLabel = " Hot-rod";
		hotRodLabel.insert(0, darkPanelID);// ImpromptuModular.hpp
		PanelThemeItem *darkItem = createMenuItem<PanelThemeItem>(hotRodLabel, CHECKMARK(module->panelTheme));
		darkItem->module = module;
		menu->addChild(darkItem);
		
		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		NextStepHitsItem *nhitsItem = createMenuItem<NextStepHitsItem>("Big and Del on next step", CHECKMARK(module->nextStepHits));
		nhitsItem->module = module;
		menu->addChild(nhitsItem);

		MetronomeItem *metroItem = createMenuItem<MetronomeItem>("Metronome light", RIGHT_ARROW);
		metroItem->module = module;
		menu->addChild(metroItem);
	}	
	
	
	BigButtonSeqWidget(BigButtonSeq *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/BigButtonSeq.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BigButtonSeq_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 0), module ? module ? &module->panelTheme : NULL : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), module ? module ? &module->panelTheme : NULL : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 365), module ? module ? &module->panelTheme : NULL : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), module ? module ? &module->panelTheme : NULL : NULL));

		
		
		// Column rulers (horizontal positions)
		static const int rowRuler0 = 49;// outputs and leds
		static const int colRulerCenter = 115;// not real center, but pos so that a jack would be centered
		static const int offsetChanOutX = 20;
		static const int colRulerT0 = colRulerCenter - offsetChanOutX * 5;
		static const int colRulerT1 = colRulerCenter - offsetChanOutX * 3;
		static const int colRulerT2 = colRulerCenter - offsetChanOutX * 1;
		static const int colRulerT3 = colRulerCenter + offsetChanOutX * 1;
		static const int colRulerT4 = colRulerCenter + offsetChanOutX * 3;
		static const int colRulerT5 = colRulerCenter + offsetChanOutX * 5;
		static const int ledOffsetY = 28;
		
		// Outputs
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerT0, rowRuler0), false, module, BigButtonSeq::CHAN_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerT1, rowRuler0), false, module, BigButtonSeq::CHAN_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerT2, rowRuler0), false, module, BigButtonSeq::CHAN_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerT3, rowRuler0), false, module, BigButtonSeq::CHAN_OUTPUTS + 3, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerT4, rowRuler0), false, module, BigButtonSeq::CHAN_OUTPUTS + 4, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerT5, rowRuler0), false, module, BigButtonSeq::CHAN_OUTPUTS + 5, module ? &module->panelTheme : NULL));
		// LEDs
		addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerT0 + offsetMediumLight - 1, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 0));
		addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerT1 + offsetMediumLight - 1, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 2));
		addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerT2 + offsetMediumLight - 1, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 4));
		addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerT3 + offsetMediumLight - 1, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 6));
		addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerT4 + offsetMediumLight - 1, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 8));
		addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerT5 + offsetMediumLight - 1, rowRuler0 + ledOffsetY + offsetMediumLight), module, BigButtonSeq::CHAN_LIGHTS + 10));

		
		
		static const int rowRuler1 = rowRuler0 + 72;// clk, chan and big CV
		static const int knobCVjackOffsetX = 52;
		
		// Clock input
		addInput(createDynamicPort<IMPort>(VecPx(colRulerT0, rowRuler1), true, module, BigButtonSeq::CLK_INPUT, module ? &module->panelTheme : NULL));
		// Chan knob and jack
		addParam(createDynamicParam<IMSixPosBigKnob>(VecPx(colRulerCenter + offsetIMBigKnob, rowRuler1 + offsetIMBigKnob), module, BigButtonSeq::CHAN_PARAM, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPort<IMPort>(VecPx(colRulerCenter - knobCVjackOffsetX, rowRuler1), true, module, BigButtonSeq::CHAN_INPUT, module ? &module->panelTheme : NULL));
		// Chan display
		ChanDisplayWidget *displayChan = new ChanDisplayWidget();
		displayChan->box.pos = VecPx(colRulerCenter + 43, rowRuler1 + vOffsetDisplay - 1);
		displayChan->box.size = VecPx(24, 30);// 1 character
		displayChan->module = module;
		addChild(displayChan);	
		// Length display
		StepsDisplayWidget *displaySteps = new StepsDisplayWidget();
		displaySteps->box.pos = VecPx(colRulerT5 - 17, rowRuler1 + vOffsetDisplay - 1);
		displaySteps->box.size = VecPx(40, 30);// 2 characters
		displaySteps->module = module;
		addChild(displaySteps);	


		
		static const int rowRuler2 = rowRuler1 + 50;// len and rnd
		static const int lenAndRndKnobOffsetX = 90;
		
		// Len knob and jack
		addParam(createDynamicParam<IMBigKnob<false, true>>(VecPx(colRulerCenter + lenAndRndKnobOffsetX + offsetIMBigKnob, rowRuler2 + offsetIMBigKnob), module, BigButtonSeq::LEN_PARAM, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPort<IMPort>(VecPx(colRulerCenter + lenAndRndKnobOffsetX - knobCVjackOffsetX, rowRuler2), true, module, BigButtonSeq::LEN_INPUT, module ? &module->panelTheme : NULL));
		// Rnd knob and jack
		addParam(createDynamicParam<IMBigKnob<false, true>>(VecPx(colRulerCenter - lenAndRndKnobOffsetX + offsetIMBigKnob, rowRuler2 + offsetIMBigKnob), module, BigButtonSeq::RND_PARAM, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPort<IMPort>(VecPx(colRulerCenter - lenAndRndKnobOffsetX + knobCVjackOffsetX, rowRuler2), true, module, BigButtonSeq::RND_INPUT, module ? &module->panelTheme : NULL));


		
		static const int rowRuler3 = rowRuler2 + 35;// bank
		static const int rowRuler4 = rowRuler3 + 22;// clear and del
		static const int rowRuler5 = rowRuler4 + 52;// reset and fill
		static const int clearAndDelButtonOffsetX = (colRulerCenter - colRulerT0) / 2 + 8;
		static const int knobCVjackOffsetY = 40;
		
		// Bank button and jack
		addParam(createDynamicParam<IMBigPushButton>(VecPx(colRulerCenter + offsetCKD6b, rowRuler3 + offsetCKD6b), module, BigButtonSeq::BANK_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPort<IMPort>(VecPx(colRulerCenter, rowRuler3 + knobCVjackOffsetY), true, module, BigButtonSeq::BANK_INPUT, module ? &module->panelTheme : NULL));
		// Clear button and jack
		addParam(createDynamicParam<IMBigPushButton>(VecPx(colRulerCenter - clearAndDelButtonOffsetX + offsetCKD6b, rowRuler4 + offsetCKD6b), module, BigButtonSeq::CLEAR_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPort<IMPort>(VecPx(colRulerCenter - clearAndDelButtonOffsetX, rowRuler4 + knobCVjackOffsetY), true, module, BigButtonSeq::CLEAR_INPUT, module ? &module->panelTheme : NULL));
		// Del button and jack
		addParam(createDynamicParam<IMBigPushButton>(VecPx(colRulerCenter + clearAndDelButtonOffsetX + offsetCKD6b, rowRuler4 + offsetCKD6b), module, BigButtonSeq::DEL_PARAM,  module ? &module->panelTheme : NULL));	
		addInput(createDynamicPort<IMPort>(VecPx(colRulerCenter + clearAndDelButtonOffsetX, rowRuler4 + knobCVjackOffsetY), true, module, BigButtonSeq::DEL_INPUT, module ? &module->panelTheme : NULL));
		// Reset button and jack
		addParam(createDynamicParam<IMBigPushButton>(VecPx(colRulerT0 + offsetCKD6b, rowRuler5 + offsetCKD6b), module, BigButtonSeq::RESET_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPort<IMPort>(VecPx(colRulerT0, rowRuler5 + knobCVjackOffsetY), true, module, BigButtonSeq::RESET_INPUT, module ? &module->panelTheme : NULL));
		// Fill button and jack
		addParam(createDynamicParam<IMBigPushButton>(VecPx(colRulerT5 + offsetCKD6b, rowRuler5 + offsetCKD6b), module, BigButtonSeq::FILL_PARAM, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPort<IMPort>(VecPx(colRulerT5, rowRuler5 + knobCVjackOffsetY), true, module, BigButtonSeq::FILL_INPUT, module ? &module->panelTheme : NULL));

		// And now time for... BIG BUTTON!
		addChild(createLight<GiantLight<RedLight>>(VecPx(colRulerCenter + offsetLEDbezelBig - offsetLEDbezelLight*2.0f, rowRuler5 + 26 + offsetLEDbezelBig - offsetLEDbezelLight*2.0f), module, BigButtonSeq::BIG_LIGHT));
		addParam(createParam<LEDBezelBig>(VecPx(colRulerCenter + offsetLEDbezelBig, rowRuler5 + 26 + offsetLEDbezelBig), module, BigButtonSeq::BIG_PARAM));
		addChild(createLight<GiantLight2<RedLight>>(VecPx(colRulerCenter + offsetLEDbezelBig - offsetLEDbezelLight*2.0f + 9, rowRuler5 + 26 + offsetLEDbezelBig - offsetLEDbezelLight*2.0f + 9), module, BigButtonSeq::BIGC_LIGHT));
		// Big input
		addInput(createDynamicPort<IMPort>(VecPx(colRulerCenter - clearAndDelButtonOffsetX, rowRuler5 + knobCVjackOffsetY), true, module, BigButtonSeq::BIG_INPUT, module ? &module->panelTheme : NULL));
		// Big snap
		addParam(createParam<LEDButton>(VecPx(colRulerCenter + clearAndDelButtonOffsetX + offsetLEDbutton, rowRuler5 + 1 + knobCVjackOffsetY + offsetLEDbutton), module, BigButtonSeq::QUANTIZEBIG_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(VecPx(colRulerCenter + clearAndDelButtonOffsetX + offsetLEDbutton + offsetLEDbuttonLight, rowRuler5 + 1 + knobCVjackOffsetY + offsetLEDbutton + offsetLEDbuttonLight), module, BigButtonSeq::QUANTIZEBIG_LIGHT));

		
		static const int rowRulerExtras = rowRuler4 + 12.0f;
		
		// Mem fill LED button
		addParam(createParam<LEDButton>(VecPx(colRulerT5 + offsetLEDbutton, rowRulerExtras - offsetLEDbuttonLight), module, BigButtonSeq::WRITEFILL_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(VecPx(colRulerT5 + offsetLEDbutton + offsetLEDbuttonLight, rowRulerExtras), module, BigButtonSeq::WRITEFILL_LIGHT));

		
		// Metronome light
		addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerT0 + offsetMediumLight - 1, rowRulerExtras), module, BigButtonSeq::METRONOME_LIGHT + 0));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((BigButtonSeq*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((BigButtonSeq*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelBigButtonSeq = createModel<BigButtonSeq, BigButtonSeqWidget>("Big-Button-Seq");
