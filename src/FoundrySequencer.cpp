//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************


#include "FoundrySequencer.hpp"


void Sequencer::construct(bool* _holdTiedNotesPtr, int* _velocityModePtr, int* _stopAtEndOfSongPtr) {// don't want regaular constructor mechanism
	velocityModePtr = _velocityModePtr;
	sek[0].construct(0, nullptr, _holdTiedNotesPtr, _stopAtEndOfSongPtr);
	for (int trkn = 1; trkn < NUM_TRACKS; trkn++)
		sek[trkn].construct(trkn, &sek[0], _holdTiedNotesPtr, _stopAtEndOfSongPtr);
}


void Sequencer::onReset(bool editingSequence) {
	stepIndexEdit = 0;
	phraseIndexEdit = 0;
	trackIndexEdit = 0;
	for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
		sek[trkn].onReset(editingSequence);
	}
	resetNonJson(editingSequence, false);// no need to propagate initRun calls in kernels, since sek[trkn].onReset() have initRun() in them
}
void Sequencer::resetNonJson(bool editingSequence, bool propagateInitRun) {
	editingType = 0ul;
	for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
		editingGate[trkn] = 0ul;
	}
	seqCPbuf.reset();
	songCPbuf.reset();
	initRun(editingSequence, propagateInitRun);
}
void Sequencer::initRun(bool editingSequence, bool propagateInitRun) {
	initDelayedSeqNumberRequest();
	if (propagateInitRun) {
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].initRun(editingSequence);
	}
}
void Sequencer::initDelayedSeqNumberRequest() {
	for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
		delayedSeqNumberRequest[trkn] = -1;
	}
}


void Sequencer::dataToJson(json_t *rootJ) {
	// stepIndexEdit
	json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));

	// phraseIndexEdit
	json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

	// trackIndexEdit
	json_object_set_new(rootJ, "trackIndexEdit", json_integer(trackIndexEdit));

	for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
		sek[trkn].dataToJson(rootJ);
}


void Sequencer::dataFromJson(json_t *rootJ, bool editingSequence) {
	// stepIndexEdit
	json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
	if (stepIndexEditJ)
		stepIndexEdit = json_integer_value(stepIndexEditJ);
	
	// phraseIndexEdit
	json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
	if (phraseIndexEditJ)
		phraseIndexEdit = json_integer_value(phraseIndexEditJ);
	
	// trackIndexEdit
	json_t *trackIndexEditJ = json_object_get(rootJ, "trackIndexEdit");
	if (trackIndexEditJ)
		trackIndexEdit = json_integer_value(trackIndexEditJ);
	
	for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
		sek[trkn].dataFromJson(rootJ, editingSequence);
	
	resetNonJson(editingSequence, false);// no need to propagate initRun calls in kernels, since sek[trkn].dataFromJson() have initRun() in them
}


void Sequencer::setVelocityVal(int trkn, int intVel, int multiStepsCount, bool multiTracks) {
	sek[trkn].setVelocityVal(stepIndexEdit, intVel, multiStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trkn) continue;
			sek[i].setVelocityVal(stepIndexEdit, intVel, multiStepsCount);
		}
	}
}
void Sequencer::setLength(int length, bool multiTracks) {
	sek[trackIndexEdit].setLength(length);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setLength(length);
		}
	}
}
void Sequencer::setPhraseReps(int reps, bool multiTracks) {
	sek[trackIndexEdit].setPhraseReps(phraseIndexEdit, reps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseReps(phraseIndexEdit, reps);
		}
	}		
}
void Sequencer::setPhraseSeqNum(int seqn, bool multiTracks) {
	sek[trackIndexEdit].setPhraseSeqNum(phraseIndexEdit, seqn);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseSeqNum(phraseIndexEdit, seqn);
		}
	}	
}
void Sequencer::setBegin(bool multiTracks) {
	sek[trackIndexEdit].setBegin(phraseIndexEdit);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setBegin(phraseIndexEdit);
		}
	}
}
void Sequencer::setEnd(bool multiTracks) {
	sek[trackIndexEdit].setEnd(phraseIndexEdit);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setEnd(phraseIndexEdit);
		}
	}
}
bool Sequencer::setGateType(int keyn, int multiSteps, float sampleRate, bool autostepClick, bool multiTracks) {// Third param is for right-click autostep. Returns success
	int newMode = keyIndexToGateTypeEx(keyn);
	if (newMode == -1) 
		return false;
	sek[trackIndexEdit].setGateType(stepIndexEdit, newMode, multiSteps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setGateType(stepIndexEdit, newMode, multiSteps);
		}
	}
	if (autostepClick){ // if right-click then move to next step
		moveStepIndexEdit(1, false);
		editingGateKeyLight = keyn;
		editingType = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
		if ( ((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL) && multiSteps < 2 )
			setGateType(keyn, 1, sampleRate, false, multiTracks);
	}
	return true;
}


