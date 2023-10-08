//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "rack.hpp"

using namespace rack;



static constexpr int panelThemeDefaultValue = 0x2;// bit 0 means isWhite for when not using rack global, bit 1 means using rack global dark setting 
static constexpr float panelContrastDefaultValue = 220.0f;
static constexpr float panelContrastMin = 190.0f;
static constexpr float panelContrastMax = 240.0f;
extern NVGcolor SCHEME_RED_IM;
extern NVGcolor SCHEME_GREEN_IM;


void readThemeAndContrastFromDefault();

bool isDark(const int* panelTheme);

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
	// This method also has the main theme refresh stepper for the main panel's frame buffer.
	// It automatically makes DisplayBackground, SwitchOutlineWidget, etc. redraw when isDark() changes since they are children to the main panel's frame buffer   
	// Components such as ports and screws also have their own steppers (that just change the svg, since they don't have framebuffers)
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


