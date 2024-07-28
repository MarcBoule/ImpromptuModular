//***********************************************************************************************
//Eliminates polyphony of identical overlapping notes; for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Pyer. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************

#include "ImpromptuModular.hpp"


struct NoteFilter : Module {	

	enum ParamIds {
		GATESD_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		GATE_INPUT,
		CV2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE_OUTPUT,
		CV2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	
	// Expander
	// none

	// Constants
	static const int MAXSD = 5;
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	float currCv[16];
	float currGate[16];
	float currCv2[16];
	
	// No need to save, with reset
	float gateDelReg[16][MAXSD];

	// No need to save, no reset
	RefreshCounter refresh;
	TriggerRiseFall gateTriggers[16];


	int getSdKnob() {
		return (int)(params[GATESD_PARAM].getValue() + 0.5f);
	}

	
	NoteFilter() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(GATESD_PARAM, 0.0f, (float)MAXSD, 0.0f, "Gate input sample delay");
		paramQuantities[GATESD_PARAM]->snapEnabled = true;
				
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(CV2_INPUT, "CV2/Velocity");

		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV2_OUTPUT, "CV2/Velocity");
		
		configBypass(CV_INPUT, CV_OUTPUT);
		configBypass(GATE_INPUT, GATE_OUTPUT);
		configBypass(CV2_INPUT, CV2_OUTPUT);

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	void onReset() override final {
		for (int p = 0; p < 16; p++) {
			currCv[p] = 0.0f;
			currGate[p] = 0.0f;
			currCv2[p] = 0.0f;
		}
		resetNonJson();
	}
	void resetNonJson() {
		for (int p = 0; p < 16; p++) {
			for (int d = 0; d < MAXSD; d++) {
				gateDelReg[p][d] = 0.0f;
			}
		}
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// currCv
		json_t *currCvJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(currCvJ, i, json_real(currCv[i]));
		json_object_set_new(rootJ, "currCv", currCvJ);

		// currGate
		json_t *currGateJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(currGateJ, i, json_real(currGate[i]));
		json_object_set_new(rootJ, "currGate", currGateJ);

		// currCv2
		json_t *currCv2J = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(currCv2J, i, json_real(currCv2[i]));
		json_object_set_new(rootJ, "currCv2", currCv2J);

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// panelContrast
		json_t *panelContrastJ = json_object_get(rootJ, "panelContrast");
		if (panelContrastJ)
			panelContrast = json_number_value(panelContrastJ);

		// currCv
		json_t *currCvJ = json_object_get(rootJ, "currCv");
		if (currCvJ) {
			for (int i = 0; i < 16; i++)
			{
				json_t *currCvArrayJ = json_array_get(currCvJ, i);
				if (currCvArrayJ)
					currCv[i] = json_number_value(currCvArrayJ);
			}
		}

		// currGate
		json_t *currGateJ = json_object_get(rootJ, "currGate");
		if (currGateJ) {
			for (int i = 0; i < 16; i++)
			{
				json_t *currGateArrayJ = json_array_get(currGateJ, i);
				if (currGateArrayJ)
					currGate[i] = json_number_value(currGateArrayJ);
			}
		}

		// currCv2
		json_t *currCv2J = json_object_get(rootJ, "currCv2");
		if (currCv2J) {
			for (int i = 0; i < 16; i++)
			{
				json_t *currCv2ArrayJ = json_array_get(currCv2J, i);
				if (currCv2ArrayJ)
					currCv2[i] = json_number_value(currCv2ArrayJ);
			}
		}

		resetNonJson();
	}


	void onSampleRateChange() override {
	}		


