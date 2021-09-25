//***********************************************************************************************
//Expander module for ProbKey, by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé
//
//***********************************************************************************************


#include "ProbKeyUtil.hpp"

/*
		{
			"slug": "Prob-Key-Expander",
			"name": "ProbKey expander",
			"description": "Expander for ProbKey",
			"manualUrl": "https://marcboule.github.io/ImpromptuModular/#expanders",
			"tags": ["Sequencer", "Random", "Polyphonic", "Expander"]
		},
*/

struct ProbKeyExpander : Module {
	enum ParamIds {
		MINOCT_PARAM,
		ENUMS(MANUAL_LOCK_LOW_PARAMS, 4),
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		MINCV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(MANUAL_LOCK_LOW_LIGHTS, 4),
		NUM_LIGHTS
	};


	// Expander
	PkxIntfFromMother leftMessages[2] = {};// messages from mother


	// No need to save, no reset
	int panelTheme;
	unsigned int expanderRefreshCounter = 0;	
	RefreshCounter refresh;


	ProbKeyExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		leftExpander.producerMessage = &(leftMessages[0]);
		leftExpander.consumerMessage = &(leftMessages[1]);
		
		configParam(MINOCT_PARAM, -4.0f, 4.0f, 0.0f, "Min CV out octave offset");
		for (int i = 0; i < 4; i++) {
			configParam(MANUAL_LOCK_LOW_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Manual lock low %i", i + 1));
		}

		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}


	void process(const ProcessArgs &args) override {		
			
		bool motherPresent = leftExpander.module && (leftExpander.module->model == modelProbKey);
		if (motherPresent) {					
			// From Mother
			PkxIntfFromMother *messagesFromMother = (PkxIntfFromMother*)leftExpander.consumerMessage;
			panelTheme = clamp(messagesFromMother->panelTheme, 0, 1);
			if (outputs[MINCV_OUTPUT].isConnected()) {
				float minCv = messagesFromMother->minCvChan0;
				minCv += params[MINOCT_PARAM].getValue();
				outputs[MINCV_OUTPUT].setVoltage(minCv);
			}
		}	

		if (refresh.processInputs()) {
			if (motherPresent) {
				// To Mother
				PkxIntfFromExp *messagesFromExpander = (PkxIntfFromExp*)(leftExpander.module->rightExpander.producerMessage);
				messagesFromExpander->manualLockLow = 0;
				for (int i = 0; i < 4; i++) {
					if (params[MANUAL_LOCK_LOW_PARAMS + i].getValue() >= 0.5f) {
						messagesFromExpander->setLowLock(i);
					}
				}
				leftExpander.module->rightExpander.messageFlipRequested = true;
			}
		}// userInputs refresh
		
		if (refresh.processLights()) {
			// manual lock lights
			for (int i = 0; i < 4; i++) {
				lights[MANUAL_LOCK_LOW_LIGHTS + i].setBrightness(params[MANUAL_LOCK_LOW_PARAMS + i].getValue());
			}
		}// processLights()
	}// process()
};


struct LEDButtonToggle : LEDButton {
	LEDButtonToggle() {
		momentary = false;
	}
};

struct ProbKeyExpanderWidget : ModuleWidget {
	SvgPanel* darkPanel;
	
	ProbKeyExpanderWidget(ProbKeyExpander *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
	
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/ProbKeyExpander.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/ProbKeyExpander_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), mode));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), mode));

		// Expansion module
		static const float col0 = 10.16f;
		static const float row0 = 26.5f;
		static const float row1 = 48.0f;
		
		// minCv output
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, row0)), false, module, ProbKeyExpander::MINCV_OUTPUT, mode));
		
		// minOct knob
		addParam(createDynamicParamCentered<IMMediumKnob<true, true>>(mm2px(Vec(col0, row1)), module, ProbKeyExpander::MINOCT_PARAM, mode));
		
		// manual lock low
		static const float row2 = 107.0f;
		static const float rowdy = 12.0f;
		for (int i = 0; i < 4; i++) {// bottom to top
			addParam(createParamCentered<LEDButtonToggle>(mm2px(Vec(col0, row2 - rowdy * i)), module, ProbKeyExpander::MANUAL_LOCK_LOW_PARAMS + i));
			addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(col0, row2 - rowdy * i)), module, ProbKeyExpander::MANUAL_LOCK_LOW_LIGHTS + i));
		}
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((ProbKeyExpander*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((ProbKeyExpander*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelProbKeyExpander = createModel<ProbKeyExpander, ProbKeyExpanderWidget>("Prob-Key-Expander");
