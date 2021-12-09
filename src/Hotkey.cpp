//***********************************************************************************************
//Chain-able clock module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé, Nigel Sixsmith, Xavier Belmont and Steve Baker
//***********************************************************************************************


#include "ImpromptuModular.hpp"


// Note: to manuall poll key states (not used in this module though): 
// glfwGetKey(APP->window->win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ? 10.0f : 0.0f


// the next two functions are copied from Rack/dep/glfw/tests/events.c
static const char* get_key_name(int key)
{
    switch (key)
    {
        // Printable keys
        case GLFW_KEY_A:            return "A";
        case GLFW_KEY_B:            return "B";
        case GLFW_KEY_C:            return "C";
        case GLFW_KEY_D:            return "D";
        case GLFW_KEY_E:            return "E";
        case GLFW_KEY_F:            return "F";
        case GLFW_KEY_G:            return "G";
        case GLFW_KEY_H:            return "H";
        case GLFW_KEY_I:            return "I";
        case GLFW_KEY_J:            return "J";
        case GLFW_KEY_K:            return "K";
        case GLFW_KEY_L:            return "L";
        case GLFW_KEY_M:            return "M";
        case GLFW_KEY_N:            return "N";
        case GLFW_KEY_O:            return "O";
        case GLFW_KEY_P:            return "P";
        case GLFW_KEY_Q:            return "Q";
        case GLFW_KEY_R:            return "R";
        case GLFW_KEY_S:            return "S";
        case GLFW_KEY_T:            return "T";
        case GLFW_KEY_U:            return "U";
        case GLFW_KEY_V:            return "V";
        case GLFW_KEY_W:            return "W";
        case GLFW_KEY_X:            return "X";
        case GLFW_KEY_Y:            return "Y";
        case GLFW_KEY_Z:            return "Z";
        case GLFW_KEY_1:            return "1";
        case GLFW_KEY_2:            return "2";
        case GLFW_KEY_3:            return "3";
        case GLFW_KEY_4:            return "4";
        case GLFW_KEY_5:            return "5";
        case GLFW_KEY_6:            return "6";
        case GLFW_KEY_7:            return "7";
        case GLFW_KEY_8:            return "8";
        case GLFW_KEY_9:            return "9";
        case GLFW_KEY_0:            return "0";
        case GLFW_KEY_SPACE:        return "SPACE";
        case GLFW_KEY_MINUS:        return "MINUS";
        case GLFW_KEY_EQUAL:        return "EQUAL";
        case GLFW_KEY_LEFT_BRACKET: return "LEFT_BRACKET";
        case GLFW_KEY_RIGHT_BRACKET: return "RIGHT_BRACKET";
        case GLFW_KEY_BACKSLASH:    return "BACKSLASH";
        case GLFW_KEY_SEMICOLON:    return "SEMICOLON";
        case GLFW_KEY_APOSTROPHE:   return "APOSTROPHE";
        case GLFW_KEY_GRAVE_ACCENT: return "GRAVE_ACCENT";
        case GLFW_KEY_COMMA:        return "COMMA";
        case GLFW_KEY_PERIOD:       return "PERIOD";
        case GLFW_KEY_SLASH:        return "SLASH";
        case GLFW_KEY_WORLD_1:      return "WORLD_1";
        case GLFW_KEY_WORLD_2:      return "WORLD_2";

        // Function keys
        case GLFW_KEY_ESCAPE:       return "ESCAPE";
        case GLFW_KEY_F1:           return "F1";
        case GLFW_KEY_F2:           return "F2";
        case GLFW_KEY_F3:           return "F3";
        case GLFW_KEY_F4:           return "F4";
        case GLFW_KEY_F5:           return "F5";
        case GLFW_KEY_F6:           return "F6";
        case GLFW_KEY_F7:           return "F7";
        case GLFW_KEY_F8:           return "F8";
        case GLFW_KEY_F9:           return "F9";
        case GLFW_KEY_F10:          return "F10";
        case GLFW_KEY_F11:          return "F11";
        case GLFW_KEY_F12:          return "F12";
        case GLFW_KEY_F13:          return "F13";
        case GLFW_KEY_F14:          return "F14";
        case GLFW_KEY_F15:          return "F15";
        case GLFW_KEY_F16:          return "F16";
        case GLFW_KEY_F17:          return "F17";
        case GLFW_KEY_F18:          return "F18";
        case GLFW_KEY_F19:          return "F19";
        case GLFW_KEY_F20:          return "F20";
        case GLFW_KEY_F21:          return "F21";
        case GLFW_KEY_F22:          return "F22";
        case GLFW_KEY_F23:          return "F23";
        case GLFW_KEY_F24:          return "F24";
        case GLFW_KEY_F25:          return "F25";
        case GLFW_KEY_UP:           return "UP";
        case GLFW_KEY_DOWN:         return "DOWN";
        case GLFW_KEY_LEFT:         return "LEFT";
        case GLFW_KEY_RIGHT:        return "RIGHT";
        case GLFW_KEY_LEFT_SHIFT:   return "LEFT_SHIFT";
        case GLFW_KEY_RIGHT_SHIFT:  return "RIGHT_SHIFT";
        case GLFW_KEY_LEFT_CONTROL: return "LEFT_CONTROL";
        case GLFW_KEY_RIGHT_CONTROL: return "RIGHT_CONTROL";
        case GLFW_KEY_LEFT_ALT:     return "LEFT_ALT";
        case GLFW_KEY_RIGHT_ALT:    return "RIGHT_ALT";
        case GLFW_KEY_TAB:          return "TAB";
        case GLFW_KEY_ENTER:        return "ENTER";
        case GLFW_KEY_BACKSPACE:    return "BACKSPACE";
        case GLFW_KEY_INSERT:       return "INSERT";
        case GLFW_KEY_DELETE:       return "DELETE";
        case GLFW_KEY_PAGE_UP:      return "PAGE_UP";
        case GLFW_KEY_PAGE_DOWN:    return "PAGE_DOWN";
        case GLFW_KEY_HOME:         return "HOME";
        case GLFW_KEY_END:          return "END";
        case GLFW_KEY_KP_0:         return "KEYPAD_0";
        case GLFW_KEY_KP_1:         return "KEYPAD_1";
        case GLFW_KEY_KP_2:         return "KEYPAD_2";
        case GLFW_KEY_KP_3:         return "KEYPAD_3";
        case GLFW_KEY_KP_4:         return "KEYPAD_4";
        case GLFW_KEY_KP_5:         return "KEYPAD_5";
        case GLFW_KEY_KP_6:         return "KEYPAD_6";
        case GLFW_KEY_KP_7:         return "KEYPAD_7";
        case GLFW_KEY_KP_8:         return "KEYPAD_8";
        case GLFW_KEY_KP_9:         return "KEYPAD_9";
        case GLFW_KEY_KP_DIVIDE:    return "KEYPAD_DIVIDE";
        case GLFW_KEY_KP_MULTIPLY:  return "KEYPAD_MULTIPLY";
        case GLFW_KEY_KP_SUBTRACT:  return "KEYPAD_SUBTRACT";
        case GLFW_KEY_KP_ADD:       return "KEYPAD_ADD";
        case GLFW_KEY_KP_DECIMAL:   return "KEYPAD_DECIMAL";
        case GLFW_KEY_KP_EQUAL:     return "KEYPAD_EQUAL";
        case GLFW_KEY_KP_ENTER:     return "KEYPAD_ENTER";
        case GLFW_KEY_PRINT_SCREEN: return "PRINT_SCREEN";
        case GLFW_KEY_NUM_LOCK:     return "NUM_LOCK";
        case GLFW_KEY_CAPS_LOCK:    return "CAPS_LOCK";
        case GLFW_KEY_SCROLL_LOCK:  return "SCROLL_LOCK";
        case GLFW_KEY_PAUSE:        return "PAUSE";
        case GLFW_KEY_LEFT_SUPER:   return "LEFT_SUPER";
        case GLFW_KEY_RIGHT_SUPER:  return "RIGHT_SUPER";
        case GLFW_KEY_MENU:         return "MENU";

        default:                    return "UNKNOWN";
    }
}
static void get_mods_name(char *name, int mods)
{
	name[0] = 0;
    if (mods & GLFW_MOD_SHIFT) {
		if (name[0] != '\0') strcat(name, "+");
        strcat(name, "Shift");
	}
    if (mods & GLFW_MOD_CONTROL) {
		if (name[0] != '\0') strcat(name, "+");
        strcat(name, "Ctrl");
	}
    if (mods & GLFW_MOD_ALT) {
		if (name[0] != '\0') strcat(name, "+");
        strcat(name, "Alt");
	}
    if (mods & GLFW_MOD_SUPER) {
		if (name[0] != '\0') strcat(name, "+");
        strcat(name, "Super");
	}
	// caps and num locks treated as normal keys
    // if (mods & GLFW_MOD_CAPS_LOCK) {
		// if (name[0] != '\0') strcat(name, "+");
        // strcat(name, "Capslock-on");
	// }
    // if (mods & GLFW_MOD_NUM_LOCK) {
		// if (name[0] != '\0') strcat(name, "+");
        // strcat(name, "Numlock-on");
	// }
}


