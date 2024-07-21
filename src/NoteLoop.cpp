//***********************************************************************************************
//CV-Gate loop module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Pyer. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************

#include "ImpromptuModular.hpp"


struct NoteLoop : Module {	

	static const int MAX_POLY = 16;
	static const int MAX_DEL = 32;
	static const int BUF_SIZE = MAX_DEL * 2;  

	enum ParamIds {
		LOOP_PARAM,
		LEN_PARAM,
		CLEAR_PARAM,
		TEMPOCV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		GATE_INPUT,
		CV2_INPUT,
		CLK_INPUT,
		CLEAR_INPUT,
		LOOP_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,
		GATE_OUTPUT,
		CV2_OUTPUT,
		CLEAR_OUTPUT,
		LOOPSTART_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLEAR_LIGHT,
		LOOP_LIGHT,
		NUM_LIGHTS
	};
	
	
	struct NoteEvent {
		// An event is considered pending when gateOffFrame==0 (waiting for falling edge of the gate  input), and ready when >0; when pending, rest of struct is populated
		int64_t gateOnFrame = 0;
		int64_t gateOffFrame = 0;
		float cv = 0.0f;
		float cv2 = 0.0f;
	};


	class EventBuffer {
		NoteEvent events[BUF_SIZE];
		uint16_t head = 0;// points to next empty entry that can be written to
		uint16_t size = 0;
		
		public:
		
		void clear() {
			head = 0;
			size = 0;
		}
		void step() {
			if (size < BUF_SIZE) {
				size++;
			}
			head = (head + 1) % BUF_SIZE;
		}
		uint16_t prev(uint16_t num) {
			// get the previous index offset by num, prev(0) returns index right before head, which is the last entry, such that scanning can be done with prev(0) to prev(size-1)
			// returns BUF_SIZE if num is too large (according to size)
			if (num > size - 1) {
				return BUF_SIZE;// error code
			}
			return (BUF_SIZE + head - 1 - num) % BUF_SIZE;
		}
		void enterEvent(const NoteEvent& e) {
			events[head] = e;
			step();
		}
		void finishDelEvent(int64_t gateOffFrame) {
			uint16_t index = prev(0);
			if (index < BUF_SIZE && events[index].gateOffFrame == 0) {
				events[index].gateOffFrame = gateOffFrame;
			}
		}
		NoteEvent* findEvent(int64_t frameOrClk) {
			for (uint16_t cnt = 0; ; cnt++) {
				uint16_t test = prev(cnt);
				if (test >= BUF_SIZE) {
					return nullptr;
				}
				if (frameOrClk >= events[test].gateOnFrame) {
					return &events[test];
				}
			}
			return nullptr;
		}
		NoteEvent* findEventGateOn(int64_t frameOrClk) {
			NoteEvent* event = findEvent(frameOrClk);
			if (event != nullptr && event->gateOffFrame != 0 && frameOrClk >= event->gateOffFrame) {
				return nullptr;
			}
			return event;
		}
	};
	
	
	// Expander
	// none

	// Constants
	static const int numMults = 7;    // 1/2  2/3 3/4  1   4/3  3/2  2
	static const int numMultsUnityIndex = 3;    
	static const int64_t multNum[numMults];
	static const int64_t multDen[numMults];
	static constexpr float cv2NormMin = -10.0f;
	static constexpr float cv2NormMax = 10.0f;
	static constexpr float cv2NormDefault = 0.0f;
	
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	float cv2NormalledVoltage;
	int64_t clockPeriod;
	int ecoMode;
	int delMult;

	// No need to save, with reset
	bool loop;
	EventBuffer channel[MAX_POLY];
	EventBuffer loopStart;
	int64_t lastRisingClkFrame;// -1 when none
	int64_t clockPeriodForLoop;// no need to save since loop is off when reload patch, and this is set when loop turns on
	int64_t lengthForLoop;// no need to save since loop is off when reload patch, and this is set when loop turns on
	
	// No need to save, no reset
	float clearLight = 0.0f;
	RefreshCounter refresh;
	TriggerRiseFall clkTrigger;
	TriggerRiseFall gateTriggers[MAX_POLY];
	Trigger loopButtonTrigger;
	Trigger loopStartTrigger;
	Trigger clearTrigger;
	dsp::PulseGenerator clearPulse;


	int getLoopLengthKnob() {
		return (int)(params[LEN_PARAM].getValue() + 0.5f);
	}
	bool isTempoCV() {
		return params[TEMPOCV_PARAM].getValue() > 0.5f;
	}

	
	
