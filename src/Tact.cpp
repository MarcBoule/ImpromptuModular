//***********************************************************************************************
//Tactile CV controller module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "comp/TactPad.hpp"


struct Tact : Module {
	static const int numLights = 10;// number of lights per channel

	enum ParamIds {
		ENUMS(TACT_PARAMS, 2),// touch pads
		ENUMS(ATTV_PARAMS, 2),// max knobs
		ENUMS(RATE_PARAMS, 2),// rate knobs
		LINK_PARAM,
		ENUMS(SLIDE_PARAMS, 2),// slide switches
		ENUMS(STORE_PARAMS, 2),// store buttons
		EXP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(TOP_INPUTS, 2),
		ENUMS(BOT_INPUTS, 2),
		ENUMS(RECALL_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 2),
		ENUMS(EOC_OUTPUTS, 2),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(TACT_LIGHTS, numLights * 2 + numLights * 2), // first N lights for channel L, other N for channel R (*2 for GreenRed)
		ENUMS(CVIN_LIGHTS, 2 * 2),// GreenRed
		NUM_LIGHTS
	};
		
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	double cv[2];// actual Tact CV since Tactknob can be different than these when transitioning
	float storeCV[2];
	float rateMultiplier;
	bool levelSensitiveTopBot;
	int8_t autoReturn[2]; //-1 is off

	// No need to save, with reset
	long infoStore;// 0 when no info, positive downward step counter when store left channel, negative upward for right
	
