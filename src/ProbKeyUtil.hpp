//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#pragma once

#include "ImpromptuModular.hpp"
#include <time.h>


// Expanders
// --------

// From mother to expander
struct PkxIntfFromMother {
	int panelTheme;
	float minCvChan0;
};


// From expander to mother
struct PkxIntfFromExp {
	uint8_t manualLockLow;// index 0 is lowest note locked
	
	bool getLowLock(uint8_t i) {
		return (manualLockLow & (0x1 << i)) != 0;
	}
	void setLowLock(uint8_t i) {
		manualLockLow |= (0x1 << i);
	}
};

