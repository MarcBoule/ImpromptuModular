//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
// 
//Interop code for common portable sequence
//
//***********************************************************************************************

#ifndef INTEROP_HPP
#define INTEROP_HPP


struct IoStep {// common intermediate format for interop conversion
	// Impromptu 
	float pitch;
	float vel;
	float prob;
	bool gate;
	bool tied;
};


struct IoNote {
	float start;
	float length;
	float pitch;
	float vel;
	float prob;
};


IoNote* ioConvertToNotes(int seqLen, IoStep* ioSteps, int *newSeqLenPtr) {
	IoNote* ioNotes = new IoNote[seqLen];// seqLen is upper bound on required array size, will be lower when some gates are off and/or some steps are tied
	
	int si = 0;// index into ioSteps
	int ni = 0;// index into ioNotes
	
	for (; si < seqLen; si++) {
		if (ioSteps[si].gate) {
			ioNotes[ni].pitch = ioSteps[si].pitch;
			ioNotes[ni].vel = ioSteps[si].vel;
			ioNotes[ni].prob = ioSteps[si].prob;
			int si2 = si + 1;
			while (si2 < seqLen && ioSteps[si2].tied) {si2++;}
			ioNotes[ni].start = (float)si;
			ioNotes[ni].length = (float)(si2 - si - 0.5f);
			si = si2 - 1; 
			ni++;
		}
	}
	
	*newSeqLenPtr = ni;
	return ioNotes;
}


static void interopCopySequence(int seqLen, IoStep* ioSteps) {
	int newSeqLen;
	IoNote* ioNotes = ioConvertToNotes(seqLen, ioSteps, &newSeqLen);
	
	// here we have an array of ioNotes of size newSeqLen

	json_t* clipboard = json_object();		
	
	// vcvrack-sequence
	// **************
	json_t* vcvrackSequence = json_object();
	
	// length
	json_object_set_new(vcvrackSequence, "length", json_real(newSeqLen));
	
	// notes
	json_t* notesJ = json_array();
	for (int i = 0; i < newSeqLen; i++) {
		json_t* noteJ = json_object();
		json_object_set_new(noteJ, "type", json_string("note"));
		json_object_set_new(noteJ, "start", json_real(ioNotes[i].start));
		json_object_set_new(noteJ, "length", json_real(ioNotes[i].length));
		json_object_set_new(noteJ, "pitch", json_real(ioNotes[i].pitch));		
		json_object_set_new(noteJ, "velocity", json_real(ioNotes[i].vel));
		json_object_set_new(noteJ, "probability", json_real(ioNotes[i].prob));
		json_array_append_new(notesJ, noteJ);
	}
	json_object_set_new(vcvrackSequence, "notes", notesJ);
	delete[] ioNotes;
	
	// clipboard
	// **************
	// clipboard only has a vcvrack-sequence for now
	json_object_set_new(clipboard, "vcvrack-sequence", vcvrackSequence);
	//json_object_set_new(clipboard, "impromptu-sequence", impromptuSequence);
	
	char* interopJson = json_dumps(clipboard, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
	glfwSetClipboardString(APP->window->win, interopJson);
}


#endif