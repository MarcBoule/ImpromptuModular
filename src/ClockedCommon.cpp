//***********************************************************************************************
//Common constants and structs for Clocked and Clkd
//***********************************************************************************************

#include "ClockedCommon.hpp"


void createBPMCVInputMenu(ui::Menu* menu, float* bpmInputScale, float* bpmInputOffset) {
		
	struct BPMCVInputItem : MenuItem {
		float* bpmInputScale = NULL;
		float* bpmInputOffset = NULL;
		bool isScale = true;
		
		Menu *createChildMenu() override {
			struct ScaleAndOffsetQuantity : Quantity {
				float* userValue = NULL;
				bool isScale = true;
				
				ScaleAndOffsetQuantity(float* _userValue, bool _isScale) {
					userValue = _userValue;
					isScale = _isScale;
				}
				void setValue(float value) override {
					*userValue = math::clamp(value, getMinValue(), getMaxValue());
				}
				float getValue() override {
					return *userValue;
				}
				float getMinValue() override {return isScale ? -1.0f : -10.0f;}
				float getMaxValue() override {return -getMinValue();}
				float getDefaultValue() override {return isScale ? 1.0f : 0.0f;}
				float getDisplayValue() override {return *userValue;}
				std::string getDisplayValueString() override {
					if (isScale) {
						return string::f("%.1f", *userValue * 100.0f);
					}
					return string::f("%.2f", *userValue);
				}
				void setDisplayValue(float displayValue) override {setValue(displayValue);}
				std::string getLabel() override {return isScale ? "Scale" : "Offset";}
				std::string getUnit() override {return isScale ? " %" : " V";}
			};
			struct ScaleAndOffsetSlider : ui::Slider {
				ScaleAndOffsetSlider(float* userValue, bool isScale) {
					quantity = new ScaleAndOffsetQuantity(userValue, isScale);
				}
				~ScaleAndOffsetSlider() {
					delete quantity;
				}
			};
		
			Menu *menu = new Menu;// submenu
			
			ScaleAndOffsetSlider *scaleSlider = new ScaleAndOffsetSlider(bpmInputScale, true);
			scaleSlider->box.size.x = 200.0f;
			menu->addChild(scaleSlider);
		
			ScaleAndOffsetSlider *offsetSlider = new ScaleAndOffsetSlider(bpmInputOffset, false);
			offsetSlider->box.size.x = 200.0f;
			menu->addChild(offsetSlider);
		
			return menu;
		}
	};
	
	BPMCVInputItem *bpmcvItem = createMenuItem<BPMCVInputItem>("BPM Input when CV mode", RIGHT_ARROW);
	bpmcvItem->bpmInputScale = bpmInputScale;
	bpmcvItem->bpmInputOffset = bpmInputOffset;
	menu->addChild(bpmcvItem);
}

