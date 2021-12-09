//***********************************************************************************************
//Expander module for Foundry, by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé, Nigel Sixsmith
//
//***********************************************************************************************


#include "FoundrySequencer.hpp"


struct FoundryExpander : Module {
	enum ParamIds {
		SYNC_SEQCV_PARAM,
		WRITEMODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(VEL_INPUTS, Sequencer::NUM_TRACKS),// needs connected
		ENUMS(SEQCV_INPUTS, Sequencer::NUM_TRACKS),// needs connected
		TRKCV_INPUT,// needs connected
		GATECV_INPUT,
		GATEPCV_INPUT,
		TIEDCV_INPUT,
		SLIDECV_INPUT,
		WRITE_SRC_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		NUM_INPUTS
	};
	
	enum LightIds {
		ENUMS(WRITE_SEL_LIGHTS, 2),
		ENUMS(WRITECV2_LIGHTS, Sequencer::NUM_TRACKS),
		NUM_LIGHTS
	};
	
	// Expander
	float leftMessages[2][2 + 2 + Sequencer::NUM_TRACKS] = {};// messages from mother


	// No need to save
	int panelTheme;
	float panelContrast;
	unsigned int expanderRefreshCounter = 0;	


	FoundryExpander() {
		config(NUM_PARAMS, NUM_INPUTS, 0, NUM_LIGHTS);
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
	
		configSwitch(SYNC_SEQCV_PARAM, 0.0f, 1.0f, 0.0f, "Synchronize Seq# changes", {"No", "Yes"});// 1.0f is top position
		configParam(WRITEMODE_PARAM, 0.0f, 1.0f, 0.0f, "Write mode");
	
		getParamQuantity(SYNC_SEQCV_PARAM)->randomizeEnabled = false;		

		for (int i = 0; i < Sequencer::NUM_TRACKS; i++) {
			configInput(VEL_INPUTS + i, string::f("Track %c CV2", i + 'A'));
			configInput(SEQCV_INPUTS + i, string::f("Track %c seq#", i + 'A'));
		}
		configInput(TRKCV_INPUT, "Track select");
		configInput(GATECV_INPUT, "Gate");
		configInput(GATEPCV_INPUT, "Gate probability");
		configInput(TIEDCV_INPUT, "Tied");
		configInput(SLIDECV_INPUT, "Slide");
		configInput(WRITE_SRC_INPUT, "Write mode");
		configInput(LEFTCV_INPUT, "Step left");
		configInput(RIGHTCV_INPUT, "Step right");
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	void process(const ProcessArgs &args) override {		
		expanderRefreshCounter++;
		if (expanderRefreshCounter >= expanderRefreshStepSkips) {
			expanderRefreshCounter = 0;
			
			bool motherPresent = leftExpander.module && leftExpander.module->model == modelFoundry;
			float *messagesFromMother = (float*)leftExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
			if (motherPresent) {
				// To Mother
				float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
				int i = 0;
				for (; i < GATECV_INPUT; i++) {
					messagesToMother[i] = (inputs[i].isConnected() ? inputs[i].getVoltage() : std::numeric_limits<float>::quiet_NaN());
				}
				for (; i < NUM_INPUTS; i++) {
					messagesToMother[i] = inputs[i].getVoltage();
				}
				messagesToMother[i++] = params[SYNC_SEQCV_PARAM].getValue();
				messagesToMother[i++] = params[WRITEMODE_PARAM].getValue();
				leftExpander.module->rightExpander.messageFlipRequested = true;

				// From Mother
				panelTheme = clamp((int)(messagesFromMother[0] + 0.5f), 0, 1);
				panelContrast = clamp(messagesFromMother[1], 0.0f, 255.0f);
			}		

			// From Mother (done outside since turn off leds with no mother; has its own motherPresent guards)
			lights[WRITE_SEL_LIGHTS + 0].setBrightness(motherPresent ? messagesFromMother[2] : 0.0f);
			lights[WRITE_SEL_LIGHTS + 1].setBrightness(motherPresent ? messagesFromMother[3] : 0.0f);			
			for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
				lights[WRITECV2_LIGHTS + trkn].setBrightness(motherPresent ? messagesFromMother[4 + trkn] : 0.0f);
			}	
		}// expanderRefreshCounter
	}// process()
};


