//***********************************************************************************************
//Random note generator module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Pyer. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the Stochastic Inspiration Generator by Stochastic Instruments
//***********************************************************************************************

#include "ImpromptuModular.hpp"




// ----------------------------------------------------------------------------


struct ProbNote : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		CLK_INPUT,
		CV_INPUT,
		GATE_INPUT,
		VEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	
	// Expander
	// none
		
	// Constants
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	
	// No need to save, with reset
	float cvOut;
	bool gateOut;
	float cvIns[PORT_MAX_CHANNELS];
	float cumulVelIns[PORT_MAX_CHANNELS];

	// No need to save, no reset
	RefreshCounter refresh;
	Trigger clkTrigger;
	


	ProbNote() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configInput(CLK_INPUT, "Clock");
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(VEL_INPUT, "Vel");

		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE_OUTPUT, "Gate");
		
		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	void onReset() override final {
		resetNonJson();
	}
	void resetNonJson() {
		cvOut = 0.0f;
		gateOut = false;
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			cvIns[i] = 0.0f;
			cumulVelIns[i] = 0.0f;
		}
	}


	void onRandomize() override {
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) {
			panelTheme = json_integer_value(panelThemeJ);
		}

		// panelContrast
		json_t *panelContrastJ = json_object_get(rootJ, "panelContrast");
		if (panelContrastJ)
			panelContrast = json_number_value(panelContrastJ);

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {						
		// bool expanderPresent = rightExpander.module && (rightExpander.module->model == modelProbNoteExpander);

		
		//********** Buttons, knobs, switches and inputs **********
		
		if (refresh.processInputs()) {

		}// userInputs refresh


		
		
		//********** Outputs and lights **********
		
		if (clkTrigger.process(inputs[CLK_INPUT].getVoltage())) {
			
			// scan inputs to see which notes are active and record cv and cumulVel
			int j = 0;
			for (int i = 0; i < inputs[GATE_INPUT].getChannels(); i++) {
				if (inputs[GATE_INPUT].getVoltage(i) >= 1.0f) {
					float newCv = 0.0;
					float newVel = 1000.0f/128.0f;
					if (inputs[CV_INPUT].isConnected() && i < inputs[CV_INPUT].getChannels()) {
						newCv = inputs[CV_INPUT].getVoltage(i);
					}
					if (inputs[VEL_INPUT].isConnected() && i < inputs[VEL_INPUT].getChannels()) {
						newVel = inputs[VEL_INPUT].getVoltage(i);
					}
					cvIns[j] = newCv;
					cumulVelIns[j] = newVel;
					if (j > 0) {
						cumulVelIns[j] += cumulVelIns[j - 1];
					}
					
					j++;
				}
			}
			
			// if at least one note active, take decision on new note to emit (or not)
			gateOut = false;
			if (j > 0) {
				float maxVel = std::max(10.0f, cumulVelIns[j - 1]);
				float randVel = random::uniform() * maxVel;
				for (int k = 0; k < j; k++) {
					if (randVel < cumulVelIns[k]) {
						gateOut = true;
						cvOut = cvIns[k];
						break;
					}
				}				
			}
		}
		

		outputs[CV_OUTPUT].setVoltage(cvOut);
		outputs[GATE_OUTPUT].setVoltage(gateOut ? inputs[CLK_INPUT].getVoltage() : 0.0f);



		
		// lights
		if (refresh.processLights()) {

		}// processLights()
	}
};



struct ProbNoteWidget : ModuleWidget {
	
	void appendContextMenu(Menu *menu) override {
		ProbNote *module = static_cast<ProbNote*>(this->module);
		assert(module);
		
		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), static_cast<SvgPanel*>(getPanel()));
	}


	ProbNoteWidget(ProbNote *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/BlankPanel.svg")));
		SvgPanel* svgPanel = static_cast<SvgPanel*>(getPanel());
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		

		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(20, 20)), true, module, ProbNote::CLK_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(20, 50)), true, module, ProbNote::CV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(20, 60)), true, module, ProbNote::GATE_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(20, 70)), true, module, ProbNote::VEL_INPUT, mode));

		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(40, 50)), false, module, ProbNote::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(40, 60)), false, module, ProbNote::GATE_OUTPUT, mode));

	}
};

Model *modelProbNote = createModel<ProbNote, ProbNoteWidget>("Prob-Note");
