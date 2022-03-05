//***********************************************************************************************
//Polyphonic gate splitter module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
// The idea for this module was provided by Andre Belt
// https://community.vcvrack.com/t/module-ideas/7175/231
// 
//***********************************************************************************************


#include "ImpromptuModular.hpp"

struct Variations : Module {
	enum ParamIds {
		MODE_PARAM,
		SPREAD_PARAM,
		OFFSET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		GATE_INPUT,
		SPREAD_INPUT,
		OFFSET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLAMP_LIGHT,
		NUM_LIGHTS
	};
		
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	float noise;
	float lowClamp;
	float highClamp;
	bool lowRangeSpread;
	bool lowRangeOffset;

	// No need to save, with reset
	// none
	
	// No need to save, no reset
	RefreshCounter refresh;
	Trigger gateTrigger;


	bool isNormalDist() {
		return params[MODE_PARAM].getValue() < 0.5f;
	}
	
	float getNewNoise() {
		float _noise = isNormalDist() ? (0.2f * random::normal()) : (random::uniform() * 2.0f - 1.0f);// all calibrated for +-1 V noise
		return _noise * 5.0f;// returns a +- 5V noise without clamping
	}
	
	float getSpreadValue() {
		float _spread = params[SPREAD_PARAM].getValue() + inputs[SPREAD_INPUT].getVoltage();
		return lowRangeSpread ? _spread * 0.2f : _spread;// +-1V noise in low range, else +-5V noise
	}

	float getOffsetValue() {
		float _offset = params[OFFSET_PARAM].getValue() + inputs[OFFSET_INPUT].getVoltage();
		return lowRangeOffset ? _offset * 0.333f : _offset;// +- 3.33V offset in low range, else +-10V offset
	}


	Variations() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configSwitch(MODE_PARAM, 0.0f, 1.0f, 0.0f, "Noise mode", {"Normal", "Uniform"});// default is 0.0f meaning left position (= V)
		configParam(SPREAD_PARAM, 0.0f, 1.0f, 0.2f, "Spread", " %", 0.0f, 100.0f, 0.0f);// diplay params are: base, mult, offset
		configParam(OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "Offset", " V");
		
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(SPREAD_INPUT, "Spread");
		configInput(OFFSET_INPUT, "Offset");

		configOutput(GATE_OUTPUT, "Gate thru");
		configOutput(CV_OUTPUT, "CV");

		configBypass(CV_INPUT, CV_OUTPUT);
		configBypass(GATE_INPUT, GATE_OUTPUT);

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	
	void onReset() override {
		noise = 0.0f;
		lowClamp = -10.0f;
		highClamp = 10.0f;
		lowRangeSpread = false;
		lowRangeOffset = false;
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

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// noise
		json_object_set_new(rootJ, "noise", json_real(noise));

		// lowClamp
		json_object_set_new(rootJ, "lowClamp", json_real(lowClamp));

		// highClamp
		json_object_set_new(rootJ, "highClamp", json_real(highClamp));

		// lowRangeSpread
		json_object_set_new(rootJ, "lowRangeSpread", json_boolean(lowRangeSpread));
		
		// lowRangeOffset
		json_object_set_new(rootJ, "lowRangeOffset", json_boolean(lowRangeOffset));
		
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
		
		// noise
		json_t *noiseJ = json_object_get(rootJ, "noise");
		if (noiseJ)
			noise = json_number_value(noiseJ);

		// lowClamp
		json_t *lowClampJ = json_object_get(rootJ, "lowClamp");
		if (lowClampJ)
			lowClamp = json_number_value(lowClampJ);

		// highClamp
		json_t *highClampJ = json_object_get(rootJ, "highClamp");
		if (highClampJ)
			highClamp = json_number_value(highClampJ);

		// lowRangeSpread
		json_t *lowRangeSpreadJ = json_object_get(rootJ, "lowRangeSpread");
		if (lowRangeSpreadJ)
			lowRangeSpread = json_is_true(lowRangeSpreadJ);
		
		// lowRangeOffset
		json_t *lowRangeOffsetJ = json_object_get(rootJ, "lowRangeOffset");
		if (lowRangeOffsetJ)
			lowRangeOffset = json_is_true(lowRangeOffsetJ);
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		// int numChan = inputs[GATE_INPUT].getChannels();
		
		if (refresh.processInputs()) {
			// outputs[GATE_OUTPUT].setChannels(numChan);
			// outputs[CV_OUTPUT].setChannels(inputs[CV_INPUT].getChannels());
		}// userInputs refresh
				
		if (gateTrigger.process(inputs[GATE_INPUT].getVoltage()) || !inputs[GATE_INPUT].isConnected()) {
			noise = getNewNoise();
		}
		
		float outCv = inputs[CV_INPUT].getVoltage();
		// spread and offset
		outCv += getSpreadValue() * noise;
		outCv += getOffsetValue();
		// clamper and its led
		if (outCv < lowClamp) {
			lights[CLAMP_LIGHT].setBrightness(1.0f);
			outCv = lowClamp;
		}
		else if (outCv > highClamp) {
			lights[CLAMP_LIGHT].setBrightness(1.0f);
			outCv = highClamp;
		}
		else {
			lights[CLAMP_LIGHT].setBrightness(0.0f);
		}
		// outputs
		outputs[CV_OUTPUT].setVoltage(outCv);
		outputs[GATE_OUTPUT].setVoltage(inputs[GATE_INPUT].getVoltage());// thru but with same sample delay as CV
		
		// lights
		if (refresh.processLights()) {
			// none, but need this since refresh counter is stepped in processLights()
		}
	}
};


