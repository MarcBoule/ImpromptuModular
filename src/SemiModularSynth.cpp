//***********************************************************************************************
//Semi-Modular Synthesizer module for VCV Rack by Marc Boulé and Xavier Belmont
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept, desing and layout by Xavier Belmont
//Code by Marc Boulé
//
//Acknowledgements: please see README.md
//***********************************************************************************************


#include "FundamentalUtil.hpp"
#include "PhraseSeqUtil.hpp"
#include "comp/PianoKey.hpp"


struct SemiModularSynth : Module {
	enum ParamIds {
		// SEQUENCER
		KEYNOTE_PARAM,// 0.6.12 replaces unused
		KEYGATE_PARAM,// 0.6.12 replaces unused
		KEYGATE2_PARAM,// no longer used
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
		
		// VCO
		VCO_MODE_PARAM,
		VCO_OCT_PARAM,
		VCO_FREQ_PARAM,
		VCO_FINE_PARAM,
		VCO_FM_PARAM,
		VCO_PW_PARAM,
		VCO_PWM_PARAM,

		// CLK
		CLK_FREQ_PARAM,
		CLK_PW_PARAM,

		// VCA
		VCA_LEVEL1_PARAM,
		
		// ADSR
		ADSR_ATTACK_PARAM,
		ADSR_DECAY_PARAM,
		ADSR_SUSTAIN_PARAM,
		ADSR_RELEASE_PARAM,
		
		// VCF
		VCF_FREQ_PARAM,
		VCF_RES_PARAM,
		VCF_FREQ_CV_PARAM,
		VCF_DRIVE_PARAM,
		
		// LFO
		LFO_FREQ_PARAM,
		LFO_GAIN_PARAM,
		LFO_OFFSET_PARAM,
		
		// ALL NEW
		ENUMS(STEP_PHRASE_PARAMS, 16),
				
		NUM_PARAMS
	};
	enum InputIds {
		// SEQUENCER
		WRITE_INPUT,
		CV_INPUT,
		RESET_INPUT,
		CLOCK_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		
		// VCO
		VCO_PITCH_INPUT,
		VCO_FM_INPUT,
		VCO_SYNC_INPUT,
		VCO_PW_INPUT,

		// CLK
		// none
		
		// VCA
		VCA_LIN1_INPUT,
		VCA_IN1_INPUT,
		
		// ADSR
		ADSR_GATE_INPUT,

		// VCF
		VCF_FREQ_INPUT,
		VCF_RES_INPUT,
		VCF_DRIVE_INPUT,
		VCF_IN_INPUT,
		
		// LFO
		LFO_RESET_INPUT,
		
		NUM_INPUTS
	};
	enum OutputIds {
		// SEQUENCER
		CV_OUTPUT,
		GATE1_OUTPUT,
		GATE2_OUTPUT,
		
		// VCO
		VCO_SIN_OUTPUT,
		VCO_TRI_OUTPUT,
		VCO_SAW_OUTPUT,
		VCO_SQR_OUTPUT,

		// CLK
		CLK_OUT_OUTPUT,
		
		// VCA
		VCA_OUT1_OUTPUT,
		
		// ADSR
		ADSR_ENVELOPE_OUTPUT,
		
		// VCF
		VCF_LPF_OUTPUT,
		VCF_HPF_OUTPUT,
		
		// LFO
		LFO_SIN_OUTPUT,
		LFO_TRI_OUTPUT,
		
		NUM_OUTPUTS
	};
	enum LightIds {
		// SEQUENCER
		ENUMS(STEP_PHRASE_LIGHTS, 16 * 3),// room for GreenRedWhite
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
		
		// VCO, CLK, VCA
		// none
		
		NUM_LIGHTS
	};

	
	// SEQUENCER
	
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
	SeqAttributes sequences[16];
	int runModeSong; 
	int seqIndexEdit;
	int phrase[16];// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 16
	float cv[16][16];// [-3.0 : 3.917]. First index is patten number, 2nd index is step
	StepAttributes attributes[16][16];// First index is patten number, 2nd index is step (see enum AttributeBitMasks for details)
	int stepIndexEdit;
	int phraseIndexEdit;
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
	
	// VCO
	// none
	
	// CLK
	float clkValue;
	
	// VCA
	// none
	
	// ADSR
	bool decaying = false;
	float env = 0.0f;
	
	// VCF
	LadderFilter filter;
	
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
	
	
	LowFrequencyOscillator oscillatorClk;
	LowFrequencyOscillator oscillatorLfo;
	VoltageControlledOscillator oscillatorVco;


	SemiModularSynth() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
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
		
		configParam(VCO_FREQ_PARAM, -54.0f, 54.0f, 0.0f, "VCO freq", " Hz", std::pow(2.0f, 1.0f/12.f), dsp::FREQ_C4);// https://community.vcvrack.com/t/rack-v1-development-blog/1149/230 or src/engine/ParamQuantity.cpp ParamQuantity::getDisplayValue()
		configParam(VCO_FINE_PARAM, -1.0f, 1.0f, 0.0f, "VCO freq fine");
		configParam(VCO_PW_PARAM, 0.0f, 1.0f, 0.5f, "VCO pulse width", "%", 0.f, 100.f);
		configParam(VCO_FM_PARAM, 0.0f, 1.0f, 0.0f, "VCO FM");
		configParam(VCO_PWM_PARAM, 0.0f, 1.0f, 0.0f, "VCO PWM", "%", 0.f, 100.f);
		configSwitch(VCO_MODE_PARAM, 0.0f, 1.0f, 1.0f, "VCO mode", {"Digital", "Analog"});
		configParam(VCO_OCT_PARAM, -2.0f, 2.0f, 0.0f, "VCO octave");
		