	NoteLoop() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(LOOP_PARAM, 0.0f, 1.0f, 0.0f, "Loop");
		configParam(LEN_PARAM, 1.0f, (float)(MAX_DEL), 4.0f, "Loop length");
		paramQuantities[LEN_PARAM]->snapEnabled = true;
		configParam(CLEAR_PARAM, 0.0f, 1.0f, 0.0f, "Clear");
		configSwitch(TEMPOCV_PARAM, 0.0f, 1.0f, 0.0f, "Tempo input mode", {"Clock pulses", "BPM CV"});
				
		configInput(CV_INPUT, "CV");
		configInput(GATE_INPUT, "Gate");
		configInput(CV2_INPUT, "CV2/Velocity");
		configInput(CLK_INPUT, "Tempo");
		configInput(CLEAR_INPUT, "Clear buffer");
		configInput(LOOP_INPUT, "Loop");

		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV2_OUTPUT, "CV2/Velocity");
		configOutput(CLEAR_OUTPUT, "Clear");
		configOutput(LOOPSTART_OUTPUT, "Start of loop");
		
		configBypass(CV_INPUT, CV_OUTPUT);
		configBypass(GATE_INPUT, GATE_OUTPUT);
		configBypass(CV2_INPUT, CV2_OUTPUT);

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}


	
	void clear() {
		for (int p = 0; p < MAX_POLY; p++) {
			channel[p].clear();
			gateTriggers[p].reset();
		}
		loopStart.clear();
		loopStartTrigger.reset();
	}

	void onReset() override final {
		cv2NormalledVoltage = 0.0f;
		clockPeriod = (int64_t)(APP->engine->getSampleRate());// 60 BPM until detected
		ecoMode = 1;
		delMult = numMultsUnityIndex;
		resetNonJson();
	}
	void resetNonJson() {
		loop = false;
		clear();
		lastRisingClkFrame = -1;// none
		clockPeriodForLoop = 0;
		lengthForLoop = 0;
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));
		
		// cv2NormalledVoltage
		json_object_set_new(rootJ, "cv2NormalledVoltage", json_real(cv2NormalledVoltage));

		// clockPeriod
		json_object_set_new(rootJ, "clockPeriod", json_integer((long)clockPeriod));

		// ecoMode
		json_object_set_new(rootJ, "ecoMode", json_integer(ecoMode));
		
		// delMult
		json_object_set_new(rootJ, "delMult", json_integer(delMult));
		
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

		// cv2NormalledVoltage (also the flag for 1st version)
		json_t *cv2NormalledVoltageJ = json_object_get(rootJ, "cv2NormalledVoltage");
		if (cv2NormalledVoltageJ)
			cv2NormalledVoltage = json_number_value(cv2NormalledVoltageJ);

		// clockPeriod
		json_t *clockPeriodJ = json_object_get(rootJ, "clockPeriod");
		if (clockPeriodJ)
			clockPeriod = (int64_t)json_integer_value(clockPeriodJ);

		// ecoMode
		json_t *ecoModeJ = json_object_get(rootJ, "ecoMode");
		if (ecoModeJ)
			ecoMode = json_integer_value(ecoModeJ);

		// delMult
		json_t *delMultJ = json_object_get(rootJ, "delMult");
		if (delMultJ)
			delMult = json_integer_value(delMultJ);

		resetNonJson();
	}


	void onSampleRateChange() override {
		clear();
	}		


	void process(const ProcessArgs &args) override {
		// user inputs
		if (refresh.processInputs()) {
		}// userInputs refresh
		
		// Loop
		if (loopButtonTrigger.process(inputs[LOOP_INPUT].getVoltage() + params[LOOP_PARAM].getValue())) {
			loop = !loop;
			if (loop) {
				// enter event in loopStart special channel
				NoteEvent e;
				e.gateOnFrame = args.frame;
				e.gateOffFrame = args.frame + (int64_t)(0.0011f * args.sampleRate);
				e.cv = 0.0f;// unused
				e.cv2 = 0.0f;// unused
				loopStart.enterEvent(e);
				
				// sample clockPeriod and loop length so that it never changes during looping
				if (isTempoCV()) {
					float clockPeriodF = (args.sampleRate * 0.5f / std::pow(2.0f, inputs[CLK_INPUT].getVoltage()));
					clockPeriod = (int64_t)clamp(clockPeriodF, args.sampleRate / 5.0f, args.sampleRate * 6.0f);
					if (delMult != numMultsUnityIndex) {
						clockPeriod = clockPeriod * multDen[delMult] / multNum[delMult];
					}
				}
				//else clockPeriod already set since non BPM-CV always processing tempo edges further below
				clockPeriodForLoop = clockPeriod;
				lengthForLoop = (int64_t)getLoopLengthKnob();
			}
			else {
				loopStart.clear();
			}
		}
	
		int poly = inputs[GATE_INPUT].getChannels();
		if (outputs[CV_OUTPUT].getChannels() != poly) {
			outputs[CV_OUTPUT].setChannels(poly);
		}
		if (outputs[GATE_OUTPUT].getChannels() != poly) {
			outputs[GATE_OUTPUT].setChannels(poly);
		}
		if (outputs[CV2_OUTPUT].getChannels() != poly) {
			outputs[CV2_OUTPUT].setChannels(poly);
		}

	
		// clear and clock
		if (clearTrigger.process(inputs[CLEAR_INPUT].getVoltage() + params[CLEAR_PARAM].getValue())) {
			clear();
			clearLight = 1.0f;
			clearPulse.trigger(0.001f);
		}
		int clkEdge = clkTrigger.process(inputs[CLK_INPUT].getVoltage());
		// update clockPeriod
		if (isTempoCV()) {
			// nothing to do since BPM-CV is sampled further above when loop turns on
		}
		else if (clkEdge == 1) {
			if (lastRisingClkFrame != -1) {
				int64_t deltaFrames = args.frame - lastRisingClkFrame;
				float deltaFramesF = (float)deltaFrames;
				if (deltaFramesF <= (args.sampleRate * 6.0f) && 
					deltaFramesF >= (args.sampleRate / 5.0f)) {// 10-300 BPM tempo range
					clockPeriod = (deltaFrames * multDen[delMult]) / multNum[delMult];
				}
				// else, remember the previous clockPeriod so nothing to do
			}
			lastRisingClkFrame = args.frame;
		}
		
		// sample the inputs on poly gates
		NoteEvent* loopEvents[MAX_POLY] = {};
		NoteEvent* loopStartEvent = nullptr;
		float gateIn[MAX_POLY];
		float loopStartIn;
		if (loop) {
			int64_t loopFrameOrClk = args.frame - clockPeriodForLoop * lengthForLoop;
			for (int p = 0; p < poly; p++) {
				loopEvents[p] = channel[p].findEventGateOn(loopFrameOrClk);
				gateIn[p] = (loopEvents[p] == nullptr ? 0.0f : 10.0f);
			}
			loopStartEvent = loopStart.findEventGateOn(loopFrameOrClk);
			loopStartIn = (loopStartEvent == nullptr ? 0.0f : 10.0f);
		}
		else {
			for (int p = 0; p < poly; p++) {
				gateIn[p] = inputs[GATE_INPUT].getChannels() > p ? inputs[GATE_INPUT].getVoltage(p) : 0.0f;
			}
			loopStartIn = 0.0f;
		}
		for (int p = 0; p < poly; p++) {
			int eventEdge = gateTriggers[p].process(gateIn[p]);
			// sample the inputs on rising poly gate edges (finish job on falling)
			if (eventEdge == 1) {
				// here we have a rising gate on poly p
				NoteEvent e;
				e.gateOnFrame = args.frame;
				e.gateOffFrame = 0;// will be completed when gate falls
				if (loopEvents[p] != nullptr) {
					e.cv = loopEvents[p]->cv;
					e.cv2 = loopEvents[p]->cv2;
				}
				else {
					e.cv  = inputs[CV_INPUT ].getChannels() > p ? inputs[CV_INPUT ].getVoltage(p) : 0.0f;
					e.cv2 = inputs[CV2_INPUT].getChannels() > p ? inputs[CV2_INPUT].getVoltage(p) : cv2NormalledVoltage;
				}
				channel[p].enterEvent(e);
			}
			else if (eventEdge == -1) {
				// here we have a falling gate on poly p
				channel[p].finishDelEvent(args.frame);
			}
		}// poly p
		if (loopStartTrigger.process(loopStartIn)) {
			NoteEvent e;
			e.gateOnFrame = args.frame;
			e.gateOffFrame = args.frame + (int64_t)(0.0011f * args.sampleRate);
			e.cv = 0.0f;
			e.cv2 = 0.0f;
			loopStart.enterEvent(e);
		}

		
		// outputs
		if (ecoMode == 1 || ((refresh.refreshCounter & 0x7) == 0) ) {
			for (int p = 0; p < poly; p++) {
				float gate = 0.0f;
				NoteEvent* event = channel[p].findEventGateOn(args.frame);
				if (event != nullptr) {
					gate = 10.0f;
					outputs[CV_OUTPUT].setVoltage(event->cv, p);
					outputs[CV2_OUTPUT].setVoltage(event->cv2, p);							
				}
				outputs[GATE_OUTPUT].setVoltage(gate, p);
			}	
		}// eco mode		
		// outputs[CLEAR_OUTPUT].setVoltage((clearPulse.process(args.sampleTime) ? 10.0f : 0.0f));
		
		// NoteEvent* sole = loopStart.findEventGateOn(args.frame);// v1 (hit on loop press, can be used as Start Of Loop = SOL)
		// outputs[LOOPSTART_OUTPUT].setVoltage(sole != nullptr ? 10.0f : 0.0f);// v1 (cont')
		// // outputs[LOOPSTART_OUTPUT].setVoltage(loopStartTrigger.state ? 10.0f : 0.0f);//v2 (no hit on loop press, can be used as EOL)
		
		// lights
		if (refresh.processLights()) {
			// lights
			// simple ones done in NoteLoopWidget::step()

			lights[CLEAR_LIGHT].setSmoothBrightness(clearLight, args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));	
			clearLight = 0.0f;
		}// lightRefreshCounter
	
	}// process()
};