	void process(const ProcessArgs &args) override {
		// user inputs
		if (refresh.processInputs()) {
		}// userInputs refresh
	
		int delay = getSdKnob();
		int poly = inputs[GATE_INPUT].getChannels();
		if (outputs[CV_OUTPUT].getChannels() != poly) {
			outputs[CV_OUTPUT].setChannels(poly);
		}
		if (outputs[GATE_OUTPUT].getChannels() != poly) {
			if (poly > 0) {
				for (int p = poly - 1; p < outputs[GATE_OUTPUT].getChannels(); p++) {
					for (int d = 0; d < MAXSD; d++) {
						gateDelReg[p][d] = 0.0f;
					}
					gateTriggers[p].reset();
				}
			}
			outputs[GATE_OUTPUT].setChannels(poly);
			
		}
		if (outputs[CV2_OUTPUT].getChannels() != poly) {
			outputs[CV2_OUTPUT].setChannels(poly);
		}

		// sd the gate
		for (int p = 0; p < poly; p++) {
			float gateIn = delay == 0 ? inputs[GATE_INPUT].getVoltage(p) : gateDelReg[p][delay - 1];
			int gateEdge = gateTriggers[p].process(gateIn);
			if (gateEdge == 1) {
				// rising gate
				float newCv = inputs[CV_INPUT].getVoltage(p);
				
				// filter check
				bool foundSame = false;
				for (int p2 = 0; p2 < poly; p2++) {
					if (p2 == p) continue;
					if ((newCv == currCv[p2]) && (currGate[p2] > 0.5f)) {
						foundSame = true;
						break;
					}
				}
				
				// keep if not redundant
				if (!foundSame) {
					currCv[p] = newCv;
					currGate[p] = gateIn;
					currCv2[p] = inputs[CV2_INPUT].getVoltage(p);
				}
			}
			else if (gateEdge == -1) {
				// falling gate
				currGate[p] = gateIn;
			}
			
			outputs[CV_OUTPUT].setVoltage(currCv[p], p);
			outputs[GATE_OUTPUT].setVoltage(currGate[p], p);
			outputs[CV2_OUTPUT].setVoltage(currCv2[p], p);
		}


		// lights
		if (refresh.processLights()) {
			// lights
			// simple ones done in NoteFilterWidget::step()

		}// lightRefreshCounter

		// sd the gate
		for (int p = 0; p < poly; p++) {
			gateDelReg[p][4] = gateDelReg[p][3];
			gateDelReg[p][3] = gateDelReg[p][2];
			gateDelReg[p][2] = gateDelReg[p][1];
			gateDelReg[p][1] = gateDelReg[p][0];
			gateDelReg[p][0] = inputs[GATE_INPUT].getVoltage(p);
		}

	}// process()
};



struct NoteFilterWidget : ModuleWidget {
	typedef IMMediumKnob LoopKnob;

	void appendContextMenu(Menu *menu) override {
		NoteFilter *module = static_cast<NoteFilter*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), static_cast<SvgPanel*>(getPanel()));
	}	

	
	NoteFilterWidget(NoteFilter *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/NoteFilter.svg")));
		SvgPanel* svgPanel = static_cast<SvgPanel*>(getPanel());
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel, mode));	
		
		// Screws
		// svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		// svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));
		
		static const float col1 = 5.08f * 2.0f;		
		
		static const float row0 = 20.0f;// CV
		static const float row1 = row0 + 15.0f;// Gate
		static const float row2 = row1 + 15.0f;// GATE SD
		static const float row3 = row2 + 15.0f;// CV2
		
		static const float row6 = 112.0f;// CV2
		static const float row5 = row6 - 15.0f;// Gate
		static const float row4 = row5 - 15.0f;// CV
		
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row0)), true, module, NoteFilter::CV_INPUT, mode));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row1)), true, module, NoteFilter::GATE_INPUT, mode));
		addParam(createParamCentered<IMSmallKnob>(mm2px(Vec(col1, row2)), module, NoteFilter::GATESD_PARAM));	

		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row3)), true, module, NoteFilter::CV2_INPUT, mode));
		
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row4)), false, module, NoteFilter::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row5)), false, module, NoteFilter::GATE_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row6)), false, module, NoteFilter::CV2_OUTPUT, mode));		

	}
};

Model *modelNoteFilter = createModel<NoteFilter, NoteFilterWidget>("NoteFilter");
