//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "rack.hpp"

using namespace rack;



static constexpr float panelContrastDefault = 220.0f;
static constexpr float panelContrastMin = 190.0f;
static constexpr float panelContrastMax = 240.0f;
extern NVGcolor SCHEME_RED_IM;
extern NVGcolor SCHEME_GREEN_IM;


void readThemeAndContrastFromDefault();

bool isDark(int* panelTheme);

void saveThemeAndContrastAsDefault(int panelTheme, float panelContrast);
void loadThemeAndContrastFromDefault(int* panelTheme, float* panelContrast);

void createPanelThemeMenu(ui::Menu* menu, int* panelTheme, float* panelContrast, SvgPanel* mainPanel);


struct PanelBaseWidget : TransparentWidget {
	float* panelContrastSrc = NULL;
	PanelBaseWidget(Vec _size, float* _panelContrastSrc) {
		box.size = _size;
		panelContrastSrc = _panelContrastSrc;
	}
	void draw(const DrawArgs& args) override;
};


struct InverterWidget : TransparentWidget {
	// this method also has the main theme refresh stepper for the main panel's frame buffer
	// it automatically makes DisplayBackground, SwitchOutlineWidget, etc. redraw when isDark() changes since
	//   they are children to the main panel's frame buffer
	// but components such as port and screws have their own steppers (TODO optimize this since screws are also children?)
	// TODO make theme menu work properly when control clicking
	SvgPanel* mainPanel;
	int* panelThemeSrc = NULL;// aka mode
	int oldMode = -1;
	InverterWidget(SvgPanel* _mainPanel, int* _panelThemeSrc) {
		mainPanel = _mainPanel;
		box.size = mainPanel->box.size;
		panelThemeSrc = _panelThemeSrc;
	}
	void refreshForTheme();
    void step() override;
	void draw(const DrawArgs& args) override;
};


