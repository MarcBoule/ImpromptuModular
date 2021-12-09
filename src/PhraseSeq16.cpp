//***********************************************************************************************
//Multi-phrase 16 step sequencer module for VCV Rack by Marc Boulé
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


struct PhraseSeq16 : Module {
	enum ParamIds {
		KEYNOTE_PARAM,
		KEYGATE_PARAM,
		KEYGATE2_PARAM,// no longer unused
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
		ROTATEL_PARAM,// no longer used
		ROTATER_PARAM,// no longer used
		PASTESYNC_PARAM,// no longer used
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),// no longer used
		TRANSPOSEU_PARAM,// no longer used
		TRANSPOSED_PARAM,// no longer used
		RUNMODE_PARAM,
		TRAN_ROT_PARAM,
		ROTATE_PARAM,//no longer used
		GATE1_KNOB_PARAM,
		GATE2_KNOB_PARAM,// no longer used
		GATE1_PROB_PARAM,
		TIE_PARAM,// Legato
		CPMODE_PARAM,
		ENUMS(STEP_PHRASE_PARAMS, 16),
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
		CV_OUTPUT,
		GATE1_OUTPUT,
		GATE2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 16 * 3),// room for GreenRedWhite
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12 * 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE1_LIGHT, 2),// room for GreenRed
		ENUMS(GATE2_LIGHT, 2),// room for GreenRed
		SLIDE_LIGHT,
		ATTACH_LIGHT,
		PENDING_LIGHT,// no longer used
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
	int seqCVmethod;// 0 is 0-10V, 1 is C4-D5#, 2 is TrigIncr
	int pulsesPerStep;// 1 means normal gate mode, alt choices are 4, 6, 12, 24 PPS (Pulses per step)
	bool running;
	int runModeSong;
	int stepIndexEdit;
	int seqIndexEdit;
	int phraseIndexEdit;
	int phrases;//1 to 16
	SeqAttributes sequences[16];
	int phrase[16];// This is the song (series of phases; a phrase is a patten number)
	float cv[16][16];// [-3.0 : 3.917]. First index is patten number, 2nd index is step
	StepAttributes attributes[16][16];// First index is patten number, 2nd index is step (see enum AttributeBitMasks for details)
	bool resetOnRun;
	bool attached;
	bool stopAtEndOfSong;

	// No need to save, with reset
	int displayState;
	float cvCPbuffer[16];// copy paste buffer for CVs
	StepAttributes attribCPbuffer[16];
	int phraseCPbuffer[16];
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
	long clockIgnoreOnReset;
	int phraseIndexRun;
	unsigned long phraseIndexRunHistory;
	int stepIndexRun;
	unsigned long stepIndexRunHistory;
	int ppqnCount;
	int gate1Code;
	int gate2Code;
	bool lastProbGate1Enable;
	unsigned long slideStepsRemain;// 0 when no slide under way, downward step counter when sliding


	// No need to save, no reset
	RefreshCounter refresh;
	float slideCVdelta;// no need to initialize, this goes with slideStepsRemain
	float editingGateCV;// no need to initialize, this goes with editingGate (output this only when editingGate > 0)
	int editingGateKeyLight;// no need to initialize, this goes with editingGate (use this only when editingGate > 0)
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
	Trigger stepTriggers[16];
	Trigger keyNoteTrigger;
	Trigger keyGateTrigger;
	Trigger seqCVTrigger;
	HoldDetect modeHoldDetect;
	PianoKeyInfo pkInfo;

	
	inline bool isEditingSequence(void) {return params[EDIT_PARAM].getValue() > 0.5f;}
	
	
	PhraseSeq16() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		// must init those that have no-connect info to non-connected, or else mother may read 0.0 init value if ever refresh limiters make it such that after a connection of expander the mother reads before the first pass through the expander's writing code, and this may do something undesired (ex: change track in Foundry on expander connected while track CV jack is empty)
		rightMessages[1][4] = std::numeric_limits<float>::quiet_NaN();

		char strBuf[32];
		for (int x = 0; x < 16; x++) {
			snprintf(strBuf, 32, "Step/phrase %i", x + 1);
			configParam(STEP_PHRASE_PARAMS + x, 0.0f, 1.0f, 0.0f, strBuf);
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

		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE1_OUTPUT, "Gate 1");
		configOutput(GATE2_OUTPUT, "Gate 2");

		onReset();
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}
	

	void onReset() override {
		autoseq = false;
		autostepLen = false;
		holdTiedNotes = true;
		seqCVmethod = 0;
		pulsesPerStep = 1;
		running = true;
		runModeSong = MODE_FWD;
		stepIndexEdit = 0;
		seqIndexEdit = 0;
		phraseIndexEdit = 0;
		phrases = 4;
		for (int i = 0; i < 16; i++) {
			sequences[i].init(16, MODE_FWD);
			phrase[i] = 0;
			for (int s = 0; s < 16; s++) {
				cv[i][s] = 0.0f;
				attributes[i][s].init();
			}
		}
		resetOnRun = false;
		attached = false;
		stopAtEndOfSong = false;
		resetNonJson();
	}
	void resetNonJson() {
		displayState = DISP_NORMAL;
		for (int i = 0; i < 16; i++) {
			cvCPbuffer[i] = 0.0f;
			attribCPbuffer[i].init();
			phraseCPbuffer[i] = 0;
		}
		seqAttribCPbuffer.init(16, MODE_FWD);
		seqCopied = true;
		countCP = 16;
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
		initRun();
	}
	void initRun() {// run button activated or run edge in run input jack
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * APP->engine->getSampleRate());
		phraseIndexRun = (runModeSong == MODE_REV ? phrases - 1 : 0);
		phraseIndexRunHistory = 0;

		int seq = (isEditingSequence() ? seqIndexEdit : phrase[phraseIndexRun]);
		stepIndexRun = (sequences[seq].getRunMode() == MODE_REV ? sequences[seq].getLength() - 1 : 0);
		stepIndexRunHistory = 0;

		ppqnCount = 0;
		lastProbGate1Enable = true;
		calcGate1Code(attributes[seq][stepIndexRun]);
		gate2Code = calcGate2Code(attributes[seq][stepIndexRun], 0, pulsesPerStep);
		slideStepsRemain = 0ul;
	}
	
	
	void onRandomize() override {
		if (isEditingSequence()) {
			for (int s = 0; s < 16; s++) {
				cv[seqIndexEdit][s] = ((float)(random::u32() % 5)) + ((float)(random::u32() % 12)) / 12.0f - 2.0f;
				attributes[seqIndexEdit][s].randomize();
			}
			sequences[seqIndexEdit].randomize(16, NUM_MODES - 1);
		}
	}
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// autoseq
		json_object_set_new(rootJ, "autoseq", json_boolean(autoseq));
		
		// autostepLen
		json_object_set_new(rootJ, "autostepLen", json_boolean(autostepLen));
		
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

		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));
	
		// seqIndexEdit
		json_object_set_new(rootJ, "sequence", json_integer(seqIndexEdit));

		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// sequences
		json_t *sequencesJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(sequencesJ, i, json_integer(sequences[i].getSeqAttrib()));
		json_object_set_new(rootJ, "sequences", sequencesJ);
		
		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 16; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase", phraseJ);

		// CV
		json_t *cvJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(cvJ, s + (i * 16), json_real(cv[i][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// attributes
		json_t *attributesJ = json_array();
		for (int i = 0; i < 16; i++)
			for (int s = 0; s < 16; s++) {
				json_array_insert_new(attributesJ, s + (i * 16), json_integer(attributes[i][s].getAttribute()));
			}
		json_object_set_new(rootJ, "attributes", attributesJ);

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// attached
		json_object_set_new(rootJ, "attached", json_boolean(attached));

		// stopAtEndOfSong
		json_object_set_new(rootJ, "stopAtEndOfSong", json_boolean(stopAtEndOfSong));

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

		// autoseq
		json_t *autoseqJ = json_object_get(rootJ, "autoseq");
		if (autoseqJ)
			autoseq = json_is_true(autoseqJ);

		// autostepLen
		json_t *autostepLenJ = json_object_get(rootJ, "autostepLen");
		if (autostepLenJ)
			autostepLen = json_is_true(autostepLenJ);

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

		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, "runModeSong3");
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
		else {// legacy
			runModeSongJ = json_object_get(rootJ, "runModeSong");
			if (runModeSongJ) {
				runModeSong = json_integer_value(runModeSongJ);
				if (runModeSong >= MODE_PEN)// this mode was not present in original version
					runModeSong++;
			}
		}
		
		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// seqIndexEdit
		json_t *seqIndexEditJ = json_object_get(rootJ, "sequence");
		if (seqIndexEditJ)
			seqIndexEdit = json_integer_value(seqIndexEditJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// sequences
		json_t *sequencesJ = json_object_get(rootJ, "sequences");
		if (sequencesJ) {
			for (int i = 0; i < 16; i++)
			{
				json_t *sequencesArrayJ = json_array_get(sequencesJ, i);
				if (sequencesArrayJ)
					sequences[i].setSeqAttrib(json_integer_value(sequencesArrayJ));
			}			
		}
		else {// legacy
			int lengths[16] = {};//1 to 16
			int runModeSeq[16] = {}; 
			int transposeOffsets[16] = {};

			// runModeSeq
			json_t *runModeSeqJ = json_object_get(rootJ, "runModeSeq3");
			if (runModeSeqJ) {
				for (int i = 0; i < 16; i++)
				{
					json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
					if (runModeSeqArrayJ)
						runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
				}			
			}		
			else {// legacy
				runModeSeqJ = json_object_get(rootJ, "runModeSeq2");
				if (runModeSeqJ) {
					for (int i = 0; i < 16; i++)
					{
						json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
						if (runModeSeqArrayJ) {
							runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
							if (runModeSeq[i] >= MODE_PEN)// this mode was not present in version runModeSeq2
								runModeSeq[i]++;
						}
					}			
				}		
				else {// legacy
					runModeSeqJ = json_object_get(rootJ, "runModeSeq");
					if (runModeSeqJ) {
						runModeSeq[0] = json_integer_value(runModeSeqJ);
						if (runModeSeq[0] >= MODE_PEN)// this mode was not present in original version
							runModeSeq[0]++;
					}
					for (int i = 1; i < 16; i++)// there was only one global runModeSeq in original version
						runModeSeq[i] = runModeSeq[0];
				}
			}
			
			// lengths
			json_t *lengthsJ = json_object_get(rootJ, "lengths");
			if (lengthsJ) {
				for (int i = 0; i < 16; i++)
				{
					json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
					if (lengthsArrayJ)
						lengths[i] = json_integer_value(lengthsArrayJ);
				}			
			}
			else {// legacy
				json_t *stepsJ = json_object_get(rootJ, "steps");
				if (stepsJ) {
					int steps = json_integer_value(stepsJ);
					for (int i = 0; i < 16; i++)
						lengths[i] = steps;
				}
			}
			
			// transposeOffsets
			json_t *transposeOffsetsJ = json_object_get(rootJ, "transposeOffsets");
			if (transposeOffsetsJ) {
				for (int i = 0; i < 16; i++)
				{
					json_t *transposeOffsetsArrayJ = json_array_get(transposeOffsetsJ, i);
					if (transposeOffsetsArrayJ)
						transposeOffsets[i] = json_integer_value(transposeOffsetsArrayJ);
				}			
			}
			
			// now write into new object
			for (int i = 0; i < 16; i++) {
				sequences[i].init(lengths[i], runModeSeq[i]);
				sequences[i].setTranspose(transposeOffsets[i]);
			}
		}
		
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase");
		if (phraseJ)
			for (int i = 0; i < 16; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
			
		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (i * 16));
					if (cvArrayJ)
						cv[i][s] = json_number_value(cvArrayJ);
				}
		}

		// attributes
		json_t *attributesJ = json_object_get(rootJ, "attributes");
		if (attributesJ) {
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++) {
					json_t *attributesArrayJ = json_array_get(attributesJ, s + (i * 16));
					if (attributesArrayJ)
						attributes[i][s].setAttribute((unsigned short)json_integer_value(attributesArrayJ));
				}
		}
		else {// legacy
			for (int i = 0; i < 16; i++)
				for (int s = 0; s < 16; s++)
					attributes[i][s].setAttribute(0u);
			// gate1
			json_t *gate1J = json_object_get(rootJ, "gate1");
			if (gate1J) {
				for (int i = 0; i < 16; i++)
					for (int s = 0; s < 16; s++) {
						json_t *gate1arrayJ = json_array_get(gate1J, s + (i * 16));
						if (gate1arrayJ)
							if (!!json_integer_value(gate1arrayJ)) attributes[i][s].setGate1(true);
					}
			}
			// gate1Prob
			json_t *gate1ProbJ = json_object_get(rootJ, "gate1Prob");
			if (gate1ProbJ) {
				for (int i = 0; i < 16; i++)
					for (int s = 0; s < 16; s++) {
						json_t *gate1ProbarrayJ = json_array_get(gate1ProbJ, s + (i * 16));
						if (gate1ProbarrayJ)
							if (!!json_integer_value(gate1ProbarrayJ)) attributes[i][s].setGate1P(true);
					}
			}
			// gate2
			json_t *gate2J = json_object_get(rootJ, "gate2");
			if (gate2J) {
				for (int i = 0; i < 16; i++)
					for (int s = 0; s < 16; s++) {
						json_t *gate2arrayJ = json_array_get(gate2J, s + (i * 16));
						if (gate2arrayJ)
							if (!!json_integer_value(gate2arrayJ)) attributes[i][s].setGate2(true);
					}
			}
			// slide
			json_t *slideJ = json_object_get(rootJ, "slide");
			if (slideJ) {
				for (int i = 0; i < 16; i++)
					for (int s = 0; s < 16; s++) {
						json_t *slideArrayJ = json_array_get(slideJ, s + (i * 16));
						if (slideArrayJ)
							if (!!json_integer_value(slideArrayJ)) attributes[i][s].setSlide(true);
					}
			}
			// tied
			json_t *tiedJ = json_object_get(rootJ, "tied");
			if (tiedJ) {
				for (int i = 0; i < 16; i++)
					for (int s = 0; s < 16; s++) {
						json_t *tiedArrayJ = json_array_get(tiedJ, s + (i * 16));
						if (tiedArrayJ)
							if (!!json_integer_value(tiedArrayJ)) attributes[i][s].setTied(true);
					}
			}
		}
	
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);
		
		// attached
		json_t *attachedJ = json_object_get(rootJ, "attached");
		if (attachedJ)
			attached = json_is_true(attachedJ);
		
		// stopAtEndOfSong
		json_t *stopAtEndOfSongJ = json_object_get(rootJ, "stopAtEndOfSong");
		if (stopAtEndOfSongJ)
			stopAtEndOfSong = json_is_true(stopAtEndOfSongJ);
		
		resetNonJson();
	}


	IoStep* fillIoSteps(int *seqLenPtr) {// caller must delete return array
		int seqLen = sequences[seqIndexEdit].getLength();
		IoStep* ioSteps = new IoStep[seqLen];
		
		// populate ioSteps array
		for (int i = 0; i < seqLen; i++) {
			ioSteps[i].pitch = cv[seqIndexEdit][i];
			StepAttributes stepAttrib = attributes[seqIndexEdit][i];
			ioSteps[i].gate = stepAttrib.getGate1();
			ioSteps[i].tied = stepAttrib.getTied();
			ioSteps[i].vel = -1.0f;// no concept of velocity in PhraseSequencers
			ioSteps[i].prob = stepAttrib.getGate1P() ? params[GATE1_KNOB_PARAM].getValue() : -1.0f;// negative means prob is not on for this note
		}
		
		// return values 
		*seqLenPtr = seqLen;
		return ioSteps;
	}
	
	
	void emptyIoSteps(IoStep* ioSteps, int seqLen) {
		sequences[seqIndexEdit].setLength(seqLen);
		
		// populate steps in the sequencer
		// first pass is done without ties
		for (int i = 0; i < seqLen; i++) {
			cv[seqIndexEdit][i] = ioSteps[i].pitch;
			
 			StepAttributes stepAttrib;
			stepAttrib.init();
			stepAttrib.setGate1(ioSteps[i].gate);
			stepAttrib.setGate1P(ioSteps[i].prob >= 0.0f);
			attributes[seqIndexEdit][i] = stepAttrib;
		}
		// now do ties, has to be done in a separate pass such that non tied that follows tied can be 
		//   there in advance for proper gate types
		for (int i = 0; i < seqLen; i++) {
			if (ioSteps[i].tied) {
				activateTiedStep(seqIndexEdit, i);
			}
		}
	}
	
	
	void rotateSeq(int seqNum, bool directionRight, int seqLength) {
		float rotCV;
		StepAttributes rotAttributes;
		int iStart = 0;
		int iEnd = seqLength - 1;
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
	
	
	void calcGate1Code(StepAttributes attribute) {
		int gateType = attribute.getGate1Mode();
		
		if (ppqnCount == 0 && !attribute.getTied()) {
			lastProbGate1Enable = !attribute.getGate1P() || (random::uniform() < params[GATE1_KNOB_PARAM].getValue()); // random::uniform is [0.0, 1.0), see include/util/common.hpp
		}
			
		if (!attribute.getGate1() || !lastProbGate1Enable) {
			gate1Code = 0;
		}
		else if (pulsesPerStep == 1 && gateType == 0) {
			gate1Code = 2;// clock high
		}
		else { 
			if (gateType == 11) {
				gate1Code = (ppqnCount == 0 ? 3 : 0);
			}
			else {
				gate1Code = getAdvGate(ppqnCount, pulsesPerStep, gateType);
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
			// Seq CV input
			if (inputs[SEQCV_INPUT].isConnected()) {
				if (seqCVmethod == 0) {// 0-10 V
					int newSeq = (int)( inputs[SEQCV_INPUT].getVoltage() * (16.0f - 1.0f) / 10.0f + 0.5f );
					seqIndexEdit = clamp(newSeq, 0, 16 - 1);
				}
				else if (seqCVmethod == 1) {// C4-D5#
					int newSeq = (int)std::round(inputs[SEQCV_INPUT].getVoltage() * 12.0f);
					seqIndexEdit = clamp(newSeq, 0, 16 - 1);
				}
				else {// TrigIncr
					if (seqCVTrigger.process(inputs[SEQCV_INPUT].getVoltage()))
						seqIndexEdit = clamp(seqIndexEdit + 1, 0, 16 - 1);
				}	
			}
			
			// Mode CV input
			if (expanderPresent && editingSequence) {
				float modeCVin = messagesFromExpander[4];
				if (!std::isnan(modeCVin))
					sequences[seqIndexEdit].setRunMode((int) clamp( std::round(modeCVin * ((float)NUM_MODES - 1.0f - 1.0f) / 10.0f), 0.0f, (float)NUM_MODES - 1.0f - 1.0f ));
			}
			
			// Attach button
			if (attachedTrigger.process(params[ATTACH_PARAM].getValue())) {
				attached = !attached;	
				displayState = DISP_NORMAL;			
			}
			if (running && attached) {
				if (editingSequence)
					stepIndexEdit = stepIndexRun;
				else
					phraseIndexEdit = phraseIndexRun;
			}
			
			// Copy button
			if (copyTrigger.process(params[COPY_PARAM].getValue())) {
				if (!attached) {
					startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
					countCP = 16;
					if (params[CPMODE_PARAM].getValue() > 1.5f)// all
						startCP = 0;
					else if (params[CPMODE_PARAM].getValue() < 0.5f)// 4
						countCP = std::min(4, 16 - startCP);
					else// 8
						countCP = std::min(8, 16 - startCP);
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
						countCP = std::min(countCP, 16 - startCP);
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
							}
						}
						else {// crossed paste to seq (seq vs song)
							if (params[CPMODE_PARAM].getValue() > 1.5f) { // ALL (init steps)
								for (int s = 0; s < 16; s++) {
									//cv[seqIndexEdit][s] = 0.0f;
									//attributes[seqIndexEdit][s].init();
									attributes[seqIndexEdit][s].toggleGate1();
								}
								sequences[seqIndexEdit].setTranspose(0);
								sequences[seqIndexEdit].setRotate(0);
							}
							else if (params[CPMODE_PARAM].getValue() < 0.5f) {// 4 (randomize CVs)
								for (int s = 0; s < 16; s++)
									cv[seqIndexEdit][s] = ((float)(random::u32() % 7)) + ((float)(random::u32() % 12)) / 12.0f - 3.0f;
								sequences[seqIndexEdit].setTranspose(0);
								sequences[seqIndexEdit].setRotate(0);
							}
							else {// 8 (randomize gate 1)
								for (int s = 0; s < 16; s++)
									if ( (random::u32() & 0x1) != 0)
										attributes[seqIndexEdit][s].toggleGate1();
							}
							startCP = 0;
							countCP = 16;
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
								for (int p = 0; p < 16; p++)
									phrase[p] = 0;
							}
							else if (params[CPMODE_PARAM].getValue() < 0.5f) {// 4 (phrases increase from 1 to 16)
								for (int p = 0; p < 16; p++)
									phrase[p] = p;						
							}
							else {// 8 (randomize phrases)
								for (int p = 0; p < 16; p++)
									phrase[p] = random::u32() % 16;
							}
							startCP = 0;
							countCP = 16;
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
					// Autostep (after grab all active inputs)
					if (params[AUTOSTEP_PARAM].getValue() > 0.5f) {
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, autostepLen ? sequences[seqIndexEdit].getLength() : 16);
						if (stepIndexEdit == 0 && autoseq && !inputs[SEQCV_INPUT].isConnected())
							seqIndexEdit = moveIndex(seqIndexEdit, seqIndexEdit + 1, 16);
					}
				}
				displayState = DISP_NORMAL;
			}
			// Left and Right CV inputs
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
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, 16);
						if (!attributes[seqIndexEdit][stepIndexEdit].getTied()) {// play if non-tied step
							if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
								editingGate = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
								editingGateCV = cv[seqIndexEdit][stepIndexEdit];
								editingGateKeyLight = -1;
							}
						}
					}
					else {
						phraseIndexEdit = moveIndex(phraseIndexEdit, phraseIndexEdit + delta, 16);
						if (!running)
							phraseIndexRun = phraseIndexEdit;
					}
				}
			}
			
			// Step button presses
			int stepPressed = -1;
			for (int i = 0; i < 16; i++) {
				if (stepTriggers[i].process(params[STEP_PHRASE_PARAMS + i].getValue()))
					stepPressed = i;
			}
			if (stepPressed != -1) {
				if (displayState == DISP_LENGTH) {
					if (editingSequence)
						sequences[seqIndexEdit].setLength(stepPressed + 1);
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
								sequences[seqIndexEdit].setRunMode(clamp(sequences[seqIndexEdit].getRunMode() + deltaKnob, 0, (NUM_MODES - 1 - 1)));
							}
						}
						else {
							runModeSong = clamp(runModeSong + deltaKnob, 0, 6 - 1);
						}
					}
					else if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							sequences[seqIndexEdit].setLength(clamp(sequences[seqIndexEdit].getLength() + deltaKnob, 1, 16));
						}
						else {
							phrases = clamp(phrases + deltaKnob, 1, 16);
						}
					}
					else if (displayState == DISP_TRANSPOSE) {
						if (editingSequence) {
							sequences[seqIndexEdit].setTranspose(clamp(sequences[seqIndexEdit].getTranspose() + deltaKnob, -99, 99));
							float transposeOffsetCV = ((float)(deltaKnob))/12.0f;// Tranpose by deltaKnob number of semi-tones
							for (int s = 0; s < 16; s++) {
								cv[seqIndexEdit][s] += transposeOffsetCV;
							}
						}
					}
					else if (displayState == DISP_ROTATE) {
						if (editingSequence) {
							int slength = sequences[seqIndexEdit].getLength();
							sequences[seqIndexEdit].setRotate(clamp(sequences[seqIndexEdit].getRotate() + deltaKnob, -99, 99));
							if (deltaKnob > 0 && deltaKnob < 201) {// Rotate right, 201 is safety
								for (int i = deltaKnob; i > 0; i--) {
									rotateSeq(seqIndexEdit, true, slength);
									if (stepIndexEdit < slength)
										stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, slength);
								}
							}
							if (deltaKnob < 0 && deltaKnob > -201) {// Rotate left, 201 is safety
								for (int i = deltaKnob; i < 0; i++) {
									rotateSeq(seqIndexEdit, false, slength);
									if (stepIndexEdit < slength)
										stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit - 1, slength);
								}
							}
						}						
					}
					else {// DISP_NORMAL
						if (editingSequence) {
							if (!inputs[SEQCV_INPUT].isConnected()) {
								seqIndexEdit = clamp(seqIndexEdit + deltaKnob, 0, 16 - 1);
							}
						}
						else {
							if (!attached || (attached && !running))
								phrase[phraseIndexEdit] = clamp(phrase[phraseIndexEdit] + deltaKnob, 0, 16 - 1);
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
								stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 16);
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
							stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 16);
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
						if (pkInfo.isRightClick) {
							stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 16);
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
					float slideFromCV = 0.0f;
					int oldStepIndexRun = stepIndexRun;
					if (editingSequence) {
						slideFromCV = cv[seqIndexEdit][stepIndexRun];
						//bool seqLoopOver = 
						moveIndexRunMode(&stepIndexRun, sequences[seqIndexEdit].getLength(), sequences[seqIndexEdit].getRunMode(), &stepIndexRunHistory);
						// if (seqLoopOver && stopAtEndOfSong) {
							// running = false;
							// stepIndexRun = oldStepIndexRun;
						// }
					}
					else {
						slideFromCV = cv[phrase[phraseIndexRun]][stepIndexRun];
						
						if (moveIndexRunMode(&stepIndexRun, sequences[phrase[phraseIndexRun]].getLength(), sequences[phrase[phraseIndexRun]].getRunMode(), &stepIndexRunHistory)) {
							int oldPhraseIndexRun = phraseIndexRun;
							bool songLoopOver = moveIndexRunMode(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
							// check for end of song if needed
							if (songLoopOver && stopAtEndOfSong) {
								running = false;
								stepIndexRun = oldStepIndexRun;
								phraseIndexRun = oldPhraseIndexRun;
							}
							else {
								stepIndexRun = (sequences[phrase[phraseIndexRun]].getRunMode() == MODE_REV ? sequences[phrase[phraseIndexRun]].getLength() - 1 : 0);// must always refresh after phraseIndexRun has changed
							}
						}
						newSeq = phrase[phraseIndexRun];
					}
					
					// Slide
					if (attributes[newSeq][stepIndexRun].getSlide()) {
						slideStepsRemain =   (unsigned long) (((float)clockPeriod * pulsesPerStep) * params[SLIDE_KNOB_PARAM].getValue() / 2.0f);
						if (slideStepsRemain != 0ul) {
							float slideToCV = cv[newSeq][stepIndexRun];
							slideCVdelta = (slideToCV - slideFromCV)/(float)slideStepsRemain;
						}
					}
					else
						slideStepsRemain = 0ul;
				}
				else {
					if (!editingSequence)
						newSeq = phrase[phraseIndexRun];
				}
				calcGate1Code(attributes[newSeq][stepIndexRun]);
				gate2Code = calcGate2Code(attributes[newSeq][stepIndexRun], ppqnCount, pulsesPerStep);
				clockPeriod = 0ul;				
			}
			clockPeriod++;
		}	
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue())) {
			initRun();// must be after sequence reset
			resetLight = 1.0f;
			displayState = DISP_NORMAL;
			clockTrigger.reset();
			if (inputs[SEQCV_INPUT].isConnected() && seqCVmethod == 2)
				seqIndexEdit = 0;
		}
		
		
		//********** Outputs and lights **********
				
		// CV and gates outputs
		int seq = editingSequence ? seqIndexEdit : phrase[phraseIndexRun];
		int step = (editingSequence && !running) ? stepIndexEdit : stepIndexRun;
		if (running) {
			bool muteGate1 = !editingSequence && ((params[GATE1_PARAM].getValue() + (expanderPresent ? messagesFromExpander[0] : 0.0f)) > 0.5f);// live mute
			bool muteGate2 = !editingSequence && ((params[GATE2_PARAM].getValue() + (expanderPresent ? messagesFromExpander[1] : 0.0f)) > 0.5f);// live mute
			float slideOffset = (slideStepsRemain > 0ul ? (slideCVdelta * (float)slideStepsRemain) : 0.0f);
			outputs[CV_OUTPUT].setVoltage(cv[seq][step] - slideOffset);
			bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
			outputs[GATE1_OUTPUT].setVoltage((calcGate(gate1Code, clockTrigger, clockPeriod, sampleRate) && !muteGate1 && !retriggingOnReset) ? 10.0f : 0.0f);
			outputs[GATE2_OUTPUT].setVoltage((calcGate(gate2Code, clockTrigger, clockPeriod, sampleRate) && !muteGate2 && !retriggingOnReset) ? 10.0f : 0.0f);
		}
		else {// not running
			outputs[CV_OUTPUT].setVoltage((editingGate > 0ul) ? editingGateCV : cv[seq][step]);
			outputs[GATE1_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
			outputs[GATE2_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
		}
		if (slideStepsRemain > 0ul)
			slideStepsRemain--;
		
		// lights
		if (refresh.processLights()) {
			// Step/phrase lights
			for (int i = 0; i < 16; i++) {
				float red = 0.0f;
				float green = 0.0f;
				float white = 0.0f;
				if (infoCopyPaste != 0l) {
					if (i >= startCP && i < (startCP + countCP))
						green = 0.71f;
				}
				else if (displayState == DISP_LENGTH) {
					if (editingSequence) {
						if (i < (sequences[seqIndexEdit].getLength() - 1))
							green = 0.32f;
						else if (i == (sequences[seqIndexEdit].getLength() - 1))
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
					red = (i == stepIndexEdit ? 1.0f : (i < sequences[seqIndexEdit].getLength() ? 0.45f : 0.0f));
				}
				else {// normal led display (i.e. not length)
					// Run cursor (green)
					if (editingSequence)
						green = ((running && (i == stepIndexRun)) ? 1.0f : 0.0f);
					else {
						green = ((running && (i == phraseIndexRun)) ? 1.0f : 0.0f);
						green += ((running && (i == stepIndexRun) && i != phraseIndexEdit) ? 0.42f : 0.0f);
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
			float cvVal = editingSequence ? cv[seqIndexEdit][stepIndexEdit] : cv[phrase[phraseIndexEdit]][stepIndexRun];
			int keyLightIndex;
			int octLightIndex;
			calcNoteAndOct(cvVal, &keyLightIndex, &octLightIndex);
			octLightIndex += 3;
			for (int i = 0; i < 7; i++) {
				if (!editingSequence && (!attached || !running))// no oct lights when song mode and either (detached [1] or stopped [2])
												// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
												// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
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
			
			// Keyboard lights
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
					if (!editingSequence && (!attached || !running))// no keyboard lights when song mode and either (detached [1] or stopped [2])
													// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
													// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
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
			
			// Gate1, Gate1Prob, Gate2, Slide and Tied lights
			if (!editingSequence && (!attached || !running)) {// no oct lights when song mode and either (detached [1] or stopped [2])
											// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
											// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
				setGateLight(false, GATE1_LIGHT);
				setGateLight(false, GATE2_LIGHT);
				setGreenRed(GATE1_PROB_LIGHT, 0.0f, 0.0f);
				lights[SLIDE_LIGHT].setBrightness(0.0f);
				lights[TIE_LIGHT].setBrightness(0.0f);
			}
			else {
				StepAttributes attributesVal = attributes[seqIndexEdit][stepIndexEdit];
				if (!editingSequence)
					attributesVal = attributes[phrase[phraseIndexEdit]][stepIndexRun];
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
		for (int i = stepn + 1; i < 16; i++) {
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
			for (int i = std::max(stepn, 1); i < 16 && attributes[seqn][i].getTied(); i++) {
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
			for (int i = stepn + 1; i < 16 && attributes[seqn][i].getTied(); i++)
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



struct PhraseSeq16Widget : ModuleWidget {
	struct SequenceDisplayWidget : TransparentWidget {
		PhraseSeq16 *module;
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
					if (module->displayState != PhraseSeq16::DISP_LENGTH)
						module->displayState = PhraseSeq16::DISP_NORMAL;
					if ((!module->running || !module->attached) && !module->isEditingSequence()) {
						module->phraseIndexEdit = moveIndex(module->phraseIndexEdit, module->phraseIndexEdit + 1, 16);
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
					else if (module->displayState == PhraseSeq16::DISP_MODE) {
					}
					else if (module->displayState == PhraseSeq16::DISP_LENGTH) {
						totalNum = clamp(totalNum, 1, 16);
						if (editingSequence)
							module->sequences[module->seqIndexEdit].setLength(totalNum);
						else
							module->phrases = totalNum;
					}
					else if (module->displayState == PhraseSeq16::DISP_TRANSPOSE) {
					}
					else if (module->displayState == PhraseSeq16::DISP_ROTATE) {
					}
					else {// DISP_NORMAL
						totalNum = clamp(totalNum, 1, 16);
						if (editingSequence) {
							if (!module->inputs[PhraseSeq16::SEQCV_INPUT].isConnected())
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
			if (num >= 0 && num < (NUM_MODES - 1))
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
							float cpMode = module->params[PhraseSeq16::CPMODE_PARAM].getValue();
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
					else if (module->displayState == PhraseSeq16::DISP_MODE) {
						if (editingSequence)
							runModeToStr(module->sequences[module->seqIndexEdit].getRunMode());
						else
							runModeToStr(module->runModeSong);
					}
					else if (module->displayState == PhraseSeq16::DISP_LENGTH) {
						if (editingSequence)
							snprintf(displayStr, 16, "L%2u", (unsigned) module->sequences[module->seqIndexEdit].getLength());
						else
							snprintf(displayStr, 16, "L%2u", (unsigned) module->phrases);
					}
					else if (module->displayState == PhraseSeq16::DISP_TRANSPOSE) {
						snprintf(displayStr, 16, "+%2u", (unsigned) abs(module->sequences[module->seqIndexEdit].getTranspose()));
						if (module->sequences[module->seqIndexEdit].getTranspose() < 0)
							displayStr[0] = '-';
					}
					else if (module->displayState == PhraseSeq16::DISP_ROTATE) {
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
			PhraseSeq16 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = module->fillIoSteps(&seqLen);
				interopCopySequence(seqLen, ioSteps);
				delete[] ioSteps;
			}
		};
		struct InteropPasteSeqItem : MenuItem {
			PhraseSeq16 *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = interopPasteSequence(16, &seqLen);
				if (ioSteps != nullptr) {
					module->emptyIoSteps(ioSteps, seqLen);
					delete[] ioSteps;
				}
			}
		};
		PhraseSeq16 *module;
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
		PhraseSeq16 *module = dynamic_cast<PhraseSeq16*>(this->module);
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
			menu->addChild(createCheckMenuItem("C4-D5#", "",
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
				PhraseSeq16* module = dynamic_cast<PhraseSeq16*>(paramQuantity->module);
				// same code structure below as in sequence knob in main step()
				if (module->editingPpqn != 0) {
					module->pulsesPerStep = 1;
					//editingPpqn = (long) (editGateLengthTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else if (module->displayState == PhraseSeq16::DISP_MODE) {
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
				else if (module->displayState == PhraseSeq16::DISP_LENGTH) {
					if (module->isEditingSequence()) {
						module->sequences[module->seqIndexEdit].setLength(16);
					}
					else {
						module->phrases = 4;
					}
				}
				else if (module->displayState == PhraseSeq16::DISP_TRANSPOSE) {
					// nothing
				}
				else if (module->displayState == PhraseSeq16::DISP_ROTATE) {
					// nothing			
				}
				else {// DISP_NORMAL
					if (module->isEditingSequence()) {
						if (!module->inputs[PhraseSeq16::SEQCV_INPUT].isConnected()) {
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
	
	
	PhraseSeq16Widget(PhraseSeq16 *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/PhraseSeq16.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		
		
		// ****** Top row ******
		
		static const float rowT0 = 48.6f;
		static const int colT0 = 27;// Step LED buttons
		static const int colT3 = 404;// Attach (also used to align rest of right side of module)

		// Step/Phrase LED buttons
		int posX = colT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		for (int x = 0; x < 16; x++) {
			// First row
			addParam(createParamCentered<LEDButton>(VecPx(posX, rowT0), module, PhraseSeq16::STEP_PHRASE_PARAMS + x));
			addChild(createLightCentered<MediumLight<GreenRedWhiteLightIM>>(VecPx(posX, rowT0), module, PhraseSeq16::STEP_PHRASE_LIGHTS + (x * 3)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// Attach button and light
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colT3, rowT0 + 7.4f), module, PhraseSeq16::ATTACH_PARAM, mode));
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(colT3 + 22, rowT0 + 7.4f), module, PhraseSeq16::ATTACH_LIGHT));		

		
		// Key mode LED buttons	
		static const float rowKM = 78.6f;
		addParam(createParamCentered<LEDButton>(VecPx(252, rowKM), module, PhraseSeq16::KEYNOTE_PARAM));
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(252, rowKM), module, PhraseSeq16::KEYNOTE_LIGHT));
		addParam(createParamCentered<LEDButton>(VecPx(140, rowKM), module, PhraseSeq16::KEYGATE_PARAM));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(140, rowKM), module, PhraseSeq16::KEYGATE_LIGHT));

		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const float octLightsIntY = 20.0f;
		for (int i = 0; i < 7; i++) {
			addParam(createParamCentered<LEDButton>(VecPx(27, 110.6f + i * octLightsIntY), module, PhraseSeq16::OCTAVE_PARAM + i));
			addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(27, 110.6f + i * octLightsIntY), module, PhraseSeq16::OCTAVE_LIGHTS + i));
		}
		
		// Keys and Key lights
		static const Vec keyboardPos = mm2px(Vec(18.222f, 33.303f));
		svgPanel->fb->addChild(new KeyboardSmall(keyboardPos, mode));
		
		static const Vec offsetLeds = Vec(PianoKeySmall::sizeX * 0.5f, PianoKeySmall::sizeY * 0.55f);
		for (int k = 0; k < 12; k++) {
			Vec keyPos = keyboardPos + mm2px(smaKeysPos[k]);
			addChild(createPianoKey<PianoKeySmall>(keyPos, k, module ? &module->pkInfo : NULL));
			addChild(createLightCentered<MediumLight<GreenRedLightIM>>(keyPos + offsetLeds, module, PhraseSeq16::KEY_LIGHTS + k * 2));
		}

		
		// ****** Right side control area ******

		static const int rowMK0 = 113;// Edit mode row
		static const int rowMK1 = rowMK0 + 56; // Run row
		static const int rowMK2 = rowMK1 + 54; // Reset row
		static const int colMK0 = 289;// Edit mode column
		static const int colMK1 = colMK0 + 59;// Display column
		static const int colMK2 = colT3 + 8;// Run mode column
		
		
		// Edit mode switch
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(colMK0, rowMK0), module, PhraseSeq16::EDIT_PARAM, mode, svgPanel));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.size = VecPx(55, 30);// 3 characters
		displaySequence->box.pos = VecPx(colMK1, rowMK0 + 4).minus(displaySequence->box.size.div(2));
		displaySequence->module = module;
		addChild(displaySequence);
		svgPanel->fb->addChild(new DisplayBackground(displaySequence->box.pos, displaySequence->box.size, mode));
		// Len/mode button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMK2, rowMK0), module, PhraseSeq16::RUNMODE_PARAM, mode));
		
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(colMK0, rowMK1 + 7), module, PhraseSeq16::RUN_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(colMK0, rowMK1 + 7), module, PhraseSeq16::RUN_LIGHT));
		// Sequence knob
		addParam(createDynamicParamCentered<SequenceKnob>(VecPx(colMK1 + 1, rowMK0 + 55), module, PhraseSeq16::SEQUENCE_PARAM, mode));		
		// Transpose/rotate button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMK2, rowMK1 + 4), module, PhraseSeq16::TRAN_ROT_PARAM, mode));
		
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(colMK0, rowMK2 + 5), module, PhraseSeq16::RESET_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(colMK0, rowMK2 + 5), module, PhraseSeq16::RESET_LIGHT));
		// Copy/paste buttons
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colMK1 - 15, rowMK2 + 5), module, PhraseSeq16::COPY_PARAM, mode));
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colMK1 + 15, rowMK2 + 5), module, PhraseSeq16::PASTE_PARAM, mode));
		// Copy-paste mode switch (3 position)
		addParam(createDynamicSwitchCentered<IMSwitch3VInv>(VecPx(colMK2 + 1, rowMK2 - 3), module, PhraseSeq16::CPMODE_PARAM, mode, svgPanel));	// 0.0f is top position

		
		
		// ****** Gate and slide section ******
		
		static const int rowMB0 = 230;
		static const int columnRulerMBspacing = 70;
		static const int colMB2 = 142;// Gate2
		static const int colMB1 = colMB2 - columnRulerMBspacing;// Gate1 
		static const int colMB3 = colMB2 + columnRulerMBspacing;// Tie
		static const int ledVsButtonDX = 27;
		
		// Gate 1 light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colMB1 + ledVsButtonDX, rowMB0), module, PhraseSeq16::GATE1_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMB1, rowMB0), module, PhraseSeq16::GATE1_PARAM, mode));
		// Gate 2 light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colMB2 + ledVsButtonDX, rowMB0), module, PhraseSeq16::GATE2_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMB2, rowMB0), module, PhraseSeq16::GATE2_PARAM, mode));
		// Tie light and button
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(colMB3 + ledVsButtonDX, rowMB0), module, PhraseSeq16::TIE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMB3, rowMB0), module, PhraseSeq16::TIE_PARAM, mode));

						
		
		// ****** Bottom two rows ******
		
		static const int outputJackSpacingX = 54;
		static const int rowB0 = 335;
		static const int rowB1 = 281;
		static const int colB0 = 34;
		static const int colB1 = colB0 + outputJackSpacingX;
		static const int colB2 = colB1 + outputJackSpacingX;
		static const int colB3 = colB2 + outputJackSpacingX;
		static const int colB4 = colB3 + outputJackSpacingX;
		static const int colB7 = colMK2 + 1;
		static const int colB6 = colB7 - outputJackSpacingX;
		static const int colB5 = colB6 - outputJackSpacingX;
		

		
		// Gate 1 probability light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colB0 + ledVsButtonDX, rowB1), module, PhraseSeq16::GATE1_PROB_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colB0, rowB1), module, PhraseSeq16::GATE1_PROB_PARAM, mode));
		// Gate 1 probability knob
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colB1, rowB1), module, PhraseSeq16::GATE1_KNOB_PARAM, mode));
		// Slide light and button
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(colB2 + ledVsButtonDX, rowB1), module, PhraseSeq16::SLIDE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colB2, rowB1), module, PhraseSeq16::SLIDE_BTN_PARAM, mode));
		// Slide knob
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colB3, rowB1), module, PhraseSeq16::SLIDE_KNOB_PARAM, mode));
		// Autostep
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(colB4, rowB1), module, PhraseSeq16::AUTOSTEP_PARAM, mode, svgPanel));		
		// CV in
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB5, rowB1), true, module, PhraseSeq16::CV_INPUT, mode));
		// Clock
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB6, rowB1), true, module, PhraseSeq16::CLOCK_INPUT, mode));
		// Reset
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB7, rowB1), true, module, PhraseSeq16::RESET_INPUT, mode));

		

		// ****** Bottom row (all aligned) ******

	
		// CV control Inputs 
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB0, rowB0), true, module, PhraseSeq16::LEFTCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB1, rowB0), true, module, PhraseSeq16::RIGHTCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB2, rowB0), true, module, PhraseSeq16::SEQCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB3, rowB0), true, module, PhraseSeq16::RUNCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB4, rowB0), true, module, PhraseSeq16::WRITE_INPUT, mode));
		// Outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colB5, rowB0), false, module, PhraseSeq16::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colB6, rowB0), false, module, PhraseSeq16::GATE1_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colB7, rowB0), false, module, PhraseSeq16::GATE2_OUTPUT, mode));
	}
};

Model *modelPhraseSeq16 = createModel<PhraseSeq16, PhraseSeq16Widget>("Phrase-Seq-16");
