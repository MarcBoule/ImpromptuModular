//***********************************************************************************************
//Blank Panel for VCV Rack by Marc Boulé
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct BlankPanel : Module {
	int panelTheme;
	float panelContrast;

	BlankPanel() {
		config(0, 0, 0, 0);
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) panelTheme = json_integer_value(panelThemeJ);

		json_t *panelContrastJ = json_object_get(rootJ, "panelContrast");
		if (panelContrastJ)
			panelContrast = json_number_value(panelContrastJ);

	}	
};


struct BlankPanelWidget : ModuleWidget {
	void appendContextMenu(Menu *menu) override {
		BlankPanel *module = dynamic_cast<BlankPanel*>(this->module);
		assert(module);
				
		menu->addChild(new MenuSeparator());

		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());
	}	


	BlankPanelWidget(BlankPanel *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/BlankPanel.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));
	}
};

Model *modelBlankPanel = createModel<BlankPanel, BlankPanelWidget>("Blank-Panel");
