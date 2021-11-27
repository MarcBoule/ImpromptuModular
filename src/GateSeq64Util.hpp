//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#pragma once

#include "ImpromptuModular.hpp"
#include "PhraseSeqUtil.hpp"


class StepAttributesGS {
	unsigned short attributes;
	
	public:
	
	static const unsigned short ATT_MSK_PROB = 0xFF;
	static const unsigned short ATT_MSK_GATEP = 0x100;
	static const unsigned short ATT_MSK_GATE = 0x200;
	static const unsigned short ATT_MSK_GATEMODE = 0x1C00, gateModeShift = 10;// 3 bits

	static const unsigned short ATT_MSK_INITSTATE =  50;
	
	inline void init() {attributes = ATT_MSK_INITSTATE;}
	inline void randomize() {attributes = ( (random::u32() % 101) | (random::u32() & (ATT_MSK_GATEP | ATT_MSK_GATE/* | ATT_MSK_GATEMODE*/)) );}
		
	inline bool getGate() {return (attributes & ATT_MSK_GATE) != 0;}
	inline bool getGateP() {return (attributes & ATT_MSK_GATEP) != 0;}
	inline int getGatePVal() {return attributes & ATT_MSK_PROB;}
	inline int getGateMode() {return (attributes & ATT_MSK_GATEMODE) >> gateModeShift;}
	inline unsigned short getAttribute() {return attributes;}

	inline void setGate(bool gateState) {attributes &= ~ATT_MSK_GATE; if (gateState) attributes |= ATT_MSK_GATE;}
	inline void setGateP(bool gatePState) {attributes &= ~ATT_MSK_GATEP; if (gatePState) attributes |= ATT_MSK_GATEP;}
	inline void setGatePVal(int pVal) {attributes &= ~ATT_MSK_PROB; attributes |= (pVal & ATT_MSK_PROB);}
	inline void setGateMode(int gateMode) {attributes &= ~ATT_MSK_GATEMODE; attributes |= (gateMode << gateModeShift);}
	inline void setAttribute(unsigned short _attributes) {attributes = _attributes;}

	inline void toggleGate() {attributes ^= ATT_MSK_GATE;}
};// class StepAttributesGS 



//*****************************************************************************


class SeqAttributesGS {
	unsigned short attributes;
	
	public:

	static const unsigned short SEQ_MSK_LENGTH  =   0x000000FF;// number of steps in each sequence, min value is 1
	static const unsigned short SEQ_MSK_RUNMODE =   0x0000FF00, runModeShift = 8;
	
	inline void init(int length, int runMode) {attributes = ((length) | (((unsigned short)runMode) << runModeShift));}
	inline void randomize(int maxSteps, int numModes) {attributes = ( (2 + (random::u32() % (maxSteps - 1))) | (((unsigned short)(random::u32() % numModes) << runModeShift)) );}
	
	inline int getLength() {return (int)(attributes & SEQ_MSK_LENGTH);}
	inline int getRunMode() {return (int)((attributes & SEQ_MSK_RUNMODE) >> runModeShift);}
	inline unsigned short getSeqAttrib() {return attributes;}
	
	inline void setLength(int length) {attributes &= ~SEQ_MSK_LENGTH; attributes |= ((unsigned short)length);}
	inline void setRunMode(int runMode) {attributes &= ~SEQ_MSK_RUNMODE; attributes |= (((unsigned short)runMode) << runModeShift);}
	inline void setSeqAttrib(unsigned short _attributes) {attributes = _attributes;}
};// class SeqAttributesGS


//*****************************************************************************

