//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "PanelTheme.hpp"


int defaultPanelTheme;
float defaultPanelContrast;

void writeThemeAndContrastAsDefault() {
	json_t *settingsJ = json_object();
	json_object_set_new(settingsJ, "themeDefault", json_integer(defaultPanelTheme));
	json_object_set_new(settingsJ, "contrastDefault", json_real(defaultPanelContrast));
	std::string settingsFilename = asset::user("ImpromptuModular.json");
	FILE *file = fopen(settingsFilename.c_str(), "w");
	if (file) {
		json_dumpf(settingsJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
		fclose(file);
	}
	json_decref(settingsJ);
}


void saveThemeAndContrastAsDefault(int panelTheme, float panelContrast) {
	defaultPanelTheme = panelTheme;
	defaultPanelContrast = panelContrast;
	writeThemeAndContrastAsDefault();
}


void loadThemeAndContrastFromDefault(int* panelTheme, float* panelContrast) {
	*panelTheme = defaultPanelTheme;
	*panelContrast = defaultPanelContrast;
}


bool isDark(int* panelTheme) {
	if (panelTheme != NULL) {
		return (*panelTheme != 0);
	}
	return (defaultPanelTheme != 0);
}


void readThemeAndContrastFromDefault() {
	std::string settingsFilename = asset::user("ImpromptuModular.json");
	FILE *file = fopen(settingsFilename.c_str(), "r");
	if (!file) {
		defaultPanelTheme = 0;
		defaultPanelContrast = panelContrastDefault;
		writeThemeAndContrastAsDefault();
		return;
	}
	json_error_t error;
	json_t *settingsJ = json_loadf(file, 0, &error);
	if (!settingsJ) {
		// invalid setting json file
		fclose(file);
		defaultPanelTheme = 0;
		defaultPanelContrast = panelContrastDefault;
		writeThemeAndContrastAsDefault();
		return;
	}
	
	json_t *themeDefaultJ = json_object_get(settingsJ, "themeDefault");
	if (themeDefaultJ) {
		defaultPanelTheme = json_integer_value(themeDefaultJ);
	}
	else {
		defaultPanelTheme = 0;
	}
	
	json_t *contrastDefaultJ = json_object_get(settingsJ, "contrastDefault");
	if (contrastDefaultJ) {
		defaultPanelContrast = json_number_value(contrastDefaultJ);
	}
	else {
		defaultPanelContrast = panelContrastDefault;
	}
	
	fclose(file);
	json_decref(settingsJ);
	return;
}


void createPanelThemeMenu(ui::Menu* menu, int* panelTheme, float* panelContrast, SvgPanel* mainPanel) {
		
	struct PanelThemeItem : MenuItem {
		int* panelTheme = NULL;
		float* panelContrast = NULL;
		SvgPanel* mainPanel;
		
		Menu *createChildMenu() override {
			struct PanelThemeDarkItem : MenuItem {
				int* panelTheme = NULL;
				SvgPanel* mainPanel;
				void onAction(const event::Action &e) override {
					*panelTheme ^= 0x1;
					mainPanel->fb->dirty = true;
					e.unconsume();
				}
			};

			struct PanelContrastQuantity : Quantity {
				float* panelContrast;
				SvgPanel* mainPanel;
				
				PanelContrastQuantity(float* _panelContrast, SvgPanel* _mainPanel) {
					panelContrast = _panelContrast;
					mainPanel = _mainPanel;
				}
				void setValue(float value) override {
					*panelContrast = math::clamp(value, getMinValue(), getMaxValue());
					mainPanel->fb->dirty = true;
				}
				float getValue() override {
					return *panelContrast;
				}
				float getMinValue() override {return panelContrastMin;}
				float getMaxValue() override {return panelContrastMax;}
				float getDefaultValue() override {return panelContrastDefault;}
				float getDisplayValue() override {return *panelContrast;}
				std::string getDisplayValueString() override {
					return string::f("%.1f", rescale(*panelContrast, getMinValue(), getMaxValue(), 0.0f, 100.0f));
				}
				void setDisplayValue(float displayValue) override {setValue(displayValue);}
				std::string getLabel() override {return "Panel contrast";}
				std::string getUnit() override {return "";}
			};
			struct PanelContrastSlider : ui::Slider {
				PanelContrastSlider(float* panelContrast, SvgPanel* mainPanel) {
					quantity = new PanelContrastQuantity(panelContrast, mainPanel);
				}
				~PanelContrastSlider() {
					delete quantity;
				}
			};
			
			struct DarkDefaultItem : MenuItem {
				int* panelTheme = NULL;
				float* panelContrast = NULL;
				void onAction(const event::Action &e) override {
					saveThemeAndContrastAsDefault(*panelTheme, *panelContrast);
				}
			};	
			
			Menu *menu = new Menu;
			
			PanelThemeDarkItem *ptdItem = createMenuItem<PanelThemeDarkItem>("Dark", CHECKMARK(*panelTheme));
			ptdItem->panelTheme = panelTheme;
			ptdItem->mainPanel = mainPanel;
			menu->addChild(ptdItem);
			
			PanelContrastSlider *cSlider = new PanelContrastSlider(panelContrast, mainPanel);
			cSlider->box.size.x = 200.0f;
			menu->addChild(cSlider);

			menu->addChild(new MenuSeparator());

			DarkDefaultItem *ddItem = createMenuItem<DarkDefaultItem>("Set as default", "");
			ddItem->panelTheme = panelTheme;
			ddItem->panelContrast = panelContrast;
			menu->addChild(ddItem);
		
			return menu;
		}
	};
	
	PanelThemeItem *ptItem = createMenuItem<PanelThemeItem>("Panel theme", RIGHT_ARROW);
	ptItem->panelTheme = panelTheme;
	ptItem->panelContrast = panelContrast;
	ptItem->mainPanel = mainPanel;
	menu->addChild(ptItem);
}


void PanelBaseWidget::draw(const DrawArgs& args) {
	nvgBeginPath(args.vg);
	NVGcolor baseColor;
	if (panelContrastSrc) {
		baseColor = nvgRGB(*panelContrastSrc, *panelContrastSrc, *panelContrastSrc);
	}
	else {
		int themeDefault;
		float contrastDefault;
		loadThemeAndContrastFromDefault(&themeDefault, &contrastDefault);
		baseColor = nvgRGB(contrastDefault, contrastDefault, contrastDefault);
	}
	nvgFillColor(args.vg, baseColor);
	nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
	nvgFill(args.vg);
	TransparentWidget::draw(args);
}


void InverterWidget::draw(const DrawArgs& args) {
	TransparentWidget::draw(args);
	if (isDark(panelThemeSrc)) {
		// nvgSave(args.vg);
		nvgBeginPath(args.vg);
		nvgFillColor(args.vg, SCHEME_WHITE);// this is the source, the current framebuffer is the dest	
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgGlobalCompositeBlendFuncSeparate(args.vg, 
			NVG_ONE_MINUS_DST_COLOR,// srcRGB
			NVG_ZERO,// dstRGB
			NVG_ONE_MINUS_DST_COLOR,// srcAlpha
			NVG_ONE);// dstAlpha
		// blend factor: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L86
		// OpenGL blend doc: https://www.khronos.org/opengl/wiki/Blending
		nvgFill(args.vg);
		nvgClosePath(args.vg);
		// nvgRestore(args.vg);
	}			
}