		configParam(CLK_FREQ_PARAM, -2.0f, 4.0f, 1.0f, "CLK freq", " BPM", 2.0f, 60.0f);// 120 BMP when default value  (120 = 60*2^1) diplay params are: base, mult, offset
		configParam(CLK_PW_PARAM, 0.0f, 1.0f, 0.5f, "CLK PW");
		
		configParam(VCA_LEVEL1_PARAM, 0.0f, 1.0f, 1.0f, "VCA level");
	
		configParam(ADSR_ATTACK_PARAM, 0.0f, 1.0f, 0.1f, "ADSR attack");
		configParam(ADSR_DECAY_PARAM,  0.0f, 1.0f, 0.5f, "ADSR decay");
		configParam(ADSR_SUSTAIN_PARAM, 0.0f, 1.0f, 0.5f, "ADSR sustain");
		configParam(ADSR_RELEASE_PARAM, 0.0f, 1.0f, 0.5f, "ADSR release");
		
		configParam(VCF_FREQ_PARAM, 0.0f, 1.0f, 0.666f, "VCF freq");
		configParam(VCF_RES_PARAM, 0.0f, 1.0f, 0.0f, "VCF res");
		configParam(VCF_FREQ_CV_PARAM, -1.0f, 1.0f, 0.0f, "VCF freq cv");
		configParam(VCF_DRIVE_PARAM, 0.0f, 1.0f, 0.0f, "VCF drive");
		
		configParam(LFO_FREQ_PARAM, -8.0f, 6.0f, -1.0f, "LFO freq", " BPM", 2.0f, 60.0f);// 30 BMP when default value  (30 = 60*2^-1) diplay params are: base, mult, offset);
		configParam(LFO_GAIN_PARAM, 0.0f, 1.0f, 0.5f, "LFO gain");
		configParam(LFO_OFFSET_PARAM, -1.0f, 1.0f, 0.0f, "LFO offset");

		getParamQuantity(CPMODE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(EDIT_PARAM)->randomizeEnabled = false;		
		getParamQuantity(AUTOSTEP_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCO_MODE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCO_FREQ_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCF_FREQ_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCO_OCT_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCO_FINE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCO_PW_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCO_FM_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCO_PWM_PARAM)->randomizeEnabled = false;		
		getParamQuantity(CLK_FREQ_PARAM)->randomizeEnabled = false;		
		getParamQuantity(CLK_PW_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCA_LEVEL1_PARAM)->randomizeEnabled = false;		
		getParamQuantity(ADSR_ATTACK_PARAM)->randomizeEnabled = false;		
		getParamQuantity(ADSR_DECAY_PARAM)->randomizeEnabled = false;		
		getParamQuantity(ADSR_SUSTAIN_PARAM)->randomizeEnabled = false;		
		getParamQuantity(ADSR_RELEASE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCF_RES_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCF_FREQ_CV_PARAM)->randomizeEnabled = false;		
		getParamQuantity(VCF_DRIVE_PARAM)->randomizeEnabled = false;		
		getParamQuantity(LFO_FREQ_PARAM)->randomizeEnabled = false;		
		getParamQuantity(LFO_GAIN_PARAM)->randomizeEnabled = false;		
		getParamQuantity(LFO_OFFSET_PARAM)->randomizeEnabled = false;		
		

		// SEQUENCER
		configInput(WRITE_INPUT, "Write");
		configInput(CV_INPUT, "CV");
		configInput(RESET_INPUT, "Reset");
		configInput(CLOCK_INPUT, "Clock");
		configInput(LEFTCV_INPUT, "Step left");
		configInput(RIGHTCV_INPUT, "Step right");
		configInput(RUNCV_INPUT, "Run");
		configInput(SEQCV_INPUT, "Seq#");
		
		// VCO
		configInput(VCO_PITCH_INPUT, "VCO pitch");
		configInput(VCO_FM_INPUT, "VCO FM");
		configInput(VCO_SYNC_INPUT, "VCO sync");
		configInput(VCO_PW_INPUT, "VCO PW");

		// CLK
		// none
		
		// VCA
		configInput(VCA_LIN1_INPUT, "VCA level");
		configInput(VCA_IN1_INPUT, "VCA");
		
		// ADSR
		configInput(ADSR_GATE_INPUT, "ADSR gate");

		// VCF
		configInput(VCF_FREQ_INPUT, "VCF cutoff freq");
		configInput(VCF_RES_INPUT, "VCF resonance");
		configInput(VCF_DRIVE_INPUT, "VCF drive");
		configInput(VCF_IN_INPUT, "VCF audio");
		
		// LFO
		configInput(LFO_RESET_INPUT, "LFO reset");

	
		// SEQUENCER
		configOutput(CV_OUTPUT, "CV");
		configOutput(GATE1_OUTPUT, "Gate 1");
		configOutput(GATE2_OUTPUT, "Gate 2");
		
		// VCO
		configOutput(VCO_SIN_OUTPUT, "VCO sine");
		configOutput(VCO_TRI_OUTPUT, "VCO triangle");
		configOutput(VCO_SAW_OUTPUT, "VCO saw");
		configOutput(VCO_SQR_OUTPUT, "VCO square");

		// CLK
		configOutput(CLK_OUT_OUTPUT, "CLK");
		
		// VCA
		configOutput(VCA_OUT1_OUTPUT, "VCA");
		
		// ADSR
		configOutput(ADSR_ENVELOPE_OUTPUT, "ADSR envelope");
		
		// VCF
		configOutput(VCF_LPF_OUTPUT, "VCF low pass");
		configOutput(VCF_HPF_OUTPUT, "VCF high pass");
		
		// LFO
		configOutput(LFO_SIN_OUTPUT, "LFO sine");
		configOutput(LFO_TRI_OUTPUT, "LFO triangle");
		

		onReset();
		
		// VCO
		oscillatorVco.soft = false;
		
		// CLK 
		oscillatorClk.offset = true;
		oscillatorClk.invert = false;
		
		// LFO
		oscillatorLfo.setPulseWidth(0.5f);
		oscillatorLfo.offset = false;
		oscillatorLfo.invert = false;
		
		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}
	