void Sequencer::initSlideVal(int multiStepsCount, bool multiTracks) {
	sek[trackIndexEdit].setSlideVal(stepIndexEdit, StepAttributes::INIT_SLIDE, multiStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setSlideVal(stepIndexEdit, StepAttributes::INIT_SLIDE, multiStepsCount);
		}
	}		
}
void Sequencer::initGatePVal(int multiStepsCount, bool multiTracks) {
	sek[trackIndexEdit].setGatePVal(stepIndexEdit, StepAttributes::INIT_PROB, multiStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setGatePVal(stepIndexEdit, StepAttributes::INIT_PROB, multiStepsCount);
		}
	}		
}
void Sequencer::initVelocityVal(int multiStepsCount, bool multiTracks) {
	sek[trackIndexEdit].setVelocityVal(stepIndexEdit, StepAttributes::INIT_VELOCITY, multiStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setVelocityVal(stepIndexEdit, StepAttributes::INIT_VELOCITY, multiStepsCount);
		}
	}		
}
void Sequencer::initPulsesPerStep(bool multiTracks) {
	sek[trackIndexEdit].initPulsesPerStep();
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].initPulsesPerStep();
		}
	}		
}
void Sequencer::initDelay(bool multiTracks) {
	sek[trackIndexEdit].initDelay();
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].initDelay();
		}
	}		
}
void Sequencer::initRunModeSong(bool multiTracks) {
	sek[trackIndexEdit].setRunModeSong(SequencerKernel::MODE_FWD);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setRunModeSong(SequencerKernel::MODE_FWD);
		}
	}		
}
void Sequencer::initRunModeSeq(bool multiTracks) {
	sek[trackIndexEdit].setRunModeSeq(SequencerKernel::MODE_FWD);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setRunModeSeq(SequencerKernel::MODE_FWD);
		}
	}		
}
void Sequencer::initLength(bool multiTracks) {
	sek[trackIndexEdit].setLength(SequencerKernel::MAX_STEPS);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setLength(SequencerKernel::MAX_STEPS);
		}
	}		
}
void Sequencer::initPhraseReps(bool multiTracks) {
	sek[trackIndexEdit].setPhraseReps(phraseIndexEdit, 1);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseReps(phraseIndexEdit, 1);
		}
	}		
}
void Sequencer::initPhraseSeqNum(bool multiTracks) {
	sek[trackIndexEdit].setPhraseSeqNum(phraseIndexEdit, 0);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseSeqNum(phraseIndexEdit, 0);
		}
	}		
}

void Sequencer::copySequence(int countCP) {
	int startCP = stepIndexEdit;
	sek[trackIndexEdit].copySequence(&seqCPbuf, startCP, countCP);
}
void Sequencer::pasteSequence(bool multiTracks) {
	int startCP = stepIndexEdit;
	sek[trackIndexEdit].pasteSequence(&seqCPbuf, startCP);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].pasteSequence(&seqCPbuf, startCP);
		}
	}
}
void Sequencer::copySong(int startCP, int countCP) {
	sek[trackIndexEdit].copySong(&songCPbuf, startCP, countCP);
}
void Sequencer::pasteSong(bool multiTracks) {
	sek[trackIndexEdit].pasteSong(&songCPbuf, phraseIndexEdit);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].pasteSong(&songCPbuf, phraseIndexEdit);
		}
	}
}

void Sequencer::writeCV(int trkn, float cvVal, int multiStepsCount, float sampleRate, bool multiTracks) {
	sek[trkn].writeCV(stepIndexEdit, cvVal, multiStepsCount);
	editingGateCV[trkn] = cvVal;
	editingGateCV2[trkn] = sek[trkn].getAttribute(stepIndexEdit).getVelocityVal();
	editingGate[trkn] = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trkn) continue;
			sek[i].writeCV(stepIndexEdit, cvVal, multiStepsCount);
		}
	}
}
void Sequencer::autostep(bool autoseq, bool autostepLen, bool multiTracks) {
	moveStepIndexEdit(1, autostepLen);
	if (stepIndexEdit == 0 && autoseq) {
		sek[trackIndexEdit].modSeqIndexEdit(1);
		if (multiTracks) {
			for (int i = 0; i < NUM_TRACKS; i++) {
				if (i == trackIndexEdit) continue;
				sek[i].modSeqIndexEdit(1);
			}
		}
	}		
}	