	// No need to save, no reset
	float infoCVinLight[2] = {0.0f, 0.0f};
	RefreshCounter refresh;
	TriggerRiseFall topTriggers[2];
	TriggerRiseFall botTriggers[2];
	Trigger storeTriggers[2];
	Trigger recallTriggers[2];
	dsp::PulseGenerator eocPulses[2];
	
	
	inline bool isLinked(void) {return params[LINK_PARAM].getValue() > 0.5f;}
	inline bool isExpSliding(void) {return params[EXP_PARAM].getValue() > 0.5f;}

	
	Tact() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(TACT_PARAMS + 0, 0.0f, 10.0f, 0.0f, "Tact pad (left)");
		configParam(TACT_PARAMS + 1, 0.0f, 10.0f, 0.0f, "Tact pad (right)");
		configParam(SLIDE_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Slide (left)");		
		configParam(SLIDE_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Slide (right)");		
		configParam(STORE_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Store (left)");
		configParam(STORE_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Store (right)");
		configParam(ATTV_PARAMS + 0, -1.0f, 1.0f, 1.0f, "Attenuverter (left)");
		configParam(ATTV_PARAMS + 1, -1.0f, 1.0f, 1.0f, "Attenuverter (right)");
		configParam(RATE_PARAMS + 0, 0.0f, 4.0f, 0.2f, "Rate (left)", " s/V");
		configParam(RATE_PARAMS + 1, 0.0f, 4.0f, 0.2f, "Rate (right)", " s/V");
		configParam(EXP_PARAM, 0.0f, 1.0f, 0.0f, "Exponential");			
		configParam(LINK_PARAM, 0.0f, 1.0f, 0.0f, "Link");		
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		for (int i = 0; i < 2; i++) {
			cv[i] = 0.0f;
			storeCV[i] = 0.0f;
			autoReturn[i] = -1;
		}
		rateMultiplier = 1.0f;
		levelSensitiveTopBot = false;
		resetNonJson();
	}
	void resetNonJson() {
		infoStore = 0l;		
	}
	
	
	void onRandomize() override {
		for (int i = 0; i < 2; i++) {
			cv[i] = params[TACT_PARAMS + i].getValue();
		}
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// cv
		json_object_set_new(rootJ, "cv0", json_real(cv[0]));
		json_object_set_new(rootJ, "cv1", json_real(cv[1]));
		
		// storeCV
		json_object_set_new(rootJ, "storeCV0", json_real(storeCV[0]));
		json_object_set_new(rootJ, "storeCV1", json_real(storeCV[1]));
		
		// rateMultiplier
		json_object_set_new(rootJ, "rateMultiplier", json_real(rateMultiplier));
		
		// levelSensitiveTopBot
		json_object_set_new(rootJ, "levelSensitiveTopBot", json_boolean(levelSensitiveTopBot));

		// autoReturnLeft
		json_object_set_new(rootJ, "autoReturnLeft", json_integer(autoReturn[0]));

		// autoReturnRight
		json_object_set_new(rootJ, "autoReturnRight", json_integer(autoReturn[1]));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
		
		// cv
		json_t *cv0J = json_object_get(rootJ, "cv0");
		if (cv0J)
			cv[0] = json_number_value(cv0J);
		json_t *cv1J = json_object_get(rootJ, "cv1");
		if (cv1J)
			cv[1] = json_number_value(cv1J);

		// storeCV[0]
		json_t *storeCV0J = json_object_get(rootJ, "storeCV0");
		if (storeCV0J)
			storeCV[0] = json_number_value(storeCV0J);
		json_t *storeCV1J = json_object_get(rootJ, "storeCV1");
		if (storeCV1J)
			storeCV[1] = json_number_value(storeCV1J);

		// rateMultiplier
		json_t *rateMultiplierJ = json_object_get(rootJ, "rateMultiplier");
		if (rateMultiplierJ)
			rateMultiplier = json_number_value(rateMultiplierJ);

		// levelSensitiveTopBot
		json_t *levelSensitiveTopBotJ = json_object_get(rootJ, "levelSensitiveTopBot");
		if (levelSensitiveTopBotJ)
			levelSensitiveTopBot = json_is_true(levelSensitiveTopBotJ);
		
		// autoReturnLeft
		json_t *autoReturnLeftJ = json_object_get(rootJ, "autoReturnLeft");
		if (autoReturnLeftJ)
			autoReturn[0] = json_integer_value(autoReturnLeftJ);

		// autoReturnRight
		json_t *autoReturnRightJ = json_object_get(rootJ, "autoReturnRight");
		if (autoReturnRightJ)
			autoReturn[1] = json_integer_value(autoReturnRightJ);

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		static const float storeInfoTime = 0.5f;// seconds	
	
		if (refresh.processInputs()) {
			// store buttons
			for (int i = 0; i < 2; i++) {
				if (storeTriggers[i].process(params[STORE_PARAMS + i].getValue())) {
					if ( !(i == 1 && isLinked()) ) {// ignore right channel store-button press when linked
						storeCV[i] = cv[i];
						infoStore = (long) (storeInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips) * (i == 0 ? 1l : -1l);
					}
				}
			}
			
			// top/bot/recall CV inputs
			for (int i = 0; i < 2; i++) {
				int topTrig = topTriggers[i].process(inputs[TOP_INPUTS + i].getVoltage());
				if ( !(i == 1 && isLinked()) ) {// ignore right channel top cv in when linked
					if (topTrig == 1) {// rise
						params[TACT_PARAMS + i].setValue(10.0f);
						infoCVinLight[i] = 1.0f;
					}
					if (topTrig == -1) {// fall
						if (autoReturn[i] != -1) {
							params[TACT_PARAMS + i].setValue(autoreturnVoltages[autoReturn[i]]);
							infoCVinLight[i] = 1.0f;
						}				
						else if (levelSensitiveTopBot) {
							params[TACT_PARAMS + i].setValue(cv[i]);
							infoCVinLight[i] = 1.0f;
						}
					}
				}
				int botTrig = botTriggers[i].process(inputs[BOT_INPUTS + i].getVoltage());
				if ( !(i == 1 && isLinked()) ) {// ignore right channel bot cv in when linked
					if (botTrig == 1) {// rise
						params[TACT_PARAMS + i].setValue(0.0f);
						infoCVinLight[i] = 1.0f;
					}
					if (botTrig == -1) {// fall
						if (autoReturn[i] != -1) {
							params[TACT_PARAMS + i].setValue(autoreturnVoltages[autoReturn[i]]);
							infoCVinLight[i] = 1.0f;
						}				
						else if (levelSensitiveTopBot) {
							params[TACT_PARAMS + i].setValue(cv[i]);
							infoCVinLight[i] = 1.0f;
						}				
					}
				}
				if (recallTriggers[i].process(inputs[RECALL_INPUTS + i].getVoltage())) {// ignore right channel recall cv in when linked
					if ( !(i == 1 && isLinked()) ) {
						params[TACT_PARAMS + i].setValue(storeCV[i]);
						if (params[SLIDE_PARAMS + i].getValue() < 0.5f) //if no slide
							cv[i]=storeCV[i];
						infoCVinLight[i] = 1.0f;
					}				
				}
			}
		}// userInputs refresh
		
		
		// cv
		bool expSliding = isExpSliding();
		for (int i = 0; i < 2; i++) {
			float newParamValue = params[TACT_PARAMS + i].getValue();
			if (newParamValue != cv[i]) {
				newParamValue = clamp(newParamValue, 0.0f, 10.0f);// legacy for when range was -1.0f to 11.0f
				double transitionRate = std::max(0.001, (double)params[RATE_PARAMS + i].getValue() * rateMultiplier); // s/V
				double dt = args.sampleTime;
				if ((newParamValue - cv[i]) > 0.001f) {
					double dV = expSliding ? (cv[i] + 1.0) * (pow(11.0, dt / (10.0 * transitionRate)) - 1.0) : dt/transitionRate;
					double newCV = cv[i] + dV;
					if (newCV > newParamValue) {
						cv[i] = newParamValue;
						eocPulses[i].trigger(0.001f);
					}
					else
						cv[i] = (float)newCV;
				}
				else if ((newParamValue - cv[i]) < -0.001f) {
					dt *= -1.0;
					double dV = expSliding ? (cv[i] + 1.0) * (pow(11.0, dt / (10.0 * transitionRate)) - 1.0) : dt/transitionRate;
					double newCV = cv[i] + dV;
					if (newCV < newParamValue) {
						cv[i] = newParamValue;
						eocPulses[i].trigger(0.001f);
					}
					else
						cv[i] = (float)newCV;
				}
				else {// too close to target or rate too fast, thus no slide
					if (fabs(cv[i] - newParamValue) > 1e-6)
						eocPulses[i].trigger(0.001f);
					cv[i] = newParamValue;	
				}
			}
		}
		
	
		// CV and EOC Outputs
		bool eocValues[2] = {eocPulses[0].process(args.sampleTime), eocPulses[1].process(args.sampleTime)};
		for (int i = 0; i < 2; i++) {
			int readChan = isLinked() ? 0 : i;
			outputs[CV_OUTPUTS + i].setVoltage((float)cv[readChan] * params[ATTV_PARAMS + readChan].getValue());
			outputs[EOC_OUTPUTS + i].setVoltage(eocValues[readChan]);
		}
		
		
		// lights
		if (refresh.processLights()) {
			// Tactile lights
			if (infoStore > 0l)
				setTLightsStore(0, infoStore, (long) (storeInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips) );
			else
				setTLights(0);
			if (infoStore < 0l)
				setTLightsStore(1, infoStore * -1l, (long) (storeInfoTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips) );
			else
				setTLights(1);
			if (infoStore != 0l) {
				if (infoStore > 0l)
					infoStore --;
				if (infoStore < 0l)
					infoStore ++;
			}
			// CV input lights
			for (int i = 0; i < 2; i++) {
				lights[CVIN_LIGHTS + i * 2].setSmoothBrightness(infoCVinLight[i], args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));
				infoCVinLight[i] = 0.0f;
			}
		}
	}
	
	void setTLights(int chan) {
		int readChan = isLinked() ? 0 : chan;
		float cvValue = (float)cv[readChan];
		for (int i = 0; i < numLights; i++) {
			float level = clamp( cvValue - ((float)(i)), 0.0f, 1.0f);
			// Green diode
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 0].setBrightness(level);
			// Red diode
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 1].setBrightness(0.0f);
		}
	}
	
