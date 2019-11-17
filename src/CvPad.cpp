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
		BANK_PARAM,
		WRITE_PARAM,
		CV_PARAM,
		SHARP_PARAM,
		QUANTIZE_PARAM,
		AUTOSTEP_PARAM,
		DETACH_PARAM,
		CONFIG_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		BANK_INPUT,
		CV_INPUT,
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
	int readHead;

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	RefreshCounter refresh;
	int bank = 0;
	Trigger padTriggers[N_PADS];
	Trigger writeTrigger;
	
	
	inline int calcBank() {
		float bankInputValue = inputs[BANK_INPUT].getVoltage() / 10.0f * (8.0f - 1.0f);
		return (int) clamp(std::round(params[BANK_PARAM].getValue() + bankInputValue), 0.0f, (8.0f - 1.0f));		
	}
	
	
	CvPad() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		for (int p = 0; p < N_PADS; p++) {
			configParam(PAD_PARAMS + p, 0.0f, 1.0f, 0.0f, string::f("CV pad %i", p + 1));
		}
		configParam(BANK_PARAM, 0.0f, 8.0f - 1.0f, 0.0f, "Bank", "", 0.0f, 1.0f, 1.0f);	// base, multiplier, offset
		configParam(WRITE_PARAM, 0.0f, 1.0f, 0.0f, "Write");				
		configParam(CV_PARAM, -INFINITY, INFINITY, 0.0f, "CV");		
		configParam(SHARP_PARAM, 0.0f, 2.0f, 0.0f, "Volts / Notation");// 0 is top position
		configParam(QUANTIZE_PARAM, 0.0f, 1.0f, 0.0f, "Quantize");
		configParam(AUTOSTEP_PARAM, 0.0f, 1.0f, 0.0f, "Autostep when write");
		configParam(DETACH_PARAM, 0.0f, 1.0f, 0.0f, "Detach");
		configParam(CONFIG_PARAM, 0.0f, 2.0f, 2.0f, "Configuration");// 0 is top position
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		for (int b = 0; b < N_BANKS; b++) {
			for (int p = 0; p < N_PADS; p++) {
				cvs[b][p] = 0.0f;
			}
		}
		readHead = 0;
		resetNonJson();
	}
	void resetNonJson() {
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
		
		// readHead
		json_object_set_new(rootJ, "readHead", json_integer(readHead));

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

		// readHead
		json_t *readHeadJ = json_object_get(rootJ, "readHead");
		if (readHeadJ)
			readHead = json_integer_value(readHeadJ);
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		
		bank = calcBank();
		
		if (refresh.processInputs()) {
			// pads
			for (int p = 0; p < N_PADS; p++) {
				if (padTriggers[p].process(params[PAD_PARAMS + p].getValue())) {
					readHead = p;
				}
				
			}
			
			// write
			if (writeTrigger.process(params[WRITE_PARAM].getValue())) {
				cvs[bank][readHead] = inputs[CV_INPUT].getVoltage();
			}
		}// userInputs refresh
		
		
		
		// gate output
		outputs[GATE_OUTPUTS + 3].setVoltage(padTriggers[readHead].isHigh() ? 10.0f : 0.0f);
		outputs[CV_OUTPUTS + 3].setVoltage(cvs[bank][readHead]);
		
		// lights
		if (refresh.processLights()) {
			for (int l = 0; l < 16; l++) {
				lights[PAD_LIGHTS + l * 2 + 0].setBrightness(readHead == l ? 1.0f : 0.0f);// green
				lights[PAD_LIGHTS + l * 2 + 1].setBrightness(0.0f);// red
			}
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

	struct BankDisplayWidget : TransparentWidget {
		CvPad *module;
		std::shared_ptr<Font> font;
		
		BankDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			//nvgTextLetterSpacing(args.vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
			nvgFillColor(args.vg, textColor);
			char displayStr[2];
			unsigned int bank = (unsigned)(module ? module->bank : 0);
			snprintf(displayStr, 2, "%1u", (unsigned) (bank + 1) );
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};


	struct CvDisplayWidget : TransparentWidget {
		CvPad *module;
		std::shared_ptr<Font> font;
		char text[7];

		CvDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void cvToStr(void) {
			if (module == NULL) {
				snprintf(text, 7, " 0,000");
			} 
			else {
				int bank = module->calcBank();
				float cvVal = module->cvs[bank][module->readHead];
				if (module->params[CvPad::SHARP_PARAM].getValue() > 0.5f) {// show notes
					text[0] = ' ';
					printNote(cvVal, &text[1], module->params[CvPad::SHARP_PARAM].getValue() < 1.5f);
				}
				else  {// show volts
					float cvValPrint = std::fabs(cvVal);
					cvValPrint = (cvValPrint > 9.999f) ? 9.999f : cvValPrint;
					snprintf(text, 7, " %4.3f", cvValPrint);// Four-wide, three positions after the decimal, left-justified
					text[0] = (cvVal<0.0f) ? '-' : ' ';
					text[2] = ',';
				}
			}
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -1.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~~~~~~", NULL);
			nvgFillColor(args.vg, textColor);
			cvToStr();
			nvgText(args.vg, textPos.x, textPos.y, text, NULL);
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
		// ----------------------------
		
		static const int padX = 71 + 21;
		static const int padXd = 68;
		static const int padY = 98 + 21;
		static const int padYd = padXd;
		static const int ledOffsetY = 30;
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				// pad
				addParam(createParamCentered<Pad>(Vec(padX + padXd * x, padY + padYd * y), module, CvPad::PAD_PARAMS + y * 4 + x));
				// light
				addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(padX + padXd * x, padY + padYd * y - ledOffsetY), module, CvPad::PAD_LIGHTS + (y * 4 + x) * 2 + 0));
			}
		}
		
		
		// left side column
		// ----------------------------
		
		static const int leftX = 35;
		static const int topY = padY - 64;
		// Volt/sharp/flat switch
		addParam(createParamCentered<CKSSThreeInvNoRandom>(Vec(leftX, topY), module, CvPad::SHARP_PARAM));
		// quantize
		addParam(createParamCentered<CKSSNoRandom>(Vec(leftX, padY), module, CvPad::QUANTIZE_PARAM));
		// autostep
		addParam(createParamCentered<CKSSNoRandom>(Vec(leftX, padY + padYd), module, CvPad::AUTOSTEP_PARAM));
		// detach
		addParam(createParamCentered<CKSSNoRandom>(Vec(leftX, padY + padYd * 2), module, CvPad::DETACH_PARAM));
		// config
		addParam(createParamCentered<CKSSThreeInvNoRandom>(Vec(leftX, padY + padYd * 3), module, CvPad::CONFIG_PARAM));
		
		
		// right side column
		// ----------------------------
		
		static const int rightX = 379;
		static const int rightO = 21;
		
		// bank knob
		addParam(createDynamicParamCentered<IMSmallSnapKnob>(Vec(rightX - rightO, topY), module, CvPad::BANK_PARAM, module ? &module->panelTheme : NULL));
		// write button
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(rightX + rightO, topY), module, CvPad::WRITE_PARAM, module ? &module->panelTheme : NULL));	
		// bank input
		addInput(createDynamicPortCentered<IMPort>(Vec(rightX - rightO, padY), true, module, CvPad::BANK_INPUT, module ? &module->panelTheme : NULL));
		// cv input
		addInput(createDynamicPortCentered<IMPort>(Vec(rightX + rightO, padY), true, module, CvPad::CV_INPUT, module ? &module->panelTheme : NULL));
		// outputs
		static const int outY = padY + padYd * 3;
		static const int outYd = 45;
		for (int y = 0; y < 4; y++) {
			addOutput(createDynamicPortCentered<IMPort>(Vec(rightX - rightO, outY - outYd * (3 - y)), false, module, CvPad::CV_OUTPUTS + y, module ? &module->panelTheme : NULL));
			addOutput(createDynamicPortCentered<IMPort>(Vec(rightX + rightO, outY - outYd * (3 - y)), false, module, CvPad::GATE_OUTPUTS + y, module ? &module->panelTheme : NULL));
		}
		
		
		// top of pad, first row
		// ----------------------------

		// cv display
		CvDisplayWidget *displayCv = new CvDisplayWidget();
		displayCv->box.size = Vec(98, 30);// 6 characters (ex.: "-1,234")
		displayCv->box.pos = Vec(padX + padXd / 2, topY);
		displayCv->box.pos = displayCv->box.pos.minus(displayCv->box.size.div(2));// centering
		displayCv->module = module;
		addChild(displayCv);
		// cv knob
		addParam(createDynamicParamCentered<IMBigKnobInf>(Vec(padX + padXd * 2, topY), module, CvPad::CV_PARAM, module ? &module->panelTheme : NULL));
		// bank display
		BankDisplayWidget *displayBank = new BankDisplayWidget();
		displayBank->box.size = Vec(24, 30);// 1 character
		displayBank->box.pos = Vec(padX + padXd * 3, topY);
		displayBank->box.pos = displayBank->box.pos.minus(displayBank->box.size.div(2));// centering
		displayBank->module = module;
		addChild(displayBank);	

		
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