	void onReset() override {
		// SEQUENCER
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
		
		// VCO
		// none
		
		// CLK
		clkValue = 0.0f;
		
		// VCF
		filter.reset();
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
		if (panelThemeJ) {
			panelTheme = json_integer_value(panelThemeJ);
			if (panelTheme > 1) panelTheme = 1;
		}

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
	
		// SEQUENCER

		static const float gateTime = 0.4f;// seconds
		static const float revertDisplayTime = 0.7f;// seconds
		static const float warningTime = 0.7f;// seconds
		static const float holdDetectTime = 2.0f;// seconds
		static const float editGateLengthTime = 3.5f;// seconds
		
		
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
						if (stepIndexEdit == 0 && autoseq)
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
							sequences[seqIndexEdit].setRunMode(clamp(sequences[seqIndexEdit].getRunMode() + deltaKnob, 0, (NUM_MODES - 1 - 1)));
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
			if (gate1Trigger.process(params[GATE1_PARAM].getValue())) {
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
			if (gate2Trigger.process(params[GATE2_PARAM].getValue())) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					attributes[seqIndexEdit][stepIndexEdit].toggleGate2();
				}
			}		
			if (slideTrigger.process(params[SLIDE_BTN_PARAM].getValue())) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (attributes[seqIndexEdit][stepIndexEdit].getTied())
						tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					else
						attributes[seqIndexEdit][stepIndexEdit].toggleSlide();
				}
			}		
			if (tiedTrigger.process(params[TIE_PARAM].getValue())) {
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
		float clockInput = inputs[CLOCK_INPUT].isConnected() ? inputs[CLOCK_INPUT].getVoltage() : clkValue;// Pre-patching
		if (running && clockIgnoreOnReset == 0l) {
			if (clockTrigger.process(clockInput)) {
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
			if (inputs[SEQCV_INPUT].isConnected() && seqCVmethod == 2)
				seqIndexEdit = 0;
		}
		
		
		//********** Outputs and lights **********
				
		// CV and gates outputs
		int seq = editingSequence ? seqIndexEdit : phrase[phraseIndexRun];
		int step = (editingSequence && !running) ? stepIndexEdit : stepIndexRun;
		if (running) {
			bool muteGate1 = !editingSequence && (params[GATE1_PARAM].getValue() > 0.5f);// live mute
			bool muteGate2 = !editingSequence && (params[GATE2_PARAM].getValue() > 0.5f);// live mute
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
		}// lightRefreshCounter
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;

		
		// VCO
		oscillatorVco.analog = params[VCO_MODE_PARAM].getValue() > 0.0f;
		float pitchFine = 3.0f * dsp::quadraticBipolar(params[VCO_FINE_PARAM].getValue());
		float pitchCv = 12.0f * (inputs[VCO_PITCH_INPUT].isConnected() ? inputs[VCO_PITCH_INPUT].getVoltage() : outputs[CV_OUTPUT].getVoltage());// Pre-patching
		float pitchOctOffset = 12.0f * params[VCO_OCT_PARAM].getValue();
		if (inputs[VCO_FM_INPUT].isConnected()) {
			pitchCv += dsp::quadraticBipolar(params[VCO_FM_PARAM].getValue()) * 12.0f * inputs[VCO_FM_INPUT].getVoltage();
		}
		oscillatorVco.setPitch(params[VCO_FREQ_PARAM].getValue(), pitchFine + pitchCv + pitchOctOffset);
		oscillatorVco.setPulseWidth(params[VCO_PW_PARAM].getValue() + params[VCO_PWM_PARAM].getValue() * inputs[VCO_PW_INPUT].getVoltage() / 10.0f);
		oscillatorVco.syncEnabled = inputs[VCO_SYNC_INPUT].isConnected();
		oscillatorVco.process(args.sampleTime, inputs[VCO_SYNC_INPUT].getVoltage());
		if (outputs[VCO_SIN_OUTPUT].isConnected()) {
			outputs[VCO_SIN_OUTPUT].setVoltage(5.0f * oscillatorVco.sin());
		}
		if (outputs[VCO_TRI_OUTPUT].isConnected()) {
			outputs[VCO_TRI_OUTPUT].setVoltage(5.0f * oscillatorVco.tri());
		}
		if (outputs[VCO_SAW_OUTPUT].isConnected()) {
			outputs[VCO_SAW_OUTPUT].setVoltage(5.0f * oscillatorVco.saw());
		}
		outputs[VCO_SQR_OUTPUT].setVoltage(5.0f * oscillatorVco.sqr());		
			
			
		// CLK
		if (refresh.processInputs()) {
			oscillatorClk.setPitch(params[CLK_FREQ_PARAM].getValue() + log2f(pulsesPerStep));
			oscillatorClk.setPulseWidth(params[CLK_PW_PARAM].getValue());
		}	
		oscillatorClk.step(args.sampleTime);
		oscillatorClk.setReset(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue() + params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage());//inputs[RESET_INPUT].getVoltage());
		clkValue = 5.0f * oscillatorClk.sqr();	
		outputs[CLK_OUT_OUTPUT].setVoltage(clkValue);
		
		
		// VCA
		float vcaIn = inputs[VCA_IN1_INPUT].isConnected() ? inputs[VCA_IN1_INPUT].getVoltage() : outputs[VCO_SQR_OUTPUT].getVoltage();// Pre-patching
		float vcaLin = inputs[VCA_LIN1_INPUT].isConnected() ? inputs[VCA_LIN1_INPUT].getVoltage() : outputs[ADSR_ENVELOPE_OUTPUT].getVoltage();// Pre-patching
		float v = vcaIn * params[VCA_LEVEL1_PARAM].getValue();
		v *= clamp(vcaLin / 10.0f, 0.0f, 1.0f);
		outputs[VCA_OUT1_OUTPUT].setVoltage(v);

				
		// ADSR
		float attack = clamp(params[ADSR_ATTACK_PARAM].getValue(), 0.0f, 1.0f);
		float decay = clamp(params[ADSR_DECAY_PARAM].getValue(), 0.0f, 1.0f);
		float sustain = clamp(params[ADSR_SUSTAIN_PARAM].getValue(), 0.0f, 1.0f);
		float release = clamp(params[ADSR_RELEASE_PARAM].getValue(), 0.0f, 1.0f);
		// Gate
		float adsrIn = inputs[ADSR_GATE_INPUT].isConnected() ? inputs[ADSR_GATE_INPUT].getVoltage() : outputs[GATE1_OUTPUT].getVoltage();// Pre-patching
		bool gated = adsrIn >= 1.0f;
		const float base = 20000.0f;
		const float maxTime = 10.0f;
		if (gated) {
			if (decaying) {
				// Decay
				if (decay < 1e-4) {
					env = sustain;
				}
				else {
					env += std::pow(base, 1 - decay) / maxTime * (sustain - env) * args.sampleTime;
				}
			}
			else {
				// Attack
				// Skip ahead if attack is all the way down (infinitely fast)
				if (attack < 1e-4) {
					env = 1.0f;
				}
				else {
					env += std::pow(base, 1 - attack) / maxTime * (1.01f - env) * args.sampleTime;
				}
				if (env >= 1.0f) {
					env = 1.0f;
					decaying = true;
				}
			}
		}
		else {
			// Release
			if (release < 1e-4) {
				env = 0.0f;
			}
			else {
				env += std::pow(base, 1 - release) / maxTime * (0.0f - env) * args.sampleTime;
			}
			decaying = false;
		}
		outputs[ADSR_ENVELOPE_OUTPUT].setVoltage(10.0f * env);
		
		
		// VCF
		if (outputs[VCF_LPF_OUTPUT].isConnected() || outputs[VCF_HPF_OUTPUT].isConnected()) {
		
			float input = (inputs[VCF_IN_INPUT].isConnected() ? inputs[VCF_IN_INPUT].getVoltage() : outputs[VCA_OUT1_OUTPUT].getVoltage()) / 5.0f;// Pre-patching
			float drive = clamp(params[VCF_DRIVE_PARAM].getValue() + inputs[VCF_DRIVE_INPUT].getVoltage() / 10.0f, 0.f, 1.f);
			float gain = std::pow(1.f + drive, 5);
			input *= gain;
			// Add -60dB noise to bootstrap self-oscillation
			input += 1e-6f * (2.f * random::uniform() - 1.f);
			// Set resonance
			float res = clamp(params[VCF_RES_PARAM].getValue() + inputs[VCF_RES_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			filter.resonance = std::pow(res, 2) * 10.f;
			// Set cutoff frequency
			float pitch = 0.f;
			if (inputs[VCF_FREQ_INPUT].isConnected())
				pitch += inputs[VCF_FREQ_INPUT].getVoltage() * dsp::quadraticBipolar(params[VCF_FREQ_CV_PARAM].getValue());
			pitch += params[VCF_FREQ_PARAM].getValue() * 10.f - 5.f;
			//pitch += dsp::quadraticBipolar(params[FINE_PARAM].getValue() * 2.f - 1.f) * 7.f / 12.f;
			float cutoff = 261.626f * std::pow(2.f, pitch);
			cutoff = clamp(cutoff, 1.f, 8000.f);
			filter.setCutoff(cutoff);
			filter.process(input, args.sampleTime);
			outputs[VCF_LPF_OUTPUT].setVoltage(5.f * filter.lowpass);
			outputs[VCF_HPF_OUTPUT].setVoltage(5.f * filter.highpass);	
		}			
		else {
			outputs[VCF_LPF_OUTPUT].setVoltage(0.0f);
			outputs[VCF_HPF_OUTPUT].setVoltage(0.0f);
		}
		
		// LFO
		if (outputs[LFO_SIN_OUTPUT].isConnected() || outputs[LFO_TRI_OUTPUT].isConnected()) {
			if (refresh.processInputs()) {
				oscillatorLfo.setPitch(params[LFO_FREQ_PARAM].getValue());
			}
			oscillatorLfo.step(args.sampleTime);
			oscillatorLfo.setReset(inputs[LFO_RESET_INPUT].getVoltage() + inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue() + params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage());
			float lfoGain = params[LFO_GAIN_PARAM].getValue();
			float lfoOffset = (2.0f - lfoGain) * params[LFO_OFFSET_PARAM].getValue();
			outputs[LFO_SIN_OUTPUT].setVoltage(5.0f * (lfoOffset + lfoGain * oscillatorLfo.sin()));
			outputs[LFO_TRI_OUTPUT].setVoltage(5.0f * (lfoOffset + lfoGain * oscillatorLfo.tri()));	
		} 
		else {
			outputs[LFO_SIN_OUTPUT].setVoltage(0.0f);
			outputs[LFO_TRI_OUTPUT].setVoltage(0.0f);
		}			
		
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



struct SemiModularSynthWidget : ModuleWidget {
	struct SequenceDisplayWidget : TransparentWidget {
		SemiModularSynth *module;
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
					if (module->displayState != SemiModularSynth::DISP_LENGTH)
						module->displayState = SemiModularSynth::DISP_NORMAL;
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
					else if (module->displayState == SemiModularSynth::DISP_MODE) {
					}
					else if (module->displayState == SemiModularSynth::DISP_LENGTH) {
						totalNum = clamp(totalNum, 1, 16);
						if (editingSequence)
							module->sequences[module->seqIndexEdit].setLength(totalNum);
						else
							module->phrases = totalNum;
					}
					else if (module->displayState == SemiModularSynth::DISP_TRANSPOSE) {
					}
					else if (module->displayState == SemiModularSynth::DISP_ROTATE) {
					}
					else {// DISP_NORMAL
						totalNum = clamp(totalNum, 1, 16);
						if (editingSequence) {
							if (!module->inputs[SemiModularSynth::SEQCV_INPUT].isConnected())
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
							float cpMode = module->params[SemiModularSynth::CPMODE_PARAM].getValue();
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
					else if (module->displayState == SemiModularSynth::DISP_MODE) {
						if (editingSequence)
							runModeToStr(module->sequences[module->seqIndexEdit].getRunMode());
						else
							runModeToStr(module->runModeSong);
					}
					else if (module->displayState == SemiModularSynth::DISP_LENGTH) {
						if (editingSequence)
							snprintf(displayStr, 16, "L%2u", (unsigned) module->sequences[module->seqIndexEdit].getLength());
						else
							snprintf(displayStr, 16, "L%2u", (unsigned) module->phrases);
					}
					else if (module->displayState == SemiModularSynth::DISP_TRANSPOSE) {
						snprintf(displayStr, 16, "+%2u", (unsigned) abs(module->sequences[module->seqIndexEdit].getTranspose()));
						if (module->sequences[module->seqIndexEdit].getTranspose() < 0)
							displayStr[0] = '-';
					}
					else if (module->displayState == SemiModularSynth::DISP_ROTATE) {
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
			SemiModularSynth *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = module->fillIoSteps(&seqLen);
				interopCopySequence(seqLen, ioSteps);
				delete[] ioSteps;
			}
		};
		struct InteropPasteSeqItem : MenuItem {
			SemiModularSynth *module;
			void onAction(const event::Action &e) override {
				int seqLen;
				IoStep* ioSteps = interopPasteSequence(16, &seqLen);
				if (ioSteps != nullptr) {
					module->emptyIoSteps(ioSteps, seqLen);
					delete[] ioSteps;
				}
			}
		};
		SemiModularSynth *module;
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
		SemiModularSynth *module = dynamic_cast<SemiModularSynth*>(this->module);
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
	}	
	
	struct SequenceKnob : IMBigKnobInf {
		SequenceKnob() {};		
		void onDoubleClick(const event::DoubleClick &e) override {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity) {
				SemiModularSynth* module = dynamic_cast<SemiModularSynth*>(paramQuantity->module);
				// same code structure below as in sequence knob in main step()
				if (module->editingPpqn != 0) {
					module->pulsesPerStep = 1;
					//editingPpqn = (long) (editGateLengthTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else if (module->displayState == SemiModularSynth::DISP_MODE) {
					if (module->isEditingSequence()) {
						module->sequences[module->seqIndexEdit].setRunMode(MODE_FWD);
					}
					else {
						module->runModeSong = MODE_FWD;
					}
				}
				else if (module->displayState == SemiModularSynth::DISP_LENGTH) {
					if (module->isEditingSequence()) {
						module->sequences[module->seqIndexEdit].setLength(16);
					}
					else {
						module->phrases = 4;
					}
				}
				else if (module->displayState == SemiModularSynth::DISP_TRANSPOSE) {
					// nothing
				}
				else if (module->displayState == SemiModularSynth::DISP_ROTATE) {
					// nothing			
				}
				else {// DISP_NORMAL
					if (module->isEditingSequence()) {
						if (!module->inputs[SemiModularSynth::SEQCV_INPUT].isConnected()) {
							module->seqIndexEdit = 0;;
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
	
	
	SemiModularSynthWidget(SemiModularSynth *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/SemiModular.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	
		
		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx((box.size.x - 90) * 1 / 3 + 30 , 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx((box.size.x - 90) * 1 / 3 + 30 , 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx((box.size.x - 90) * 2 / 3 + 45 , 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx((box.size.x - 90) * 2 / 3 + 45 , 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));


		// SEQUENCER 
		
		static const float rowT0 = 48.6f + 4.0f;
		static const int colT0 = 27 + 4;// Step LED buttons
		static const int colT3 = 404 + 4;// Attach (also used to align rest of right side of module)

		// Step/Phrase LED buttons
		int posX = colT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		for (int x = 0; x < 16; x++) {
			// First row
			addParam(createParamCentered<LEDButton>(VecPx(posX, rowT0 + 2), module, SemiModularSynth::STEP_PHRASE_PARAMS + x));
			addChild(createLightCentered<MediumLight<GreenRedWhiteLightIM>>(VecPx(posX, rowT0 + 2), module, SemiModularSynth::STEP_PHRASE_LIGHTS + (x * 3)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// Attach button and light
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colT3, rowT0 + 7.4f), module, SemiModularSynth::ATTACH_PARAM, mode));
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(colT3 + 22, rowT0 + 7.4f), module, SemiModularSynth::ATTACH_LIGHT));		

		
		// Key mode LED buttons	
		static const float rowKM = 78.6f + 4.0f;
		addParam(createParamCentered<LEDButton>(VecPx(252 + 4, rowKM), module, SemiModularSynth::KEYNOTE_PARAM));
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(252 + 4, rowKM), module, SemiModularSynth::KEYNOTE_LIGHT));
		addParam(createParamCentered<LEDButton>(VecPx(140 + 4, rowKM), module, SemiModularSynth::KEYGATE_PARAM));
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(140 + 4, rowKM), module, SemiModularSynth::KEYGATE_LIGHT));
		
		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const float octLightsIntY = 20.0f;
		for (int i = 0; i < 7; i++) {
			addParam(createParamCentered<LEDButton>(VecPx(27 + 4, 110.6f + 4 + i * octLightsIntY), module, SemiModularSynth::OCTAVE_PARAM + i));
			addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(27 + 4, 110.6f + 4 + i * octLightsIntY), module, SemiModularSynth::OCTAVE_LIGHTS + i));
		}
		
		// Keys and Key lights
		static const Vec keyboardPos = mm2px(Vec(19.577f, 34.657f));
		svgPanel->fb->addChild(new KeyboardSmall(keyboardPos, mode));
		
		static const Vec offsetLeds = Vec(PianoKeySmall::sizeX * 0.5f, PianoKeySmall::sizeY * 0.55f);
		for (int k = 0; k < 12; k++) {
			Vec keyPos = keyboardPos + mm2px(smaKeysPos[k]);
			addChild(createPianoKey<PianoKeySmall>(keyPos, k, module ? &module->pkInfo : NULL));
			addChild(createLightCentered<MediumLight<GreenRedLightIM>>(keyPos + offsetLeds, module, SemiModularSynth::KEY_LIGHTS + k * 2));
		}

		
		
		
		// ****** Right side control area ******

		static const int rowMK0 = 113 + 4;// Edit mode row
		static const int rowMK1 = rowMK0 + 56; // Run row
		static const int rowMK2 = rowMK1 + 54; // Reset row
		static const int colMK0 = 289 + 4;// Edit mode column
		static const int colMK1 = colMK0 + 59;// Display column
		static const int colMK2 = colT3 + 8;// Run mode column
		
		
		// Edit mode switch
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(colMK0, rowMK0), module, SemiModularSynth::EDIT_PARAM, mode, svgPanel));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.size = VecPx(55, 30);// 3 characters
		displaySequence->box.pos = VecPx(colMK1, rowMK0 + 4).minus(displaySequence->box.size.div(2));
		displaySequence->module = module;
		addChild(displaySequence);
		svgPanel->fb->addChild(new DisplayBackground(displaySequence->box.pos, displaySequence->box.size, mode));
		// Len/mode button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMK2, rowMK0), module, SemiModularSynth::RUNMODE_PARAM, mode));
		
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(colMK0, rowMK1 + 7), module, SemiModularSynth::RUN_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(colMK0, rowMK1 + 7), module, SemiModularSynth::RUN_LIGHT));
		// Sequence knob
		addParam(createDynamicParamCentered<SequenceKnob>(VecPx(colMK1 + 1, rowMK0 + 55), module, SemiModularSynth::SEQUENCE_PARAM, mode));		
		// Transpose/rotate button
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMK2, rowMK1 + 4), module, SemiModularSynth::TRAN_ROT_PARAM, mode));
		
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(VecPx(colMK0, rowMK2 + 5), module, SemiModularSynth::RESET_PARAM));
		addChild(createLightCentered<LEDBezelLight<GreenLightIM>>(VecPx(colMK0, rowMK2 + 5), module, SemiModularSynth::RESET_LIGHT));
		// Copy/paste buttons
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colMK1 - 15, rowMK2 + 5), module, SemiModularSynth::COPY_PARAM, mode));
		addParam(createDynamicParamCentered<IMPushButton>(VecPx(colMK1 + 15, rowMK2 + 5), module, SemiModularSynth::PASTE_PARAM, mode));
		// Copy-paste mode switch (3 position)
		addParam(createDynamicSwitchCentered<IMSwitch3VInv>(VecPx(colMK2 + 1, rowMK2 - 3), module, SemiModularSynth::CPMODE_PARAM, mode, svgPanel));	// 0.0f is top position

		
		
		// ****** Gate and slide section ******
		
		static const int rowMB0 = 230 + 4;
		static const int columnRulerMBspacing = 70;
		static const int colMB2 = 142 + 4;// Gate2
		static const int colMB1 = colMB2 - columnRulerMBspacing;// Gate1 
		static const int colMB3 = colMB2 + columnRulerMBspacing;// Tie
		static const int ledVsButtonDX = 27;
		
		// Gate 1 light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colMB1 + ledVsButtonDX, rowMB0), module, SemiModularSynth::GATE1_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMB1, rowMB0), module, SemiModularSynth::GATE1_PARAM, mode));
		// Gate 2 light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colMB2 + ledVsButtonDX, rowMB0), module, SemiModularSynth::GATE2_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMB2, rowMB0), module, SemiModularSynth::GATE2_PARAM, mode));
		// Tie light and button
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(colMB3 + ledVsButtonDX, rowMB0), module, SemiModularSynth::TIE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colMB3, rowMB0), module, SemiModularSynth::TIE_PARAM, mode));

						
		
		// ****** Bottom two rows ******
		
		static const int outputJackSpacingX = 54;
		static const int rowB0 = 335 + 4;
		static const int rowB1 = 281 + 4;
		static const int colB0 = 34 + 4;
		static const int colB1 = colB0 + outputJackSpacingX;
		static const int colB2 = colB1 + outputJackSpacingX;
		static const int colB3 = colB2 + outputJackSpacingX;
		static const int colB4 = colB3 + outputJackSpacingX;
		static const int colB7 = colMK2 + 1;
		static const int colB6 = colB7 - outputJackSpacingX;
		static const int colB5 = colB6 - outputJackSpacingX;
		

		
		// Gate 1 probability light and button
		addChild(createLightCentered<MediumLight<GreenRedLightIM>>(VecPx(colB0 + ledVsButtonDX, rowB1), module, SemiModularSynth::GATE1_PROB_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colB0, rowB1), module, SemiModularSynth::GATE1_PROB_PARAM, mode));
		// Gate 1 probability knob
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colB1, rowB1), module, SemiModularSynth::GATE1_KNOB_PARAM, mode));
		// Slide light and button
		addChild(createLightCentered<MediumLight<RedLightIM>>(VecPx(colB2 + ledVsButtonDX, rowB1), module, SemiModularSynth::SLIDE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(VecPx(colB2, rowB1), module, SemiModularSynth::SLIDE_BTN_PARAM, mode));
		// Slide knob
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colB3, rowB1), module, SemiModularSynth::SLIDE_KNOB_PARAM, mode));
		// Autostep
		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(colB4, rowB1), module, SemiModularSynth::AUTOSTEP_PARAM, mode, svgPanel));		
		// CV in
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB5, rowB1), true, module, SemiModularSynth::CV_INPUT, mode));
		// Reset
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB6, rowB1), true, module, SemiModularSynth::RESET_INPUT, mode));
		// Clock
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB7, rowB1), true, module, SemiModularSynth::CLOCK_INPUT, mode));

		

		// ****** Bottom row (all aligned) ******

	
		// CV control Inputs 
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB0, rowB0), true, module, SemiModularSynth::LEFTCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB1, rowB0), true, module, SemiModularSynth::RIGHTCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB2, rowB0), true, module, SemiModularSynth::SEQCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB3, rowB0), true, module, SemiModularSynth::RUNCV_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colB4, rowB0), true, module, SemiModularSynth::WRITE_INPUT, mode));
		// Outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colB5, rowB0), false, module, SemiModularSynth::CV_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colB6, rowB0), false, module, SemiModularSynth::GATE1_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colB7, rowB0), false, module, SemiModularSynth::GATE2_OUTPUT, mode));
		
		// END OF SEQUENCER
	
	
		
		// VCO
		static const int rowVCO0 = 66 + 12;// Freq
		static const int rowVCO1 = rowVCO0 + 40;// Fine, PW
		static const int rowVCO2 = rowVCO1 + 35;// FM, PWM, exact value from svg
		static const int rowVCO3 = rowVCO2 + 46;// switches (Don't change this, tweak the switches' v offset instead since jacks lines up with this)
		static const int rowSpacingJacks = 35;// exact value from svg
		static const int rowVCO4 = rowVCO3 + rowSpacingJacks;// jack row 1
		static const int rowVCO5 = rowVCO4 + rowSpacingJacks;// jack row 2
		static const int rowVCO6 = rowVCO5 + rowSpacingJacks;// jack row 3
		static const int rowVCO7 = rowVCO6 + rowSpacingJacks;// jack row 4
		static const int colVCO0 = 460 + 12;
		static const int colVCO1 = colVCO0 + 55;// exact value from svg


		addParam(createDynamicParamCentered<IMBigKnob>(VecPx(colVCO0 + 55 / 2, rowVCO0), module, SemiModularSynth::VCO_FREQ_PARAM, mode));
		
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVCO0, rowVCO1), module, SemiModularSynth::VCO_FINE_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVCO1, rowVCO1), module, SemiModularSynth::VCO_PW_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVCO0, rowVCO2), module, SemiModularSynth::VCO_FM_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVCO1, rowVCO2), module, SemiModularSynth::VCO_PWM_PARAM, mode));

		addParam(createDynamicSwitchCentered<IMSwitch2V>(VecPx(colVCO0, rowVCO3), module, SemiModularSynth::VCO_MODE_PARAM, mode, svgPanel));
		addParam(createDynamicParamCentered<IMFivePosSmallKnob>(VecPx(colVCO1, rowVCO3), module, SemiModularSynth::VCO_OCT_PARAM, mode));

		addOutput(createDynamicPortCentered<IMPort>(VecPx(colVCO0, rowVCO4), false, module, SemiModularSynth::VCO_SIN_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colVCO1, rowVCO4), false, module, SemiModularSynth::VCO_TRI_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colVCO0, rowVCO5), false, module, SemiModularSynth::VCO_SAW_OUTPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colVCO1, rowVCO5), false, module, SemiModularSynth::VCO_SQR_OUTPUT, mode));		
		
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVCO0, rowVCO6), true, module, SemiModularSynth::VCO_PITCH_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVCO1, rowVCO6), true, module, SemiModularSynth::VCO_FM_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVCO0, rowVCO7), true, module, SemiModularSynth::VCO_SYNC_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVCO1, rowVCO7), true, module, SemiModularSynth::VCO_PW_INPUT, mode));

		
		// CLK
		static const int rowClk0 = 41 + 12;
		static const int rowClk1 = rowClk0 + 45;// exact value from svg
		static const int rowClk2 = rowClk1 + 38;
		static const int colClk0 = colVCO1 + 55;// exact value from svg
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colClk0, rowClk0), module, SemiModularSynth::CLK_FREQ_PARAM, mode));// 120 BMP when default value
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colClk0, rowClk1), module, SemiModularSynth::CLK_PW_PARAM, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colClk0, rowClk2), false, module, SemiModularSynth::CLK_OUT_OUTPUT, mode));
		
		
		// VCA
		static const int colVca1 = colClk0 + 55;// exact value from svg
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colClk0, rowVCO3), module, SemiModularSynth::VCA_LEVEL1_PARAM, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colClk0, rowVCO4), true, module, SemiModularSynth::VCA_LIN1_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colClk0, rowVCO5), true, module, SemiModularSynth::VCA_IN1_INPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colVca1, rowVCO5), false, module, SemiModularSynth::VCA_OUT1_OUTPUT, mode));
		
		
		// ADSR
		static const int rowAdsr0 = rowClk0;
		static const int rowAdsr3 = rowVCO2 + 6;
		static const int rowAdsr1 = rowAdsr0 + (rowAdsr3 - rowAdsr0) * 1 / 3;
		static const int rowAdsr2 = rowAdsr0 + (rowAdsr3 - rowAdsr0) * 2 / 3;
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVca1, rowAdsr0), module, SemiModularSynth::ADSR_ATTACK_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVca1, rowAdsr1), module, SemiModularSynth::ADSR_DECAY_PARAM,  mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVca1, rowAdsr2), module, SemiModularSynth::ADSR_SUSTAIN_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVca1, rowAdsr3), module, SemiModularSynth::ADSR_RELEASE_PARAM, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colVca1, rowVCO3), false, module, SemiModularSynth::ADSR_ENVELOPE_OUTPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVca1, rowVCO4), true, module, SemiModularSynth::ADSR_GATE_INPUT, mode));

		
		// VCF
		static const int colVCF0 = colVca1 + 55;// exact value from svg
		static const int colVCF1 = colVCF0 + 55;// exact value from svg
		addParam(createDynamicParamCentered<IMBigKnob>(VecPx(colVCF0 + 55 / 2, rowVCO0), module, SemiModularSynth::VCF_FREQ_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVCF0 + 55 / 2, rowVCO1), module, SemiModularSynth::VCF_RES_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVCF0 , rowVCO2), module, SemiModularSynth::VCF_FREQ_CV_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colVCF1 , rowVCO2 ), module, SemiModularSynth::VCF_DRIVE_PARAM, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVCF0, rowVCO3), true, module, SemiModularSynth::VCF_FREQ_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVCF1, rowVCO3), true, module, SemiModularSynth::VCF_RES_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVCF0, rowVCO4), true, module, SemiModularSynth::VCF_DRIVE_INPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colVCF0, rowVCO5), true, module, SemiModularSynth::VCF_IN_INPUT, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colVCF1, rowVCO4), false, module, SemiModularSynth::VCF_HPF_OUTPUT, mode));		
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colVCF1, rowVCO5), false, module, SemiModularSynth::VCF_LPF_OUTPUT, mode));
		
		
		// LFO
		static const int colLfo = colVCF1 + 55;// exact value from svg
		static const int rowLfo0 = rowAdsr0;
		static const int rowLfo2 = rowVCO2;
		static const int rowLfo1 = rowLfo0 + (rowLfo2 - rowLfo0) / 2;
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colLfo, rowLfo0), module, SemiModularSynth::LFO_FREQ_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colLfo, rowLfo1), module, SemiModularSynth::LFO_GAIN_PARAM, mode));
		addParam(createDynamicParamCentered<IMSmallKnob>(VecPx(colLfo, rowLfo2), module, SemiModularSynth::LFO_OFFSET_PARAM, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colLfo, rowVCO3), false, module, SemiModularSynth::LFO_TRI_OUTPUT, mode));		
		addOutput(createDynamicPortCentered<IMPort>(VecPx(colLfo, rowVCO4), false, module, SemiModularSynth::LFO_SIN_OUTPUT, mode));
		addInput(createDynamicPortCentered<IMPort>(VecPx(colLfo, rowVCO5), true, module, SemiModularSynth::LFO_RESET_INPUT, mode));
	}
};

Model *modelSemiModularSynth = createModel<SemiModularSynth, SemiModularSynthWidget>("Semi-ModularSynth");
