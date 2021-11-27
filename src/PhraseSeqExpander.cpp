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
	float leftMessages[2][2] = {};// messages from mother


	// No need to save
	int panelTheme;
	float panelContrast;
	unsigned int expanderRefreshCounter = 0;	


	PhraseSeqExpander() {
		config(0, NUM_INPUTS, 0, 0);
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		configInput(GATE1CV_INPUT, "Gate 1");
		configInput(GATE2CV_INPUT, "Gate 2");
		configInput(TIEDCV_INPUT, "Tied");
		configInput(SLIDECV_INPUT, "Slide");
		configInput(MODECV_INPUT, "Mode");

		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
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
				panelContrast = clamp(messagesFromMother[1], 0.0f, 255.0f);
			}		
		}// expanderRefreshCounter			
	}// process()
};


struct PhraseSeqExpanderWidget : ModuleWidget {
	int lastPanelTheme = -1;
	float lastPanelContrast = -1.0f;
	
	PhraseSeqExpanderWidget(PhraseSeqExpander *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
	
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/PhraseSeqExpander.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

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
			float panelContrast = (((PhraseSeqExpander*)module)->panelContrast);
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

Model *modelPhraseSeqExpander = createModel<PhraseSeqExpander, PhraseSeqExpanderWidget>("Phrase-Seq-Expander");
