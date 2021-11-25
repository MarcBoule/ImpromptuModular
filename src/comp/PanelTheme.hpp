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
	int* panelThemeSrc = NULL;
	InverterWidget(Vec _size, int* _panelThemeSrc) {
		box.size = _size;
		panelThemeSrc = _panelThemeSrc;
	}
	void draw(const DrawArgs& args) override;
};