//*****************************************************************************


struct Hotkey : Module {
	enum ParamIds {
		RECORD_KEY_PARAM,
		DELAY_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		TRIG_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(RECORD_KEY_LIGHT, 2),
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float maxDelay = 1.0f;// in seconds
		
	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	int hotkey;
	int hotkeyMods;
	bool requestTrig;// will stay high as long as delayCnt is not at 0

	// No need to save, with reset
	unsigned long delayCnt;// emit pulse when reaches 0, 
	
	// No need to save, no reset
	dsp::PulseGenerator trigOutPulse;
	dsp::PulseGenerator trigLightPulse;
	RefreshCounter refresh;

	
	Hotkey() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(RECORD_KEY_PARAM, 0.0f, 1.0f, 0.0f, "Record hotkey");
		configParam(DELAY_PARAM, 0.0f, maxDelay, 0.0f, "Delay");
		
		configOutput(TRIG_OUTPUT, "Trigger");
		
		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}
	

	void onReset() override {
		hotkey = GLFW_KEY_SPACE;
		hotkeyMods = 0;
		requestTrig = false;
		resetNonJson(false);
	}
	void resetNonJson(bool delayed) {// delay thread sensitive parts (i.e. schedule them so that process() will do them)
		delayCnt = 0;
	}
	
