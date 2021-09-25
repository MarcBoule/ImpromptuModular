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


struct PhraseSeqExpander : Module {
	enum InputIds {
		GATE1CV_INPUT,
		GATE2CV_INPUT,
		TIEDCV_INPUT,
		SLIDECV_INPUT,
		MODECV_INPUT,// needs connected
		NUM_INPUTS
	};


	// Expander
	float leftMessages[2][1] = {};// messages from mother


	// No need to save
	int panelTheme;
	unsigned int expanderRefreshCounter = 0;	


	PhraseSeqExpander() {
		config(0, NUM_INPUTS, 0, 0);
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}


	void process(const ProcessArgs &args) override {		
		expanderRefreshCounter++;
		if (expanderRefreshCounter >= expanderRefreshStepSkips) {
			expanderRefreshCounter = 0;
			
			bool motherPresent = leftExpander.module && (leftExpander.module->model == modelPhraseSeq16 || leftExpander.module->model == modelPhraseSeq32);
			if (motherPresent) {
				// To Mother
				float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
				int i = 0;
				for (; i < NUM_INPUTS - 1; i++) {
					messagesToMother[i] = inputs[i].getVoltage();
				}
				messagesToMother[i] = (inputs[i].isConnected() ? inputs[i].getVoltage() : std::numeric_limits<float>::quiet_NaN());
				leftExpander.module->rightExpander.messageFlipRequested = true;
					
				// From Mother
				float *messagesFromMother = (float*)leftExpander.consumerMessage;
				panelTheme = clamp((int)(messagesFromMother[0] + 0.5f), 0, 1);
			}		
		}// expanderRefreshCounter			
	}// process()
};


struct PhraseSeqExpanderWidget : ModuleWidget {
	int lastPanelTheme = -1;
	
	PhraseSeqExpanderWidget(PhraseSeqExpander *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
	
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/PhraseSeqExpander.svg")));
		panel->addChild(new InverterWidget(panel->box.size, mode));
		
		// Screws
		panel->addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), mode));
		panel->addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), mode));

		// Expansion module
		static const int rowRulerExpTop = 77;
		static const int rowSpacingExp = 60;
		static const int colExp = 30;
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowRulerExpTop + rowSpacingExp * 0), true, module, PhraseSeqExpander::GATE1CV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowRulerExpTop + rowSpacingExp * 1), true, module, PhraseSeqExpander::GATE2CV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowRulerExpTop + rowSpacingExp * 2), true, module, PhraseSeqExpander::TIEDCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowRulerExpTop + rowSpacingExp * 3), true, module, PhraseSeqExpander::SLIDECV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colExp, rowRulerExpTop + rowSpacingExp * 4), true, module, PhraseSeqExpander::MODECV_INPUT, mode));
	}
	
	void step() override {
		if (module) {
			int panelTheme = (((PhraseSeqExpander*)module)->panelTheme);
			if (panelTheme != lastPanelTheme) {
				((FramebufferWidget*)panel)->dirty = true;
				lastPanelTheme = panelTheme;
			}
		}
		Widget::step();
	}
};

Model *modelPhraseSeqExpander = createModel<PhraseSeqExpander, PhraseSeqExpanderWidget>("Phrase-Seq-Expander");
