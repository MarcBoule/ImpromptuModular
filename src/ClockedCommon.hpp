//***********************************************************************************************
//Common constants and structs for Clocked and Clkd
//***********************************************************************************************

#pragma once

#include "ImpromptuModular.hpp"


static const float ratioValues[34] = {1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 23, 24, 29, 31, 32, 37, 41, 43, 47, 48, 53, 59, 61, 64};

static const int bpmMax = 300;
static const int bpmMin = 30;

static const unsigned int ON_STOP_INT_RST_MSK = 0x1;
static const unsigned int ON_START_INT_RST_MSK = 0x2;
static const unsigned int ON_STOP_EXT_RST_MSK = 0x4;
static const unsigned int ON_START_EXT_RST_MSK = 0x8;


	
struct RatioParam : ParamQuantity {
	float getDisplayValue() override {
		int knobVal = (int) std::round(getValue());
		knobVal = clamp(knobVal, (34 - 1) * -1, 34 - 1);
		if (knobVal < 0) {
			knobVal *= -1;
		}
		return ratioValues[knobVal];
	}
	void setDisplayValue(float displayValue) override {}	
	std::string getUnit() override {
		if (getValue() >= 0.0f) return std::string("x");
		return std::string("÷");
	}
};


struct ResetModeBitToggleItem : MenuItem {
	unsigned int *resetOnStartStopPtr;
	unsigned int mask;
	void onAction(const event::Action &e) override {
		*resetOnStartStopPtr ^= mask;
	}
};

struct OnStartItem : MenuItem {
	unsigned int *resetOnStartStopPtr;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		ResetModeBitToggleItem *startIntItem = createMenuItem<ResetModeBitToggleItem>("Do internal reset", CHECKMARK(*resetOnStartStopPtr & ON_START_INT_RST_MSK));
		startIntItem->resetOnStartStopPtr = resetOnStartStopPtr;
		startIntItem->mask = ON_START_INT_RST_MSK;
		menu->addChild(startIntItem);

		ResetModeBitToggleItem *startExtItem = createMenuItem<ResetModeBitToggleItem>("Send reset pulse", CHECKMARK(*resetOnStartStopPtr & ON_START_EXT_RST_MSK));
		startExtItem->resetOnStartStopPtr = resetOnStartStopPtr;
		startExtItem->mask = ON_START_EXT_RST_MSK;
		menu->addChild(startExtItem);

		return menu;
	}
};	

struct OnStopItem : MenuItem {
	unsigned int *resetOnStartStopPtr;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		ResetModeBitToggleItem *stopIntItem = createMenuItem<ResetModeBitToggleItem>("Do internal reset", CHECKMARK(*resetOnStartStopPtr & ON_STOP_INT_RST_MSK));
		stopIntItem->resetOnStartStopPtr = resetOnStartStopPtr;
		stopIntItem->mask = ON_STOP_INT_RST_MSK;
		menu->addChild(stopIntItem);

		ResetModeBitToggleItem *stopExtItem = createMenuItem<ResetModeBitToggleItem>("Send reset pulse", CHECKMARK(*resetOnStartStopPtr & ON_STOP_EXT_RST_MSK));
		stopExtItem->resetOnStartStopPtr = resetOnStartStopPtr;
		stopExtItem->mask = ON_STOP_EXT_RST_MSK;
		menu->addChild(stopExtItem);

		return menu;
	}
};	



// must have done validateClockModule() before calling this
static void autopatch(PortWidget **slaveResetRunBpmInputs, bool *slaveResetClockOutputsHighPtr) {
	for (Widget* widget : APP->scene->rack->moduleContainer->children) {
		ModuleWidget* moduleWidget = dynamic_cast<ModuleWidget *>(widget);
		if (moduleWidget) {
			int otherId = moduleWidget->module->id;
			if (otherId == clockMaster.id && moduleWidget->model->slug.substr(0, 7) == std::string("Clocked")) {
				// here we have found the clock master, so autopatch to it
				// first we need to find the PortWidgets of the proper outputs of the clock master
				PortWidget* masterResetRunBpmOutputs[3];
				for (PortWidget* outputWidgetOnMaster : moduleWidget->outputs) {
					int outId = outputWidgetOnMaster->portId;
					if (outId >= 4 && outId <= 6) {
						masterResetRunBpmOutputs[outId - 4] = outputWidgetOnMaster;
					}
				}
				// now we can make the actual cables between master and slave
				for (int i = 0; i < 3; i++) {
					std::list<CableWidget*> cablesOnSlaveInput = APP->scene->rack->getCablesOnPort(slaveResetRunBpmInputs[i]);
					if (cablesOnSlaveInput.empty()) {
						CableWidget* cable = new CableWidget();
						cable->setInput(slaveResetRunBpmInputs[i]);
						cable->setOutput(masterResetRunBpmOutputs[i]);
						APP->scene->rack->addCable(cable);
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
	int *idPtr;
	bool *resetClockOutputsHighPtr;
	PortWidget **slaveResetRunBpmInputs;
		
	struct AutopatchMakeMasterItem : MenuItem {
		int *idPtr;
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
			MenuLabel *thismLabel = new MenuLabel();
			thismLabel->text = "This is the current master";
			menu->addChild(thismLabel);
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
				MenuLabel *nomLabel = new MenuLabel();
				nomLabel->text = "No valid master to auto-patch to";
				menu->addChild(nomLabel);
			}
		}

		return menu;
	}
};	
