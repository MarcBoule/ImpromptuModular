//***********************************************************************************************
//Expander module for GateSeq64, by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct GateSeq64Expander : Module {
	enum InputIds {
		GATE_INPUT,// needs connected
		PROB_INPUT,// needs connected
		WRITE_INPUT,
		WRITE1_INPUT,
		WRITE0_INPUT,
		STEPL_INPUT,
		NUM_INPUTS
	};


	// Expander
	float leftMessages[2][2] = {};// messages from mother


	// No need to save
	int panelTheme;
	float panelContrast;
	unsigned int expanderRefreshCounter = 0;	


	GateSeq64Expander() {
		config(0, NUM_INPUTS, 0, 0);
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		configInput(GATE_INPUT, "Gate");
		configInput(PROB_INPUT, "Probability");
		configInput(WRITE_INPUT, "Write");
		configInput(WRITE1_INPUT, "Write 1 (on)");
		configInput(WRITE0_INPUT, "Write 0 (off)");
		configInput(STEPL_INPUT, "Step left");

		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	void process(const ProcessArgs &args) override {		
		expanderRefreshCounter++;
		if (expanderRefreshCounter >= expanderRefreshStepSkips) {
			expanderRefreshCounter = 0;
			
			bool motherPresent = (leftExpander.module && leftExpander.module->model == modelGateSeq64);
			if (motherPresent) {
				// To Mother
				float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
				messagesToMother[0] = (inputs[0].isConnected() ? inputs[0].getVoltage() : std::numeric_limits<float>::quiet_NaN());
				messagesToMother[1] = (inputs[1].isConnected() ? inputs[1].getVoltage() : std::numeric_limits<float>::quiet_NaN());
				for (int i = 2; i < NUM_INPUTS; i++) {
					messagesToMother[i] = inputs[i].getVoltage();
				}
				leftExpander.module->rightExpander.messageFlipRequested = true;

				// From Mother
				float *messagesFromMother = (float*)leftExpander.consumerMessage;
				panelTheme = clamp((int)(messagesFromMother[0] + 0.5f), 0, 1);
				panelContrast = clamp(messagesFromMother[1], 0.0f, 255.0f);
			}		
		}// expanderRefreshCounter
	}// process()
};


struct GateSeq64ExpanderWidget : ModuleWidget {
	int lastPanelTheme = -1;
	float lastPanelContrast = -1.0f;
	
	GateSeq64ExpanderWidget(GateSeq64Expander *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
	
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/GateSeq64Expander.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		// Expansion module
		static const int rowExpTop = 72;
		static const int rowSpacingExp = 50;
		static const int colExp = 30;
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowExpTop + rowSpacingExp * 0), true, module, GateSeq64Expander::WRITE_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowExpTop + rowSpacingExp * 1), true, module, GateSeq64Expander::GATE_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowExpTop + rowSpacingExp * 2), true, module, GateSeq64Expander::PROB_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowExpTop + rowSpacingExp * 3), true, module, GateSeq64Expander::WRITE0_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowExpTop + rowSpacingExp * 4), true, module, GateSeq64Expander::WRITE1_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowExpTop + rowSpacingExp * 5), true, module, GateSeq64Expander::STEPL_INPUT, mode));
	}
	
	void step() override {
		if (module) {
			int panelTheme = (((GateSeq64Expander*)module)->panelTheme);
			float panelContrast = (((GateSeq64Expander*)module)->panelContrast);
			if (panelTheme != lastPanelTheme || panelContrast != lastPanelContrast) {
				SvgPanel* svgPanel = (SvgPanel*)getPanel();
				svgPanel->fb->dirty = true;
				lastPanelTheme = panelTheme;
				lastPanelContrast = panelContrast;
			}
		}
		Widget::step();
	}
};

Model *modelGateSeq64Expander = createModel<GateSeq64Expander, GateSeq64ExpanderWidget>("Gate-Seq-64-Expander");
