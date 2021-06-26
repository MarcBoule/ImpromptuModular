//***********************************************************************************************
//Expander module for PhraseSeq16/32, by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé, Nigel Sixsmith
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "ProbKeyUtil.hpp"



struct ProbKeyExpander : Module {
	enum ParamIds {
		MINOCT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		MINCV_OUTPUT,
		NUM_OUTPUTS
	};


	// Expander
	PkxInterface leftMessages[2] = {};// messages from mother


	// No need to save
	int panelTheme;
	unsigned int expanderRefreshCounter = 0;	


	ProbKeyExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		
		leftExpander.producerMessage = &(leftMessages[0]);
		leftExpander.consumerMessage = &(leftMessages[1]);
		
		configParam(MINOCT_PARAM, -4.0f, 4.0f, 0.0f, "Min CV out octave offset");

		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}


	void process(const ProcessArgs &args) override {		
		expanderRefreshCounter++;
		if (expanderRefreshCounter >= expanderRefreshStepSkips) {
			expanderRefreshCounter = 0;
			
			bool motherPresent = leftExpander.module && (leftExpander.module->model == modelProbKey);
			if (motherPresent) {					
				// From Mother
				PkxInterface *messagesFromMother = (PkxInterface*)leftExpander.consumerMessage;
				panelTheme = clamp(messagesFromMother->panelTheme, 0, 1);
				
				// Min CV out
				if (outputs[MINCV_OUTPUT].isConnected()) {
					float minCv = messagesFromMother->minCvChan0;
					minCv += params[MINOCT_PARAM].getValue();
					outputs[MINCV_OUTPUT].setVoltage(minCv);
				}
			}		
		}// expanderRefreshCounter			
	}// process()
};


struct ProbKeyExpanderWidget : ModuleWidget {
	SvgPanel* darkPanel;
	
	ProbKeyExpanderWidget(ProbKeyExpander *module) {
		setModule(module);
	
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/ProbKeyExpander.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/ProbKeyExpander_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), module ? &module->panelTheme : NULL));

		// Expansion module
		static const float col0 = 10.16f;
		static const float row0 = 26.5f;
		static const float row1 = 46.0f;
		
		// minCv output
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col0, row0)), false, module, ProbKeyExpander::MINCV_OUTPUT, module ? &module->panelTheme : NULL));
		
		// minOct knob
		addParam(createDynamicParamCentered<IMMediumKnob<true, true>>(mm2px(Vec(col0, row1)), module, ProbKeyExpander::MINOCT_PARAM, module ? &module->panelTheme : NULL));
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
