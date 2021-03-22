//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
// 
//Interop code for common portable sequence
//
//***********************************************************************************************

#pragma once

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

void interopCopySequenceNotes(int seqLen, std::vector<IoNote>* ioNotes);// function does not delete vector when finished
void interopCopySequence(int seqLen, IoStep* ioSteps);// function does not delete array when finished


// Paste from clipboard
// *****************


std::vector<IoNote>* interopPasteSequenceNotes(int maxSeqLen, int* seqLenPtr);// caller must delete return vector when non null
IoStep* interopPasteSequence(int maxSeqLen, int* seqLenPtr);// will return an array of size *seqLenPtr (when non null), which caller must delete[]
