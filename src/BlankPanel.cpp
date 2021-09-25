//***********************************************************************************************
//Blank Panel for VCV Rack by Marc Boulé
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct BlankPanel : Module {
	int panelTheme;

	BlankPanel() {
		config(0, 0, 0, 0);
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) panelTheme = json_integer_value(panelThemeJ);
	}	
};


struct BlankPanelWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		BlankPanel *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	
	void appendContextMenu(Menu *menu) override {
		BlankPanel *module = dynamic_cast<BlankPanel*>(this->module);
		assert(module);
				
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *darkItem = createMenuItem<PanelThemeItem>(darkPanelID, CHECKMARK(module->panelTheme));
		darkItem->module = module;
		menu->addChild(darkItem);

		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));
	}	


	BlankPanelWidget(BlankPanel *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/BlankPanel.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BlankPanel_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 0), mode));
		addChild(createDynamicWidget<IMScrew>(VecPx(15, 365), mode));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 0), mode));
		addChild(createDynamicWidget<IMScrew>(VecPx(box.size.x-30, 365), mode));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((BlankPanel*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((BlankPanel*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
	
};

Model *modelBlankPanel = createModel<BlankPanel, BlankPanelWidget>("Blank-Panel");