bool Sequencer::applyNewOctave(int octn, int multiSteps, float sampleRate, bool multiTracks) { // returns true if tied
	StepAttributes stepAttrib = sek[trackIndexEdit].getAttribute(stepIndexEdit);
	if (stepAttrib.getTied())
		return true;
	editingGateCV[trackIndexEdit] = sek[trackIndexEdit].applyNewOctave(stepIndexEdit, octn, multiSteps);
	editingGateCV2[trackIndexEdit] = stepAttrib.getVelocityVal();
	editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
	editingGateKeyLight = -1;
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].applyNewOctave(stepIndexEdit, octn, multiSteps);
		}
	}
	return false;
}
bool Sequencer::applyNewKey(int keyn, int multiSteps, float sampleRate, bool autostepClick, bool multiTracks) { // returns true if tied
	bool ret = false;
	StepAttributes stepAttrib = sek[trackIndexEdit].getAttribute(stepIndexEdit);
	if (stepAttrib.getTied()) {
		if (autostepClick)
			moveStepIndexEdit(1, false);
		else
			ret = true;
	}
	else {
		editingGateCV[trackIndexEdit] = sek[trackIndexEdit].applyNewKey(stepIndexEdit, keyn, multiSteps);
		editingGateCV2[trackIndexEdit] = stepAttrib.getVelocityVal();
		editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
		editingGateKeyLight = -1;
		if (multiTracks) {
			for (int i = 0; i < NUM_TRACKS; i++) {
				if (i == trackIndexEdit) continue;
				sek[i].applyNewKey(stepIndexEdit, keyn, multiSteps);
			}
		}
		if (autostepClick) {// if right-click then move to next step
			moveStepIndexEdit(1, false);
			if ( ((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL) && multiSteps < 2 ) // if ctrl-right-click and SEL is off
				writeCV(trackIndexEdit, editingGateCV[trackIndexEdit], 1, sampleRate, multiTracks);// copy CV only to next step
			editingGateKeyLight = keyn;
		}
	}
	return ret;
}

void Sequencer::moveStepIndexEditWithEditingGate(int delta, bool writeTrig, float sampleRate) {
	moveStepIndexEdit(delta, false);
	for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
		StepAttributes stepAttrib = sek[trkn].getAttribute(stepIndexEdit);
		if (!stepAttrib.getTied()) {// play if non-tied step
			if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
				editingGate[trkn] = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				editingGateCV[trkn] = sek[trkn].getCV(stepIndexEdit);
				editingGateCV2[trkn] = stepAttrib.getVelocityVal();
				editingGateKeyLight = -1;
			}
		}
	}
}