struct VariationsWidget : ModuleWidget {

	struct CvClampQuantity : Quantity {
		bool isMaxClamper;
		float* clampPtr;
		
		CvClampQuantity(float* _clampPtr, bool _isMaxClamper) {
			clampPtr = _clampPtr;
			isMaxClamper = _isMaxClamper;
		}
		void setValue(float value) override {
			*clampPtr = math::clamp(value, getMinValue(), getMaxValue());
		}
		float getValue() override {
			return *clampPtr;
		}
		float getMinValue() override {return -10.0f;}
		float getMaxValue() override {return 10.0f;}
		float getDefaultValue() override {return isMaxClamper ? getMaxValue() : getMinValue();}
		float getDisplayValue() override {return *clampPtr;}
		std::string getDisplayValueString() override {
			return string::f("%.1f", *clampPtr);
		}
		void setDisplayValue(float displayValue) override {setValue(displayValue);}
		std::string getLabel() override {return isMaxClamper ? "Max CV out" : "Min CV out";}
		std::string getUnit() override {return "V";}
	};
	struct CvClampSlider : ui::Slider {
		CvClampSlider(float* clampPtr, bool isMaxClamper) {
			quantity = new CvClampQuantity(clampPtr, isMaxClamper);
		}
		~CvClampSlider() {
			delete quantity;
		}
	};


	void appendContextMenu(Menu *menu) override {
		Variations *module = dynamic_cast<Variations*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());
		
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createBoolPtrMenuItem("Low range spread", "", &module->lowRangeSpread));
		menu->addChild(createBoolPtrMenuItem("Low range offset", "", &module->lowRangeOffset));
		
		CvClampSlider *maxCvSlider = new CvClampSlider(&module->highClamp, true);
		maxCvSlider->box.size.x = 200.0f;
		menu->addChild(maxCvSlider);

		CvClampSlider *minCvSlider = new CvClampSlider(&module->lowClamp, false);
		minCvSlider->box.size.x = 200.0f;
		menu->addChild(minCvSlider);

	}	
	
	
	VariationsWidget(Variations *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Variations.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));
		
		
		const float colM = box.size.x / 2;
		static const int colL = 25;
		static const int colR = 65;
		
		static const int row0 = 56;// mode switch
		static const int row1 = 108;// spread knob
		static const int row2 = 172;// offset knob
		static const int row5 = 326;// cv and gate outputs
		static const int row4 = row5 - 54;// cv and gate inputs
		static const int row3 = row4 - 54;// spread and offset cv inputs
		

		// Noise mode switch
		addParam(createDynamicSwitchCentered<IMSwitch2H>(VecPx(colM, row0), module, Variations::MODE_PARAM, mode, svgPanel));		
		// Spread knob 
		addParam(createDynamicParamCentered<IMBigKnob>(VecPx(colM, row1), module, Variations::SPREAD_PARAM, mode));

		// Offset knob 
		addParam(createDynamicParamCentered<IMBigKnob>(VecPx(colM, row2), module, Variations::OFFSET_PARAM, mode));


		// Spread and offset cv inputs
		addInput(createDynamicPortCentered<IMPort>(VecPx(colL, row3), true, module, Variations::OFFSET_INPUT, mode));		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colR, row3), true, module, Variations::SPREAD_INPUT, mode));		
		
		// CV and gate inputs
		addInput(createDynamicPortCentered<IMPort>(VecPx(colL, row4), true, module, Variations::CV_INPUT, mode));		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colR, row4), true, module, Variations::GATE_INPUT, mode));		
		
		// CV and gate outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colL, row5), false, module, Variations::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colR, row5), false, module, Variations::GATE_OUTPUT, mode));
		
		// Clamp light
		addChild(createLightCentered<SmallLight<RedLightIM>>(VecPx(colM, row5 + 13), module, Variations::CLAMP_LIGHT));
	}
};


Model *modelVariations = createModel<Variations, VariationsWidget>("Variations");
