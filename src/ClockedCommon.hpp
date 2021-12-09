//***********************************************************************************************
//Common constants and structs for Clocked and Clkd
//***********************************************************************************************

#pragma once

#include "ImpromptuModular.hpp"

static const int numRatios = 35;
static const float ratioValues[numRatios] = {1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 23, 24, 29, 31, 32, 37, 41, 43, 47, 48, 53, 59, 61, 64, 96};

static const int bpmMax = 300;
static const int bpmMin = 30;

static const unsigned int ON_STOP_INT_RST_MSK = 0x1;
static const unsigned int ON_START_INT_RST_MSK = 0x2;
static const unsigned int ON_STOP_EXT_RST_MSK = 0x4;
static const unsigned int ON_START_EXT_RST_MSK = 0x8;


	
struct RatioParam : ParamQuantity {
	float getDisplayValue() override {
		int knobVal = (int) std::round(getValue());
		knobVal = clamp(knobVal, (numRatios - 1) * -1, numRatios - 1);
		if (knobVal < 0) {
			return -ratioValues[-knobVal];
		}
		return ratioValues[knobVal];
	}
	void setDisplayValue(float displayValue) override {
		bool div = displayValue < 0;
		float setVal;
		if (div) {
			displayValue = -displayValue;
		}
		// code below is not pretty, but easiest since irregular spacing of ratio values
		if (displayValue > 80.0f) {
			setVal = 34.0f;
		}
		else if (displayValue >= 62.5f) {
			setVal = 33.0f;
		}
		else if (displayValue >= 60.0f) {
			setVal = 32.0f;
		}
		else if (displayValue >= 56.0f) {
			setVal = 31.0f;
		}
		else if (displayValue >= 50.5f) {
			setVal = 30.0f;
		}
		else if (displayValue >= 47.5f) {
			setVal = 29.0f;
		}
		else if (displayValue >= 45.0f) {
			setVal = 28.0f;
		}
		else if (displayValue >= 42.0f) {
			setVal = 27.0f;
		}
		else if (displayValue >= 39.0f) {
			setVal = 26.0f;
		}
		else if (displayValue >= 34.5f) {
			setVal = 25.0f;
		}
		else if (displayValue >= 31.5f) {
			setVal = 24.0f;
		}
		else if (displayValue >= 30.0f) {
			setVal = 23.0f;
		}
		else if (displayValue >= 26.5f) {
			setVal = 22.0f;
		}
		else if (displayValue >= 23.5f) {
			setVal = 21.0f;
		}
		else if (displayValue >= 21.0f) {
			setVal = 20.0f;
		}
		else if (displayValue >= 18.0f) {
			setVal = 19.0f;
		}
		else if (displayValue >= 16.5f) {
			setVal = 18.0f;
		}
		else if (displayValue >= 2.75f) {
			// 3 to 16 map into 4 to 17
			setVal = 1.0 + std::round(displayValue);
		}
		else if (displayValue >= 2.25f) {
			setVal = 3.0f;
		}
		else if (displayValue >= 1.75f) {
			setVal = 2.0f;
		}
		else if (displayValue >= 1.25f) {
			setVal = 1.0f;
		}
		else {
			setVal = 0.0f;
		}
		if (setVal != 0.0f && div) {
			setVal *= -1.0f;
		}
		setValue(setVal);
	}	
	std::string getUnit() override {
		if (getValue() >= 0.0f) return std::string("x");
		return std::string(" (÷)");
	}
};


struct ResetModeBitToggleItem : MenuItem {
	unsigned int *resetOnStartStopPtr;
	unsigned int mask;
	void onAction(const event::Action &e) override {
		*resetOnStartStopPtr ^= mask;
	}
};


// must have done validateClockModule() before calling this
static void autopatch(PortWidget **slaveResetRunBpmInputs, bool *slaveResetClockOutputsHighPtr) {
	for (Widget* widget : APP->scene->rack->getModuleContainer()->children) {
		ModuleWidget* moduleWidget = dynamic_cast<ModuleWidget *>(widget);
		if (moduleWidget) {
			int64_t otherId = moduleWidget->module->id;
			if (otherId == clockMaster.id && moduleWidget->model->slug.substr(0, 7) == std::string("Clocked")) {
				// here we have found the clock master, so autopatch to it
				// first we need to find the PortWidgets of the proper outputs of the clock master
				PortWidget* masterResetRunBpmOutputs[3];
				for (PortWidget* outputWidgetOnMaster : moduleWidget->getOutputs()) {
					int outId = outputWidgetOnMaster->portId;
					if (outId >= 4 && outId <= 6) {
						masterResetRunBpmOutputs[outId - 4] = outputWidgetOnMaster;
					}
				}
				// now we can make the actual cables between master and slave
				for (int i = 0; i < 3; i++) {
					std::vector<CableWidget*> cablesOnSlaveInput = APP->scene->rack->getCablesOnPort(slaveResetRunBpmInputs[i]);
					if (cablesOnSlaveInput.empty()) {
						CableWidget* cw = new CableWidget();
						// cable->setInput(slaveResetRunBpmInputs[i]);
						// cable->setOutput(masterResetRunBpmOutputs[i]);
						cw->color = APP->scene->rack->getNextCableColor();
						cw->inputPort = slaveResetRunBpmInputs[i];
						cw->outputPort = masterResetRunBpmOutputs[i];
						cw->updateCable();
						APP->scene->rack->addCable(cw);
					}
				}
				*slaveResetClockOutputsHighPtr = clockMaster.resetClockOutputsHigh;
				return;
			}
		}
	}
	// assert(false);
	// here the clock master was not found; this should never happen, since AutopatchToMasterItem is never invoked when a valid master does not exist
}

struct AutopatchItem : MenuItem {
	int64_t* idPtr;
	bool *resetClockOutputsHighPtr;
	PortWidget **slaveResetRunBpmInputs;
		
	struct AutopatchMakeMasterItem : MenuItem {
		int64_t* idPtr;
		bool *resetClockOutputsHighPtr;
		void onAction(const event::Action &e) override {
			clockMaster.setAsMaster(*idPtr, *resetClockOutputsHighPtr);
		}
	};

	struct AutopatchToMasterItem : MenuItem {// must have done validateClockModule() before allowing this onAction to execute
		PortWidget **slaveResetRunBpmInputs;
		bool *resetClockOutputsHighPtr;
		void onAction(const event::Action &e) override {
			autopatch(slaveResetRunBpmInputs, resetClockOutputsHighPtr);
		}
	};


	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		if (clockMaster.id == *idPtr) {
			// the module is the clock master
			menu->addChild(createMenuLabel("This is the current master"));
		}
		else {
			// the module is not the master
			AutopatchMakeMasterItem *mastItem = createMenuItem<AutopatchMakeMasterItem>("Make this the master", "");
			mastItem->idPtr = idPtr;
			mastItem->resetClockOutputsHighPtr = resetClockOutputsHighPtr;
			menu->addChild(mastItem);
			
			if (clockMaster.validateClockModule()) {
				AutopatchToMasterItem *connItem = createMenuItem<AutopatchToMasterItem>("Connect to master (Ctrl/Cmd + M)", "");
				connItem->slaveResetRunBpmInputs = slaveResetRunBpmInputs;
				connItem->resetClockOutputsHighPtr = resetClockOutputsHighPtr;
				menu->addChild(connItem);
			}
			else {
				menu->addChild(createMenuLabel("No valid master to auto-patch to"));
			}
		}

		return menu;
	}
};	