	void setTLightsStore(int chan, long infoCount, long initInfoStore) {
		for (int i = 0; i < numLights; i++) {
			float level = (i == (int) std::round((float(infoCount)) / ((float)initInfoStore) * (float)(numLights - 1)) ? 1.0f : 0.0f);
			// Green diode
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 0].setBrightness(level);
			// Red diode
			lights[TACT_LIGHTS + (chan * numLights * 2) + (numLights - 1 - i) * 2 + 1].setBrightness(0.0f);
		}	
	}
};


struct TactWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		Tact *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	struct ExtendRateItem : MenuItem {
		Tact *module;
		void onAction(const event::Action &e) override {
			if (module->rateMultiplier < 2.0f)
				module->rateMultiplier = 3.0f;
			else
				module->rateMultiplier = 1.0f;
		}
	};
	struct LevelSensitiveItem : MenuItem {
		Tact *module;
		void onAction(const event::Action &e) override {
			module->levelSensitiveTopBot = !module->levelSensitiveTopBot;
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Tact *module = dynamic_cast<Tact*>(this->module);
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
		
		ExtendRateItem *extRateItem = createMenuItem<ExtendRateItem>("Rate knob x3 (max 12 s/V)", CHECKMARK(module->rateMultiplier > 2.0f));
		extRateItem->module = module;
		menu->addChild(extRateItem);

		LevelSensitiveItem *levelSensItem = createMenuItem<LevelSensitiveItem>("Level sensitive arrow CV inputs", CHECKMARK(module->levelSensitiveTopBot));
		levelSensItem->module = module;
		menu->addChild(levelSensItem);
		
		AutoReturnItem *autoRetLItem = createMenuItem<AutoReturnItem>("Auto-return (left pad)", RIGHT_ARROW);
		autoRetLItem->autoReturnSrc = &(module->autoReturn[0]);
		autoRetLItem->tactParamSrc = &(module->params[Tact::TACT_PARAMS + 0]);
		menu->addChild(autoRetLItem);

		AutoReturnItem *autoRetRItem = createMenuItem<AutoReturnItem>("Auto-return (right pad)", RIGHT_ARROW);
		autoRetRItem->autoReturnSrc = &(module->autoReturn[1]);
		autoRetRItem->tactParamSrc = &(module->params[Tact::TACT_PARAMS + 1]);
		menu->addChild(autoRetRItem);

	}	
	
	struct TactPad2 : TactPad {
		static const int padInterSpace = 18;
		static const int padWidthWide = padWidth * 2 + padInterSpace;

		void step() override {
			if (paramQuantity) {
				Tact* module = dynamic_cast<Tact*>(paramQuantity->module);
				if ((module->params[Tact::LINK_PARAM].getValue()) > 0.5f) {
					box.size.x = padWidthWide;
				}
				else {
					box.size.x = padWidth;
				}
			}
			ParamWidget::step();
		}
	};
	
	TactWidget(Tact *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/Tact.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/Tact_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), module ? &module->panelTheme : NULL));
		
		
		static const int rowRuler0 = 34;
		static const int colRulerPadL = 73;
		static const int colRulerPadR = 136;
		
		// Tactile touch pads
		TactPad2 *tpadR;
		TactPad2 *tpadL;
		// Right (no dynamic width, but must do first so that left will get mouse events when wider overlaps)
		addParam(tpadR = createParam<TactPad2>(VecPx(colRulerPadR, rowRuler0), module, Tact::TACT_PARAMS + 1));
		// Left (with width dependant on Link value)	
		addParam(tpadL = createParam<TactPad2>(VecPx(colRulerPadL, rowRuler0), module, Tact::TACT_PARAMS + 0));
		if (module) {
			tpadR->autoReturnSrc = &(module->autoReturn[1]);
			tpadL->autoReturnSrc = &(module->autoReturn[0]);
		}

			
		static const int colRulerLedL = colRulerPadL - 20;
		static const int colRulerLedR = colRulerPadR + 56;
		static const int lightsOffsetY = 19;
		static const int lightsSpacingY = 17;
				
		// Tactile lights
		for (int i = 0 ; i < Tact::numLights; i++) {
			addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerLedL, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + i * 2));
			addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerLedR, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + (Tact::numLights + i) * 2));
		}

		
		static const int colRulerCenter = 115;// not real center, but pos so that a jack would be centered
		static const int rowRuler2 = 265;// outputs and link
		static const int colRulerC3L = colRulerCenter - 101 - 1;
		static const int colRulerC3R = colRulerCenter + 101;
		
		// Recall CV inputs
		addInput(createDynamicPort<IMPort>(VecPx(colRulerC3L, rowRuler2), true, module, Tact::RECALL_INPUTS + 0, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPort<IMPort>(VecPx(colRulerC3R, rowRuler2), true, module, Tact::RECALL_INPUTS + 1, module ? &module->panelTheme : NULL));		
		

		static const int rowRuler1d = rowRuler2 - 54;
		
		// Slide switches
		addParam(createParam<CKSS>(VecPx(colRulerC3L + hOffsetCKSS, rowRuler1d + vOffsetCKSS), module, Tact::SLIDE_PARAMS + 0));		
		addParam(createParam<CKSS>(VecPx(colRulerC3R + hOffsetCKSS, rowRuler1d + vOffsetCKSS), module, Tact::SLIDE_PARAMS + 1));		


		static const int rowRuler1c = rowRuler1d - 46;

		// Store buttons
		addParam(createDynamicParam<IMPushButton>(VecPx(colRulerC3L + offsetTL1105, rowRuler1c + offsetTL1105), module, Tact::STORE_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<IMPushButton>(VecPx(colRulerC3R + offsetTL1105, rowRuler1c + offsetTL1105), module, Tact::STORE_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		
		static const int rowRuler1b = rowRuler1c - 59;
		
		// Attv knobs
		addParam(createDynamicParam<IMSmallKnob<true, false>>(VecPx(colRulerC3L + offsetIMSmallKnob, rowRuler1b + offsetIMSmallKnob), module, Tact::ATTV_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<IMSmallKnob<true, false>>(VecPx(colRulerC3R + offsetIMSmallKnob, rowRuler1b + offsetIMSmallKnob), module, Tact::ATTV_PARAMS + 1, module ? &module->panelTheme : NULL));

		
		static const int rowRuler1a = rowRuler1b - 59;
		
		// Rate knobs
		addParam(createDynamicParam<IMSmallKnob<true, false>>(VecPx(colRulerC3L + offsetIMSmallKnob, rowRuler1a + offsetIMSmallKnob), module, Tact::RATE_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<IMSmallKnob<true, false>>(VecPx(colRulerC3R + offsetIMSmallKnob, rowRuler1a + offsetIMSmallKnob), module, Tact::RATE_PARAMS + 1, module ? &module->panelTheme : NULL));
		

		static const int colRulerC1L = colRulerCenter - 30 - 1;
		static const int colRulerC1R = colRulerCenter + 30; 
		static const int colRulerC2L = colRulerCenter - 65 - 1;
		static const int colRulerC2R = colRulerCenter + 65 + 1; 

		// Exp switch
		addParam(createParam<CKSS>(VecPx(colRulerCenter + hOffsetCKSS, rowRuler2 + vOffsetCKSS), module, Tact::EXP_PARAM));		

		// Top/bot CV Inputs
		addInput(createDynamicPort<IMPort>(VecPx(colRulerC2L, rowRuler2), true, module, Tact::TOP_INPUTS + 0, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPort<IMPort>(VecPx(colRulerC1L, rowRuler2), true, module, Tact::BOT_INPUTS + 0, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPort<IMPort>(VecPx(colRulerC1R, rowRuler2), true, module, Tact::BOT_INPUTS + 1, module ? &module->panelTheme : NULL));	
		addInput(createDynamicPort<IMPort>(VecPx(colRulerC2R, rowRuler2), true, module, Tact::TOP_INPUTS + 1, module ? &module->panelTheme : NULL));		

		
		static const int rowRuler3 = rowRuler2 + 54;

		// Link switch
		addParam(createParam<CKSS>(VecPx(colRulerCenter + hOffsetCKSS, rowRuler3 + vOffsetCKSS), module, Tact::LINK_PARAM));		

		// Outputs
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerCenter - 49 - 1, rowRuler3), false, module, Tact::CV_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerCenter + 49, rowRuler3), false, module, Tact::CV_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		
		// EOC
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerCenter - 89 - 1, rowRuler3), false, module, Tact::EOC_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(VecPx(colRulerCenter + 89, rowRuler3), false, module, Tact::EOC_OUTPUTS + 1, module ? &module->panelTheme : NULL));

		
		// Lights
		addChild(createLight<SmallLight<GreenLight>>(VecPx(colRulerCenter - 47 - 1 + offsetMediumLight, rowRuler2 - 24 + offsetMediumLight), module, Tact::CVIN_LIGHTS + 0 * 2));		
		addChild(createLight<SmallLight<GreenLight>>(VecPx(colRulerCenter + 47 + 1 + offsetMediumLight, rowRuler2 - 24 + offsetMediumLight), module, Tact::CVIN_LIGHTS + 1 * 2));		
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Tact*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Tact*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};


//*****************************************************************************

struct Tact1 : Module {
	static const int numLights = 10;// number of lights per channel

	enum ParamIds {
		TACT_PARAM,// touch pad
		ATTV_PARAM,// attenuverter
		RATE_PARAM,// rate knob
		EXP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(TACT_LIGHTS, numLights * 2), // (*2 for GreenRed)
		NUM_LIGHTS
	};
		
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	double cv;// actual Tact CV since Tactknob can be different than these when transitioning
	float rateMultiplier;
	int8_t autoReturn; //-1 is off

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	RefreshCounter refresh;	
	

	inline bool isExpSliding(void) {return params[EXP_PARAM].getValue() > 0.5f;}

	
	Tact1() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(TACT_PARAM, 0.0f, 10.0f, 0.0f, "Tact pad");
		configParam(ATTV_PARAM, -1.0f, 1.0f, 1.0f, "Attenuverter");
		configParam(RATE_PARAM, 0.0f, 4.0f, 0.2f, "Rate", " s/V");
		configParam(EXP_PARAM, 0.0f, 1.0f, 0.0f, "Exponential");			
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		cv = 0.0f;
		rateMultiplier = 1.0f;
		autoReturn = -1;
		// resetNonJson();
	}
	// void resetNonJson() {
		// none
	// }

	
	void onRandomize() override {
		cv = params[TACT_PARAM].getValue();
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// cv
		json_object_set_new(rootJ, "cv", json_real(cv));
		
		// rateMultiplier
		json_object_set_new(rootJ, "rateMultiplier", json_real(rateMultiplier));
		
		// autoReturn
		json_object_set_new(rootJ, "autoReturn", json_integer(autoReturn));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// cv
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ)
			cv = json_number_value(cvJ);

		// rateMultiplier
		json_t *rateMultiplierJ = json_object_get(rootJ, "rateMultiplier");
		if (rateMultiplierJ)
			rateMultiplier = json_number_value(rateMultiplierJ);
		
		// autoReturn
		json_t *autoReturnJ = json_object_get(rootJ, "autoReturn");
		if (autoReturnJ)
			autoReturn = json_integer_value(autoReturnJ);

		// resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		// cv
		float newParamValue = params[TACT_PARAM].getValue();
		if (newParamValue != cv) {
			newParamValue = clamp(newParamValue, 0.0f, 10.0f);// legacy for when range was -1.0f to 11.0f
			double transitionRate = std::max(0.001, (double)params[RATE_PARAM].getValue() * rateMultiplier); // s/V
			double dt = args.sampleTime;
			if ((newParamValue - cv) > 0.001f) {
				double dV = isExpSliding() ? (cv + 1.0) * (pow(11.0, dt / (10.0 * transitionRate)) - 1.0) : dt/transitionRate;
				double newCV = cv + dV;
				if (newCV > newParamValue)
					cv = newParamValue;
				else
					cv = (float)newCV;
			}
			else if ((newParamValue - cv) < -0.001f) {
				dt *= -1.0;
				double dV = isExpSliding() ? (cv + 1.0) * (pow(11.0, dt / (10.0 * transitionRate)) - 1.0) : dt/transitionRate;
				double newCV = cv + dV;
				if (newCV < newParamValue)
					cv = newParamValue;
				else
					cv = (float)newCV;
			}
			else {// too close to target or rate too fast, thus no slide
				cv = newParamValue;	
			}
		}
		
	
		// CV Output
		outputs[CV_OUTPUT].setVoltage((float)cv * params[ATTV_PARAM].getValue());
		
		
		// lights
		if (refresh.processLights()) {
			setTLights();
		}
	}
	
	void setTLights() {
		float cvValue = (float)cv;
		for (int i = 0; i < numLights; i++) {
			float level = clamp( cvValue - ((float)(i)), 0.0f, 1.0f);
			// Green diode
			lights[TACT_LIGHTS + (numLights - 1 - i) * 2 + 0].setBrightness(level);
			// Red diode
			lights[TACT_LIGHTS + (numLights - 1 - i) * 2 + 1].setBrightness(0.0f);
		}
	}
};

struct Tact1Widget : ModuleWidget {
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		Tact1 *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	struct ExtendRateItem : MenuItem {
		Tact1 *module;
		void onAction(const event::Action &e) override {
			if (module->rateMultiplier < 2.0f)
				module->rateMultiplier = 3.0f;
			else
				module->rateMultiplier = 1.0f;
		}
	};
	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Tact1 *module = dynamic_cast<Tact1*>(this->module);
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
		
		ExtendRateItem *extRateItem = createMenuItem<ExtendRateItem>("Rate knob x3 (max 12 s/V)", CHECKMARK(module->rateMultiplier > 2.0f));
		extRateItem->module = module;
		menu->addChild(extRateItem);

		AutoReturnItem *autoRetItem = createMenuItem<AutoReturnItem>("Auto-return", RIGHT_ARROW);
		autoRetItem->autoReturnSrc = &(module->autoReturn);
		autoRetItem->tactParamSrc = &(module->params[Tact1::TACT_PARAM]);
		menu->addChild(autoRetItem);
	}	
	
	Tact1Widget(Tact1 *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/Tact1.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/Tact1_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), module ? &module->panelTheme : NULL));
		
		
		static const int rowRuler0 = 42;
		static const int colRulerPad = 14;
		
		// Tactile touch pad
		TactPad *tpad;
		addParam(tpad = createParam<TactPad>(VecPx(colRulerPad, rowRuler0), module, Tact1::TACT_PARAM));
		if (module) {
			tpad->autoReturnSrc = &(module->autoReturn);
		}
			
		static const int colRulerLed = colRulerPad + 56;
		static const int lightsOffsetY = 19;
		static const int lightsSpacingY = 17;
				
		// Tactile lights
		for (int i = 0 ; i < Tact1::numLights; i++) {
			addChild(createLight<MediumLight<GreenRedLight>>(VecPx(colRulerLed, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact1::TACT_LIGHTS + i * 2));
		}

		static const int rowRuler2 = 275;// rate and exp
		static const int offsetFromSide2 = 25;
		// Rate knob
		addParam(createDynamicParamCentered<IMSmallKnob<true, false>>(VecPx(offsetFromSide2, rowRuler2), module, Tact1::RATE_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMSmallKnob<true, false>>(VecPx(box.size.x - offsetFromSide2, rowRuler2), module, Tact1::ATTV_PARAM, module ? &module->panelTheme : NULL));
		
		static const int rowRuler3 = 320;
		
		// Output
		addOutput(createDynamicPort<IMPort>(VecPx(18, rowRuler3), false, module, Tact1::CV_OUTPUT, module ? &module->panelTheme : NULL));
		// Exp switch
		addParam(createParam<CKSS>(VecPx(57 + hOffsetCKSS, rowRuler3 + vOffsetCKSS), module, Tact1::EXP_PARAM));		
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Tact1*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Tact1*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};


//*****************************************************************************


struct TactG : Module {
	static const int numLights = 10;// number of lights per channel

	enum ParamIds {
		TACT_PARAM,// touch pad
		ATTV_PARAM,// attenuverter
		RATE_PARAM,// rate knob
		EXP_PARAM,
		OFFSET_PARAM,
		OFFSET2_CV_PARAM,
		RATE_MULT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		GATE_INPUT,
		OFFSET2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(TACT_LIGHTS, numLights * 2), // (*2 for GreenRed)
		NUM_LIGHTS
	};
		
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	double cv;// actual Tact CV since Tactknob can be different than these when transitioning
	int8_t autoReturn; //-1 is off

	// No need to save, with reset
	int8_t gate;	
	
	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	inline bool isExpSliding(void) {return params[EXP_PARAM].getValue() > 0.5f;}

	
	TactG() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(TACT_PARAM, 0.0f, 10.0f, 0.0f, "Tact pad");
		configParam(ATTV_PARAM, -1.0f, 1.0f, 1.0f, "Attenuverter");
		configParam(RATE_PARAM, 0.0f, 4.0f, 0.2f, "Rate", " s/V");
		configParam(EXP_PARAM, 0.0f, 1.0f, 0.0f, "Exponential");			
		configParam(OFFSET_PARAM,  -10.0f, 10.0f, 0.0f, "Offset", " V");			
		configParam(OFFSET2_CV_PARAM,  -1.0f, 1.0f, 0.0f, "Offset2 CV");			
		configParam(RATE_MULT_PARAM,  0.0f, 1.0f, 0.0f, "Slow (rate knob x3)");			
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		cv = 0.0f;
		autoReturn = -1;
		resetNonJson();
	}
	void resetNonJson() {
		gate = 0;
	}

	
	void onRandomize() override {
		cv = params[TACT_PARAM].getValue();
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// cv
		json_object_set_new(rootJ, "cv", json_real(cv));
		
		// autoReturn
		json_object_set_new(rootJ, "autoReturn", json_integer(autoReturn));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// cv
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ)
			cv = json_number_value(cvJ);

		// autoReturn
		json_t *autoReturnJ = json_object_get(rootJ, "autoReturn");
		if (autoReturnJ)
			autoReturn = json_integer_value(autoReturnJ);

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		// cv
		float newParamValue = params[TACT_PARAM].getValue();
		if (newParamValue != cv) {
			newParamValue = clamp(newParamValue, 0.0f, 10.0f);// legacy for when range was -1.0f to 11.0f
			float rateMultiplier = params[RATE_MULT_PARAM].getValue() * 2.0f + 1.0f;
			double transitionRate = std::max(0.001, (double)params[RATE_PARAM].getValue() * rateMultiplier); // s/V
			double dt = args.sampleTime;
			if ((newParamValue - cv) > 0.001f) {
				double dV = isExpSliding() ? (cv + 1.0) * (pow(11.0, dt / (10.0 * transitionRate)) - 1.0) : dt/transitionRate;
				double newCV = cv + dV;
				if (newCV > newParamValue)
					cv = newParamValue;
				else
					cv = (float)newCV;
			}
			else if ((newParamValue - cv) < -0.001f) {
				dt *= -1.0;
				double dV = isExpSliding() ? (cv + 1.0) * (pow(11.0, dt / (10.0 * transitionRate)) - 1.0) : dt/transitionRate;
				double newCV = cv + dV;
				if (newCV < newParamValue)
					cv = newParamValue;
				else
					cv = (float)newCV;
			}
			else {// too close to target or rate too fast, thus no slide
				cv = newParamValue;	
			}
		}
		
	
		// Gate Output
		float gateOut = (inputs[GATE_INPUT].getVoltage() >= 1.0f || gate != 0) ?  10.0f : 0.0f;
		outputs[GATE_OUTPUT].setVoltage(std::min(10.0f, gateOut));
	
		// CV Output
		float cvOut = (float)cv * params[ATTV_PARAM].getValue();
		cvOut += params[OFFSET_PARAM].getValue();
		cvOut += inputs[OFFSET2_INPUT].getVoltage() * params[OFFSET2_CV_PARAM].getValue();
		outputs[CV_OUTPUT].setVoltage(clamp(cvOut, -10.0f, 10.0f));
		
		
		// lights
		if (refresh.processLights()) {
			setTLights();
		}
	}
	
	void setTLights() {
		float cvValue = (float)cv;
		for (int i = 0; i < numLights; i++) {
			float level = clamp( cvValue - ((float)(i)), 0.0f, 1.0f);
			// Green diode
			lights[TACT_LIGHTS + (numLights - 1 - i) * 2 + 0].setBrightness(level);
			// Red diode
			lights[TACT_LIGHTS + (numLights - 1 - i) * 2 + 1].setBrightness(0.0f);
		}
	}
};

