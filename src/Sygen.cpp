//***********************************************************************************************
//Synchronous gate enable module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
// 
//***********************************************************************************************


#include "ImpromptuModular.hpp"

struct Sygen : Module {
	enum ParamIds {
		ENUMS(ENABLE_PARAMS, 4),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(GATE_INPUTS, 4),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(PENDING_LIGHTS, 4),
		ENUMS(SYNC_ENABLED_LIGHTS, 4),
		NUM_LIGHTS
	};
		
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool syncEnabled[4];
	bool pending[4];
	int fastToogleWhenGateLow;

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	RefreshCounter refresh;
	Trigger buttonTriggers[4];
	Trigger gateInTriggers[4];


	Sygen() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		for (int i = 0; i < 4; i++) {
			configParam(ENABLE_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Gate enable %i", i + 1));
			configInput(GATE_INPUTS + i, string::f("Gate %i", i + 1));
			configOutput(GATE_OUTPUTS + i, string::f("Gate %i", i + 1));
		}

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	
	void onReset() override {
		for (int i = 0; i < 4; i++) {
			syncEnabled[i] = true;
			pending[i] = false;
		}
		fastToogleWhenGateLow = 0x1;
		resetNonJson();
	}
	void resetNonJson() {
	}
	
	
	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// syncEnabled
		json_t *syncEnabledJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(syncEnabledJ, i, json_boolean(syncEnabled[i]));
		json_object_set_new(rootJ, "syncEnabled", syncEnabledJ);

		// pending
		json_t *pendingJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(pendingJ, i, json_boolean(pending[i]));
		json_object_set_new(rootJ, "pending", pendingJ);

		// fastToogleWhenGateLow
		json_object_set_new(rootJ, "fastToogleWhenGateLow", json_integer(fastToogleWhenGateLow));

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

		// syncEnabled
		json_t *syncEnabledJ = json_object_get(rootJ, "syncEnabled");
		if (syncEnabledJ) {
			for (int i = 0; i < 4; i++) {
				json_t *syncEnabledArrayJ = json_array_get(syncEnabledJ, i);
				if (syncEnabledArrayJ)
					syncEnabled[i] = json_is_true(syncEnabledArrayJ);
			}
		}
		
		// pending
		json_t *pendingJ = json_object_get(rootJ, "pending");
		if (pendingJ) {
			for (int i = 0; i < 4; i++) {
				json_t *pendingArrayJ = json_array_get(pendingJ, i);
				if (pendingArrayJ)
					pending[i] = json_is_true(pendingArrayJ);
			}
		}
		
		// fastToogleWhenGateLow
		json_t *fastToogleWhenGateLowJ = json_object_get(rootJ, "fastToogleWhenGateLow");
		if (fastToogleWhenGateLowJ)
			fastToogleWhenGateLow = json_integer_value(fastToogleWhenGateLowJ);
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		
		if (refresh.processInputs()) {
			for (int i = 0; i < 4; i++) {
				if (buttonTriggers[i].process(params[ENABLE_PARAMS + i].getValue())) {
					if (fastToogleWhenGateLow) {
						if (gateInTriggers[i].isHigh()) {
							pending[i] = !pending[i];
						}
						else {
							pending[i] = false;
							syncEnabled[i] = !syncEnabled[i];
						}
					}
					else {
						pending[i] = !pending[i];
					}
				}
			}
		}// userInputs refresh


		for (int i = 0; i < 4; i++) {
			if (gateInTriggers[i].process(inputs[GATE_INPUTS + i].getVoltage())) {
				if (pending[i]) {
					syncEnabled[i] = !syncEnabled[i];
					pending[i] = false;
				}
			}
			outputs[GATE_OUTPUTS + i].setVoltage(syncEnabled[i] ? inputs[GATE_INPUTS + i].getVoltage() : 0.0f);
		}
		
		// lights
		if (refresh.processLights()) {
			for (int i = 0; i < 4; i++) {
				lights[PENDING_LIGHTS + i].setBrightness(pending[i] ? 1.0f : 0.0f);
				lights[SYNC_ENABLED_LIGHTS + i].setBrightness(syncEnabled[i] ? 1.0f : 0.0f);
			}
		}	
	}// process()
};


struct SygenWidget : ModuleWidget {
	void appendContextMenu(Menu *menu) override {
		Sygen *module = dynamic_cast<Sygen*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());
		
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createCheckMenuItem("Fast toggle when gate input is low", "",
			[=]() {return module->fastToogleWhenGateLow != 0;},
			[=]() {module->fastToogleWhenGateLow ^= 0x1;}
		));

	}	
	
	
	SygenWidget(Sygen *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Sygen.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));
		
		
		const float colM = box.size.x / 2;
		static const int colL = 25;
		static const int colR = 65;
		
		static const int rowSw = 64;// first button
		static const int rowSwD = 34;// delta button
		static const int rowJk = 221;// first jacks
		static const int rowJkD = 35;// delta jacks

		for (int i = 0; i < 4; i++) {
			// buttons
			addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colM, rowSw + i * rowSwD), module, Sygen::ENABLE_PARAMS + i,  mode));
			// lights
			addChild(createLightCentered<MediumLight<RedLight>>(VecPx(colL - 6, rowSw + i * rowSwD), module, Sygen::PENDING_LIGHTS + i));
			addChild(createLightCentered<MediumLight<GreenLight>>(VecPx(colR + 6, rowSw + i * rowSwD), module, Sygen::SYNC_ENABLED_LIGHTS + i));
			// inputs
			addInput(createDynamicPortCentered<IMPort>(VecPx(colL, rowJk + i * rowJkD), true, module, Sygen::GATE_INPUTS + i, mode));		
			// outputs
			addOutput(createDynamicPortCentered<IMPort>(VecPx(colR, rowJk + i * rowJkD), false, module, Sygen::GATE_OUTPUTS + i, mode));
		}
		
	}
};


Model *modelSygen = createModel<Sygen, SygenWidget>("Sygen");