	void onRandomize() override {
	}
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// hotkey
		json_object_set_new(rootJ, "hotkey", json_integer(hotkey));

		// hotkeyMods
		json_object_set_new(rootJ, "hotkeyMods", json_integer(hotkeyMods));

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

		// hotkey
		json_t *hotkeyJ = json_object_get(rootJ, "hotkey");
		if (hotkeyJ)
			hotkey = json_integer_value(hotkeyJ);

		// hotkeyMods
		json_t *hotkeyModsJ = json_object_get(rootJ, "hotkeyMods");
		if (hotkeyModsJ)
			hotkeyMods = json_integer_value(hotkeyModsJ);

		params[RECORD_KEY_PARAM].setValue(0.0f);

		resetNonJson(true);
	}

	
	void onSampleRateChange() override {
	}		
	
	
	bool hotkeyPressed(int newKey, int newMods) {// return true if processed the key	
		bool processed = false;
		
		if (params[RECORD_KEY_PARAM].getValue() >= 0.5f) {// if recording the keypress
			hotkey = newKey;
			hotkeyMods = newMods;
			params[RECORD_KEY_PARAM].setValue(0.0f);
			processed = true;
		}
		else {// normal key press when not recording
			if (newKey == hotkey && newMods == hotkeyMods) {
				delayCnt = (unsigned long)(APP->engine->getSampleRate() * maxDelay * params[DELAY_PARAM].getValue());
				requestTrig = true;
				processed = true;
			}
		}
		
		return processed;
	}


	void process(const ProcessArgs &args) override {
		
		// Inputs
		if (refresh.processInputs()) {

		}// userInputs refresh


		if (requestTrig && (delayCnt == 0)) {
			trigOutPulse.trigger(0.002f);
			trigLightPulse.trigger(0.1f);
			requestTrig = false;
		}
		
		bool trigOutState = trigOutPulse.process(args.sampleTime);
		outputs[TRIG_OUTPUT].setVoltage((trigOutState ? 10.0f : 0.0f));
		
		// lights
		if (refresh.processLights()) {
			float deltaTime = (float)args.sampleTime * (RefreshCounter::displayRefreshStepSkips);
			lights[RECORD_KEY_LIGHT + 0].setSmoothBrightness(trigLightPulse.process(deltaTime) > 0.0f ? 1.0f : 0.0f, deltaTime);// green
			lights[RECORD_KEY_LIGHT + 1].setBrightness(params[RECORD_KEY_PARAM].getValue());// red
		}// lightRefreshCounter
		
		if (delayCnt > 0) {
			delayCnt--;
		}
	}// process()
	
};


struct HotkeyWidget : ModuleWidget {
	char strBuf[512];
	
	void appendContextMenu(Menu *menu) override {
		Hotkey *module = dynamic_cast<Hotkey*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Current hotkey:"));
		
		get_mods_name(strBuf, module->hotkeyMods);
		if (strBuf[0] != '\0') strcat(strBuf, "+");
		strcat(strBuf, get_key_name(module->hotkey));
		menu->addChild(createMenuLabel(strBuf));
	}	

	
	HotkeyWidget(Hotkey *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Hotkey.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));


		static const float centerX = 22.5f;
		
		// Record-key LED bezel and light
		static const int buttonY = 84;
		SvgSwitch *ledBez;
		addParam(ledBez = createParamCentered<LEDBezel>(VecPx(centerX, buttonY), module, Hotkey::RECORD_KEY_PARAM));
		ledBez->momentary = false;
		addChild(createLightCentered<LEDBezelLight<GreenRedLightIM>>(VecPx(centerX, buttonY), module, Hotkey::RECORD_KEY_LIGHT));
		
		// Delay knob
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(centerX, 220.0f), module, Hotkey::DELAY_PARAM, mode));

		// trig out
		addOutput(createDynamicPortCentered<IMPort>(VecPx(centerX, 288.0f), false, module, Hotkey::TRIG_OUTPUT, mode));
	}
	
	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			Hotkey *module = dynamic_cast<Hotkey*>(this->module);
			if (module->hotkeyPressed(e.key, e.mods & RACK_MOD_MASK)) {
				e.consume(this);
				return;
			}
		}
		ModuleWidget::onHoverKey(e); 
	}
	
};

Model *modelHotkey = createModel<Hotkey, HotkeyWidget>("Hotkey");
