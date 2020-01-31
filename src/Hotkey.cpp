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


#include "ImpromptuModular.hpp"


struct Hotkey : Module {
	enum ParamIds {
		RECORD_KEY_PARAM,
		DELAY_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		TRIG_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RECORD_KEY_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	char hotkey;
	bool requestTrig;

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	dsp::PulseGenerator trigOutPulse;
	RefreshCounter refresh;

	
	Hotkey() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(RECORD_KEY_PARAM, 0.0f, 1.0f, 0.0f, "Record hotkey");
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}
	

	void onReset() override {
		hotkey = ' ';
		requestTrig = false;
		resetNonJson(false);
	}
	void resetNonJson(bool delayed) {// delay thread sensitive parts (i.e. schedule them so that process() will do them)
	}
	
	void onRandomize() override {
	}
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// hotkey
		json_object_set_new(rootJ, "hotkey", json_integer(hotkey));

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// hotkey
		json_t *hotkeyJ = json_object_get(rootJ, "hotkey");
		if (hotkeyJ)
			hotkey = json_integer_value(hotkeyJ);

		params[RECORD_KEY_PARAM].setValue(0.0f);

		resetNonJson(true);
	}

	
	void onSampleRateChange() override {
	}		
	
	
	bool hotkeyPressed(int newKey) {// return true if processed the key	
		bool processed = false;
		
		if (params[RECORD_KEY_PARAM].getValue() >= 0.5f) {// if recording the keypress
			hotkey = (char)newKey;
			params[RECORD_KEY_PARAM].setValue(0.0f);
			processed = true;
		}
		else {// normal key press when not recording
			if (newKey == hotkey) {
				requestTrig = true;
				processed = true;
			}
		}
		
		return processed;
	}


	void process(const ProcessArgs &args) override {
		
		// Inputs
		if (refresh.processInputs()) {

		}// userInputs refresh


		if (requestTrig) {
			trigOutPulse.trigger(0.002f);
			requestTrig = false;
		}

		outputs[TRIG_OUTPUT].setVoltage((trigOutPulse.process(args.sampleTime) ? 10.0f : 0.0f));
		
		// lights
		if (refresh.processLights()) {
			lights[RECORD_KEY_LIGHT].setBrightness(params[RECORD_KEY_PARAM].getValue());
		}// lightRefreshCounter
	}// process()
	
};


struct HotkeyWidget : ModuleWidget {
	SvgPanel* darkPanel;
	
	struct PanelThemeItem : MenuItem {
		Hotkey *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};

	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Hotkey *module = dynamic_cast<Hotkey*>(this->module);
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
		
		MenuLabel *hotkeyLabel = new MenuLabel();
		char strBuf[32];
		snprintf(strBuf, 32, "Current hotkey: '%c'", module->hotkey);
		hotkeyLabel->text = strBuf;
		menu->addChild(hotkeyLabel);
	}	

	
	HotkeyWidget(Hotkey *module) {
		setModule(module);
		
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/Hotkey.svg")));
        // if (module) {
			// darkPanel = new SvgPanel();
			// darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/Clocked_dark.svg")));
			// darkPanel->visible = false;
			// addChild(darkPanel);
		// }
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), NULL));//module ? &module->panelTheme : NULL));
		// addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		// addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), NULL));//module ? &module->panelTheme : NULL));



		
		// Record-key LED bezel and light
		SvgSwitch *ledBez;
		addParam(ledBez = createParamCentered<LEDBezel>(Vec(30.0f, 50.0f), module, Hotkey::RECORD_KEY_PARAM));
		ledBez->momentary = false;
		addChild(createLightCentered<MuteLight<RedLight>>(Vec(30.0f, 50.0f), module, Hotkey::RECORD_KEY_LIGHT));

		// trig out
		addOutput(createDynamicPortCentered<IMPort>(Vec(30.0f, 380.0f - mm2px(31.25f)), false, module, Hotkey::TRIG_OUTPUT, NULL));//module ? &module->panelTheme : NULL));
	}
	
	// void step() override {
		// if (module) {
			// panel->visible = ((((Clocked*)module)->panelTheme) == 0);
			// darkPanel->visible  = ((((Clocked*)module)->panelTheme) == 1);
		// }
		// Widget::step();
	// }
	
	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			Hotkey *module = dynamic_cast<Hotkey*>(this->module);
			if ((e.mods & RACK_MOD_MASK) == 0 && module->hotkeyPressed(e.key)) {
				e.consume(this);
				return;
			}
		}
		ModuleWidget::onHoverKey(e); 
	}
};

Model *modelHotkey = createModel<Hotkey, HotkeyWidget>("Hotkey");