/*
inline int ppsToIndexGS(int pulsesPerStep) {// map 1,4,6,12,24, to 0,1,2,3,4
	if (pulsesPerStep == 1) return 0;
	if (pulsesPerStep == 4) return 1; 
	if (pulsesPerStep == 6) return 2;
	if (pulsesPerStep == 12) return 3; 
	return 4; 
}
inline int indexToPpsGS(int index) {// inverse map of ppsToIndex()
	index = clamp(index, 0, 4); 
	if (index == 0) return 1;
	if (index == 1) return 4; 
	if (index == 2) return 6;
	if (index == 3) return 12; 
	return 24; 
}
inline bool ppsRequirementMet(int gateButtonIndex, int pulsesPerStep) {
	return !( (pulsesPerStep < 2) || 
			  (pulsesPerStep == 4 && gateButtonIndex > 2) || 
			  (pulsesPerStep == 6 && gateButtonIndex <= 2) 
			); 
}
*/


inline int ppsToIndexGS(int pulsesPerStep) {// map 1,4,6,8,12,16,18,24, to 0,1,2,3,4,5,6,7
	if (pulsesPerStep == 1) return 0;
	if (pulsesPerStep == 4) return 1; 
	if (pulsesPerStep == 6) return 2;
	if (pulsesPerStep == 8) return 3;
	if (pulsesPerStep == 12) return 4; 
	if (pulsesPerStep == 16) return 5; 
	if (pulsesPerStep == 18) return 6; 
	return 7; 
}
inline int indexToPpsGS(int index) {// inverse map of ppsToIndex()
	index = clamp(index, 0, 7); 
	if (index == 0) return 1;
	if (index == 1) return 4; 
	if (index == 2) return 6;
	if (index == 3) return 8;
	if (index == 4) return 12; 
	if (index == 5) return 16; 
	if (index == 6) return 18; 
	return 24; 
}
inline bool ppsRequirementMet(int gateButtonIndex, int pulsesPerStep) {
	if (pulsesPerStep < 4) {
		return false;
	}
	if (gateButtonIndex <= 2) {
		return ((pulsesPerStep % 4) == 0);
	}
	return ((pulsesPerStep % 6) == 0);
}


/*
inline int ppsToIndexGS(int pulsesPerStep) {// map 1,2,4,6,8,10,12...24, to 0,1,2,3,4,5,6...12
	if (pulsesPerStep == 1) return 0;
	return pulsesPerStep >> 1;
}
inline int indexToPpsGS(int index) {// inverse map of ppsToIndex()
	index = clamp(index, 0, 12);
	if (index == 0) return 1;
	return index <<	1;
}
inline bool ppsRequirementMet(int gateButtonIndex, int pulsesPerStep) {
	if (pulsesPerStep < 4) {
		return false;
	}
	// here pulsesPerStep is an even number in [4; 24]
	if (gateButtonIndex <= 2) {
		return ((pulsesPerStep % 4) == 0);
	}
	return ((pulsesPerStep % 6) == 0);
}
*/


inline bool calcGate(int gateCode, Trigger clockTrigger) {
	if (gateCode < 2) 
		return gateCode == 1;
	return clockTrigger.isHigh();
}		


//										1/4		DUO			D2			TR1		TR2		TR3 		TR23	   TRI
const uint32_t advGateHitMaskGS[8] = {0x00003F, 0x03F03F, 0x03F000, 0x00000F, 0x000F00, 0x0F0000, 0x0F0F00, 0x0F0F0F};

int getAdvGateGS(int ppqnCount, int pulsesPerStep, int gateMode) { 
	uint32_t shiftAmt = ppqnCount * (24 / pulsesPerStep);
	return (int)((advGateHitMaskGS[gateMode] >> shiftAmt) & (uint32_t)0x1);
}	


int calcGateCode(StepAttributesGS attribute, int ppqnCount, int pulsesPerStep) {
	// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high
	if (ppqnCount == 0 && attribute.getGateP() && !(random::uniform() < ((float)(attribute.getGatePVal())/100.0f)))// random::uniform is [0.0, 1.0), see include/util/common.hpp
		return -1;
	if (!attribute.getGate())
		return 0;
	if (pulsesPerStep == 1)
		return 2;// clock high
	return getAdvGateGS(ppqnCount, pulsesPerStep, attribute.getGateMode());
}		