struct TactGWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		TactG *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		TactG *module = dynamic_cast<TactG*>(this->module);
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
		
		AutoReturnItem *autoRetItem = createMenuItem<AutoReturnItem>("Auto-return", RIGHT_ARROW);
		autoRetItem->autoReturnSrc = &(module->autoReturn);
		autoRetItem->tactParamSrc = &(module->params[TactG::TACT_PARAM]);
		menu->addChild(autoRetItem);
	}	
	
	TactGWidget(TactG *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/TactG.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/TactG_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), module ? &module->panelTheme : NULL));
		
		
		static constexpr float padY = 12.8f;
		
		// Tactile touch pad
		TactPad *tpad;
		addParam(tpad = createParam<TactPad>(mm2px(Vec(16.0f, padY)), module, TactG::TACT_PARAM));
		if (module) {
			tpad->autoReturnSrc = &(module->autoReturn);
			tpad->gateSrc = &(module->gate);
		}
			
			
		static constexpr float colRulerLed = 34.0f;
		static constexpr float lightsSpacingY = 5.76f;
				
		// Tactile lights
		for (int i = 0 ; i < TactG::numLights; i++) {
			addChild(createLight<MediumLight<GreenRedLight>>(mm2px(Vec(colRulerLed, padY + 6.2f + i * lightsSpacingY)), module, TactG::TACT_LIGHTS + i * 2));
		}


		static constexpr float knobX = 8.0f;
		static constexpr float knobTopY = 22.0f;
		static constexpr float knobOffsetY = 20.5f;
		
		// Rate, Attv, Ofst Knobs
		addParam(createDynamicParamCentered<IMSmallKnob<true, false>>(mm2px(Vec(knobX, knobTopY)), module, TactG::RATE_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMSmallKnob<true, false>>(mm2px(Vec(knobX, knobTopY + knobOffsetY)), module, TactG::ATTV_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMSmallKnob<true, false>>(mm2px(Vec(knobX, knobTopY + knobOffsetY * 2.0f)), module, TactG::OFFSET_PARAM, module ? &module->panelTheme : NULL));
		
		
		static constexpr float rowRulerB1 = 94.8f;
		static constexpr float rowRulerB2 = 110.9f;
		static constexpr float colRulerM = 23.454f;
		static constexpr float colRulerR = 35.1f;
	
		// Offset 2 input and cv knob
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(knobX, knobTopY + knobOffsetY * 3.0f - 1.6f)), true, module, TactG::OFFSET2_INPUT, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMSmallKnob<true, false>>(mm2px(Vec(knobX, rowRulerB1)), module, TactG::OFFSET2_CV_PARAM, module ? &module->panelTheme : NULL));
		
		// x3 and Exp switches
		addParam(createParamCentered<CKSS>(mm2px(Vec(colRulerR, rowRulerB1)), module, TactG::RATE_MULT_PARAM));	
		addParam(createParamCentered<CKSS>(mm2px(Vec(colRulerR, rowRulerB2)), module, TactG::EXP_PARAM));	

		// Gate in port (chain)
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(knobX, rowRulerB2)), true, module, TactG::GATE_INPUT, module ? &module->panelTheme : NULL));

		// Gate and CV outputs
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(colRulerM, rowRulerB1)), false, module, TactG::CV_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(colRulerM, rowRulerB2)), false, module, TactG::GATE_OUTPUT, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((TactG*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((TactG*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};


//*****************************************************************************


Model *modelTact = createModel<Tact, TactWidget>("Tact");

Model *modelTact1 = createModel<Tact1, Tact1Widget>("Tact1");

Model *modelTactG = createModel<TactG, TactGWidget>("TactG");
