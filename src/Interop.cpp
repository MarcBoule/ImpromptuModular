//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
// 
//Interop code for common portable sequence
//
//***********************************************************************************************


#include "Interop.hpp"


// Copy to clipboard
// *****************


void ioConvertToNotes(int seqLen, IoStep* ioSteps, std::vector<IoNote> &ioNotes) {
	for (int si = 0; si < seqLen; si++) {
		if (ioSteps[si].gate) {
			int si2 = si + 1;
			while (si2 < seqLen && ioSteps[si2].tied) {si2++;}
			IoNote ioNote;
			ioNote.start = (float)si;
			ioNote.length = (float)(si2 - si) - 0.5f;
			ioNote.pitch = ioSteps[si].pitch;
			ioNote.vel = ioSteps[si].vel;
			ioNote.prob = ioSteps[si].prob;
			ioNotes.push_back(ioNote);
			si = si2 - 1;
		}
	}
}


void interopCopySequenceNotes(int seqLen, std::vector<IoNote> *ioNotes) {// function does not delete vector when finished
	// vcvrack-sequence
	json_t* vcvrackSequenceJ = json_object();
	
	// length
	json_object_set_new(vcvrackSequenceJ, "length", json_real((float)seqLen));
	
	// notes
	json_t* notesJ = json_array();
	for (unsigned int i = 0; i < ioNotes->size(); i++) {
		json_t* noteJ = json_object();
		json_object_set_new(noteJ, "type", json_string("note"));
		json_object_set_new(noteJ, "start", json_real((*ioNotes)[i].start));
		json_object_set_new(noteJ, "length", json_real((*ioNotes)[i].length));
		json_object_set_new(noteJ, "pitch", json_real((*ioNotes)[i].pitch));		
		if ((*ioNotes)[i].vel >= 0.0f) {
			json_object_set_new(noteJ, "velocity", json_real((*ioNotes)[i].vel));
		}
		if ((*ioNotes)[i].prob >= 0.0f) {
			json_object_set_new(noteJ, "playProbability", json_real((*ioNotes)[i].prob));
		}
		json_array_append_new(notesJ, noteJ);
	}
	json_object_set_new(vcvrackSequenceJ, "notes", notesJ);	
	
	// clipboard
	json_t* clipboardJ = json_object();		
	json_object_set_new(clipboardJ, "vcvrack-sequence", vcvrackSequenceJ);
	//json_object_set_new(clipboardJ, "impromptu-sequence", impromptuSequenceJ);// if ever advanced gates are to be included
	
	char* interopClip = json_dumps(clipboardJ, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
	json_decref(clipboardJ);
	glfwSetClipboardString(APP->window->win, interopClip);
	free(interopClip);
}


void interopCopySequence(int seqLen, IoStep* ioSteps) {// function does not delete array when finished
	std::vector<IoNote> ioNotes;
	ioConvertToNotes(seqLen, ioSteps, ioNotes);	
	interopCopySequenceNotes(seqLen, &ioNotes);
}


// Paste from clipboard
// *****************


IoStep* ioConvertToSteps(const std::vector<IoNote> &ioNotes, int maxSeqLen) {
	IoStep* ioSteps = new IoStep[maxSeqLen];
	
	for (int i = 0; i < maxSeqLen; i++) {
		ioSteps[i].init(false, false, 0.0f, -1.0f, -1.0f);
	}
	
	// Scan notes and write into steps
	for (unsigned int ni = 0; ni < ioNotes.size(); ni++) {
		int si = std::max((int)0, (int)ioNotes[ni].start);
		if (si >= maxSeqLen) continue;
		int si2 = si + std::max((int)1, (int)std::ceil(ioNotes[ni].length));
		bool headStep = true;
		for (; si < si2 && si < maxSeqLen; si++) {
			ioSteps[si].gate = true;
			ioSteps[si].tied = !headStep;
			ioSteps[si].pitch = ioNotes[ni].pitch;
			ioSteps[si].vel = ioNotes[ni].vel;
			ioSteps[si].prob = headStep ? ioNotes[ni].prob : -1.0f;
			headStep = false;
		}
	}
	
	// Scan steps and extend pitches into unused steps
	float lastPitch = 0.0f;
	for (int si = 0; si < maxSeqLen; si++) {
		if (ioSteps[si].gate) {
			lastPitch = ioSteps[si].pitch;
		}
		else {
			ioSteps[si].pitch = lastPitch;
		}
	}
	// second pass to assign empty first steps to real lastPitch (not 0.0f)
	for (int si = 0; si < maxSeqLen; si++) { 
		if (ioSteps[si].gate) {
			break;
		}
		else {
			ioSteps[si].pitch = lastPitch;
		}
	}	
	return ioSteps;
}


std::vector<IoNote>* interopPasteSequenceNotes(int maxSeqLen, int *seqLenPtr) {// caller must delete return vector when non null
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
	*seqLenPtr = (int)std::ceil(json_number_value(jsonLength));
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
	std::vector<IoNote> *ioNotes = new std::vector<IoNote>;
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
			WARN("IOP missing or unrecognized note type, skipping it");
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
		
		// vel (optional)
		json_t* velJ = json_object_get(noteJ, "velocity");
		newNote.vel = velJ ? json_number_value(velJ) : -1.0f;

		// prob (optional)
		json_t* probJ = json_object_get(noteJ, "playProbability");
		newNote.prob = probJ ? json_number_value(probJ) : -1.0f;

		ioNotes->push_back(newNote);
	}
	if (ioNotes->size() < 1) {
		WARN("IOP error in vcvrack-sequence, no notes in notes array ");
		delete ioNotes;
		return nullptr;		
	}
	return ioNotes;
}

	
IoStep* interopPasteSequence(int maxSeqLen, int *seqLenPtr) {// will return an array of size *seqLenPtr (when non null), which caller must delete[]
	std::vector<IoNote> *ioNotes = interopPasteSequenceNotes(maxSeqLen, seqLenPtr);
	if (ioNotes) {
		IoStep* ioSteps = ioConvertToSteps(*ioNotes, maxSeqLen);
		delete ioNotes;
		return ioSteps;
	}
	return nullptr;
}
