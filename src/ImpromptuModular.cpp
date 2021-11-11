//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"


Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	p->addModel(modelBigButtonSeq);
	p->addModel(modelBigButtonSeq2);
	p->addModel(modelChordKey);
	p->addModel(modelChordKeyExpander);
	p->addModel(modelClocked);
	p->addModel(modelClockedExpander);
	p->addModel(modelClkd);
	p->addModel(modelCvPad);
	p->addModel(modelFoundry);
	p->addModel(modelFoundryExpander);
	p->addModel(modelFourView);
	p->addModel(modelGateSeq64);
	p->addModel(modelGateSeq64Expander);
	p->addModel(modelHotkey);
	p->addModel(modelPart);
	p->addModel(modelPhraseSeq16);
	p->addModel(modelPhraseSeq32);
	p->addModel(modelPhraseSeqExpander);
	p->addModel(modelProbKey);
	// p->addModel(modelProbKeyExpander);
	p->addModel(modelSemiModularSynth);
	p->addModel(modelTact);
	p->addModel(modelTact1);
	p->addModel(modelTactG);
	p->addModel(modelTwelveKey);
	p->addModel(modelWriteSeq32);
	p->addModel(modelWriteSeq64);
	p->addModel(modelBlankPanel);
}


// General objects

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


ClockMaster clockMaster;  



// General functions

static const char noteLettersSharp[12] = {'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B'};
static const char noteLettersFlat [12] = {'C', 'D', 'D', 'E', 'E', 'F', 'G', 'G', 'A', 'A', 'B', 'B'};
static const char isBlackKey      [12] = { 0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0 };

void printNoteNoOct(int note, char* text, bool sharp) {// text must be at least 3 chars long (three displayed chars plus end of string)
	// given note is a pitch CV multiplied by 12 and rounded to nearest integer
	note = eucMod(note, 12);
	text[0] = sharp ? noteLettersSharp[note] : noteLettersFlat[note];// note letter
	text[1] = (isBlackKey[note] == 1) ? (sharp ? '\"' : 'b' ) : ' ';// sharp/flat
	text[2] = 0;
}


int printNote(float cvVal, char* text, bool sharp) {// text must be at least 4 chars long (three displayed chars plus end of string)
	// return cursor position of eos
	
	int indexNote;
	int octave;
	calcNoteAndOct(cvVal, &indexNote, &octave);
	
	// note letter
	text[0] = sharp ? noteLettersSharp[indexNote] : noteLettersFlat[indexNote];
	
	// octave number
	int cursor = 1;
	octave += 4;
	if (octave >= 0 && octave <= 9) {
		text[1] = (char) ( 0x30 + octave);
		cursor++;
	}
	
	// sharp/flat
	if (isBlackKey[indexNote] == 1) {
		text[cursor] = (sharp ? '\"' : 'b' );
		cursor++;
	}
	
	text[cursor] = 0;
	return cursor;
}


int moveIndex(int index, int indexNext, int numSteps) {
	if (indexNext < 0)
		index = numSteps - 1;
	else
	{
		if (indexNext - index >= 0) { // if moving right or same place
			if (indexNext >= numSteps)
				index = 0;
			else
				index = indexNext;
		}
		else { // moving left 
			if (indexNext >= numSteps)
				index = numSteps - 1;
			else
				index = indexNext;
		}
	}
	return index;
}


void InstantiateExpanderItem::onAction(const event::Action &e) {
	// Create Module and ModuleWidget
	engine::Module* module = model->createModule();
	APP->engine->addModule(module);

	ModuleWidget* mw = model->createModuleWidget(module);
	// APP->scene->rack->addModuleAtMouse(mw);
	
	// ModuleWidget *mw = model->createModuleWidget();
	if (mw) {
		APP->scene->rack->setModulePosNearest(mw, posit);
		APP->scene->rack->addModule(mw);
		history::ModuleAdd *h = new history::ModuleAdd;
		h->name = "create expander module";
		h->setModule(mw);
		APP->history->push(h);
	}
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
				float getMinValue() override {return 195.0f;}
				float getMaxValue() override {return 230.0f;}
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
				void onAction(const event::Action &e) override {
					saveDarkAsDefault(rightText.empty());
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

			menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));
		
			return menu;
		}
	};
	
	PanelThemeItem *ptItem = createMenuItem<PanelThemeItem>("Panel theme", RIGHT_ARROW);
	ptItem->panelTheme = panelTheme;
	ptItem->panelContrast = panelContrast;
	ptItem->mainPanel = mainPanel;
	menu->addChild(ptItem);
}