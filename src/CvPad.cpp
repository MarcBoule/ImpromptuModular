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
		ATTACH_PARAM,
		CONFIG_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		BANK_INPUT,
		CV_INPUT,
		WRITE_INPUT,
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
	static constexpr float cvKnobSensitivity = 4.0f;
	static const int read1_16 = 0;// index into readHeads[7]
	static const int read2_8 = 1;// index into readHeads[7]
	static const int read4_4 = 3;// index into readHeads[7]
	typedef float cvsArray[N_BANKS][N_PADS];
		
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	cvsArray cvs;
	int readHeads[7];// values are 0-15 for all heads, for example, in 4x4 mode, last readHead (read4_4 + 3) is 12-15
	int writeHead;
	bool highSensitivityCvKnob;

	// No need to save, with reset
	float cvsCpBuf[N_PADS];
	float cvCpBuf;
	
	// No need to save, no reset
	RefreshCounter refresh;
	int bank = 0;
	float cvKnobValue = 0.0f;
	Trigger padTriggers[N_PADS];
	Trigger writeTrigger;
	
	
	inline int calcBank() {
		float bankInputValue = inputs[BANK_INPUT].getVoltage() / 10.0f * (8.0f - 1.0f);
		return (int) clamp(std::round(params[BANK_PARAM].getValue() + bankInputValue), 0.0f, (8.0f - 1.0f));		
	}
	
	inline int calcConfig() {
		if (params[CONFIG_PARAM].getValue() > 1.5f)// 2 is 1x16
			return 4;
		if (params[CONFIG_PARAM].getValue() > 0.5f)// 1 is 2x8
			return 2;
		// here it is 0, and 0 is 4x4;
		return 1;
	}
	
	inline bool isAttached() {
		return params[ATTACH_PARAM].getValue() > 0.5f;
	}
	
	inline float quantize(float cv) {
		bool enable = params[QUANTIZE_PARAM].getValue() > 0.5f;
		return enable ? (std::round(cv * 12.0f) / 12.0f) : cv;
	}
	
	
	CvPad() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		for (int p = 0; p < N_PADS; p++) {
			configParam(PAD_PARAMS + p, 0.0f, 1.0f, 0.0f, string::f("CV pad %i", p + 1));
		}
		configParam(BANK_PARAM, 0.0f, 8.0f - 1.0f, 0.0f, "Bank", "", 0.0f, 1.0f, 1.0f);	// base, multiplier, offset
		paramQuantities[BANK_PARAM]->snapEnabled = true;
		configParam(WRITE_PARAM, 0.0f, 1.0f, 0.0f, "Write");				
		configParam(CV_PARAM, -INFINITY, INFINITY, 0.0f, "CV");		
		configSwitch(SHARP_PARAM, 0.0f, 2.0f, 0.0f, "Display mode", {"Volts", "Sharp", "Flat"});// 0 is top position
		configSwitch(QUANTIZE_PARAM, 0.0f, 1.0f, 0.0f, "Quantize", {"No", "Yes"});
		configSwitch(AUTOSTEP_PARAM, 0.0f, 1.0f, 0.0f, "Autostep when write", {"No", "Yes"});
		configSwitch(ATTACH_PARAM, 0.0f, 1.0f, 1.0f, "Attach", {"No", "Yes"});
		configSwitch(CONFIG_PARAM, 0.0f, 2.0f, 0.0f, "Configuration", {"4x4", "2x8", "1x16"});// 0 is top position (4x4), 1 is middle (2x8), 2 is bot (1x16)
		
		getParamQuantity(SHARP_PARAM)->randomizeEnabled = false;		
		getParamQuantity(CONFIG_PARAM)->randomizeEnabled = false;		
		getParamQuantity(QUANTIZE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(ATTACH_PARAM)->randomizeEnabled = false;		
		getParamQuantity(AUTOSTEP_PARAM)->randomizeEnabled = false;		
		getParamQuantity(BANK_PARAM)->randomizeEnabled = false;		

		configInput(BANK_INPUT, "Bank select");
		configInput(CV_INPUT, "CV");
		configInput(WRITE_INPUT, "Write");

		for (int i = 0; i < 4; i++) {
			configOutput(CV_OUTPUTS + i, string::f("CV %i", i + 1));
			configOutput(GATE_OUTPUTS + i, string::f("Gate %i", i + 1));
		}

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	
	void onReset() override {
		for (int b = 0; b < N_BANKS; b++) {
			for (int p = 0; p < N_PADS; p++) {
				cvs[b][p] = 0.0f;
			}
		}
		readHeads[read1_16] = 0;
		readHeads[read2_8 + 0] = 0;
		readHeads[read2_8 + 1] = 8;
		for (int i = 0; i < 4; i++) {
			readHeads[read4_4 + i] = i * 4;
		}
		writeHead = 0;
		highSensitivityCvKnob = true;
		resetNonJson();
	}
	void resetNonJson() {
		for (int p = 0; p < N_PADS; p++) {
			cvsCpBuf[p] = 0.0f;
		}
		cvCpBuf = 0.0f;
	}
	
	
	void onRandomize() override {
		for (int p = 0; p < N_PADS; p++) {
			cvs[bank][p] = random::uniform() * 20.0f - 10.0f;
		}
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// cvs
		json_t *cvsJ = json_array();
		for (int b = 0; b < N_BANKS; b++) {
			for (int p = 0; p < N_PADS; p++) {
				json_array_insert_new(cvsJ, b * N_PADS + p, json_real(cvs[b][p]));
			}
		}
		json_object_set_new(rootJ, "cvs", cvsJ);
		
		// readHeads
		json_t *readHeadsJ = json_array();
		for (int i = 0; i < 7; i++) {
			json_array_insert_new(readHeadsJ, i, json_integer(readHeads[i]));
		}
		json_object_set_new(rootJ, "readHeads", readHeadsJ);

		// writeHead
		json_object_set_new(rootJ, "writeHead", json_integer(writeHead));

		// highSensitivityCvKnob
		json_object_set_new(rootJ, "highSensitivityCvKnob", json_boolean(highSensitivityCvKnob));

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

		// readHeads
		json_t *readHeadsJ = json_object_get(rootJ, "readHeads");
		if (readHeadsJ) {
			for (int i = 0; i < 7; i++) {
				json_t *readHeadsArrayJ = json_array_get(readHeadsJ, i);
				if (readHeadsArrayJ)
					readHeads[i] = json_number_value(readHeadsArrayJ);
			}
		}

		// writeHead
		json_t *writeHeadJ = json_object_get(rootJ, "writeHead");
		if (writeHeadJ)
			writeHead = json_integer_value(writeHeadJ);
		
		// highSensitivityCvKnob
		json_t *highSensitivityCvKnobJ = json_object_get(rootJ, "highSensitivityCvKnob");
		if (highSensitivityCvKnobJ)
			highSensitivityCvKnob = json_is_true(highSensitivityCvKnobJ);
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		
		bank = calcBank();
		int config = calcConfig();
		
		if (refresh.processInputs()) {
			// attach 
			if (isAttached()) {
				setWriteHeadToRead(config);
			}
			
			// pads
			for (int p = 0; p < N_PADS; p++) {
				if (padTriggers[p].process(params[PAD_PARAMS + p].getValue())) {
					writeHead = p;
					if (isAttached()) {
						setReadHeadToWrite(config);
					}
				}
			}
			
			// write
			if (writeTrigger.process(params[WRITE_PARAM].getValue() + inputs[WRITE_INPUT].getVoltage())) {
				cvs[bank][writeHead] = clamp(inputs[CV_INPUT].getVoltage(), -10.0f, 10.0f);
				// autostep
				if (params[AUTOSTEP_PARAM].getValue() > 0.5f) {
					writeHead = (writeHead + 1) % N_PADS;
					// attached
					if (isAttached()) {
						setReadHeadToWrite(config);
					}
				}
			}
			
			// cv knob 
			float newCvKnobValue = params[CV_PARAM].getValue();
			if (newCvKnobValue == 0.0f)// true when constructor or dataFromJson() occured
				cvKnobValue = 0.0f;
			float deltaCvKnobValue = newCvKnobValue - cvKnobValue;
			if (deltaCvKnobValue != 0.0f) {
				if (abs(deltaCvKnobValue) <= 0.1f) {// avoid discontinuous step (initialize for example)
					// any changes in here should may also require right click behavior to be updated in the knob's onMouseDown()
					float cvDelta = highSensitivityCvKnob ? deltaCvKnobValue * 4.0f : deltaCvKnobValue;
					cvs[bank][writeHead] = clamp(cvs[bank][writeHead] + cvDelta, -10.0f, 10.0f);
				}
				cvKnobValue = newCvKnobValue;
			}	
			
		}// userInputs refresh
		
		
		
		// gate and cv outputs
		if (config == 4) {// 1x16
			outputs[GATE_OUTPUTS + 0].setVoltage(padTriggers[readHeads[read1_16]].isHigh() && isAttached() ? 10.0f : 0.0f);
			outputs[CV_OUTPUTS + 0].setVoltage(quantize(cvs[bank][readHeads[read1_16]]));
			for (int i = 1; i < 4; i++) {
				outputs[GATE_OUTPUTS + i].setVoltage(0.0f);
				outputs[CV_OUTPUTS + i].setVoltage(0.0f);
			}
		}
		else if (config == 2) {// 2x8
			outputs[GATE_OUTPUTS + 0].setVoltage(padTriggers[readHeads[read2_8 + 0]].isHigh() && isAttached() ? 10.0f : 0.0f);
			outputs[CV_OUTPUTS + 0].setVoltage(quantize(cvs[bank][readHeads[read2_8 + 0]]));
			outputs[GATE_OUTPUTS + 1].setVoltage(0.0f);
			outputs[CV_OUTPUTS + 1].setVoltage(0.0f);
			outputs[GATE_OUTPUTS + 2].setVoltage(padTriggers[readHeads[read2_8 + 1]].isHigh() && isAttached() ? 10.0f : 0.0f);
			outputs[CV_OUTPUTS + 2].setVoltage(quantize(cvs[bank][readHeads[read2_8 + 1]]));
			outputs[GATE_OUTPUTS + 3].setVoltage(0.0f);
			outputs[CV_OUTPUTS + 3].setVoltage(0.0f);
		}
		else {// config == 1 : 4x4
			for (int i = 0; i < 4; i++) {
				outputs[GATE_OUTPUTS + i].setVoltage(padTriggers[readHeads[read4_4 + i]].isHigh() && isAttached() ? 10.0f : 0.0f);
				outputs[CV_OUTPUTS + i].setVoltage(quantize(cvs[bank][readHeads[read4_4 + i]]));
			}
		}		
		
		// lights
		if (refresh.processLights()) {
			// red
			for (int l = 0; l < N_PADS; l++) {
				lights[PAD_LIGHTS + l * 2 + 1].setBrightness(writeHead == l ? 1.0f : 0.0f);
			}
			// green
			if (config == 4) {// 1x16
				for (int l = 0; l < N_PADS; l++) {
					lights[PAD_LIGHTS + l * 2 + 0].setBrightness(readHeads[read1_16] == l ? 1.0f : 0.0f);
				}
			}
			else if (config == 2) {// 2x8
				for (int l = 0; l < 8; l++) {
					lights[PAD_LIGHTS + l * 2 + 0].setBrightness(readHeads[read2_8 + 0] == l ? 1.0f : 0.0f);
				}
				for (int l = 8; l < N_PADS; l++) {
					lights[PAD_LIGHTS + l * 2 + 0].setBrightness(readHeads[read2_8 + 1] == l ? 1.0f : 0.0f);
				}
			}
			else {// config == 1 : 4x4
				for (int l = 0; l < 4; l++) {
					lights[PAD_LIGHTS + l * 2 + 0].setBrightness(readHeads[read4_4 + 0] == l ? 1.0f : 0.0f);
				}
				for (int l = 4; l < 8; l++) {
					lights[PAD_LIGHTS + l * 2 + 0].setBrightness(readHeads[read4_4 + 1] == l ? 1.0f : 0.0f);
				}
				for (int l = 8; l < 12; l++) {
					lights[PAD_LIGHTS + l * 2 + 0].setBrightness(readHeads[read4_4 + 2] == l ? 1.0f : 0.0f);
				}
				for (int l = 12; l < N_PADS; l++) {
					lights[PAD_LIGHTS + l * 2 + 0].setBrightness(readHeads[read4_4 + 3] == l ? 1.0f : 0.0f);
				}
			}
		}// processLights()
		
		if (refresh.processInputs()) {
			// To Expander
			if (rightExpander.module && rightExpander.module->model == modelFourView) {
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				if (config == 4) {// 1x16
					messageToExpander[0] = outputs[CV_OUTPUTS + 0].getVoltage();
					for (int i = 1; i < 4; i++) {
						messageToExpander[i] = -100.0f;// unused code
					}
				}
				else if (config == 2) {// 2x8
					messageToExpander[0] = outputs[CV_OUTPUTS + 0].getVoltage();
					messageToExpander[1] = -100.0f;// unused code
					messageToExpander[2] = outputs[CV_OUTPUTS + 2].getVoltage();
					messageToExpander[3] = -100.0f;// unused code
				}
				else {// config == 1 : 4x4
					for (int i = 0; i < 4; i++) {
						messageToExpander[i] = outputs[CV_OUTPUTS + i].getVoltage();
					}
				}
				messageToExpander[4] = (float)panelTheme;
				messageToExpander[5] = panelContrast;
				rightExpander.module->leftExpander.messageFlipRequested = true;
			}			
		}
	}
	
	void setReadHeadToWrite(int config) {
		if (config == 4) {// 1x16
			readHeads[read1_16] = writeHead;
		}
		else if (config == 2) {// 2x8
			if (writeHead < 8) {
				readHeads[read2_8 + 0] = writeHead;
			}
			else {
				readHeads[read2_8 + 1] = writeHead;
			}
		}
		else {// config == 1 : 4x4
			if (writeHead < 4) {
				readHeads[read4_4 + 0] = writeHead;
			}
			else if (writeHead < 8) {
				readHeads[read4_4 + 1] = writeHead;
			}
			else if (writeHead < 12) {
				readHeads[read4_4 + 2] = writeHead;
			}
			else {
				readHeads[read4_4 + 3] = writeHead;
			}
		}
	}
	
	void setWriteHeadToRead(int config) {
		if (config == 4) {// 1x16
			writeHead = readHeads[read1_16];
		}
		else if (config == 2) {// 2x8
			if (writeHead < 8) {
				writeHead = readHeads[read2_8 + 0];
			}
			else {
				writeHead = readHeads[read2_8 + 1];
			}
		}
		else {// config == 1 : 4x4
			if (writeHead < 4) {
				writeHead = readHeads[read4_4 + 0] ;
			}
			else if (writeHead < 8) {
				writeHead = readHeads[read4_4 + 1];
			}
			else if (writeHead < 12) {
				writeHead = readHeads[read4_4 + 2] ;
			}
			else {
				writeHead = readHeads[read4_4 + 3];
			}
		}
	}
};


