//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
// 
//Interop code for common portable sequence
//
//***********************************************************************************************

#ifndef INTEROP_HPP
#define INTEROP_HPP

#include "ImpromptuModular.hpp"


static const std::string portableSequenceID = "Portable sequence";
static const std::string portableSequenceCopyID = "Copy sequence";
static const std::string portableSequencePasteID = "Paste sequence";


struct IoStep {// common intermediate format for interop conversion
	bool gate;
	bool tied;// when tied is true, gate is always true and prob is always -1.0
	float pitch;
	float vel;// 0.0 to 10.0, -1.0 will indicate that the note is not using velocity
	float prob;// 0.0 to 1.0, -1.0 will indicate that the note is not using probability (always so when tied)
	
	void init(bool _gate, bool _tied, float _pitch, float _vel, float _prob) {
		gate = _gate; tied = _tied; pitch = _pitch, vel = _vel, prob = _prob;
	}
};


struct IoNote {
	float start;// required
	float length;// required
	float pitch;// required
	float vel;// optional, 0.0 to 10.0, -1.0 will indicate that the note is not using velocity
	float prob;// optional, 0.0 to 1.0, -1.0 will indicate that the note is not using probability
};


// Copy to clipboard
// *****************


void ioConvertToNotes(int seqLen, IoStep* ioSteps, std::vector<IoNote> &ioNotes);
void interopCopySequence(int seqLen, IoStep* ioSteps);


// Paste from clipboard
// *****************


IoStep* ioConvertToSteps(const std::vector<IoNote> &ioNotes, int maxSeqLen);
IoStep* interopPasteSequence(int maxSeqLen, int *seqLenPtr); // will return an array of size *seqLenPtr, which called must delete[]


#endif