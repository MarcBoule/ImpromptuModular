//***********************************************************************************************
//16-Pad CV controller module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"

struct CvPad : Module {
	enum ParamIds {
		ENUMS(PAD_PARAMS, 16),// touch pads
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(PAD_LIGHTS, 16 * 2), // (*2 for GreenRed)
		NUM_LIGHTS
	};
		
	// constants
	static const int N_BANKS = 8;
	static const int N_PADS = 16;
		
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	float cvs[N_BANKS][N_PADS];

	// No need to save, with reset
	int lastPress;
	
	// No need to save, no reset
	RefreshCounter refresh;
	Trigger padTriggers[N_PADS];
	
	
	
	CvPad() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		for (int p = 0; p < N_PADS; p++) {
			configParam(PAD_PARAMS + p, 0.0f, 1.0f, 0.0f, string::f("CV pad %i", p + 1));
		}
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		for (int b = 0; b < N_BANKS; b++) {
			for (int p = 0; p < N_PADS; p++) {
				cvs[b][p] = 0.0f;
			}
		}
		resetNonJson();
	}
	void resetNonJson() {
		lastPress = 0;
	}
	
	
	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// cvs
		json_t *cvsJ = json_array();
		for (int b = 0; b < N_BANKS; b++) {
			for (int p = 0; p < N_PADS; p++) {
				json_array_insert_new(cvsJ, b * N_PADS + p, json_real(cvs[b][p]));
			}
		}
		json_object_set_new(rootJ, "cvs", cvsJ);
		
		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
		
		// cvs
		json_t *cvsJ = json_object_get(rootJ, "cvs");
		if (cvsJ) {
			for (int b = 0; b < N_BANKS; b++)
				for (int p = 0; p < N_PADS; p++) {
					json_t *cvsArrayJ = json_array_get(cvsJ, b * N_PADS + p);
					if (cvsArrayJ)
						cvs[b][p] = json_number_value(cvsArrayJ);
				}
		}

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		if (refresh.processInputs()) {
			for (int p = 0; p < N_PADS; p++) {
				if (padTriggers[p].process(params[PAD_PARAMS + p].getValue())) {
					lastPress = p;
				}
				
			}
		}// userInputs refresh
		
		
		
		// gate output
		outputs[GATE_OUTPUTS + 3].setVoltage(padTriggers[lastPress].isHigh() ? 10.0f : 0.0f);
		outputs[CV_OUTPUTS + 3].setVoltage(cvs[0][lastPress]);
		
		// lights
		if (refresh.processLights()) {

		}
	}
	

};


struct CvPadWidget : ModuleWidget {
	SvgPanel* darkPanel;
	
	struct Pad : Switch {
		Pad() {
			momentary = true;
			box.size = Vec(42.0f, 42.0f);
		}
	};
	
	CvPadWidget(CvPad *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/CvPad.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/CvPad_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));
				
		// pads and lights
		static const int padX = 146 + 21;
		static const int padXd = 68;
		static const int padY = 84 + 21;
		static const int padYd = padXd;
		static const int ledOffsetY = 30;
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				// pad
				addParam(createParamCentered<Pad>(Vec(padX + padXd * x, padY + padYd * y), module, CvPad::PAD_PARAMS + y * 4 + x));
				// light
				addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(padX + padXd * x, padY + padYd * y - ledOffsetY), module, CvPad::PAD_LIGHTS + y * 4 + x));
			}
		}
		
		// outputs
		static const int outX = 435;
		static const int outY = padY;
		static const int outYd = padYd;
		for (int y = 0; y < 4; y++) {
			addOutput(createDynamicPortCentered<IMPort>(Vec(outX, outY + outYd * y), false, module, CvPad::CV_OUTPUTS + y, module ? &module->panelTheme : NULL));
			addOutput(createDynamicPortCentered<IMPort>(Vec(outX + 37, outY + outYd * y), false, module, CvPad::GATE_OUTPUTS + y, module ? &module->panelTheme : NULL));
		}
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((CvPad*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((CvPad*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};


//*****************************************************************************

Model *modelCvPad = createModel<CvPad, CvPadWidget>("Cv-Pad");