struct FoundryExpanderWidget : ModuleWidget {
	int lastPanelTheme = -1;
	float lastPanelContrast = -1.0f;
	
	FoundryExpanderWidget(FoundryExpander *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
	
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/FoundryExpander.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		// Expansion module
		static const int rowSpacingExp = 49;
		static const int colRulerExp = box.size.x / 2;
		static const int colOffsetX = 44;
		static const int se = -10;
		
		static const int rowRulerBLow = 335;
		static const int rowRulerBHigh = 286;
		static const int writeLEDoffsetX = 16;
		static const int writeLEDoffsetY = 18;

		
		// Seq A,B and track row
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh - rowSpacingExp * 4 + 2*se), true, module, FoundryExpander::SEQCV_INPUTS + 0, mode));

		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh - rowSpacingExp * 4 + 2*se), true, module, FoundryExpander::SEQCV_INPUTS + 2, mode));
		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh - rowSpacingExp * 4 + 2*se), true, module, FoundryExpander::TRKCV_INPUT, mode));
		
		// Seq C,D and write source cv 
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh - rowSpacingExp * 3 + 2*se), true, module, FoundryExpander::SEQCV_INPUTS + 1, mode));

		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh - rowSpacingExp * 3 + 2*se), true, module, FoundryExpander::SEQCV_INPUTS + 3, mode));

		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh - rowSpacingExp * 3 + 2*se), module, FoundryExpander::SYNC_SEQCV_PARAM, mode, svgPanel));// 1.0f is top position

		
		// Gate, tied, slide
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh - rowSpacingExp * 2 + se), true, module, FoundryExpander::GATECV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh - rowSpacingExp * 2 + se), true, module, FoundryExpander::TIEDCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh - rowSpacingExp * 2 + se), true, module, FoundryExpander::SLIDECV_INPUT, mode));

		// GateP, left, right
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh - rowSpacingExp * 1 + se), true, module, FoundryExpander::GATEPCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh - rowSpacingExp * 1 + se), true, module, FoundryExpander::LEFTCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh - rowSpacingExp * 1 + se), true, module, FoundryExpander::RIGHTCV_INPUT, mode));
	
		
		// before-last row
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh), true, module, FoundryExpander::VEL_INPUTS + 0, mode));
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(colRulerExp - colOffsetX + writeLEDoffsetX, rowRulerBHigh + writeLEDoffsetY), module, FoundryExpander::WRITECV2_LIGHTS + 0));
		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh), true, module, FoundryExpander::VEL_INPUTS + 2, mode));
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(colRulerExp - writeLEDoffsetX, rowRulerBHigh + writeLEDoffsetY), module, FoundryExpander::WRITECV2_LIGHTS + 2));

		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh + 18), module, FoundryExpander::WRITEMODE_PARAM, mode));
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(colRulerExp + colOffsetX - 12, rowRulerBHigh + 3), module, FoundryExpander::WRITE_SEL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(colRulerExp + colOffsetX + 12, rowRulerBHigh + 3), module, FoundryExpander::WRITE_SEL_LIGHTS + 1));
		
		// last row
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBLow), true, module, FoundryExpander::VEL_INPUTS + 1, mode));
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(colRulerExp - colOffsetX + writeLEDoffsetX, rowRulerBLow - writeLEDoffsetY), module, FoundryExpander::WRITECV2_LIGHTS + 1));

		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBLow), true, module, FoundryExpander::VEL_INPUTS + 3, mode));
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(colRulerExp - writeLEDoffsetX, rowRulerBLow - writeLEDoffsetY), module, FoundryExpander::WRITECV2_LIGHTS + 3));
		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp + colOffsetX, rowRulerBLow), true, module, FoundryExpander::WRITE_SRC_INPUT, mode));
	}
	
	void step() override {
		if (module) {
			int panelTheme = (((FoundryExpander*)module)->panelTheme);
			float panelContrast = (((FoundryExpander*)module)->panelContrast);
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

Model *modelFoundryExpander = createModel<FoundryExpander, FoundryExpanderWidget>("Foundry-Expander");
