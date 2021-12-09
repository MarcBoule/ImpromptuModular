//***********************************************************************************************
//Multi-phrase 32 step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the SA-100 Stepper Acid sequencer by Transistor Sounds Labs
//
//Acknowledgements: please see README.md
//***********************************************************************************************


#include "PhraseSeqUtil.hpp"
#include "comp/PianoKey.hpp"


struct PhraseSeq32 : Module {
	enum ParamIds {
		LEFT_PARAM,
		RIGHT_PARAM,
		RIGHT8_PARAM,// not used
		EDIT_PARAM,
		SEQUENCE_PARAM,
		RUN_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		ENUMS(OCTAVE_PARAM, 7),
		GATE1_PARAM,
		GATE2_PARAM,
		SLIDE_BTN_PARAM,
		SLIDE_KNOB_PARAM,
		ATTACH_PARAM,
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),// no longer used
		RUNMODE_PARAM,
		TRAN_ROT_PARAM,
		GATE1_KNOB_PARAM,
		GATE1_PROB_PARAM,
		TIE_PARAM,// Legato
		CPMODE_PARAM,
		ENUMS(STEP_PHRASE_PARAMS, 32),
		CONFIG_PARAM,
		KEYNOTE_PARAM,
		KEYGATE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		CV_INPUT,
		RESET_INPUT,
		CLOCK_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CVA_OUTPUT,
		GATE1A_OUTPUT,
		GATE2A_OUTPUT,
		CVB_OUTPUT,
		GATE1B_OUTPUT,
		GATE2B_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 32 * 3),// room for GreenRedWhite
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12 * 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE1_LIGHT, 2),// room for GreenRed
		ENUMS(GATE2_LIGHT, 2),// room for GreenRed
		SLIDE_LIGHT,
		ATTACH_LIGHT,
		ENUMS(GATE1_PROB_LIGHT, 2),// room for GreenRed
		TIE_LIGHT,
		KEYNOTE_LIGHT,
		ENUMS(KEYGATE_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	
	
	// Expander
	float rightMessages[2][5] = {};// messages from expander


	// Constants
	enum DisplayStateIds {DISP_NORMAL, DISP_MODE, DISP_LENGTH, DISP_TRANSPOSE, DISP_ROTATE};


	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	bool autoseq;
	bool autostepLen;
	bool holdTiedNotes;
	int seqCVmethod;// 0 is 0-10V, 1 is C4-G6, 2 is TrigIncr
	int pulsesPerStep;// 1 means normal gate mode, alt choices are 4, 6, 12, 24 PPS (Pulses per step)
	bool running;
	int runModeSong;
	int stepIndexEdit;
	int seqIndexEdit;
	int phraseIndexEdit;
	int phrases;//1 to 32
	SeqAttributes sequences[32];
	int phrase[32];// This is the song (series of phases; a phrase is a patten number)
	float cv[32][32];// [-3.0 : 3.917]. First index is patten number, 2nd index is step
	StepAttributes attributes[32][32];// First index is patten number, 2nd index is step (see enum AttributeBitMasks for details)
	bool resetOnRun;
	bool attached;
	bool stopAtEndOfSong;

	// No need to save, with reset
	int displayState;
	float cvCPbuffer[32];// copy paste buffer for CVs
	StepAttributes attribCPbuffer[32];
	int phraseCPbuffer[32];
	SeqAttributes seqAttribCPbuffer;
	bool seqCopied;
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	int startCP;
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate
	unsigned long editingType;// similar to editingGate, but just for showing remanent gate type (nothing played); uses editingGateKeyLight
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	unsigned long clockPeriod;// counts number of step() calls upward from last clock (reset after clock processed)
	long tiedWarning;// 0 when no warning, positive downward step counter timer when warning
	long attachedWarning;// 0 when no warning, positive downward step counter timer when warning
	long revertDisplay;
	long editingGateLength;// 0 when no info, positive when gate1, negative when gate2
	long lastGateEdit;
	long editingPpqn;// 0 when no info, positive downward step counter timer when editing ppqn
	int stepConfig;
	long clockIgnoreOnReset;
	int phraseIndexRun;	
	unsigned long phraseIndexRunHistory;
	int stepIndexRun[2];
	unsigned long stepIndexRunHistory;
	int ppqnCount;
	int gate1Code[2];
	int gate2Code[2];
	bool lastProbGate1Enable[2];	
	unsigned long slideStepsRemain[2];// 0 when no slide under way, downward step counter when sliding
	
	// No need to save, no reset
	int stepConfigSync = 0;// 0 means no sync requested, 1 means synchronous read of lengths requested
	RefreshCounter refresh;
	float slideCVdelta[2];// no need to initialize, this is a companion to slideStepsRemain	
	float editingGateCV;// no need to initialize, this is a companion to editingGate (output this only when editingGate > 0)
	int editingGateKeyLight;// no need to initialize, this is a companion to editingGate (use this only when editingGate > 0)
	int editingChannel;// 0 means channel A, 1 means channel B. no need to initialize, this is a companion to editingGate
	float resetLight = 0.0f;
	int sequenceKnob = 0;
	Trigger resetTrigger;
	Trigger leftTrigger;
	Trigger rightTrigger;
	Trigger runningTrigger;
	Trigger clockTrigger;
	Trigger octTriggers[7];
	Trigger octmTrigger;
	Trigger gate1Trigger;
	Trigger gate1ProbTrigger;
	Trigger gate2Trigger;
	Trigger slideTrigger;
	dsp::BooleanTrigger keyTrigger;
	Trigger writeTrigger;
	Trigger attachedTrigger;
	Trigger copyTrigger;
	Trigger pasteTrigger;
	Trigger modeTrigger;
	Trigger rotateTrigger;
	Trigger transposeTrigger;
	Trigger tiedTrigger;
	Trigger stepTriggers[32];
	Trigger keyNoteTrigger;
	Trigger keyGateTrigger;
	Trigger seqCVTrigger;
	HoldDetect modeHoldDetect;
	SeqAttributes seqAttribBuffer[32];// buffer from Json for thread safety
	PianoKeyInfo pkInfo;


	bool isEditingSequence(void) {return params[EDIT_PARAM].getValue() > 0.5f;}
	int getStepConfig() {// 1 = 2x16 = 1.0f,  2 = 1x32 = 0.0f
		return (params[CONFIG_PARAM].getValue() > 0.5f) ? 1 : 2;
	}

	
	void fillStepIndexRunVector(int runMode, int len) {
		if (runMode != MODE_RN2) 
			stepIndexRun[1] = stepIndexRun[0];
		else
			stepIndexRun[1] = random::u32() % len;
	}
	
	void moveStepIndexEdit(int delta, bool _autostepLen) {// 2nd param is for rotate that uses this method also
		if (stepConfig == 2 || !_autostepLen) // 32
			stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, _autostepLen ? sequences[seqIndexEdit].getLength() : 32);
		else {// here 1x16 and _autostepLen limit wanted
			if (stepIndexEdit < 16) {
				stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, sequences[seqIndexEdit].getLength());
				if (stepIndexEdit == 0) stepIndexEdit = 16;
			}
			else
				stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, sequences[seqIndexEdit].getLength() + 16);
		}
	}
	
		
	PhraseSeq32() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		// must init those that have no-connect info to non-connected, or else mother may read 0.0 init value if ever refresh limiters make it such that after a connection of expander the mother reads before the first pass through the expander's writing code, and this may do something undesired (ex: change track in Foundry on expander connected while track CV jack is empty)
		rightMessages[1][4] = std::numeric_limits<float>::quiet_NaN();

		configSwitch(CONFIG_PARAM, 0.0f, 1.0f, 0.0f, "Configuration", {"1x32", "2x16"});
		char strBuf[32];
		for (int x = 0; x < 16; x++) {
			snprintf(strBuf, 32, "Step/phrase %i", x + 1);
			configParam(STEP_PHRASE_PARAMS + x, 0.0f, 1.0f, 0.0f, strBuf);
			snprintf(strBuf, 32, "Step/phrase %i", x + 16 + 1);
			configParam(STEP_PHRASE_PARAMS + x + 16, 0.0f, 1.0f, 0.0f, strBuf);
		}
		configParam(ATTACH_PARAM, 0.0f, 1.0f, 0.0f, "Attach");
		configParam(KEYNOTE_PARAM, 0.0f, 1.0f, 0.0f, "Keyboard note mode");
		configParam(KEYGATE_PARAM, 0.0f, 1.0f, 0.0f, "Keyboard gate-type mode");
		for (int i = 0; i < 7; i++) {
			snprintf(strBuf, 32, "Octave %i", i + 1);
			configParam(OCTAVE_PARAM + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		
		configSwitch(EDIT_PARAM, 0.0f, 1.0f, 1.0f, "Seq/song mode", {"Song", "Sequence"});// 1.0f is top position
		configParam(RUNMODE_PARAM, 0.0f, 1.0f, 0.0f, "Length / run mode");
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, "Sequence");		
		configParam(TRAN_ROT_PARAM, 0.0f, 1.0f, 0.0f, "Transpose / rotate");
		
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
		configParam(COPY_PARAM, 0.0f, 1.0f, 0.0f, "Copy");
		configParam(PASTE_PARAM, 0.0f, 1.0f, 0.0f, "Paste");
		configSwitch(CPMODE_PARAM, 0.0f, 2.0f, 0.0f, "Copy-paste mode", {"4 steps", "8 steps", "All steps"});// 0.0f is top position

		configParam(GATE1_PARAM, 0.0f, 1.0f, 0.0f, "Gate 1");
		configParam(GATE2_PARAM, 0.0f, 1.0f, 0.0f, "Gate 2");
		configParam(TIE_PARAM, 0.0f, 1.0f, 0.0f, "Tied");

		configParam(GATE1_PROB_PARAM, 0.0f, 1.0f, 0.0f, "Gate 1 probability");
		configParam(GATE1_KNOB_PARAM, 0.0f, 1.0f, 1.0f, "Probability");
		configParam(SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f, "CV slide");
		configParam(SLIDE_KNOB_PARAM, 0.0f, 2.0f, 0.2f, "Slide rate");
		configSwitch(AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f, "Autostep when write", {"No", "Yes"});
		
		getParamQuantity(CPMODE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(CONFIG_PARAM)->randomizeEnabled = false;		
		getParamQuantity(EDIT_PARAM)->randomizeEnabled = false;		
		getParamQuantity(AUTOSTEP_PARAM)->randomizeEnabled = false;		

		configInput(WRITE_INPUT, "Write");
		configInput(CV_INPUT, "CV");
		configInput(RESET_INPUT, "Reset");
		configInput(CLOCK_INPUT, "Clock");
		configInput(LEFTCV_INPUT, "Step left");
		configInput(RIGHTCV_INPUT, "Step right");
		configInput(RUNCV_INPUT, "Run");
		configInput(SEQCV_INPUT, "Seq#");

		configOutput(CVA_OUTPUT, "Track A CV");
		configOutput(GATE1A_OUTPUT, "Track A Gate 1");
		configOutput(GATE2A_OUTPUT, "Track A Gate 2");
		configOutput(CVB_OUTPUT, "Track B CV");
		configOutput(GATE1B_OUTPUT, "Track B Gate 1");
		configOutput(GATE2B_OUTPUT, "Track B Gate 2");

		for (int i = 0; i < 32; i++) {
			seqAttribBuffer[i].init(16, MODE_FWD);
		}
		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}

	
	void onReset() override {
		autoseq = false;
		autostepLen = false;
		holdTiedNotes = true;
		seqCVmethod = 0;// 0 is 0-10V, 1 is C4-G6, 2 is TrigIncr
		pulsesPerStep = 1;
		running = true;
		runModeSong = MODE_FWD;
		stepIndexEdit = 0;
		seqIndexEdit = 0;
		phraseIndexEdit = 0;
		phrases = 4;
		for (int i = 0; i < 32; i++) {
			sequences[i].init(16 * getStepConfig(), MODE_FWD);
			phrase[i] = 0;
			for (int s = 0; s < 32; s++) {
				cv[i][s] = 0.0f;
				attributes[i][s].init();
			}
		}
		resetOnRun = false;
		attached = false;
		stopAtEndOfSong = false;
		resetNonJson(false);
	}
	void resetNonJson(bool delayed) {// delay thread sensitive parts (i.e. schedule them so that process() will do them)
		displayState = DISP_NORMAL;
		for (int i = 0; i < 32; i++) {
			cvCPbuffer[i] = 0.0f;
			attribCPbuffer[i].init();
			phraseCPbuffer[i] = 0;
		}
		seqAttribCPbuffer.init(32, MODE_FWD);
		seqCopied = true;
		countCP = 32;
		startCP = 0;
		editingGate = 0ul;
		editingType = 0ul;
		infoCopyPaste = 0l;
		clockPeriod = 0ul;
		tiedWarning = 0ul;
		attachedWarning = 0l;
		revertDisplay = 0l;
		editingGateLength = 0l;
		lastGateEdit = 1l;
		editingPpqn = 0l;
		if (delayed) {
			stepConfigSync = 1;// signal a sync from dataFromJson so that step will get lengths from seqAttribBuffer
		}
		else {
			stepConfig = getStepConfig();
			initRun();
		}
	}
	void initRun() {// run button activated, or run edge in run input jack, or stepConfig switch changed, or fromJson()
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * APP->engine->getSampleRate());
		phraseIndexRun = (runModeSong == MODE_REV ? phrases - 1 : 0);
		phraseIndexRunHistory = 0;

		int seq = (isEditingSequence() ? seqIndexEdit : phrase[phraseIndexRun]);
		stepIndexRun[0] = (sequences[seq].getRunMode() == MODE_REV ? sequences[seq].getLength() - 1 : 0);
		fillStepIndexRunVector(sequences[seq].getRunMode(), sequences[seq].getLength());
		stepIndexRunHistory = 0;

		ppqnCount = 0;
		for (int i = 0; i < 2; i += stepConfig) {
			lastProbGate1Enable[i] = true;
			calcGate1Code(attributes[seq][(i * 16) + stepIndexRun[i]], i);
			gate2Code[i] = calcGate2Code(attributes[seq][(i * 16) + stepIndexRun[i]], 0, pulsesPerStep);
		}
		slideStepsRemain[0] = 0ul;
		slideStepsRemain[1] = 0ul;
	}	

	
	void onRandomize() override {
		if (isEditingSequence()) {
			for (int s = 0; s < 32; s++) {
				cv[seqIndexEdit][s] = ((float)(random::u32() % 5)) + ((float)(random::u32() % 12)) / 12.0f - 2.0f;
				attributes[seqIndexEdit][s].randomize();
			}
			sequences[seqIndexEdit].randomize(16 * stepConfig, NUM_MODES);// ok to use stepConfig since CONFIG_PARAM is not randomizable		
		}
	}
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// autostepLen
		json_object_set_new(rootJ, "autostepLen", json_boolean(autostepLen));
		
		// autoseq
		json_object_set_new(rootJ, "autoseq", json_boolean(autoseq));
		
		// holdTiedNotes
		json_object_set_new(rootJ, "holdTiedNotes", json_boolean(holdTiedNotes));
		
		// seqCVmethod
		json_object_set_new(rootJ, "seqCVmethod", json_integer(seqCVmethod));

		// pulsesPerStep
		json_object_set_new(rootJ, "pulsesPerStep", json_integer(pulsesPerStep));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// runModeSong
		json_object_set_new(rootJ, "runModeSong3", json_integer(runModeSong));

		// seqIndexEdit
		json_object_set_new(rootJ, "sequence", json_integer(seqIndexEdit));

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase", phraseJ);

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// CV
		json_t *cvJ = json_array();
		for (int i = 0; i < 32; i++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(cvJ, s + (i * 32), json_real(cv[i][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// attributes
		json_t *attributesJ = json_array();
		for (int i = 0; i < 32; i++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(attributesJ, s + (i * 32), json_integer(attributes[i][s].getAttribute()));
			}
		json_object_set_new(rootJ, "attributes", attributesJ);

		// attached
		json_object_set_new(rootJ, "attached", json_boolean(attached));

		// stopAtEndOfSong
		json_object_set_new(rootJ, "stopAtEndOfSong", json_boolean(stopAtEndOfSong));

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));
	
		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

		// sequences
		json_t *sequencesJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(sequencesJ, i, json_integer(sequences[i].getSeqAttrib()));
		json_object_set_new(rootJ, "sequences", sequencesJ);

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// panelContrast
		json_t *panelContrastJ = json_object_get(rootJ, "panelContrast");
		if (panelContrastJ)
			panelContrast = json_number_value(panelContrastJ);

		// autostepLen
		json_t *autostepLenJ = json_object_get(rootJ, "autostepLen");
		if (autostepLenJ)
			autostepLen = json_is_true(autostepLenJ);

		// autoseq
		json_t *autoseqJ = json_object_get(rootJ, "autoseq");
		if (autoseqJ)
			autoseq = json_is_true(autoseqJ);

		// holdTiedNotes
		json_t *holdTiedNotesJ = json_object_get(rootJ, "holdTiedNotes");
		if (holdTiedNotesJ)
			holdTiedNotes = json_is_true(holdTiedNotesJ);
		else
			holdTiedNotes = false;// legacy
		
		// seqCVmethod
		json_t *seqCVmethodJ = json_object_get(rootJ, "seqCVmethod");
		if (seqCVmethodJ)
			seqCVmethod = json_integer_value(seqCVmethodJ);

		// pulsesPerStep
		json_t *pulsesPerStepJ = json_object_get(rootJ, "pulsesPerStep");
		if (pulsesPerStepJ)
			pulsesPerStep = json_integer_value(pulsesPerStepJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// sequences
		json_t *sequencesJ = json_object_get(rootJ, "sequences");
		if (sequencesJ) {
			for (int i = 0; i < 32; i++)
			{
				json_t *sequencesArrayJ = json_array_get(sequencesJ, i);
				if (sequencesArrayJ)
					seqAttribBuffer[i].setSeqAttrib(json_integer_value(sequencesArrayJ));
			}			
		}
		else {// legacy
			int lengths[32] = {};//1 to 32
			int runModeSeq[32] = {}; 
			int transposeOffsets[32] = {};	
			
			// runModeSeq
			json_t *runModeSeqJ = json_object_get(rootJ, "runModeSeq3");
			if (runModeSeqJ) {
				for (int i = 0; i < 32; i++)
				{
					json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
					if (runModeSeqArrayJ)
						runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
				}			
			}		
			else {// legacy
				runModeSeqJ = json_object_get(rootJ, "runModeSeq2");
				if (runModeSeqJ) {
					for (int i = 0; i < 32; i++)// bug, should be 32 but keep since legacy patches were written with 16
					{
						json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
						if (runModeSeqArrayJ) {
							runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
							if (runModeSeq[i] >= MODE_PEN)// this mode was not present in version runModeSeq2
								runModeSeq[i]++;
						}
					}			
				}			
			}
			// lengths
			json_t *lengthsJ = json_object_get(rootJ, "lengths");
			if (lengthsJ)
				for (int i = 0; i < 32; i++)
				{
					json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
					if (lengthsArrayJ)
						lengths[i] = json_integer_value(lengthsArrayJ);
				}
			// transposeOffsets
			json_t *transposeOffsetsJ = json_object_get(rootJ, "transposeOffsets");
			if (transposeOffsetsJ) {
				for (int i = 0; i < 32; i++)
				{
					json_t *transposeOffsetsArrayJ = json_array_get(transposeOffsetsJ, i);
					if (transposeOffsetsArrayJ)
						transposeOffsets[i] = json_integer_value(transposeOffsetsArrayJ);
				}			
			}
			
			// now write into new object
			for (int i = 0; i < 32; i++) {
				seqAttribBuffer[i].init(lengths[i], runModeSeq[i]);
				seqAttribBuffer[i].setTranspose(transposeOffsets[i]);
			}
		}
		
		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, "runModeSong3");
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
		else {// legacy
			json_t *runModeSongJ = json_object_get(rootJ, "runModeSong");
			if (runModeSongJ) {
				runModeSong = json_integer_value(runModeSongJ);
				if (runModeSong >= MODE_PEN)// this mode was not present in original version
					runModeSong++;
			}
		}
		
		// seqIndexEdit
		json_t *sequenceJ = json_object_get(rootJ, "sequence");
		if (sequenceJ)
			seqIndexEdit = json_integer_value(sequenceJ);
		
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase");
		if (phraseJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int i = 0; i < 32; i++)
				for (int s = 0; s < 32; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (i * 32));
					if (cvArrayJ)
						cv[i][s] = json_number_value(cvArrayJ);
				}
		}
		
		// attributes
		json_t *attributesJ = json_object_get(rootJ, "attributes");
		if (attributesJ) {
			for (int i = 0; i < 32; i++)
				for (int s = 0; s < 32; s++) {
					json_t *attributesArrayJ = json_array_get(attributesJ, s + (i * 32));
					if (attributesArrayJ)
						attributes[i][s].setAttribute((unsigned short)json_integer_value(attributesArrayJ));
				}
		}
		
		// attached
		json_t *attachedJ = json_object_get(rootJ, "attached");
		if (attachedJ)
			attached = json_is_true(attachedJ);
		
		// stopAtEndOfSong
		json_t *stopAtEndOfSongJ = json_object_get(rootJ, "stopAtEndOfSong");
		if (stopAtEndOfSongJ)
			stopAtEndOfSong = json_is_true(stopAtEndOfSongJ);
		
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		resetNonJson(true);
	}
	
	
	IoStep* fillIoSteps(int *seqLenPtr) {// caller must delete return array
		int seqLen = sequences[seqIndexEdit].getLength();
		IoStep* ioSteps = new IoStep[seqLen];
		
		int ofs16 = (stepIndexEdit >= 16 && stepConfig == 1 && seqLen <= 16) ? 16 : 0;// offset needed to grab correct seq when in 2x16  (last condition is safety)
		
		// populate ioSteps array
		for (int i = 0; i < seqLen; i++) {
			ioSteps[i].pitch = cv[seqIndexEdit][i + ofs16];
			StepAttributes stepAttrib = attributes[seqIndexEdit][i + ofs16];
			ioSteps[i].gate = stepAttrib.getGate1();
			ioSteps[i].tied = stepAttrib.getTied();
			ioSteps[i].vel = -1.0f;// no concept of velocity in PhraseSequencers
			ioSteps[i].prob = stepAttrib.getGate1P() ? params[GATE1_KNOB_PARAM].getValue() : -1.0f;// negative means prob is not on for this note
		}
		
		// return values 
		*seqLenPtr = seqLen;
		return ioSteps;
	}
	
	
	void emptyIoSteps(IoStep* ioSteps, int seqLen) {// seqLen is max 32 when in 1x32 and max 16 when in 2x16
		sequences[seqIndexEdit].setLength(seqLen);
		
		int ofs16 = (stepIndexEdit >= 16 && stepConfig == 1 && seqLen <= 16) ? 16 : 0;// offset needed to put correct seq when in 2x16  (last condition is safety)
		
		// populate steps in the sequencer
		// first pass is done without ties
		for (int i = 0; i < seqLen; i++) {
			cv[seqIndexEdit][i + ofs16] = ioSteps[i].pitch;
			
 			StepAttributes stepAttrib;
			stepAttrib.init();
			stepAttrib.setGate1(ioSteps[i].gate);
			stepAttrib.setGate1P(ioSteps[i].prob >= 0.0f);
			attributes[seqIndexEdit][i + ofs16] = stepAttrib;
		}
		// now do ties, has to be done in a separate pass such that non tied that follows tied can be 
		//   there in advance for proper gate types
		for (int i = 0; i < seqLen; i++) {
			if (ioSteps[i].tied) {
				activateTiedStep(seqIndexEdit, i);
			}
		}
	}
	

	void rotateSeq(int seqNum, bool directionRight, int seqLength, bool chanB_16) {
		// set chanB_16 to false to rotate chan A in 2x16 config (length will be <= 16) or single chan in 1x32 config (length will be <= 32)
		// set chanB_16 to true  to rotate chan B in 2x16 config (length must be <= 16)
		float rotCV;
		StepAttributes rotAttributes;
		int iStart = chanB_16 ? 16 : 0;
		int iEnd = iStart + seqLength - 1;
		int iRot = iStart;
		int iDelta = 1;
		if (directionRight) {
			iRot = iEnd;
			iDelta = -1;
		}
		rotCV = cv[seqNum][iRot];
		rotAttributes = attributes[seqNum][iRot];
		for ( ; ; iRot += iDelta) {
			if (iDelta == 1 && iRot >= iEnd) break;
			if (iDelta == -1 && iRot <= iStart) break;
			cv[seqNum][iRot] = cv[seqNum][iRot + iDelta];
			attributes[seqNum][iRot] = attributes[seqNum][iRot + iDelta];
		}
		cv[seqNum][iRot] = rotCV;
		attributes[seqNum][iRot] = rotAttributes;
	}
	

	void calcGate1Code(StepAttributes attribute, int index) {
		int gateType = attribute.getGate1Mode();
		
		if (ppqnCount == 0 && !attribute.getTied()) {
			lastProbGate1Enable[index] = !attribute.getGate1P() || (random::uniform() < params[GATE1_KNOB_PARAM].getValue()); // random::uniform is [0.0, 1.0), see include/util/common.hpp
		}
			
		if (!attribute.getGate1() || !lastProbGate1Enable[index]) {
			gate1Code[index] = 0;
		}
		else if (pulsesPerStep == 1 && gateType == 0) {
			gate1Code[index] = 2;// clock high
		}
		else { 
			if (gateType == 11) {
				gate1Code[index] = (ppqnCount == 0 ? 3 : 0);
			}
			else {
				gate1Code[index] = getAdvGate(ppqnCount, pulsesPerStep, gateType);
			}
		}
	}
	

	void process(const ProcessArgs &args) override {
		float sampleRate = args.sampleRate;
		static const float gateTime = 0.4f;// seconds
		static const float revertDisplayTime = 0.7f;// seconds
		static const float warningTime = 0.7f;// seconds
		static const float holdDetectTime = 2.0f;// seconds
		static const float editGateLengthTime = 3.5f;// seconds
		
		bool expanderPresent = (rightExpander.module && rightExpander.module->model == modelPhraseSeqExpander);
		float *messagesFromExpander = (float*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent

		
		//********** Buttons, knobs, switches and inputs **********
		
		// Edit mode
		bool editingSequence = isEditingSequence();// true = editing sequence, false = editing song
		
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage())) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running) {
				if (resetOnRun) {
					initRun();
				}
			}
			displayState = DISP_NORMAL;
		}

		if (refresh.processInputs()) {
			// Config switch
			// switch may move in the pre-fromJson, but no problem, it will trigger the init lenght below, but then when
			//    the lengths are loaded and we see the stepConfigSync request later,
			//    they will get overwritten anyways. Is simultaneous, also ok.
			int oldStepConfig = stepConfig;
			stepConfig = getStepConfig();
			if (stepConfigSync != 0) {// sync from dataFromJson, so read lengths from seqAttribBuffer
				for (int i = 0; i < 32; i++)
					sequences[i].setSeqAttrib(seqAttribBuffer[i].getSeqAttrib());
				initRun();			
				stepConfigSync = 0;
			}
			else if (stepConfig != oldStepConfig) {// switch moved, so init lengths
				for (int i = 0; i < 32; i++)
					sequences[i].setLength(16 * stepConfig);
				initRun();			
			}				
			
			// Seq CV input
			if (inputs[SEQCV_INPUT].isConnected()) {
				if (seqCVmethod == 0) {// 0-10 V
					int newSeq = (int)( inputs[SEQCV_INPUT].getVoltage() * (32.0f - 1.0f) / 10.0f + 0.5f );
					seqIndexEdit = clamp(newSeq, 0, 32 - 1);
				}
				else if (seqCVmethod == 1) {// C4-G6
					int newSeq = (int)std::round(inputs[SEQCV_INPUT].getVoltage() * 12.0f);
					seqIndexEdit = clamp(newSeq, 0, 32 - 1);
				}
				else {// TrigIncr
					if (seqCVTrigger.process(inputs[SEQCV_INPUT].getVoltage()))
						seqIndexEdit = clamp(seqIndexEdit + 1, 0, 32 - 1);
				}	
			}
			
			// Mode CV input
			if (expanderPresent && editingSequence) {
				float modeCVin = messagesFromExpander[4];
				if (!std::isnan(modeCVin))
					sequences[seqIndexEdit].setRunMode((int) clamp( std::round(modeCVin * ((float)NUM_MODES - 1.0f) / 10.0f), 0.0f, (float)NUM_MODES - 1.0f ));
			}
			
			// Attach button
			if (attachedTrigger.process(params[ATTACH_PARAM].getValue())) {
				attached = !attached;
				displayState = DISP_NORMAL;			
			}
			if (running && attached) {
				if (editingSequence) {
					if (stepIndexEdit >= 16 && stepConfig == 1)
						stepIndexEdit = stepIndexRun[1] + 16;
					else
						stepIndexEdit = stepIndexRun[0] + 0;
				}
				else
					phraseIndexEdit = phraseIndexRun;
			}
			
			// Copy button
			if (copyTrigger.process(params[COPY_PARAM].getValue())) {
				if (!attached) {
					startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
					countCP = 32;
					if (params[CPMODE_PARAM].getValue() > 1.5f)// all
						startCP = 0;
					else if (params[CPMODE_PARAM].getValue() < 0.5f)// 4
						countCP = std::min(4, 32 - startCP);
					else// 8
						countCP = std::min(8, 32 - startCP);
					if (editingSequence) {
						for (int i = 0, s = startCP; i < countCP; i++, s++) {
							cvCPbuffer[i] = cv[seqIndexEdit][s];
							attribCPbuffer[i] = attributes[seqIndexEdit][s];
						}
						seqAttribCPbuffer.setSeqAttrib(sequences[seqIndexEdit].getSeqAttrib());
						seqCopied = true;
					}
					else {
						for (int i = 0, p = startCP; i < countCP; i++, p++)
							phraseCPbuffer[i] = phrase[p];
						seqCopied = false;// so that a cross paste can be detected
					}
					infoCopyPaste = (long) (revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					displayState = DISP_NORMAL;
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			// Paste button
			if (pasteTrigger.process(params[PASTE_PARAM].getValue())) {
				if (!attached) {
					infoCopyPaste = (long) (-1 * revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					startCP = 0;
					if (countCP <= 8) {
						startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
						countCP = std::min(countCP, 32 - startCP);
					}
					// else nothing to do for ALL

					if (editingSequence) {
						if (seqCopied) {// non-crossed paste (seq vs song)
							for (int i = 0, s = startCP; i < countCP; i++, s++) {
								cv[seqIndexEdit][s] = cvCPbuffer[i];
								attributes[seqIndexEdit][s] = attribCPbuffer[i];
							}
							if (params[CPMODE_PARAM].getValue() > 1.5f) {// all
								sequences[seqIndexEdit].setSeqAttrib(seqAttribCPbuffer.getSeqAttrib());
								if (sequences[seqIndexEdit].getLength() > 16 * stepConfig)
									sequences[seqIndexEdit].setLength(16 * stepConfig);
							}
						}
						else {// crossed paste to seq (seq vs song)
							if (params[CPMODE_PARAM].getValue() > 1.5f) { // ALL (init steps)
								for (int s = 0; s < 32; s++) {
									//cv[seqIndexEdit][s] = 0.0f;
									//attributes[seqIndexEdit][s].init();
									attributes[seqIndexEdit][s].toggleGate1();
								}
								sequences[seqIndexEdit].setTranspose(0);
								sequences[seqIndexEdit].setRotate(0);
							}
							else if (params[CPMODE_PARAM].getValue() < 0.5f) {// 4 (randomize CVs)
								for (int s = 0; s < 32; s++)
									cv[seqIndexEdit][s] = ((float)(random::u32() % 7)) + ((float)(random::u32() % 12)) / 12.0f - 3.0f;
								sequences[seqIndexEdit].setTranspose(0);
								sequences[seqIndexEdit].setRotate(0);
							}
							else {// 8 (randomize gate 1)
								for (int s = 0; s < 32; s++)
									if ( (random::u32() & 0x1) != 0)
										attributes[seqIndexEdit][s].toggleGate1();
							}
							startCP = 0;
							countCP = 32;
							infoCopyPaste *= 2l;
						}
					}
					else {
						if (!seqCopied) {// non-crossed paste (seq vs song)
							for (int i = 0, p = startCP; i < countCP; i++, p++)
								phrase[p] = phraseCPbuffer[i];
						}
						else {// crossed paste to song (seq vs song)
							if (params[CPMODE_PARAM].getValue() > 1.5f) { // ALL (init phrases)
								for (int p = 0; p < 32; p++)
									phrase[p] = 0;
							}
							else if (params[CPMODE_PARAM].getValue() < 0.5f) {// 4 (phrases increase from 1 to 32)
								for (int p = 0; p < 32; p++)
									phrase[p] = p;						
							}
							else {// 8 (randomize phrases)
								for (int p = 0; p < 32; p++)
									phrase[p] = random::u32() % 32;
							}
							startCP = 0;
							countCP = 32;
							infoCopyPaste *= 2l;
						}					
					}
					displayState = DISP_NORMAL;
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			
			// Write input (must be before Left and Right in case route gate simultaneously to Right and Write for example)
			//  (write must be to correct step)
			bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].getVoltage());
			if (writeTrig) {
				if (editingSequence) {
					if (!attributes[seqIndexEdit][stepIndexEdit].getTied()) {
						cv[seqIndexEdit][stepIndexEdit] = inputs[CV_INPUT].getVoltage();
						propagateCVtoTied(seqIndexEdit, stepIndexEdit);
					}
					editingGate = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					editingGateCV = inputs[CV_INPUT].getVoltage();// cv[seqIndexEdit][stepIndexEdit];
					editingGateKeyLight = -1;
					editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
					// Autostep (after grab all active inputs)
					if (params[AUTOSTEP_PARAM].getValue() > 0.5f) {
						moveStepIndexEdit(1, autostepLen);
						if (stepIndexEdit == 0 && autoseq && !inputs[SEQCV_INPUT].isConnected())
							seqIndexEdit = moveIndex(seqIndexEdit, seqIndexEdit + 1, 32);
					}
				}
				displayState = DISP_NORMAL;
			}
			// Left and right CV inputs
			int delta = 0;
			if (leftTrigger.process(inputs[LEFTCV_INPUT].getVoltage())) { 
				delta = -1;
			}
			if (rightTrigger.process(inputs[RIGHTCV_INPUT].getVoltage())) {
				delta = +1;
			}
			if (delta != 0) {
				if (displayState != DISP_LENGTH)
					displayState = DISP_NORMAL;
				if (!running || !attached) {// don't move heads when attach and running
					if (editingSequence) {
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, 32);
						if (!attributes[seqIndexEdit][stepIndexEdit].getTied()) {// play if non-tied step
							if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
								editingGate = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
								editingGateCV = cv[seqIndexEdit][stepIndexEdit];
								editingGateKeyLight = -1;
								editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
							}
						}
					}
					else {
						phraseIndexEdit = moveIndex(phraseIndexEdit, phraseIndexEdit + delta, 32);
						if (!running)
							phraseIndexRun = phraseIndexEdit;	
					}						
				}
			}

			// Step button presses
			int stepPressed = -1;
			for (int i = 0; i < 32; i++) {
				if (stepTriggers[i].process(params[STEP_PHRASE_PARAMS + i].getValue()))
					stepPressed = i;
			}
			if (stepPressed != -1) {
				if (displayState == DISP_LENGTH) {
					if (editingSequence)
						sequences[seqIndexEdit].setLength((stepPressed % (16 * stepConfig)) + 1);
					else
						phrases = stepPressed + 1;
					revertDisplay = (long) (revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else {
					if (!running || !attached) {// not running or detached
						if (editingSequence) {
							stepIndexEdit = stepPressed;
							if (!attributes[seqIndexEdit][stepIndexEdit].getTied()) {// play if non-tied step
								editingGate = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
								editingGateCV = cv[seqIndexEdit][stepIndexEdit];
								editingGateKeyLight = -1;
								editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
							}
						}
						else {
							phraseIndexEdit = stepPressed;
							if (!running)
								phraseIndexRun = phraseIndexEdit;
						}
					}
					else {// attached and running
						attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						if (editingSequence) {
							if (stepPressed >= 16 && stepConfig == 1)
								stepIndexEdit = stepIndexRun[1] + 16;
							else
								stepIndexEdit = stepIndexRun[0] + 0;
						}
					}
					displayState = DISP_NORMAL;
				}
			} 
			
			// Mode/Length button
			if (modeTrigger.process(params[RUNMODE_PARAM].getValue())) {
				if (!attached) {
					if (editingPpqn != 0l)
						editingPpqn = 0l;			
					if (displayState == DISP_NORMAL || displayState == DISP_TRANSPOSE || displayState == DISP_ROTATE)
						displayState = DISP_LENGTH;
					else if (displayState == DISP_LENGTH)
						displayState = DISP_MODE;
					else
						displayState = DISP_NORMAL;
					modeHoldDetect.start((long) (holdDetectTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			
			// Transpose/Rotate button
			if (transposeTrigger.process(params[TRAN_ROT_PARAM].getValue())) {
				if (editingSequence && !attached) {
					if (displayState == DISP_NORMAL || displayState == DISP_MODE || displayState == DISP_LENGTH) {
						displayState = DISP_TRANSPOSE;
					}
					else if (displayState == DISP_TRANSPOSE) {
						displayState = DISP_ROTATE;
					}
					else 
						displayState = DISP_NORMAL;
				}
				else if (attached)
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}			
			
			// Sequence knob 
			float seqParamValue = params[SEQUENCE_PARAM].getValue();
			int newSequenceKnob = (int)std::round(seqParamValue * 7.0f);
			if (seqParamValue == 0.0f)// true when constructor or dataFromJson() occured
				sequenceKnob = newSequenceKnob;
			int deltaKnob = newSequenceKnob - sequenceKnob;
			if (deltaKnob != 0) {
				if (abs(deltaKnob) <= 3) {// avoid discontinuous step (initialize for example)
					// any changes in here should may also require right click behavior to be updated in the knob's onMouseDown()
					if (editingPpqn != 0) {
						pulsesPerStep = indexToPps(ppsToIndex(pulsesPerStep) + deltaKnob);// indexToPps() does clamping
						editingPpqn = (long) (editGateLengthTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODE) {
						if (editingSequence) {
							if (!expanderPresent || std::isnan(messagesFromExpander[4])) {
								sequences[seqIndexEdit].setRunMode(clamp(sequences[seqIndexEdit].getRunMode() + deltaKnob, 0, NUM_MODES - 1));
							}
						}
						else {
							runModeSong = clamp(runModeSong + deltaKnob, 0, 6 - 1);
						}
					}
					else if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							sequences[seqIndexEdit].setLength(clamp(sequences[seqIndexEdit].getLength() + deltaKnob, 1, (16 * stepConfig)));
						}
						else {
							phrases = clamp(phrases + deltaKnob, 1, 32);
						}
					}
					else if (displayState == DISP_TRANSPOSE) {
						if (editingSequence) {
							sequences[seqIndexEdit].setTranspose(clamp(sequences[seqIndexEdit].getTranspose() + deltaKnob, -99, 99));
							float transposeOffsetCV = ((float)(deltaKnob))/12.0f;// Tranpose by deltaKnob number of semi-tones
							if (stepConfig == 1){ // 2x16 (transpose only the 16 steps corresponding to row where stepIndexEdit is located)
								int offset = stepIndexEdit < 16 ? 0 : 16;
								for (int s = offset; s < offset + 16; s++) 
									cv[seqIndexEdit][s] += transposeOffsetCV;
							}
							else { // 1x32 (transpose all 32 steps)
								for (int s = 0; s < 32; s++) 
									cv[seqIndexEdit][s] += transposeOffsetCV;
							}
						}
					}
					else if (displayState == DISP_ROTATE) {
						if (editingSequence) {
							int slength = sequences[seqIndexEdit].getLength();
							bool rotChanB = (stepConfig == 1 && stepIndexEdit >= 16);
							sequences[seqIndexEdit].setRotate(clamp(sequences[seqIndexEdit].getRotate() + deltaKnob, -99, 99));
							if (deltaKnob > 0 && deltaKnob < 201) {// Rotate right, 201 is safety
								for (int i = deltaKnob; i > 0; i--) {
									rotateSeq(seqIndexEdit, true, slength, rotChanB);
									if ((stepConfig == 2 || !rotChanB ) && (stepIndexEdit < slength))
										stepIndexEdit = (stepIndexEdit + 1) % slength;
									if (rotChanB && (stepIndexEdit < (slength + 16)) && (stepIndexEdit >= 16))
										stepIndexEdit = ((stepIndexEdit - 16 + 1) % slength) + 16;
								}
							}
							if (deltaKnob < 0 && deltaKnob > -201) {// Rotate left, 201 is safety
								for (int i = deltaKnob; i < 0; i++) {
									rotateSeq(seqIndexEdit, false, slength, rotChanB);
									if ((stepConfig == 2 || !rotChanB ) && (stepIndexEdit < slength))
										stepIndexEdit = (stepIndexEdit + (stepConfig * 16 - 1) ) % slength;
									if (rotChanB && (stepIndexEdit < (slength + 16)) && (stepIndexEdit >= 16))
										stepIndexEdit = ((stepIndexEdit - 1 ) % slength) + 16;
								}
							}
						}						
					}
					else {// DISP_NORMAL
						if (editingSequence) {
							if (!inputs[SEQCV_INPUT].isConnected()) {
								seqIndexEdit = clamp(seqIndexEdit + deltaKnob, 0, 32 - 1);
							}
						}
						else {
							if (!attached || (attached && !running)) {
								int newPhrase = phrase[phraseIndexEdit] + deltaKnob;
								if (newPhrase < 0)
									newPhrase += (1 - newPhrase / 32) * 32;// newPhrase now positive
								newPhrase = newPhrase % 32;
								phrase[phraseIndexEdit] = newPhrase;
							}
							else 
								attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						}
					}
				}
				sequenceKnob = newSequenceKnob;
			}	
			
			// Octave buttons
			for (int i = 0; i < 7; i++) {
				if (octTriggers[i].process(params[OCTAVE_PARAM + i].getValue())) {
					if (editingSequence) {
						displayState = DISP_NORMAL;
						if (attributes[seqIndexEdit][stepIndexEdit].getTied())
							tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						else {			
							cv[seqIndexEdit][stepIndexEdit] = applyNewOct(cv[seqIndexEdit][stepIndexEdit], 3 - i);
							propagateCVtoTied(seqIndexEdit, stepIndexEdit);
							editingGate = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
							editingGateCV = cv[seqIndexEdit][stepIndexEdit];
							editingGateKeyLight = -1;
							editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
						}
					}
				}
			}
			
			// Keyboard buttons
			if (keyTrigger.process(pkInfo.gate)) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (editingGateLength != 0l) {
						int newMode = keyIndexToGateMode(pkInfo.key, pulsesPerStep);
						if (newMode != -1) {
							editingPpqn = 0l;
							attributes[seqIndexEdit][stepIndexEdit].setGateMode(newMode, editingGateLength > 0l);
							if (pkInfo.isRightClick) {
								stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
								editingType = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
								editingGateKeyLight = pkInfo.key;
								if ((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL)
									attributes[seqIndexEdit][stepIndexEdit].setGateMode(newMode, editingGateLength > 0l);
							}
						}
						else
							editingPpqn = (long) (editGateLengthTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
					else if (attributes[seqIndexEdit][stepIndexEdit].getTied()) {
						if (pkInfo.isRightClick)
							stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
						else
							tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
					else {			
						float newCV = std::floor(cv[seqIndexEdit][stepIndexEdit]) + ((float) pkInfo.key) / 12.0f;
						cv[seqIndexEdit][stepIndexEdit] = newCV;
						propagateCVtoTied(seqIndexEdit, stepIndexEdit);
						editingGate = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						editingGateCV = cv[seqIndexEdit][stepIndexEdit];
						editingGateKeyLight = -1;
						editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
						if (pkInfo.isRightClick) {
							stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
							editingGateKeyLight = pkInfo.key;
							if ((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL)
								cv[seqIndexEdit][stepIndexEdit] = newCV;
						}
					}						
				}
			}
			
			// Keyboard mode (note or gate type)
			if (keyNoteTrigger.process(params[KEYNOTE_PARAM].getValue())) {
				editingGateLength = 0l;
			}
			if (keyGateTrigger.process(params[KEYGATE_PARAM].getValue())) {
				if (editingGateLength == 0l) {
					editingGateLength = lastGateEdit;
				}
				else {
					editingGateLength *= -1l;
					lastGateEdit = editingGateLength;
				}
			}

			// Gate1, Gate1Prob, Gate2, Slide and Tied buttons
			if (gate1Trigger.process(params[GATE1_PARAM].getValue() + (expanderPresent ? messagesFromExpander[0] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					attributes[seqIndexEdit][stepIndexEdit].toggleGate1();
				}
			}		
			if (gate1ProbTrigger.process(params[GATE1_PROB_PARAM].getValue())) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (attributes[seqIndexEdit][stepIndexEdit].getTied())
						tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					else
						attributes[seqIndexEdit][stepIndexEdit].toggleGate1P();
				}
			}		
			if (gate2Trigger.process(params[GATE2_PARAM].getValue() + (expanderPresent ? messagesFromExpander[1] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					attributes[seqIndexEdit][stepIndexEdit].toggleGate2();
				}
			}		
			if (slideTrigger.process(params[SLIDE_BTN_PARAM].getValue() + (expanderPresent ? messagesFromExpander[3] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (attributes[seqIndexEdit][stepIndexEdit].getTied())
						tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					else
						attributes[seqIndexEdit][stepIndexEdit].toggleSlide();
				}
			}		
			if (tiedTrigger.process(params[TIE_PARAM].getValue() + (expanderPresent ? messagesFromExpander[2] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (attributes[seqIndexEdit][stepIndexEdit].getTied()) {
						deactivateTiedStep(seqIndexEdit, stepIndexEdit);
					}
					else {
						activateTiedStep(seqIndexEdit, stepIndexEdit);
					}
				}
			}		
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (running && clockIgnoreOnReset == 0l) {
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				ppqnCount++;
				if (ppqnCount >= pulsesPerStep)
					ppqnCount = 0;

				int newSeq = seqIndexEdit;// good value when editingSequence, overwrite if not editingSequence
				if (ppqnCount == 0) {
					float slideFromCV[2] = {0.0f, 0.0f};
					int oldStepIndexRun0 = stepIndexRun[0];
					int oldStepIndexRun1 = stepIndexRun[1];
					if (editingSequence) {
						for (int i = 0; i < 2; i += stepConfig)
							slideFromCV[i] = cv[seqIndexEdit][(i * 16) + stepIndexRun[i]];
						//bool seqLoopOver = 
						moveIndexRunMode(&stepIndexRun[0], sequences[seqIndexEdit].getLength(), sequences[seqIndexEdit].getRunMode(), &stepIndexRunHistory);
						// if (seqLoopOver && stopAtEndOfSong) {
							// running = false;
							// stepIndexRun[0] = oldStepIndexRun0;
							// stepIndexRun[1] = oldStepIndexRun1;
						// }
					}
					else {
						for (int i = 0; i < 2; i += stepConfig)
							slideFromCV[i] = cv[phrase[phraseIndexRun]][(i * 16) + stepIndexRun[i]];
						if (moveIndexRunMode(&stepIndexRun[0], sequences[phrase[phraseIndexRun]].getLength(), sequences[phrase[phraseIndexRun]].getRunMode(), &stepIndexRunHistory)) {
							int oldPhraseIndexRun = phraseIndexRun;
							bool songLoopOver = moveIndexRunMode(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
							// check for end of song if needed
							if (songLoopOver && stopAtEndOfSong) {
								running = false;
								stepIndexRun[0] = oldStepIndexRun0;
								stepIndexRun[1] = oldStepIndexRun1;
								phraseIndexRun = oldPhraseIndexRun;
							}
							else {
								stepIndexRun[0] = (sequences[phrase[phraseIndexRun]].getRunMode() == MODE_REV ? sequences[phrase[phraseIndexRun]].getLength() - 1 : 0);// must always refresh after phraseIndexRun has changed
							}
						}
						newSeq = phrase[phraseIndexRun];
					}
					if (running)// end of song may have stopped it
						fillStepIndexRunVector(sequences[newSeq].getRunMode(), sequences[newSeq].getLength());

					// Slide
					for (int i = 0; i < 2; i += stepConfig) {
						if (attributes[newSeq][(i * 16) + stepIndexRun[i]].getSlide()) {
							slideStepsRemain[i] = (unsigned long) (((float)clockPeriod  * pulsesPerStep) * params[SLIDE_KNOB_PARAM].getValue() / 2.0f);
							if (slideStepsRemain[i] != 0ul) {
								float slideToCV = cv[newSeq][(i * 16) + stepIndexRun[i]];
								slideCVdelta[i] = (slideToCV - slideFromCV[i])/(float)slideStepsRemain[i];
							}
						}
						else
							slideStepsRemain[i] = 0ul;
					}
				}
				else {
					if (!editingSequence)
						newSeq = phrase[phraseIndexRun];
				}
				for (int i = 0; i < 2; i += stepConfig) {
					calcGate1Code(attributes[newSeq][(i * 16) + stepIndexRun[i]], i);
					gate2Code[i] = calcGate2Code(attributes[newSeq][(i * 16) + stepIndexRun[i]], ppqnCount, pulsesPerStep);	
				}
				clockPeriod = 0ul;
			}
			clockPeriod++;
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue())) {
			initRun();// must be before SEQCV_INPUT below
			resetLight = 1.0f;
			displayState = DISP_NORMAL;
			clockTrigger.reset();
			if (inputs[SEQCV_INPUT].isConnected() && seqCVmethod == 2)
				seqIndexEdit = 0;
		}
		
		
		//********** Outputs and lights **********
				
		// CV and gates outputs
		int seq = editingSequence ? seqIndexEdit : phrase[phraseIndexRun];
		int step0 = (editingSequence && !running) ? stepIndexEdit : stepIndexRun[0];
		if (running) {
			bool muteGate1A = !editingSequence && ((params[GATE1_PARAM].getValue() + (expanderPresent ? messagesFromExpander[0] : 0.0f)) > 0.5f);// live mute
			bool muteGate1B = muteGate1A;
			bool muteGate2A = !editingSequence && ((params[GATE2_PARAM].getValue() + (expanderPresent ? messagesFromExpander[1] : 0.0f)) > 0.5f);// live mute
			bool muteGate2B = muteGate2A;
			if (!attached && (muteGate1B || muteGate2B) && stepConfig == 1) {
				// if not attached in 2x16, mute only the channel where phraseIndexEdit is located (hack since phraseIndexEdit's row has no relation to channels)
				if (phraseIndexEdit < 16) {
					muteGate1B = false;
					muteGate2B = false;
				}
				else {
					muteGate1A = false;
					muteGate2A = false;
				}
			}
			float slideOffset[2];
			for (int i = 0; i < 2; i += stepConfig)
				slideOffset[i] = (slideStepsRemain[i] > 0ul ? (slideCVdelta[i] * (float)slideStepsRemain[i]) : 0.0f);
			outputs[CVA_OUTPUT].setVoltage(cv[seq][step0] - slideOffset[0]);
			bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
			outputs[GATE1A_OUTPUT].setVoltage((calcGate(gate1Code[0], clockTrigger, clockPeriod, sampleRate) && !muteGate1A && !retriggingOnReset) ? 10.0f : 0.0f);
			outputs[GATE2A_OUTPUT].setVoltage((calcGate(gate2Code[0], clockTrigger, clockPeriod, sampleRate) && !muteGate2A && !retriggingOnReset) ? 10.0f : 0.0f);
			if (stepConfig == 1) {// 2x16
				int step1 = (editingSequence && !running) ? stepIndexEdit : stepIndexRun[1];
				outputs[CVB_OUTPUT].setVoltage(cv[seq][16 + step1] - slideOffset[1]);
				outputs[GATE1B_OUTPUT].setVoltage((calcGate(gate1Code[1], clockTrigger, clockPeriod, sampleRate) && !muteGate1B && !retriggingOnReset) ? 10.0f : 0.0f);
				outputs[GATE2B_OUTPUT].setVoltage((calcGate(gate2Code[1], clockTrigger, clockPeriod, sampleRate) && !muteGate2B && !retriggingOnReset) ? 10.0f : 0.0f);
			} 
			else {// 1x32
				outputs[CVB_OUTPUT].setVoltage(0.0f);
				outputs[GATE1B_OUTPUT].setVoltage(0.0f);
				outputs[GATE2B_OUTPUT].setVoltage(0.0f);
			}
		}
		else {// not running 
			if (stepConfig > 1) {// 1x32
				outputs[CVA_OUTPUT].setVoltage((editingGate > 0ul) ? editingGateCV : cv[seq][step0]);
				outputs[GATE1A_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
				outputs[GATE2A_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
				outputs[CVB_OUTPUT].setVoltage(0.0f);
				outputs[GATE1B_OUTPUT].setVoltage(0.0f);
				outputs[GATE2B_OUTPUT].setVoltage(0.0f);
			}
			else {// 2x16
				float cvA = 0.0f;
				float cvB = 0.0f;
				if (editingSequence) {
					cvA = (step0 >= 16 ? cv[seq][step0 - 16] : cv[seq][step0]);
					cvB = (step0 >= 16 ? cv[seq][step0] : cv[seq][step0 + 16]);
				}
				else {
					cvA = cv[seq][step0];
					cvB = cv[seq][stepIndexRun[1]];
				}
				if (editingChannel == 0) {
					outputs[CVA_OUTPUT].setVoltage((editingGate > 0ul) ? editingGateCV : cvA);
					outputs[GATE1A_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
					outputs[GATE2A_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
					outputs[CVB_OUTPUT].setVoltage(cvB);
					outputs[GATE1B_OUTPUT].setVoltage(0.0f);
					outputs[GATE2B_OUTPUT].setVoltage(0.0f);
				}
				else {
					outputs[CVA_OUTPUT].setVoltage(cvA);
					outputs[GATE1A_OUTPUT].setVoltage(0.0f);
					outputs[GATE2A_OUTPUT].setVoltage(0.0f);
					outputs[CVB_OUTPUT].setVoltage((editingGate > 0ul) ? editingGateCV : cvB);
					outputs[GATE1B_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
					outputs[GATE2B_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
				}
			}	
		}
		for (int i = 0; i < 2; i++)
			if (slideStepsRemain[i] > 0ul)
				slideStepsRemain[i]--;

		
		// lights
		if (refresh.processLights()) {
			// Step/phrase lights
			for (int i = 0; i < 32; i++) {
				int col = (stepConfig == 1 ? (i & 0xF) : i);//i % (16 * stepConfig);// optimized
				float red = 0.0f;
				float green = 0.0f;
				float white = 0.0f;
				if (infoCopyPaste != 0l) {
					if (i >= startCP && i < (startCP + countCP))
						green = 0.71f;
				}
				else if (displayState == DISP_LENGTH) {
					if (editingSequence) {
						if (col < (sequences[seqIndexEdit].getLength() - 1))
							green = 0.32f;
						else if (col == (sequences[seqIndexEdit].getLength() - 1))
							green = 1.0f;
					}
					else {
						if (i < phrases - 1)
							green = 0.32f;
						else
							green = (i == phrases - 1) ? 1.0f : 0.0f;
					}
				}
				else if (displayState == DISP_TRANSPOSE) {
					red = 0.71f;
				}
				else if (displayState == DISP_ROTATE) {
					red = (i == stepIndexEdit ? 1.0f : (col < sequences[seqIndexEdit].getLength() ? 0.45f : 0.0f));
				}
				else {// normal led display (i.e. not length)
					int row = i >> (3 + stepConfig);//i / (16 * stepConfig);// optimized (not equivalent code, but in this case has same effect)
					// Run cursor (green)
					if (editingSequence)
						green = ((running && (col == stepIndexRun[row])) ? 1.0f : 0.0f);
					else {
						green = ((running && (i == phraseIndexRun)) ? 1.0f : 0.0f);
						green += ((running && (col == stepIndexRun[row]) && i != phraseIndexEdit) ? 0.42f : 0.0f);
						green = std::min(green, 1.0f);
					}
					// Edit cursor (red)
					if (editingSequence)
						red = (i == stepIndexEdit ? 1.0f : 0.0f);
					else
						red = (i == phraseIndexEdit ? 1.0f : 0.0f);
					bool gate = false;
					if (editingSequence)
						gate = attributes[seqIndexEdit][i].getGate1();
					else if (!editingSequence && (attached && running))
						gate = attributes[phrase[phraseIndexRun]][i].getGate1();
					white = ((green == 0.0f && red == 0.0f && gate && displayState != DISP_MODE) ? 0.15f : 0.0f);
					if (editingSequence && white != 0.0f) {
						green = 0.14f; white = 0.0f;
					}
				}
				setGreenRed(STEP_PHRASE_LIGHTS + i * 3, green, red);
				lights[STEP_PHRASE_LIGHTS + i * 3 + 2].setBrightness(white);
			}
		
			// Octave lights
			float cvVal = editingSequence ? cv[seqIndexEdit][stepIndexEdit] : cv[phrase[phraseIndexEdit]][stepIndexRun[0]];
			int keyLightIndex;
			int octLightIndex;
			calcNoteAndOct(cvVal, &keyLightIndex, &octLightIndex);
			octLightIndex += 3;
			for (int i = 0; i < 7; i++) {
				if (!editingSequence && (!attached || !running || (stepConfig == 1)))// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
												// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
												// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
												// [3] makes no sense, which sequence would be displayed, top or bottom row!
					lights[OCTAVE_LIGHTS + i].setBrightness(0.0f);
				else {
					if (tiedWarning > 0l) {
						bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
						lights[OCTAVE_LIGHTS + i].setBrightness((warningFlashState && (i == (6 - octLightIndex))) ? 1.0f : 0.0f);
					}
					else				
						lights[OCTAVE_LIGHTS + i].setBrightness(i == (6 - octLightIndex) ? 1.0f : 0.0f);
				}
			}
			
			// Keyboard lights (can only show channel A when running attached in 1x16 mode, does not pose problem for all other situations)
			if (editingPpqn != 0) {
				for (int i = 0; i < 12; i++) {
					if (keyIndexToGateMode(i, pulsesPerStep) != -1) {
						setGreenRed(KEY_LIGHTS + i * 2, 1.0f, 1.0f);
					}
					else {
						setGreenRed(KEY_LIGHTS + i * 2, 0.0f, 0.0f);
					}
				}
			} 
			else if (editingGateLength != 0l && editingSequence) {
				int modeLightIndex = gateModeToKeyLightIndex(attributes[seqIndexEdit][stepIndexEdit], editingGateLength > 0l);
				for (int i = 0; i < 12; i++) {
					float green = editingGateLength > 0l ? 1.0f : 0.45f;
					float red = editingGateLength > 0l ? 0.45f : 1.0f;
					if (editingType > 0ul) {
						if (i == editingGateKeyLight) {
							float dimMult = ((float) editingType / (float)(gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
							setGreenRed(KEY_LIGHTS + i * 2, green * dimMult, red * dimMult);
						}
						else
							setGreenRed(KEY_LIGHTS + i * 2, 0.0f, 0.0f);
					}
					else {
						if (i == modeLightIndex) {
							setGreenRed(KEY_LIGHTS + i * 2, green, red);
						}
						else { // show dim note if gatetype is different than note
							setGreenRed(KEY_LIGHTS + i * 2, 0.0f, (i == keyLightIndex ? 0.32f : 0.0f));
						}
					}
				}
			}
			else {
				for (int i = 0; i < 12; i++) {
					lights[KEY_LIGHTS + i * 2 + 0].setBrightness(0.0f);
					if (!editingSequence && (!attached || !running || (stepConfig == 1)))// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
													// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
													// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
													// [3] makes no sense, which sequence would be displayed, top or bottom row!
						lights[KEY_LIGHTS + i * 2 + 1].setBrightness(0.0f);
					else {
						if (tiedWarning > 0l) {
							bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
							lights[KEY_LIGHTS + i * 2 + 1].setBrightness((warningFlashState && i == keyLightIndex) ? 1.0f : 0.0f);
						}
						else {
							if (editingGate > 0ul && editingGateKeyLight != -1)
								lights[KEY_LIGHTS + i * 2 + 1].setBrightness(i == editingGateKeyLight ? ((float) editingGate / (float)(gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips)) : 0.0f);
							else
								lights[KEY_LIGHTS + i * 2 + 1].setBrightness(i == keyLightIndex ? 1.0f : 0.0f);
						}
					}
				}
			}		

			// Key mode light (note or gate type)
			lights[KEYNOTE_LIGHT].setBrightness(editingGateLength == 0l ? 1.0f : 0.0f);
			if (editingGateLength == 0l)
				setGreenRed(KEYGATE_LIGHT, 0.0f, 0.0f);
			else if (editingGateLength > 0l)
				setGreenRed(KEYGATE_LIGHT, 1.0f, 0.45f);
			else
				setGreenRed(KEYGATE_LIGHT, 0.45f, 1.0f);
			
			// Gate1, Gate1Prob, Gate2, Slide and Tied lights (can only show channel A when running attached in 1x32 mode, does not pose problem for all other situations)
			if (!editingSequence && (!attached || !running || (stepConfig == 1))) {// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
											// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
											// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
											// [3] makes no sense, which sequence would be displayed, top or bottom row!
				setGateLight(false, GATE1_LIGHT);
				setGateLight(false, GATE2_LIGHT);
				setGreenRed(GATE1_PROB_LIGHT, 0.0f, 0.0f);
				lights[SLIDE_LIGHT].setBrightness(0.0f);
				lights[TIE_LIGHT].setBrightness(0.0f);
			}
			else {
				StepAttributes attributesVal = attributes[seqIndexEdit][stepIndexEdit];
				if (!editingSequence)
					attributesVal = attributes[phrase[phraseIndexEdit]][stepIndexRun[0]];
				//
				setGateLight(attributesVal.getGate1(), GATE1_LIGHT);
				setGateLight(attributesVal.getGate2(), GATE2_LIGHT);
				setGreenRed(GATE1_PROB_LIGHT, attributesVal.getGate1P() ? 1.0f : 0.0f, attributesVal.getGate1P() ? 1.0f : 0.0f);
				lights[SLIDE_LIGHT].setBrightness(attributesVal.getSlide() ? 1.0f : 0.0f);
				if (tiedWarning > 0l) {
					bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
					lights[TIE_LIGHT].setBrightness(warningFlashState ? 1.0f : 0.0f);
				}
				else
					lights[TIE_LIGHT].setBrightness(attributesVal.getTied() ? 1.0f : 0.0f);
			}
			
			// Attach light
			if (attachedWarning > 0l) {
				bool warningFlashState = calcWarningFlash(attachedWarning, (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
				lights[ATTACH_LIGHT].setBrightness(warningFlashState ? 1.0f : 0.0f);
			}
			else
				lights[ATTACH_LIGHT].setBrightness(attached ? 1.0f : 0.0f);
			
			// Reset light
			lights[RESET_LIGHT].setSmoothBrightness(resetLight, args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));
			resetLight = 0.0f;
			
			// Run light
			lights[RUN_LIGHT].setBrightness(running ? 1.0f : 0.0f);

			if (editingGate > 0ul)
				editingGate--;
			if (editingType > 0ul)
				editingType--;
			if (infoCopyPaste != 0l) {
				if (infoCopyPaste > 0l)
					infoCopyPaste --;
				if (infoCopyPaste < 0l)
					infoCopyPaste ++;
			}
			if (editingPpqn > 0l)
				editingPpqn--;
			if (tiedWarning > 0l)
				tiedWarning--;
			if (attachedWarning > 0l)
				attachedWarning--;
			if (modeHoldDetect.process(params[RUNMODE_PARAM].getValue())) {
				displayState = DISP_NORMAL;
				editingPpqn = (long) (editGateLengthTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			if (revertDisplay > 0l) {
				if (revertDisplay == 1)
					displayState = DISP_NORMAL;
				revertDisplay--;
			}		
			
			// To Expander
			if (rightExpander.module && rightExpander.module->model == modelPhraseSeqExpander) {
				float *messagesToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				messagesToExpander[0] = (float)panelTheme;
				messagesToExpander[1] = panelContrast;
				rightExpander.module->leftExpander.messageFlipRequested = true;
			}
		}// lightRefreshCounter
				
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// process()
	

	inline void setGreenRed(int id, float green, float red) {
		lights[id + 0].setBrightness(green);
		lights[id + 1].setBrightness(red);
	}

	inline void propagateCVtoTied(int seqn, int stepn) {
		for (int i = stepn + 1; i < 32; i++) {
			if (!attributes[seqn][i].getTied())
				break;
			cv[seqn][i] = cv[seqn][i - 1];
		}	
	}

	void activateTiedStep(int seqn, int stepn) {
		attributes[seqn][stepn].setTied(true);
		if (stepn > 0) 
			propagateCVtoTied(seqn, stepn - 1);
		
		if (holdTiedNotes) {// new method
			attributes[seqn][stepn].setGate1(true);
			for (int i = std::max(stepn, 1); i < 32 && attributes[seqn][i].getTied(); i++) {
				attributes[seqn][i].setGate1Mode(attributes[seqn][i - 1].getGate1Mode());
				attributes[seqn][i - 1].setGate1Mode(5);
				attributes[seqn][i - 1].setGate1(true);
			}
		}
		else {// old method
			if (stepn > 0) {
				attributes[seqn][stepn] = attributes[seqn][stepn - 1];
				attributes[seqn][stepn].setTied(true);
			}
		}
	}
	
	void deactivateTiedStep(int seqn, int stepn) {
		attributes[seqn][stepn].setTied(false);
		if (holdTiedNotes) {// new method
			int lastGateType = attributes[seqn][stepn].getGate1Mode();
			for (int i = stepn + 1; i < 32 && attributes[seqn][i].getTied(); i++)
				lastGateType = attributes[seqn][i].getGate1Mode();
			if (stepn > 0)
				attributes[seqn][stepn - 1].setGate1Mode(lastGateType);
		}
		//else old method, nothing to do
	}
	
	inline void setGateLight(bool gateOn, int lightIndex) {
		if (!gateOn) {
			lights[lightIndex + 0].setBrightness(0.0f);
			lights[lightIndex + 1].setBrightness(0.0f);
		}
		else if (editingGateLength == 0l) {
			lights[lightIndex + 0].setBrightness(0.0f);
			lights[lightIndex + 1].setBrightness(1.0f);
		}
		else {
			lights[lightIndex + 0].setBrightness(lightIndex == GATE1_LIGHT ? 1.0f : 0.45f);
			lights[lightIndex + 1].setBrightness(lightIndex == GATE1_LIGHT ? 0.45f : 1.0f);
		}
	}

};



struct PhraseSeq32Widget : ModuleWidget {
	struct SequenceDisplayWidget : TransparentWidget {
		PhraseSeq32 *module;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char displayStr[16];
		int lastNum = -1;// -1 means timedout; >= 0 means we have a first number potential, if ever second key comes fast enough
		clock_t lastTime = 0;
		
		SequenceDisplayWidget() {
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}


		void onHoverKey(const event::HoverKey& e) override {
			if (e.action == GLFW_PRESS) {
				int num1 = -1;
				clock_t currentTime = clock();
				float realTimeDelta = ((float)(currentTime - lastTime))/CLOCKS_PER_SEC;
				if (realTimeDelta < 1.0f) {
					num1 = lastNum;
				}
			
				int num = -1;
				if (e.key >= GLFW_KEY_0 && e.key <= GLFW_KEY_9) {
					num = e.key - GLFW_KEY_0;
				}
				else if (e.key >= GLFW_KEY_KP_0 && e.key <= GLFW_KEY_KP_9) {
					num = e.key - GLFW_KEY_KP_0;
				}
				else if (e.key == GLFW_KEY_SPACE) {
					if (module->displayState != PhraseSeq32::DISP_LENGTH)
						module->displayState = PhraseSeq32::DISP_NORMAL;
					if ((!module->running || !module->attached) && !module->isEditingSequence()) {
						module->phraseIndexEdit = moveIndex(module->phraseIndexEdit, module->phraseIndexEdit + 1, 32);
						if (!module->running)
							module->phraseIndexRun = module->phraseIndexEdit;
					}
				}
				if (num != -1) {
					int totalNum = num;
					if (num1 != -1) {
						totalNum += num1 * 10;
					}

					bool editingSequence = module->isEditingSequence();
					if (module->infoCopyPaste != 0l) {
					}
					else if (module->editingPpqn != 0ul) {
					}
					else if (module->displayState == PhraseSeq32::DISP_MODE) {
					}
					else if (module->displayState == PhraseSeq32::DISP_LENGTH) {
						if (editingSequence)
							module->sequences[module->seqIndexEdit].setLength(clamp(totalNum, 1, (16 * module->stepConfig)));
						else
							module->phrases = clamp(totalNum, 1, 32);
					}
					else if (module->displayState == PhraseSeq32::DISP_TRANSPOSE) {
					}
					else if (module->displayState == PhraseSeq32::DISP_ROTATE) {
					}
					else {// DISP_NORMAL
						totalNum = clamp(totalNum, 1, 32);
						if (editingSequence) {
							if (!module->inputs[PhraseSeq32::SEQCV_INPUT].isConnected())
								module->seqIndexEdit = totalNum - 1;
						}
						else {
							if (!module->attached || (module->attached && !module->running))
								module->phrase[module->phraseIndexEdit] = totalNum - 1;
						}

					}
				}				
				
				lastTime = currentTime;
				lastNum = num;
			}
		}

		
		void runModeToStr(int num) {
			if (num >= 0 && num < NUM_MODES)
				snprintf(displayStr, 4, "%s", modeLabels[num].c_str());
		}

		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, 18);
				nvgFontFaceId(args.vg, font->handle);
				Vec textPos = VecPx(6, 24);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);		
				if (module == NULL) {
					snprintf(displayStr, 4, "  1");
				}
				else {
					bool editingSequence = module->isEditingSequence();
					if (module->infoCopyPaste != 0l) {
						if (module->infoCopyPaste > 0l)
							snprintf(displayStr, 4, "CPY");
						else {
							float cpMode = module->params[PhraseSeq32::CPMODE_PARAM].getValue();
							if (editingSequence && !module->seqCopied) {// cross paste to seq
								if (cpMode > 1.5f)// All = toggle gate 1
									snprintf(displayStr, 4, "TG1");
								else if (cpMode < 0.5f)// 4 = random CV
									snprintf(displayStr, 4, "RCV");
								else// 8 = random gate 1
									snprintf(displayStr, 4, "RG1");
							}
							else if (!editingSequence && module->seqCopied) {// cross paste to song
								if (cpMode > 1.5f)// All = init
									snprintf(displayStr, 4, "CLR");
								else if (cpMode < 0.5f)// 4 = increase by 1
									snprintf(displayStr, 4, "INC");
								else// 8 = random phrases
									snprintf(displayStr, 4, "RPH");
							}
							else
								snprintf(displayStr, 4, "PST");
						}
					}
					else if (module->editingPpqn != 0ul) {
						snprintf(displayStr, 16, "x%2u", (unsigned) module->pulsesPerStep);
					}
					else if (module->displayState == PhraseSeq32::DISP_MODE) {
						if (editingSequence)
							runModeToStr(module->sequences[module->seqIndexEdit].getRunMode());
						else
							runModeToStr(module->runModeSong);
					}
					else if (module->displayState == PhraseSeq32::DISP_LENGTH) {
						if (editingSequence)
							snprintf(displayStr, 16, "L%2u", (unsigned) module->sequences[module->seqIndexEdit].getLength());
						else
							snprintf(displayStr, 16, "L%2u", (unsigned) module->phrases);
					}
					else if (module->displayState == PhraseSeq32::DISP_TRANSPOSE) {
						snprintf(displayStr, 16, "+%2u", (unsigned) abs(module->sequences[module->seqIndexEdit].getTranspose()));
						if (module->sequences[module->seqIndexEdit].getTranspose() < 0)
							displayStr[0] = '-';
					}
					else if (module->displayState == PhraseSeq32::DISP_ROTATE) {
						snprintf(displayStr, 16, ")%2u", (unsigned) abs(module->sequences[module->seqIndexEdit].getRotate()));
						if (module->sequences[module->seqIndexEdit].getRotate() < 0)
							displayStr[0] = '(';
					}
					else {// DISP_NORMAL
						snprintf(displayStr, 16, " %2u", (unsigned) (editingSequence ? 
							module->seqIndexEdit : module->phrase[module->phraseIndexEdit]) + 1 );
					}
				}
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
	};		
	
	struct InteropSeqItem : MenuItem {
		struct InteropCopySeqItem : MenuItem {
			PhraseSeq32 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = module->fillIoSteps(&seqLen);
				interopCopySequence(seqLen, ioSteps);
				delete[] ioSteps;
			}
		};
		struct InteropPasteSeqItem : MenuItem {
			PhraseSeq32 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = interopPasteSequence(module->stepConfig * 16, &seqLen);
				if (ioSteps != nullptr) {
					module->emptyIoSteps(ioSteps, seqLen);
					delete[] ioSteps;
				}
			}
		};
		PhraseSeq32 *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			InteropCopySeqItem *interopCopySeqItem = createMenuItem<InteropCopySeqItem>(portableSequenceCopyID, "");
			interopCopySeqItem->module = module;
			interopCopySeqItem->disabled = disabled;
			menu->addChild(interopCopySeqItem);		
			
			InteropPasteSeqItem *interopPasteSeqItem = createMenuItem<InteropPasteSeqItem>(portableSequencePasteID, "");
			interopPasteSeqItem->module = module;
			interopPasteSeqItem->disabled = disabled;
			menu->addChild(interopPasteSeqItem);		

			return menu;
		}
	};	
	
	void appendContextMenu(Menu *menu) override {
		PhraseSeq32 *module = dynamic_cast<PhraseSeq32*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());

		InteropSeqItem *interopSeqItem = createMenuItem<InteropSeqItem>(portableSequenceID, RIGHT_ARROW);
		interopSeqItem->module = module;
		interopSeqItem->disabled = !module->isEditingSequence();
		menu->addChild(interopSeqItem);		

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createBoolPtrMenuItem("Reset on run", "", &module->resetOnRun));

		menu->addChild(createBoolPtrMenuItem("Hold tied notes", "", &module->holdTiedNotes));		

		menu->addChild(createBoolPtrMenuItem("Single shot song", "", &module->stopAtEndOfSong));

		menu->addChild(createSubmenuItem("Seq CV in level", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("0-10V", "",
				[=]() {return module->seqCVmethod == 0;},
				[=]() {module->seqCVmethod = 0;}
			));
			menu->addChild(createCheckMenuItem("C4-G6", "",
				[=]() {return module->seqCVmethod == 1;},
				[=]() {module->seqCVmethod = 1;}
			));
			menu->addChild(createCheckMenuItem("Trig-Incr", "",
				[=]() {return module->seqCVmethod == 2;},
				[=]() {module->seqCVmethod = 2;}
			));
		}));			
		
		menu->addChild(createBoolPtrMenuItem("AutoStep write bounded by seq length", "", &module->autostepLen));

		menu->addChild(createBoolPtrMenuItem("AutoSeq when writing via CV inputs", "", &module->autoseq));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Actions"));

		InstantiateExpanderItem *expItem = createMenuItem<InstantiateExpanderItem>("Add expander (4HP right side)", "");
		expItem->module = module;
		expItem->model = modelPhraseSeqExpander;
		expItem->posit = box.pos.plus(math::Vec(box.size.x,0));
		menu->addChild(expItem);	
	}	
	
	
	struct SequenceKnob : IMBigKnobInf {
		SequenceKnob() {};		
		void onDoubleClick(const event::DoubleClick &e) override {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity) {
				PhraseSeq32* module = dynamic_cast<PhraseSeq32*>(paramQuantity->module);
				// same code structure below as in sequence knob in main step()
				if (module->editingPpqn != 0) {
					module->pulsesPerStep = 1;
					//editingPpqn = (long) (editGateLengthTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else if (module->displayState == PhraseSeq32::DISP_MODE) {
					if (module->isEditingSequence()) {
						bool expanderPresent = (module->rightExpander.module && module->rightExpander.module->model == modelPhraseSeqExpander);
						float *messagesFromExpander = (float*)module->rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent						
						if (!expanderPresent || std::isnan(messagesFromExpander[4])) {
							module->sequences[module->seqIndexEdit].setRunMode(MODE_FWD);
						}
					}
					else {
						module->runModeSong = MODE_FWD;
					}
				}
				else if (module->displayState == PhraseSeq32::DISP_LENGTH) {
					if (module->isEditingSequence()) {
						module->sequences[module->seqIndexEdit].setLength(16 * module->stepConfig);
					}
					else {
						module->phrases = 4;
					}
				}
				else if (module->displayState == PhraseSeq32::DISP_TRANSPOSE) {
					// nothing
				}
				else if (module->displayState == PhraseSeq32::DISP_ROTATE) {
					// nothing			
				}
				else {// DISP_NORMAL
					if (module->isEditingSequence()) {
						if (!module->inputs[PhraseSeq32::SEQCV_INPUT].isConnected()) {
							module->seqIndexEdit = 0;
						}
					}
					else {
						module->phrase[module->phraseIndexEdit] = 0;
					}
				}
			}
			ParamWidget::onDoubleClick(e);
		}
	};		
	
	
	PhraseSeq32Widget(PhraseSeq32 *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/PhraseSeq32.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		
		
		// ****** Top row ******
		
		static const int rowT0 = 57;
		static const int columnT0 = 27;// Step/Phase LED buttons
		static const int columnT3 = 389;// Attach 
		static const int columnT4 = 442;// Config 

		// Step/Phrase LED buttons
		int posX = columnT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		for (int x = 0; x < 16; x++) {
			// First row
			addParam(createParamCentered<LEDButton>(VecPx(posX, rowT0 - 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_PARAMS + x));
			addChild(createLightCentered<MediumLight<GreenRedWhiteLightIM>>(VecPx(posX, rowT0 - 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_LIGHTS + (x * 3)));
			// Second row
			addParam(createParamCentered<LEDButton>(VecPx(posX, rowT0 + 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_PARAMS + x + 16));
			addChild(createLightCentered<MediumLight<GreenRedWhiteLightIM>>(VecPx(posX, rowT0 + 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_LIGHTS + ((x + 16) * 3)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// Attach button and light
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(columnT3 - 8, rowT0 - 1), module, PhraseSeq32::ATTACH_PARAM, mode));
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(columnT3 - 8 + 22, rowT0 - 1), module, PhraseSeq32::ATTACH_LIGHT));		
		// Config switch
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(columnT4 + 1, rowT0 - 3), module, PhraseSeq32::CONFIG_PARAM, mode, svgPanel));

		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const float octLightsIntY = 20.0f;
		for (int i = 0; i < 7; i++) {
			addParam(createParamCentered<LEDButton>(VecPx(27, 110.6f + i * octLightsIntY), module, PhraseSeq32::OCTAVE_PARAM + i));
			addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(27, 110.6f + i * octLightsIntY), module, PhraseSeq32::OCTAVE_LIGHTS + i));
		}
		
		// Keys and Key lights
		static const Vec keyboardPos = mm2px(Vec(18.222f, 33.303f));
		svgPanel->fb->addChild(new KeyboardSmall(keyboardPos, mode));
		
		static const Vec offsetLeds = Vec(PianoKeySmall::sizeX * 0.5f, PianoKeySmall::sizeY * 0.55f);
		for (int k = 0; k < 12; k++) {
			Vec keyPos = keyboardPos + mm2px(smaKeysPos[k]);
			addChild(createPianoKey<PianoKeySmall>(keyPos, k, module ? &module->pkInfo : NULL));
			addChild(createLightCentered<MediumLight<GreenRedLightIM>>(keyPos + offsetLeds, module, PhraseSeq32::KEY_LIGHTS + k * 2));
		}

		
		// Key mode LED buttons	
		static const int KeyBlackY = 103;
		static const int KeyWhiteY = 141;
		static const int colRulerKM = 276;
		static const int offsetKeyLEDy = 16;
		addParam(createParamCentered<LEDButton>(VecPx(colRulerKM, KeyBlackY + offsetKeyLEDy + 4.4f), module, PhraseSeq32::KEYNOTE_PARAM));
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(colRulerKM,  KeyBlackY + offsetKeyLEDy + 4.4f), module, PhraseSeq32::KEYNOTE_LIGHT));
		addParam(createParamCentered<LEDButton>(VecPx(colRulerKM, KeyWhiteY + offsetKeyLEDy + 4.4f), module, PhraseSeq32::KEYGATE_PARAM));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colRulerKM,  KeyWhiteY + offsetKeyLEDy + 4.4f), module, PhraseSeq32::KEYGATE_LIGHT));

		
		
		// ****** Right side control area ******

		static const int rowMK0 = 113;// Edit mode row
		static const int rowMK1 = rowMK0 + 56; // Run row
		static const int rowMK2 = rowMK1 + 54; // Copy-paste Tran/rot row
		static const int columnMK0 = 319;// Edit mode column
		static const int columnMK2 = columnT4;// Mode/Len column
		static const int columnMK1 = 378;// Display column 
		
		// Edit mode switch
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(columnMK0 + 2, rowMK0), module, PhraseSeq32::EDIT_PARAM, mode, svgPanel));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.size = VecPx(55, 30);// 3 characters
		displaySequence->box.pos = VecPx(columnMK1, rowMK0 + 4).minus(displaySequence->box.size.div(2));
		displaySequence->module = module;
		addChild(displaySequence);
		svgPanel->fb->addChild(new DisplayBackground(displaySequence->box.pos, displaySequence->box.size, mode));
		// Len/mode button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(columnMK2, rowMK0), module, PhraseSeq32::RUNMODE_PARAM, mode));

		// Autostep
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(columnMK0 + 2, rowMK1 + 7), module, PhraseSeq32::AUTOSTEP_PARAM, mode, svgPanel));		
		// Sequence knob
		addParam(createDynamicParamCentered<SequenceKnob>(VecPx(columnMK1 + 1, rowMK0 + 55), module, PhraseSeq32::SEQUENCE_PARAM, mode));		
		// Transpose/rotate button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(columnMK2, rowMK1 + 4), module, PhraseSeq32::TRAN_ROT_PARAM, mode));
		
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(columnMK0 - 43, rowMK2 + 5), module, PhraseSeq32::RESET_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(columnMK0 - 43, rowMK2 + 5), module, PhraseSeq32::RESET_LIGHT));
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(columnMK0 + 3, rowMK2 + 5), module, PhraseSeq32::RUN_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(columnMK0 + 3, rowMK2 + 5), module, PhraseSeq32::RUN_LIGHT));
		// Copy/paste buttons
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(columnMK1 - 15, rowMK2 + 5), module, PhraseSeq32::COPY_PARAM, mode));
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(columnMK1 + 15, rowMK2 + 5), module, PhraseSeq32::PASTE_PARAM, mode));
		// Copy-paste mode switch (3 position)
		addParam(createDynamicSwitchCentered<IMSwitch3VInv>(VecPx(columnMK2 + 1, rowMK2 - 3), module, PhraseSeq32::CPMODE_PARAM, mode, svgPanel));	// 0.0f is top position

		
		
		// ****** Gate and slide section ******
		
		static const int rowMB0 = 230;
		static const int columnMBspacing = 70;
		static const int columnMB2 = 142;// Gate2
		static const int columnMB1 = columnMB2 - columnMBspacing;// Gate1 
		static const int columnMB3 = columnMB2 + columnMBspacing;// Tie
		static const int ledVsButtonDX = 27;
		
		
		
		// Gate 1 light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(columnMB1 + ledVsButtonDX, rowMB0), module, PhraseSeq32::GATE1_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(columnMB1, rowMB0), module, PhraseSeq32::GATE1_PARAM, mode));
		// Gate 2 light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(columnMB2 + ledVsButtonDX, rowMB0), module, PhraseSeq32::GATE2_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(columnMB2, rowMB0), module, PhraseSeq32::GATE2_PARAM, mode));
		// Tie light and button
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(columnMB3 + ledVsButtonDX, rowMB0), module, PhraseSeq32::TIE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(columnMB3, rowMB0), module, PhraseSeq32::TIE_PARAM, mode));

						
		
		// ****** Bottom two rows ******
		
		static const int inputJackSpacingX = 54;
		static const int outputJackSpacingX = 45;
		static const int rowB0 = 335;
		static const int rowB1 = 281;
		static const int columnB0 = 34;
		static const int columnB1 = columnB0 + inputJackSpacingX;
		static const int columnB2 = columnB1 + inputJackSpacingX;
		static const int columnB3 = columnB2 + inputJackSpacingX;
		static const int columnB4 = columnB3 + inputJackSpacingX;
		static const int columnB8 = columnMK2 + 1;
		static const int columnB7 = columnB8 - outputJackSpacingX;
		static const int columnB6 = columnB7 - outputJackSpacingX;
		static const int columnB5 = columnB6 - outputJackSpacingX - 4;// clock and reset

		
		// Gate 1 probability light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(columnB0 + ledVsButtonDX, rowB1), module, PhraseSeq32::GATE1_PROB_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(columnB0, rowB1), module, PhraseSeq32::GATE1_PROB_PARAM, mode));
		// Gate 1 probability knob
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(columnB1, rowB1), module, PhraseSeq32::GATE1_KNOB_PARAM, mode));
		// Slide light and button
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(columnB2 + ledVsButtonDX, rowB1), module, PhraseSeq32::SLIDE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(columnB2, rowB1), module, PhraseSeq32::SLIDE_BTN_PARAM, mode));
		// Slide knob
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(columnB3, rowB1), module, PhraseSeq32::SLIDE_KNOB_PARAM, mode));
		// CV in
		addInput(createDynamicPortCentered<IMPort>(VecPx(columnB4, rowB1), true, module, PhraseSeq32::CV_INPUT, mode));
		// Clock input
		addInput(createDynamicPortCentered<IMPort>(VecPx(columnB5, rowB1), true, module, PhraseSeq32::CLOCK_INPUT, mode));
		// Channel A outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(columnB6, rowB1), false, module, PhraseSeq32::CVA_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(columnB7, rowB1), false, module, PhraseSeq32::GATE1A_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(columnB8, rowB1), false, module, PhraseSeq32::GATE2A_OUTPUT, mode));


		// CV control Inputs 
		addInput(createDynamicPortCentered<IMPort>(VecPx(columnB0, rowB0), true, module, PhraseSeq32::LEFTCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(columnB1, rowB0), true, module, PhraseSeq32::RIGHTCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(columnB2, rowB0), true, module, PhraseSeq32::SEQCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(columnB3, rowB0), true, module, PhraseSeq32::RUNCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(columnB4, rowB0), true, module, PhraseSeq32::WRITE_INPUT, mode));
		// Reset input
		addInput(createDynamicPortCentered<IMPort>(VecPx(columnB5, rowB0), true, module, PhraseSeq32::RESET_INPUT, mode));
		// Channel B outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(columnB6, rowB0), false, module, PhraseSeq32::CVB_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(columnB7, rowB0), false, module, PhraseSeq32::GATE1B_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(columnB8, rowB0), false, module, PhraseSeq32::GATE2B_OUTPUT, mode)); 
	}
};

Model *modelPhraseSeq32 = createModel<PhraseSeq32, PhraseSeq32Widget>("Phrase-Seq-32");
