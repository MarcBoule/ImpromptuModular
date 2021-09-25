//***********************************************************************************************
//Expander module for Clocked, by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé, Nigel Sixsmith, Xavier Belmont and Steve Baker
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct ClockedExpander : Module {
	enum InputIds {
		ENUMS(PW_INPUTS, 4),// fourth element not used
		ENUMS(SWING_INPUTS, 4),// fourth element not used
		NUM_INPUTS
	};


	// Expander
	float leftMessages[2][1] = {};// messages from mother


	// No need to save, no reset
	int panelTheme;
	unsigned int expanderRefreshCounter = 0;


	ClockedExpander() {
		config(0, NUM_INPUTS, 0, 0);
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}


	void process(const ProcessArgs &args) override {		
		expanderRefreshCounter++;
		if (expanderRefreshCounter >= expanderRefreshStepSkips) {
			expanderRefreshCounter = 0;
			
			bool motherPresent = (leftExpander.module && leftExpander.module->model == modelClocked);
			if (motherPresent) {
				// To Mother
				float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
				for (int i = 0; i < 8; i++) {
					messagesToMother[i] = inputs[i].getVoltage();
				}
				leftExpander.module->rightExpander.messageFlipRequested = true;
				
				// From Mother
				float *messagesFromMother = (float*)leftExpander.consumerMessage;
				panelTheme = clamp((int)(messagesFromMother[0] + 0.5f), 0, 1);			
			}		
		}// expanderRefreshCounter
	}// process()
};


struct ClockedExpanderWidget : ModuleWidget {
	SvgPanel* darkPanel;
	
	ClockedExpanderWidget(ClockedExpander *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
	
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/ClockedExpander.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/ClockedExpander_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), mode));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), mode));

		// Expansion module
		static const int rowRulerExpTop = 72;
		static const int rowSpacingExp = 50;
		static const int colRulerExp = 30;
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerExpTop + rowSpacingExp * 0), true, module, ClockedExpander::PW_INPUTS + 0, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerExpTop + rowSpacingExp * 1), true, module, ClockedExpander::PW_INPUTS + 1, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerExpTop + rowSpacingExp * 2), true, module, ClockedExpander::PW_INPUTS + 2, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerExpTop + rowSpacingExp * 3), true, module, ClockedExpander::SWING_INPUTS + 0, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerExpTop + rowSpacingExp * 4), true, module, ClockedExpander::SWING_INPUTS + 1, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerExpTop + rowSpacingExp * 5), true, module, ClockedExpander::SWING_INPUTS + 2, mode));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((ClockedExpander*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((ClockedExpander*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelClockedExpander = createModel<ClockedExpander, ClockedExpanderWidget>("Clocked-Expander");