struct CvPadWidget : ModuleWidget {
	// Widgets
	// --------------------------------
	
	struct Pad : Switch {
		Pad() {
			momentary = true;
			box.size = VecPx(40.0f, 40.0f);
		}
	};


	struct CvKnob : IMBigKnobInf {
		CvKnob() {};		
		void onDoubleClick(const event::DoubleClick &e) override {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity) {
				CvPad* module = dynamic_cast<CvPad*>(paramQuantity->module);
				module->cvs[module->bank][module->writeHead] = 0.0f;
			}
			ParamWidget::onDoubleClick(e);
		}
	};	


	struct BankDisplayWidget : TransparentWidget {
		CvPad *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		
		BankDisplayWidget() {
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, 18);
				nvgFontFaceId(args.vg, font->handle);
				//nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = VecPx(6, 24);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				char displayStr[2];
				unsigned int bank = (unsigned)(module ? module->bank : 0);
				snprintf(displayStr, 2, "%1u", (unsigned) (bank + 1) );
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};


	struct CvParamField : ui::TextField {
		float* valueSrc;

		void step() override {
			// Keep selected
			APP->event->setSelectedWidget(this);
			TextField::step();
		}

		void setValueSrc(float* _valueSrc) {
			valueSrc = _valueSrc;
			text = string::f("%.*g", 5, math::normalizeZero(*valueSrc));
			selectAll();
		}

		void onSelectKey(const event::SelectKey& e) override {
			if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
				float v = 0.f;
				if (std::sscanf(text.c_str(), "%f", &v) >= 1) {
					*valueSrc = v;
				}
				
				ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
				overlay->requestDelete();
				e.consume(this);
			}

			if (!e.getTarget())
				TextField::onSelectKey(e);
		}
	};
	

	struct CvDisplayWidget : TransparentWidget {
		CvPad *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char text[7];

		CvDisplayWidget() {
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void cvToStr(void) {
			if (module == NULL) {
				snprintf(text, 7, " 0,000");
			} 
			else {
				int bank = module->calcBank();
				float cvVal = module->quantize(module->cvs[bank][module->writeHead]);
				if (module->params[CvPad::SHARP_PARAM].getValue() > 0.5f) {// show notes
					text[0] = ' ';
					printNote(cvVal, &text[1], module->params[CvPad::SHARP_PARAM].getValue() < 1.5f);
				}
				else  {// show volts
					float cvValPrint = std::fabs(cvVal);
					if (cvValPrint > 9.9995f) {
						snprintf(text, 7, " %4.2f", 10.0f);
						text[3] = ',';
					}
					else {
						snprintf(text, 7, " %4.3f", cvValPrint);// Four-wide, three positions after the decimal, left-justified
						text[2] = ',';
					}
					text[0] = (cvVal<0.0f) ? '-' : ' ';
				}
			}
		}

		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, 18);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, -1.5);

				Vec textPos = VecPx(6, 24);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~~~~~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				cvToStr();
				nvgText(args.vg, textPos.x, textPos.y, text, NULL);
			}
		}
		
		void createContextMenu() {
			ui::Menu* menu = createMenu();

			menu->addChild(createMenuLabel("Voltage (V)"));
			
			CvParamField* paramField = new CvParamField;
			paramField->box.size.x = 100;
			int bank = module->calcBank();
			paramField->setValueSrc(&(module->cvs[bank][module->writeHead]));
			menu->addChild(paramField);
		}	

		void onButton(const event::Button& e) override {
			// Right click to open context menu
			if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT && (e.mods & RACK_MOD_MASK) == 0) {
				createContextMenu();
				e.consume(this);
				return;
			}
			TransparentWidget::onButton(e);
		}
	};


	// Menu
	// --------------------------------
	
	
	struct OffsetDeciQuantity : Quantity {
		CvPad::cvsArray* cvSrc;
		int* bankSrc;
		float valueLocal;// must be reset to 0 when enter menu (i.e. constructor)
		int valueIntLocal;
		int valueIntLocalLast;
		float increment;
		  
		OffsetDeciQuantity() {
			valueLocal = 0.0f;
			valueIntLocal = 0;
			valueIntLocalLast = 0;
			increment = 0.1f;
		}
		virtual float quantize(float cv) {
			return std::round(cv * 100.f) / 100.0f;
		}
		void setValue(float value) override {
			valueLocal = math::clamp(value, getMinValue(), getMaxValue());; 
			valueIntLocal = (int)(std::round(valueLocal));
			int delta = valueIntLocal - valueIntLocalLast;// delta is number of deci-volts
			if (delta != 0) {
				float deltaIncrement = ((float)delta) * increment;
				for (int i = 0; i < CvPad::N_PADS; i++) {
					(*cvSrc)[*bankSrc][i] = quantize((*cvSrc)[*bankSrc][i] + deltaIncrement);
				}
				valueIntLocalLast = valueIntLocal;
			}
		}
		float getValue() override {
			return valueLocal;
		}
		float getMinValue() override {return -100.0f;}
		float getMaxValue() override {return 100.0f;}
		float getDefaultValue() override {return 0.0f;}
		float getDisplayValue() override {return getValue();}
		std::string getDisplayValueString() override {
			return string::f("%.1f", math::normalizeZero(getDisplayValue() * increment));
		}
		void setDisplayValue(float displayValue) override {setValue(displayValue);}
		std::string getLabel() override {return "Offset deci";}
		std::string getUnit() override {return " V";}
	};


	struct OffsetCentiQuantity : OffsetDeciQuantity {
		OffsetCentiQuantity() {
			increment = 0.01f;
		}
		std::string getDisplayValueString() override {
			return string::f("%.2f", math::normalizeZero(getDisplayValue() * increment));
		}
		std::string getLabel() override {return "Offset centi";}
	};
	
	
	struct OffsetSemitoneQuantity : OffsetDeciQuantity {
		OffsetSemitoneQuantity() {
			increment = 1.0f / 12.0f;
		}
		float quantize(float cv) override {
			return std::round(cv * 12.0f) / 12.0f;
		}
		std::string getDisplayValueString() override {
			return string::f("%i", (int)std::round(getDisplayValue()));
		}
		std::string getLabel() override {return "Offset note";}
		std::string getUnit() override {return " semitone(s)";}
	};

	
	template <typename T>
	struct OffsetSlider : ui::Slider {
		OffsetSlider(CvPad::cvsArray* cvSrc, int* bankSrc) {
			quantity = new T();
			((T*)quantity)->cvSrc = cvSrc;
			((T*)quantity)->bankSrc = bankSrc;
		}
		~OffsetSlider() {
			delete quantity;
		}
	};
	
	
	struct OperationsItem : MenuItem {
		CvPad::cvsArray* cvSrc;
		int* bankSrc;
		float* cvsCpBufSrc;

		struct CopySubItem : MenuItem {
			CvPad::cvsArray* cvSrc;
			int* bankSrc;
			float* cvsCpBufSrc;
			void onAction(const event::Action &e) override {
				for (int i = 0; i < CvPad::N_PADS; i++) {
					cvsCpBufSrc[i] = (*cvSrc)[*bankSrc][i];
				}
			}
		};

		struct PasteSubItem : MenuItem {
			CvPad::cvsArray* cvSrc;
			int* bankSrc;
			float* cvsCpBufSrc;
			void onAction(const event::Action &e) override {
				for (int i = 0; i < CvPad::N_PADS; i++) {
					(*cvSrc)[*bankSrc][i] = cvsCpBufSrc[i];
				}
			}
		};

		struct MultiplyItem : MenuItem {
			CvPad::cvsArray* cvSrc;
			int* bankSrc;
			float factor;		
			void onAction(const event::Action &e) override {
				for (int i = 0; i < CvPad::N_PADS; i++) {
					(*cvSrc)[*bankSrc][i] *= factor;
				}
			}
		};
		
		struct FillVoltsItem : MenuItem {
			CvPad::cvsArray* cvSrc;
			int* bankSrc;
			float rootVoct = 0.0f;		
			float incVoct = 1.0f / 12.0f;		
			void onAction(const event::Action &e) override {
				for (int i = 0; i < CvPad::N_PADS; i++) {
					(*cvSrc)[*bankSrc][i] = rootVoct + ((float)i) * incVoct;
				}
			}
		};


		Menu *createChildMenu() override {
			Menu *menu = new Menu;
			
			// Copy
			CopySubItem *copyItem = createMenuItem<CopySubItem>("Copy");
			copyItem->cvSrc = cvSrc;
			copyItem->bankSrc = bankSrc;
			copyItem->cvsCpBufSrc = cvsCpBufSrc;
			menu->addChild(copyItem);
		
			// Paste
			PasteSubItem *pasteItem = createMenuItem<PasteSubItem>("Paste");
			pasteItem->cvSrc = cvSrc;
			pasteItem->bankSrc = bankSrc;
			pasteItem->cvsCpBufSrc = cvsCpBufSrc;
			menu->addChild(pasteItem);

			// Offset deci
			OffsetSlider<OffsetDeciQuantity> *offsetDeciSlider = new OffsetSlider<OffsetDeciQuantity>(cvSrc, bankSrc);
			offsetDeciSlider->box.size.x = 200.0f;
			menu->addChild(offsetDeciSlider);
			
			// Offset centi
			OffsetSlider<OffsetCentiQuantity> *offsetCentiSlider = new OffsetSlider<OffsetCentiQuantity>(cvSrc, bankSrc);
			offsetCentiSlider->box.size.x = 200.0f;
			menu->addChild(offsetCentiSlider);
			
			// Offset semitone
			OffsetSlider<OffsetSemitoneQuantity> *offsetSemitoneSlider = new OffsetSlider<OffsetSemitoneQuantity>(cvSrc, bankSrc);
			offsetSemitoneSlider->box.size.x = 200.0f;
			menu->addChild(offsetSemitoneSlider);
	
			// Divide 5
			MultiplyItem* dfItem = createMenuItem<MultiplyItem>("Divide by 5");
			dfItem->cvSrc = cvSrc;
			dfItem->bankSrc = bankSrc;
			dfItem->factor = 1.0f / 5.0f;
			menu->addChild(dfItem);
	
			// Divide 2
			MultiplyItem* dtItem = createMenuItem<MultiplyItem>("Divide by 2");
			dtItem->cvSrc = cvSrc;
			dtItem->bankSrc = bankSrc;
			dtItem->factor = 1.0f / 2.0f;
			menu->addChild(dtItem);
	
			// Divide 1.5
			MultiplyItem* dofItem = createMenuItem<MultiplyItem>("Divide by 1.5");
			dofItem->cvSrc = cvSrc;
			dofItem->bankSrc = bankSrc;
			dofItem->factor = 1.0f / 1.5f;
			menu->addChild(dofItem);
	
			// Multiply 1.5
			MultiplyItem* mofItem = createMenuItem<MultiplyItem>("Multiply by 1.5");
			mofItem->cvSrc = cvSrc;
			mofItem->bankSrc = bankSrc;
			mofItem->factor = 1.5f;
			menu->addChild(mofItem);
	
			// Multiply 2
			MultiplyItem* mtItem = createMenuItem<MultiplyItem>("Multiply by 2");
			mtItem->cvSrc = cvSrc;
			mtItem->bankSrc = bankSrc;
			mtItem->factor = 2.0f;
			menu->addChild(mtItem);

			// Multiply 5
			MultiplyItem* mfItem = createMenuItem<MultiplyItem>("Multiply by 5");
			mfItem->cvSrc = cvSrc;
			mfItem->bankSrc = bankSrc;
			mfItem->factor = 5.0f;
			menu->addChild(mfItem);

			// Fill with notes
			FillVoltsItem* notesItem = createMenuItem<FillVoltsItem>("Fill with notes C4-D5#");
			notesItem->cvSrc = cvSrc;
			notesItem->bankSrc = bankSrc;
			menu->addChild(notesItem);

			// Fill with 0.1 increasing volts
			FillVoltsItem* voltsItem = createMenuItem<FillVoltsItem>("Fill with 0V, 0.1V ... 1.5V");
			voltsItem->cvSrc = cvSrc;
			voltsItem->bankSrc = bankSrc;
			voltsItem->incVoct = 0.1f;
			menu->addChild(voltsItem);

			return menu;
		}
	};
	
	
	struct CopyPadItem : MenuItem {
		CvPad *module;
		void onAction(const event::Action &e) override {
			module->cvCpBuf = module->cvs[module->calcBank()][module->writeHead];
		}
	};
	struct PastePadItem : MenuItem {
		CvPad *module;
		void onAction(const event::Action &e) override {
			module->cvs[module->calcBank()][module->writeHead] = module->cvCpBuf;
		}
	};
	
	
	void appendContextMenu(Menu *menu) override {
		CvPad *module = (CvPad*)(this->module);
		
		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());
		
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createBoolPtrMenuItem("High sensitivity CV knob", "", &module->highSensitivityCvKnob));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Actions"));

		CopyPadItem *cvCopyItem = createMenuItem<CopyPadItem>("Copy selected pad");
		cvCopyItem->module = module;
		menu->addChild(cvCopyItem);
		
		PastePadItem *cvPasteItem = createMenuItem<PastePadItem>("Paste selected pad");
		cvPasteItem->module = module;
		menu->addChild(cvPasteItem);	
		
		OperationsItem *opItem = createMenuItem<OperationsItem>("Current bank", RIGHT_ARROW);
		opItem->cvSrc = &(module->cvs);
		opItem->bankSrc = &(module->bank);
		opItem->cvsCpBufSrc = module->cvsCpBuf;
		menu->addChild(opItem);
	}
	
	CvPadWidget(CvPad *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/CvPad.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));
				
				
		// pads and lights
		// ----------------------------
		
		static const int padX = 64 + 20;
		static const int padXd = 58;
		static const int padY = 112 + 20;
		static const int padYd = 64;
		static const int ledOffsetY = 30;
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				svgPanel->fb->addChild(new CvPadSvg(mm2px(Vec(21.749f, 37.908f)).plus(Vec(padXd * x, padYd * y)), mode));
				// pad
				addParam(createParamCentered<Pad>(VecPx(padX + padXd * x, padY + padYd * y), module, CvPad::PAD_PARAMS + y * 4 + x));
				// light
				addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(padX + padXd * x, padY + padYd * y - ledOffsetY), module, CvPad::PAD_LIGHTS + (y * 4 + x) * 2 + 0));
			}
		}
		
		
		// left side column
		// ----------------------------
		
		static const int leftX = 31;
		static const int topY = 60;
		static constexpr float leftYd = 53.6f;
		// quantize
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(leftX, topY), module, CvPad::QUANTIZE_PARAM, mode, svgPanel));
		// attach
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(leftX, topY + leftYd), module, CvPad::ATTACH_PARAM, mode, svgPanel));
		// autostep
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(leftX, topY + leftYd * 2), module, CvPad::AUTOSTEP_PARAM, mode, svgPanel));
		// write button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(leftX, topY + leftYd * 3), module, CvPad::WRITE_PARAM, mode));	
		// write input
		addInput(createDynamicPortCentered<IMPort>(VecPx(leftX, topY + leftYd * 4 - 5), true, module, CvPad::WRITE_INPUT, mode));
		// cv input
		addInput(createDynamicPortCentered<IMPort>(VecPx(leftX, topY + leftYd * 5 - 5), true, module, CvPad::CV_INPUT, mode));
		
		
		// right side column
		// ----------------------------
		
		static const int rightX = 337;
		static const int rightO = 21;
		
		// bank display
		BankDisplayWidget *displayBank = new BankDisplayWidget();
		displayBank->box.size = VecPx(24, 30);// 1 character
		displayBank->box.pos = VecPx(rightX - rightO - 6, topY);
		displayBank->box.pos = displayBank->box.pos.minus(displayBank->box.size.div(2));// centering
		displayBank->module = module;
		addChild(displayBank);	
		svgPanel->fb->addChild(new DisplayBackground(displayBank->box.pos, displayBank->box.size, mode));
		// bank input
		addInput(createDynamicPortCentered<IMPort>(VecPx(rightX + rightO, topY), true, module, CvPad::BANK_INPUT, mode));
		// Volt/sharp/flat switch
		static const int triSwitchY = 119;
		addParam(createDynamicSwitchCentered<IMSwitch3VInv>(VecPx(rightX - 32, triSwitchY - 6), module, CvPad::SHARP_PARAM, mode, svgPanel));
		// config
		addParam(createDynamicSwitchCentered<IMSwitch3VInv>(VecPx(rightX + 8, triSwitchY - 6), module, CvPad::CONFIG_PARAM, mode, svgPanel));
		// outputs
		static const int outY = triSwitchY + 68 * 3;
		static const int outYd = 45;
		for (int y = 0; y < 4; y++) {
			addOutput(createDynamicPortCentered<IMPort>(VecPx(rightX - rightO, outY - outYd * (3 - y)), false, module, CvPad::CV_OUTPUTS + y, mode));
			addOutput(createDynamicPortCentered<IMPort>(VecPx(rightX + rightO, outY - outYd * (3 - y)), false, module, CvPad::GATE_OUTPUTS + y, mode));
		}
		
		
		// top of pad, first row
		// ----------------------------

		// cv display
		CvDisplayWidget *displayCv = new CvDisplayWidget();
		displayCv->box.size = VecPx(98, 30);// 6 characters (ex.: "-1,234")
		displayCv->box.pos = VecPx(padX + padXd / 2, topY);
		displayCv->box.pos = displayCv->box.pos.minus(displayCv->box.size.div(2));// centering
		displayCv->module = module;
		addChild(displayCv);
		svgPanel->fb->addChild(new DisplayBackground(displayCv->box.pos, displayCv->box.size, mode));
		// cv knob
		addParam(createDynamicParamCentered<CvKnob>(VecPx(padX + padXd * 2, topY), module, CvPad::CV_PARAM, mode));
		// bank knob
		addParam(createDynamicParamCentered<IMMediumKnob>(VecPx(padX + padXd * 3, topY), module, CvPad::BANK_PARAM, mode));
	}
};


//*****************************************************************************

Model *modelCvPad = createModel<CvPad, CvPadWidget>("Cv-Pad");
