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
	bool gate;
	bool tied;
	float pitch;
	float vel;// 0.0 to 10.0, -1.0 will indicate that the note is not using velocity
	float prob;// 0.0 to 1.0, -1.0 will indicate that the note is not using probability
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


IoNote* ioConvertToNotes(int seqLen, IoStep* ioSteps, int *newSeqLenPtr) {
	IoNote* ioNotes = new IoNote[seqLen];// seqLen is upper bound on required array size, will be lower when some gates are off and/or some steps are tied
	
	int si = 0;// index into ioSteps
	int ni = 0;// index into ioNotes
	
	for (; si < seqLen; si++) {
		if (ioSteps[si].gate) {
			int si2 = si + 1;
			while (si2 < seqLen && ioSteps[si2].tied) {si2++;}
			ioNotes[ni].start = (float)si;
			ioNotes[ni].length = (float)(si2 - si - 0.5f);
			ioNotes[ni].pitch = ioSteps[si].pitch;
			ioNotes[ni].vel = ioSteps[si].vel;
			ioNotes[ni].prob = ioSteps[si].prob;
			si = si2 - 1;
			ni++;
		}
	}
	
	*newSeqLenPtr = ni;
	return ioNotes;
}


void interopCopySequence(int seqLen, IoStep* ioSteps) {
	int newSeqLen;
	IoNote* ioNotes = ioConvertToNotes(seqLen, ioSteps, &newSeqLen);	
	// here we have an array of ioNotes of size newSeqLen

	
	// vcvrack-sequence
	// **************
	json_t* vcvrackSequenceJ = json_object();
	
	// length
	json_object_set_new(vcvrackSequenceJ, "length", json_real(newSeqLen));
	
	// notes
	json_t* notesJ = json_array();
	for (int i = 0; i < newSeqLen; i++) {
		json_t* noteJ = json_object();
		json_object_set_new(noteJ, "type", json_string("note"));
		json_object_set_new(noteJ, "start", json_real(ioNotes[i].start));
		json_object_set_new(noteJ, "length", json_real(ioNotes[i].length));
		json_object_set_new(noteJ, "pitch", json_real(ioNotes[i].pitch));		
		if (ioNotes[i].vel >= 0.0f) {
			json_object_set_new(noteJ, "velocity", json_real(ioNotes[i].vel));
		}
		if (ioNotes[i].prob >= 0.0f) {
			json_object_set_new(noteJ, "playProbability", json_real(ioNotes[i].prob));
		}
		json_array_append_new(notesJ, noteJ);
	}
	json_object_set_new(vcvrackSequenceJ, "notes", notesJ);
	delete[] ioNotes;
	
	
	// clipboard
	// **************
	json_t* clipboardJ = json_object();		
	json_object_set_new(clipboardJ, "vcvrack-sequence", vcvrackSequenceJ);
	//json_object_set_new(clipboardJ, "impromptu-sequence", impromptuSequence);
	
	char* interopClip = json_dumps(clipboardJ, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
	json_decref(clipboardJ);
	glfwSetClipboardString(APP->window->win, interopClip);
	free(interopClip);
}


// Paste from clipboard
// *****************



IoStep* ioConvertToSteps(const std::vector<IoNote> &ioNotes, int maxSeqLen) {
	IoStep* ioSteps = new IoStep[maxSeqLen];
	
	return ioSteps;
}


IoStep* interopPasteSequence(int maxSeqLen, int *seqLenPtr) {// will return an array of size *seqLenPtr
	const char* interopClip = glfwGetClipboardString(APP->window->win);

    if (!interopClip) {
        WARN("IOP error getting clipboard string");
		return nullptr;
    }

    json_error_t error;
    json_t* clipboardJ = json_loads(interopClip, 0, &error);
	if (!clipboardJ) {
        WARN("IOP error json parsing clipboard");
		return nullptr;
	}
	DEFER({json_decref(clipboardJ);});

	// vcvrack-sequence
    json_t* vcvrackSequenceJ = json_object_get(clipboardJ, "vcvrack-sequence");
    if (!vcvrackSequenceJ) {
        WARN("IOP error no vcvrack-sequence present in clipboard");
		return nullptr;
    }
	
	// length	
	json_t* jsonLength = json_object_get(vcvrackSequenceJ, "length");
	if (!jsonLength) {
        WARN("IOP error vcvrack-seqence length missing");
		return nullptr;
	}
	*seqLenPtr = (int)json_number_value(jsonLength);
	if (*seqLenPtr < 1) {
        WARN("IOP error vcvrack-sequence must have positive length");
		return nullptr;
	}
	if (*seqLenPtr > maxSeqLen) {
        *seqLenPtr = maxSeqLen;
		WARN("IOP vcvrack-sequence truncated during paste");
	}

	// notes
	json_t* notesJ = json_object_get(vcvrackSequenceJ, "notes");
	if ( !notesJ || !json_is_array(notesJ) ) {
        WARN("IOP error vcvrack-sequence notes array malformed or missing");
		return nullptr;
	}
	std::vector<IoNote> ioNotes;
	for (size_t i = 0; i < json_array_size(notesJ); i++) {
		// note
		json_t* noteJ = json_array_get(notesJ, i);
		if (!noteJ) {
			WARN("IOP error missing note in notes array");
			return nullptr;		
		}
		
		// type
		json_t* typeJ = json_object_get(noteJ, "type");
		if (!typeJ || (std::string("note").compare(json_string_value(typeJ)) != 0)) {
			WARN("IOP unrecognized note type in notes array, skipping it");
			continue;
		}
		IoNote newNote;
		
		// start
		json_t* startJ = json_object_get(noteJ, "start");
		if (!startJ) {
			WARN("IOP bad start time for note, note skipped");
			continue;
		}
		newNote.start = json_number_value(startJ);
		
		// length
		json_t* lengthJ = json_object_get(noteJ, "length");
		if (!lengthJ) {
			WARN("IOP bad length for note, note skipped");
			continue;
		}
		newNote.length = json_number_value(lengthJ);
		
		// pitch
		json_t* pitchJ = json_object_get(noteJ, "pitch");
		if (!pitchJ) {
			WARN("IOP bad pitch for note, note skipped");
			continue;
		}
		newNote.pitch = json_number_value(pitchJ);
		
		// vel
		json_t* velJ = json_object_get(noteJ, "velocity");
		newNote.vel = velJ ? json_number_value(velJ) : -1.0f;

		// prob
		json_t* probJ = json_object_get(noteJ, "playProbability");
		newNote.prob = probJ ? json_number_value(probJ) : -1.0f;

		ioNotes.push_back(newNote);
	}
	if (ioNotes.size() < 1) {
		WARN("IOP error vcvrack-sequence no notes in notes array ");
		return nullptr;		
	}
	
	IoStep* ioSteps = ioConvertToSteps(ioNotes, maxSeqLen);

	return ioSteps;
}


#endif