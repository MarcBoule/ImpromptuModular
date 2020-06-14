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
	float leftMessages[2][1 + 2 + Sequencer::NUM_TRACKS] = {};// messages from mother


	// No need to save
	int panelTheme;
	unsigned int expanderRefreshCounter = 0;	


	FoundryExpander() {
		config(NUM_PARAMS, NUM_INPUTS, 0, NUM_LIGHTS);
	
		configParam(SYNC_SEQCV_PARAM, 0.0f, 1.0f, 0.0f, "Sync Seq#");// 1.0f is top position
		configParam(WRITEMODE_PARAM, 0.0f, 1.0f, 0.0f, "Write mode");
	
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
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
			}		

			// From Mother
			lights[WRITE_SEL_LIGHTS + 0].setBrightness(motherPresent ? messagesFromMother[1] : 0.0f);
			lights[WRITE_SEL_LIGHTS + 1].setBrightness(motherPresent ? messagesFromMother[2] : 0.0f);			
			for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
				lights[WRITECV2_LIGHTS + trkn].setBrightness(motherPresent ? messagesFromMother[trkn + 3] : 0.0f);
			}	
		}// expanderRefreshCounter
	}// process()
};


struct FoundryExpanderWidget : ModuleWidget {
	SvgPanel* darkPanel;
	
	FoundryExpanderWidget(FoundryExpander *module) {
		setModule(module);
	
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/FoundryExpander.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/FoundryExpander_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), module ? &module->panelTheme : NULL));

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
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh - rowSpacingExp * 4 + 2*se), true, module, FoundryExpander::SEQCV_INPUTS + 0, module ? &module->panelTheme : NULL));

		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh - rowSpacingExp * 4 + 2*se), true, module, FoundryExpander::SEQCV_INPUTS + 2, module ? &module->panelTheme : NULL));
		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh - rowSpacingExp * 4 + 2*se), true, module, FoundryExpander::TRKCV_INPUT, module ? &module->panelTheme : NULL));
		
		// Seq C,D and write source cv 
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh - rowSpacingExp * 3 + 2*se), true, module, FoundryExpander::SEQCV_INPUTS + 1, module ? &module->panelTheme : NULL));

		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh - rowSpacingExp * 3 + 2*se), true, module, FoundryExpander::SEQCV_INPUTS + 3, module ? &module->panelTheme : NULL));

		addParam(createParamCentered<CKSSNoRandom>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh - rowSpacingExp * 3 + 2*se), module, FoundryExpander::SYNC_SEQCV_PARAM));// 1.0f is top position

		
		// Gate, tied, slide
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh - rowSpacingExp * 2 + se), true, module, FoundryExpander::GATECV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh - rowSpacingExp * 2 + se), true, module, FoundryExpander::TIEDCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh - rowSpacingExp * 2 + se), true, module, FoundryExpander::SLIDECV_INPUT, module ? &module->panelTheme : NULL));

		// GateP, left, right
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh - rowSpacingExp * 1 + se), true, module, FoundryExpander::GATEPCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh - rowSpacingExp * 1 + se), true, module, FoundryExpander::LEFTCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh - rowSpacingExp * 1 + se), true, module, FoundryExpander::RIGHTCV_INPUT, module ? &module->panelTheme : NULL));
	
		
		// before-last row
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBHigh), true, module, FoundryExpander::VEL_INPUTS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(VecPx(colRulerExp - colOffsetX + writeLEDoffsetX, rowRulerBHigh + writeLEDoffsetY), module, FoundryExpander::WRITECV2_LIGHTS + 0));
		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBHigh), true, module, FoundryExpander::VEL_INPUTS + 2, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(VecPx(colRulerExp - writeLEDoffsetX, rowRulerBHigh + writeLEDoffsetY), module, FoundryExpander::WRITECV2_LIGHTS + 2));

		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colRulerExp + colOffsetX, rowRulerBHigh + 18), module, FoundryExpander::WRITEMODE_PARAM, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(VecPx(colRulerExp + colOffsetX - 12, rowRulerBHigh + 3), module, FoundryExpander::WRITE_SEL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<RedLight>>(VecPx(colRulerExp + colOffsetX + 12, rowRulerBHigh + 3), module, FoundryExpander::WRITE_SEL_LIGHTS + 1));
		
		// last row
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp - colOffsetX, rowRulerBLow), true, module, FoundryExpander::VEL_INPUTS + 1, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(VecPx(colRulerExp - colOffsetX + writeLEDoffsetX, rowRulerBLow - writeLEDoffsetY), module, FoundryExpander::WRITECV2_LIGHTS + 1));

		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp, rowRulerBLow), true, module, FoundryExpander::VEL_INPUTS + 3, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(VecPx(colRulerExp - writeLEDoffsetX, rowRulerBLow - writeLEDoffsetY), module, FoundryExpander::WRITECV2_LIGHTS + 3));
		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colRulerExp + colOffsetX, rowRulerBLow), true, module, FoundryExpander::WRITE_SRC_INPUT, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((FoundryExpander*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((FoundryExpander*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelFoundryExpander = createModel<FoundryExpander, FoundryExpanderWidget>("Foundry-Expander");
