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
	bool manualLockLow[4];// index 0 is lowest
};