void Sequencer::modSlideVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks) {
	int sVal = sek[trackIndexEdit].modSlideVal(stepIndexEdit, deltaVelKnob, mutliStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setSlideVal(stepIndexEdit, sVal, mutliStepsCount);
		}
	}		
}
void Sequencer::modGatePVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks) {
	int gpVal = sek[trackIndexEdit].modGatePVal(stepIndexEdit, deltaVelKnob, mutliStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setGatePVal(stepIndexEdit, gpVal, mutliStepsCount);
		}
	}		
}
void Sequencer::modVelocityVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks) {
	int upperLimit = ((*velocityModePtr) == 0 ? 200 : 127);
	int vVal = sek[trackIndexEdit].modVelocityVal(stepIndexEdit, deltaVelKnob, upperLimit, mutliStepsCount);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setVelocityVal(stepIndexEdit, vVal, mutliStepsCount);
		}
	}		
}
void Sequencer::modRunModeSong(int deltaPhrKnob, bool multiTracks) {
	int newRunMode = sek[trackIndexEdit].modRunModeSong(deltaPhrKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setRunModeSong(newRunMode);
		}
	}		
}
void Sequencer::modPulsesPerStep(int deltaSeqKnob, bool multiTracks) {
	int newPPS = sek[trackIndexEdit].modPulsesPerStep(deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPulsesPerStep(newPPS);
		}
	}		
}
void Sequencer::modDelay(int deltaSeqKnob, bool multiTracks) {
	int newDelay = sek[trackIndexEdit].modDelay(deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setDelay(newDelay);
		}
	}		
}
void Sequencer::modRunModeSeq(int deltaSeqKnob, bool multiTracks) {
	int newRunMode = sek[trackIndexEdit].modRunModeSeq(deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setRunModeSeq(newRunMode);
		}
	}		
}
void Sequencer::modLength(int deltaSeqKnob, bool multiTracks) {
	int newLength = sek[trackIndexEdit].modLength(deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setLength(newLength);
		}
	}		
}
void Sequencer::modPhraseReps(int deltaSeqKnob, bool multiTracks) {
	int newReps = sek[trackIndexEdit].modPhraseReps(phraseIndexEdit, deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseReps(phraseIndexEdit, newReps);
		}
	}		
}
void Sequencer::modPhraseSeqNum(int deltaSeqKnob, bool multiTracks) {
	int newSeqn = sek[trackIndexEdit].modPhraseSeqNum(phraseIndexEdit, deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].setPhraseSeqNum(phraseIndexEdit, newSeqn);
		}
	}		
}
void Sequencer::transposeSeq(int deltaSeqKnob, bool multiTracks) {
	sek[trackIndexEdit].transposeSeq(deltaSeqKnob);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].transposeSeq(deltaSeqKnob);
		}
	}		
}
void Sequencer::unTransposeSeq(bool multiTracks) {
	sek[trackIndexEdit].unTransposeSeq();
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].unTransposeSeq();
		}
	}		
}
void Sequencer::rotateSeq(int deltaSeqKnob, bool multiTracks) {
	sek[trackIndexEdit].rotateSeq(deltaSeqKnob);
	if (stepIndexEdit < getLength())
		moveStepIndexEdit(deltaSeqKnob, true);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].rotateSeq(deltaSeqKnob);
		}
	}		
}
void Sequencer::unRotateSeq(bool multiTracks) {
	sek[trackIndexEdit].unRotateSeq();
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;
			sek[i].unRotateSeq();
		}
	}		
}
void Sequencer::toggleGate(int multiSteps, bool multiTracks) {
	bool newGate = sek[trackIndexEdit].toggleGate(stepIndexEdit, multiSteps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;			
			sek[i].setGate(stepIndexEdit, newGate, multiSteps);
		}
	}		
}
bool Sequencer::toggleGateP(int multiSteps, bool multiTracks) { // returns true if tied
	if (sek[trackIndexEdit].getAttribute(stepIndexEdit).getTied())
		return true;
	bool newGateP = sek[trackIndexEdit].toggleGateP(stepIndexEdit, multiSteps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;			
			sek[i].setGateP(stepIndexEdit, newGateP, multiSteps);
		}
	}				
	return false;
}
bool Sequencer::toggleSlide(int multiSteps, bool multiTracks) { // returns true if tied
	if (sek[trackIndexEdit].getAttribute(stepIndexEdit).getTied())
		return true;
	bool newSlide = sek[trackIndexEdit].toggleSlide(stepIndexEdit, multiSteps);
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;			
			sek[i].setSlide(stepIndexEdit, newSlide, multiSteps);
		}
	}				
	return false;
}
void Sequencer::toggleTied(int multiSteps, bool multiTracks) {
	bool newTied = sek[trackIndexEdit].toggleTied(stepIndexEdit, multiSteps);// will clear other attribs if new state is on
	if (multiTracks) {
		for (int i = 0; i < NUM_TRACKS; i++) {
			if (i == trackIndexEdit) continue;			
			sek[i].setTied(stepIndexEdit, newTied, multiSteps);
		}
	}						
}


bool Sequencer::clockStep(int trkn, bool editingSequence) {// returns true to signal that run should be turned off
	int phraseChangeOrStop = sek[trkn].clockStep(editingSequence, delayedSeqNumberRequest[trkn]);
	
	if (phraseChangeOrStop == 2)// kernel request that run should be turned off 
		return true;
	
	if (editingSequence) {
		if (phraseChangeOrStop == 1) {
			delayedSeqNumberRequest[trkn] = -1;// consumed
		}
	}
	else {
		if (trkn == 0 && phraseChangeOrStop == 1) {
			for (int tkbcd = 1; tkbcd < NUM_TRACKS; tkbcd++) {// check for song run mode slaving
				if (sek[tkbcd].getRunModeSong() == SequencerKernel::MODE_TKA) {
					sek[tkbcd].setPhraseIndexRun(sek[0].getPhraseIndexRun());
					// The code below is to make it such that stepIndexRun should re-init upon phraseChange
					//   example for phrase jump does not reset stepIndexRun in B
						//A (FWD): 1 = 1x10, 2 = 1x10
						//B (TKA): 1 = 1x20, 2 = 1x20
					// next line will not work, it will result in a double move of stepIndexRun since clock will move it also
					//sek[tkbcd].moveStepIndexRun(true); 
					// this next mechanism works, the chain of events will make moveStepIndexRun(true) happen automatically and no double move will occur
					sek[tkbcd].setMoveStepIndexRunIgnore();// 
				}
			}
		}
	}
	return false;
}
