//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#pragma once

#include "ImpromptuModular.hpp"
#include "Interop.hpp"


inline float quantize(float cv, bool enable) {
	return enable ? (std::round(cv * 12.0f) / 12.0f) : cv;
};


inline void rotateSeq(int delta, int numSteps, float *cv, int *gates) {
	float rotCV;
	int rotGate;
	int iStart = 0;
	int iEnd = numSteps - 1;
	int iRot = iStart;
	int iDelta = 1;
	if (delta == 1) {
		iRot = iEnd;
		iDelta = -1;
	}
	rotCV = cv[iRot];
	rotGate = gates[iRot];
	for ( ; ; iRot += iDelta) {
		if (iDelta == 1 && iRot >= iEnd) break;
		if (iDelta == -1 && iRot <= iStart) break;
		cv[iRot] = cv[iRot + iDelta];
		gates[iRot] = gates[iRot + iDelta];
	}
	cv[iRot] = rotCV;
	gates[iRot] = rotGate;
};