const int64_t NoteLoop::multNum[numMults] = {1, 2, 3, 1, 4, 3, 2};
const int64_t NoteLoop::multDen[numMults] = {2, 3, 4, 1, 3, 2, 1};



struct NoteLoopWidget : ModuleWidget {
	typedef IMMediumKnob LoopKnob;

	struct LenDisplayWidget : TransparentWidget {// a centered display
		NoteLoop *module = nullptr;
		std::shared_ptr<Font> font;
		std::string fontPath;
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f; // 18.2f for 14 pt, 19.7f for 15pt
		
		LenDisplayWidget(Vec _pos, Vec _size, NoteLoop *_module) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, textFontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, -0.4);

				Vec textPos = VecPx(5.7f, textOffsetY);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				std::string initString(2,'~');
				nvgText(args.vg, textPos.x, textPos.y, initString.c_str(), NULL);
				
				nvgFillColor(args.vg, displayColOn);
				std::string dispStr = " 4";
				if (module != NULL) {
					int len = module->getLoopLengthKnob();
					dispStr = string::f("%2u", (unsigned)len);
				}
				nvgText(args.vg, textPos.x, textPos.y, dispStr.c_str(), NULL);
			}
		}
	};	
	

	void createCv2NormalizationMenu(ui::Menu* menu, float* cv2Normalled) {
			
		struct Cv2NormItem : MenuItem {
			float* cv2Normalled = NULL;
			
			Menu *createChildMenu() override {
				struct Cv2NormQuantity : Quantity {
					float* cv2Normalled;
					
					Cv2NormQuantity(float* _cv2Normalled) {
						cv2Normalled = _cv2Normalled;
					}
					void setValue(float value) override {
						*cv2Normalled = math::clamp(value, getMinValue(), getMaxValue());
					}
					float getValue() override {
						return *cv2Normalled;
					}
					float getMinValue() override {return NoteLoop::cv2NormMin;}
					float getMaxValue() override {return NoteLoop::cv2NormMax;}
					float getDefaultValue() override {return NoteLoop::cv2NormDefault;}
					float getDisplayValue() override {return *cv2Normalled;}
					std::string getDisplayValueString() override {
						return string::f("%.2f", clamp(*cv2Normalled, getMinValue(), getMaxValue()));
					}
					void setDisplayValue(float displayValue) override {setValue(displayValue);}
					std::string getLabel() override {return "CV2";}
					std::string getUnit() override {return " V";}
				};
				struct Cv2NormSlider : ui::Slider {
					Cv2NormSlider(float* cv2Normalled) {
						quantity = new Cv2NormQuantity(cv2Normalled);
					}
					~Cv2NormSlider() {
						delete quantity;
					}
				};
			
				Menu *menu = new Menu;

				Cv2NormSlider *cSlider = new Cv2NormSlider(cv2Normalled);
				cSlider->box.size.x = 200.0f;
				menu->addChild(cSlider);
			
				return menu;
			}
		};
		
		Cv2NormItem *c2nItem = createMenuItem<Cv2NormItem>("CV2 input normalization", RIGHT_ARROW);
		c2nItem->cv2Normalled = cv2Normalled;
		menu->addChild(c2nItem);
	}	
	

	void appendContextMenu(Menu *menu) override {
		NoteLoop *module = static_cast<NoteLoop*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), static_cast<SvgPanel*>(getPanel()));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));

		menu->addChild(createSubmenuItem("Tempo multiplier", "", [=](Menu* menu) {
			for (int i = 0; i < NoteLoop::numMults; i++) {
				std::string label = string::f("x %i", (int)NoteLoop::multNum[i]);
				if (NoteLoop::multDen[i] != 1) {
					label += string::f("/%i", (int)NoteLoop::multDen[i]);
				}
				menu->addChild(createCheckMenuItem(label, "",
					[=]() {return module->delMult == i;},
					[=]() {module->delMult = i;}
				));
			}
		}));

		createCv2NormalizationMenu(menu, &(module->cv2NormalledVoltage));
	}	

	
	
	NoteLoopWidget(NoteLoop *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/NoteLoop.svg")));
		SvgPanel* svgPanel = static_cast<SvgPanel*>(getPanel());
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));
		
		static const float col1 = 9.5f;
		static const float col2 = 5.08f * 7.0f - col1;
		// static const float col3 = col2 + 15.0f;
		
		
		static const float row0 = 22.0f;// Len
		static const float row1 = row0 + 16.0f;// Clear+Loop buttons
		static const float row2 = row1 + 10.0f;// Clear+Loop inputs
		static const float row3 = 64.5f;// tempo
		
		static const float row6 = 112.0f;// CV2
		static const float row5 = row6 - 15.0f;// Gate
		static const float row4 = row5 - 15.0f;// CV
		
		
		// row 0
		LenDisplayWidget* lenDisplayWidget = new LenDisplayWidget(mm2px(Vec(col1, row0)), VecPx(35, 24), module);
		addChild(lenDisplayWidget);
		svgPanel->fb->addChild(new DisplayBackground(lenDisplayWidget->box.pos, lenDisplayWidget->box.size, mode));
		
		addParam(createParamCentered<LoopKnob>(mm2px(Vec(col2, row0)), module, NoteLoop::LEN_PARAM));	

		// row 1
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(col1, row1)), module, NoteLoop::CLEAR_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(mm2px(Vec(col1, row1)), module, NoteLoop::CLEAR_LIGHT));
		
		addParam(createParamCentered<LEDBezel>(mm2px(VecPx(col2, row1)), module, NoteLoop::LOOP_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(mm2px(VecPx(col2, row1)), module, NoteLoop::LOOP_LIGHT));		
		
		// row 2
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row2)), true, module, NoteLoop::CLEAR_INPUT, mode));
		
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2, row2)), true, module, NoteLoop::LOOP_INPUT, mode));
		
		// row 3
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row3)), true, module, NoteLoop::CLK_INPUT, mode));
		addParam(createDynamicSwitchCentered<IMSwitch2V>(mm2px(Vec(col2, row3)), module, NoteLoop::TEMPOCV_PARAM, mode, svgPanel));
	
		// row 4
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row4)), true, module, NoteLoop::CV_INPUT, mode));
		
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2, row4)), false, module, NoteLoop::CV_OUTPUT, mode));
		
		// row 5
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row5)), true, module, NoteLoop::GATE_INPUT, mode));
		
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2, row5)), false, module, NoteLoop::GATE_OUTPUT, mode));
		
		// row 6
		addInput(createDynamicPortCentered<IMPort>(mm2px(Vec(col1, row6)), true, module, NoteLoop::CV2_INPUT, mode));
		
		addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col2, row6)), false, module, NoteLoop::CV2_OUTPUT, mode));		
		
		
		
		// addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col3, row4)), false, module, NoteLoop::LOOPSTART_OUTPUT, mode));		
		// addOutput(createDynamicPortCentered<IMPort>(mm2px(Vec(col3, row5)), false, module, NoteLoop::CLEAR_OUTPUT, mode));		
	}
	
	void step() override {
		if (module) {
			NoteLoop* m = static_cast<NoteLoop*>(module);
			
			// Loop light
			m->lights[NoteLoop::LOOP_LIGHT].setBrightness(m->loop ? 1.0f : 0.0f);
		}
		ModuleWidget::step();
	}
};

Model *modelNoteLoop = createModel<NoteLoop, NoteLoopWidget>("NoteLoop");
