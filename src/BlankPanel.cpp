//***********************************************************************************************
//Blank Panel for VCV Rack by Marc Boulé
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct BlankPanel : Module {
	BlankPanel() {
		config(0, 0, 0, 0);
	}
};


struct BlankPanelWidget : ModuleWidget {
	int screwType = 1;
	
	BlankPanelWidget(BlankPanel *module) {
		setModule(module);
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BlankPanel_dark.svg")));

		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), &screwType));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0),  &screwType));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365),  &screwType));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365),  &screwType));
	}
};

Model *modelBlankPanel = createModel<BlankPanel, BlankPanelWidget>("Blank-Panel");
