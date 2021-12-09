//***********************************************************************************************
//Four channel note viewer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "Interop.hpp"


// Intervals (two notes)
// // https://en.wikipedia.org/wiki/Interval_(music)#Main_intervals
static const std::string intervalNames[13] = {"PER","MIN","MAJ","MIN","MAJ","PER","DIM","PER","MIN","MAJ","MIN","MAJ","PER"};
static const int       intervalNumbers[13] = {  1,    2,    2,    3,    3,    4,   	5, 	  5,	6,	  6,	7,	  7,	8};
// short										P1	  m2	M2	  m3	M3	  P4	d5	  P5	m6	  M6	m7	  M7	P8
// semitone distance							0	  1		2	  3		4	  5		6	  7		8	  9		10	  11	12

// Triads (three notes)
// https://en.wikipedia.org/wiki/Chord_(music)#Examples
// https://en.wikipedia.org/wiki/Chord_(music)#Suspended_chords
static const int NUM_TRIADS = 6;
static const int triadIntervals[NUM_TRIADS][2] =  {{4,7}, {4,8}, {3,7}, {3,6}, {2,7}, {5,7}};
static const std::string triadNames[NUM_TRIADS] = {"MAJ", "AUG", "MIN", "DIM", "SUS", "SUS"};
static const int       triadNumbers[NUM_TRIADS] = { -1,    -1,    -1,    -1,    2,     4};

// 4-note chords
// https://en.wikipedia.org/wiki/Chord_(music)#Examples
static const int NUM_CHORDS = 9;
static const int chordIntervals[NUM_CHORDS][3]    {{4,7,9},{4,7,10},{4,7,11},{4,8,10},{3,7,9},{3,7,10},{3,7,11},{3,6,9},{3,6,10}};
static const std::string chordNames[NUM_CHORDS] = {"MAJ",  "DOM",   "MAJ",   "AUG",   "MIN",  "MIN",   "M_M",   "DIM",  "0"};
static const int       chordNumbers[NUM_CHORDS] = { 6,      7,       7,       7,       6,      7,       7,       7,      7};



struct FourView : Module {
	enum ParamIds {
		MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(CV_INPUTS, 4),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Constants
	const float unusedValue = -100.0f;

	// Expander
	float leftMessages[2][6] = {};// messages from mother (CvPad or ChordKey): 4 CV values, panelTheme, panelContrast

	// Need to save, no reset
	int panelTheme;
	float panelContrast;
	
	// Need to save, with reset
	int allowPolyOverride;
	bool showSharp;

	// No need to save, with reset
	float displayValues[4];
	char displayChord[16];// 4 displays of 3-char strings each having a fourth null termination char

	// No need to save, no reset
	RefreshCounter refresh;


	
	inline float quantize(float cv, bool enable) {
		return enable ? (std::round(cv * 12.0f) / 12.0f) : cv;
	}
	
	
	FourView() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		onReset();
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		configSwitch(MODE_PARAM, 0.0, 1.0, 0.0, "Display mode", {"Notes", "Chord"});// 0.0 is left, notes by default left, chord right
		
		for (int i = 0; i < 4; i++) {
			configInput(CV_INPUTS + i, string::f("CV %i", i + 1));
			configOutput(CV_OUTPUTS + i, string::f("CV %i thru", i + 1));
			configBypass(CV_INPUTS + i, CV_OUTPUTS + i);
		}

		loadThemeAndContrastFromDefault(&panelTheme, &panelContrast);
	}
	

	void onReset() override {
		allowPolyOverride = 1;
		showSharp = true;
		resetNonJson();
	}
	void resetNonJson() {
		for (int i = 0; i < 4; i++) {
			displayValues[i] = unusedValue;
		}
		memset(displayChord, 0, 16);
	}
	
	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// panelContrast
		json_object_set_new(rootJ, "panelContrast", json_real(panelContrast));

		// allowPolyOverride
		json_object_set_new(rootJ, "allowPolyOverride", json_integer(allowPolyOverride));

		// showSharp
		json_object_set_new(rootJ, "showSharp", json_boolean(showSharp));
		
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

		// allowPolyOverride
		json_t *allowPolyOverrideJ = json_object_get(rootJ, "allowPolyOverride");
		if (allowPolyOverrideJ)
			allowPolyOverride = json_integer_value(allowPolyOverrideJ);

		// showSharp
		json_t *showSharpJ = json_object_get(rootJ, "showSharp");
		if (showSharpJ)
			showSharp = json_is_true(showSharpJ);
		
		resetNonJson();
	}

	
	IoStep* fillIoSteps(int *seqLenPtr) {// caller must delete return array
		IoStep* ioSteps = new IoStep[4];
		
		// populate ioSteps array
		int j = 0;// write head also
		for (int i = 0; i < 4; i++) {
			if (displayValues[i] != unusedValue) {
				ioSteps[j].pitch = displayValues[i];
				ioSteps[j].gate = true;
				ioSteps[j].tied = false;
				ioSteps[j].vel = -1.0f;// no concept of velocity in BigButton2
				ioSteps[j].prob = -1.0f;// no concept of probability in BigButton2
				j++;
			}
		}
		
		// return values 
		*seqLenPtr = j;
		return ioSteps;
	}


	std::vector<IoNote>* fillIoNotes(int *seqLenPtr) {// caller must delete return vector
		std::vector<IoNote> *ioNotes = new std::vector<IoNote>;
		
		// populate ioNotes array
		int j = 0;// write head also
		for (int i = 0; i < 4; i++) {
			if (displayValues[i] != unusedValue) {
				IoNote newNote;
				newNote.start = 0.0f;
				newNote.length = 0.5f;
				newNote.pitch = displayValues[i];
				newNote.vel = -1.0f;// no concept of velocity in BigButton2
				newNote.prob = -1.0f;// no concept of probability in BigButton2
				ioNotes->push_back(newNote);
				j++;
			}
		}
		
		// return values 
		*seqLenPtr = j;
		return ioNotes;
	}


	void interopCopySeq() {
		int seqLen;
		IoStep* ioSteps = fillIoSteps(&seqLen);
		interopCopySequence(seqLen, ioSteps);
		delete[] ioSteps;
	};
	void interopCopyChord() {
		int seqLen;
		std::vector<IoNote>* ioNotes = fillIoNotes(&seqLen);
		interopCopySequenceNotes(seqLen, ioNotes);
		delete ioNotes;
	};
		
	
	void process(const ProcessArgs &args) override {
		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelCvPad ||
													  leftExpander.module->model == modelChordKey ||
													  leftExpander.module->model == modelChordKeyExpander));

		if (motherPresent) {
			// From Mother
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			memcpy(displayValues, messagesFromMother, 4 * 4);
			panelTheme = clamp((int)(messagesFromMother[4] + 0.5f), 0, 1);
			panelContrast = clamp(messagesFromMother[5], 0.0f, 255.0f);
		}	
		else {
			for (int i = 0; i < 4; i++) {
				displayValues[i] = unusedValue;
			}
		}
		
		int numChanIn0 = inputs[CV_INPUTS + 0].isConnected() ? inputs[CV_INPUTS + 0].getChannels() : 0;
		int i = 0;// write head
		if (allowPolyOverride == 1) {
			for (; i < numChanIn0; i++) {
				displayValues[i] = inputs[CV_INPUTS].getVoltage(i);
			}
		}
		for (; i < 4; i++) {
			if (inputs[CV_INPUTS + i].isConnected()) {
				displayValues[i] = inputs[CV_INPUTS + i].getVoltage();
			}
		}


		if (refresh.processInputs()) {
		}// userInputs refresh
		
		
		for (int i = 0; i < 4; i++) {
			outputs[CV_OUTPUTS + i].setVoltage(displayValues[i] == unusedValue ? 0.0f : displayValues[i]);
		}
		
		
		if (refresh.processLights()) {
			if (params[MODE_PARAM].getValue() >= 0.5f) {
				calcDisplayChord();
			}
		}// lightRefreshCounter
		
	}
	
	void calcDisplayChord() {
		// count notes and prepare sorted (will be sorted just in time later)
		int numNotes = 0;
		int packedNotes[4];// contains pitch CVs multiplied by 12 and rounded to integers
		for (int i = 0; i < 4; i++) {
			if (displayValues[i] != unusedValue) {
				int displayNote = (int)std::round(displayValues[i] * 12.0f);
				int j = 0;
				for (; j < numNotes; j++) {
					if (packedNotes[j] == displayNote) 
						break;// don't count duplicates
				}
				if (j == numNotes) {
					packedNotes[numNotes] = displayNote;
					numNotes ++;
				}
			}
		}		
		
		if (numNotes == 0) {
			printDashes();
		}
		else { 
			// Single note
			if (numNotes == 1) {
				printNoteNoOct(packedNotes[0], &displayChord[0], showSharp);
				displayChord[4] = 0;
				displayChord[8] = 0;
				displayChord[12] = 0;				
			}
			// Interval
			else if (numNotes == 2) {
				sortInt<2>(packedNotes);	
				if (!printInterval(packedNotes)) {
					printDashes();
				}					
			}
			// Triad
			else if (numNotes == 3) {
				sortInt<3>(packedNotes);	
				if (!printTriad(packedNotes)) {
					printDashes();
				}
			}
			// 4-note chord
			else {
				sortInt<4>(packedNotes);
				if (!print4Chord(packedNotes)) {
					printDashes();
				}
			}
		}
	}
	
	
	void printDashes() {
		snprintf(&displayChord[0 ], 4, " - ");
		snprintf(&displayChord[4 ], 4, " - ");
		snprintf(&displayChord[8 ], 4, " - ");
		snprintf(&displayChord[12], 4, " - ");				
	}
	
	template<int N>
	void sortInt(int *a) {
		int temp;
		for (int i = 0; i < N; i++) {
			for (int j = 0; j < N - i - 1; j++) {
				if (a[j] > a[j + 1]) {
					temp = a[j + 1];
					a[j + 1] = a[j];
					a[j] = temp;
				}
			}
		}	
	}
	
	
	bool printInterval(int *packedNotes) {// prints to all of displayChord if match found 
		int interval = packedNotes[1] - packedNotes[0];
		
		if (interval >= 0 && interval <= 12) {
			printNoteNoOct(packedNotes[0], &displayChord[0], showSharp);
			snprintf(&displayChord[4], 4, "%s", intervalNames[interval].c_str());
			snprintf(&displayChord[8], 4, "%i", intervalNumbers[interval]);
			displayChord[12] = 0;		
			return true;
		}
		return false;
	}
	
	
	bool printTriad(int *packedNotes) {// prints to all of displayChord if match found 
		int interval1 = packedNotes[1] - packedNotes[0];			
		int interval2 = packedNotes[2] - packedNotes[0];

		// No inversion
		for (int t = 0; t < NUM_TRIADS; t++) {
			if (triadIntervals[t][0] == interval1 && triadIntervals[t][1] == interval2) {
				printNoteNoOct(packedNotes[0], &displayChord[0], showSharp);
				snprintf(&displayChord[4], 4, "%s", triadNames[t].c_str());
				if (triadNumbers[t] != -1) {
					snprintf(&displayChord[8], 4, "%i", triadNumbers[t]);
				}
				else {
					displayChord[8] = 0;
				}
				displayChord[12] = 0;
				return true;
			}
		}
		
		// First inversion down (= second inversion up)
		for (int t = 0; t < NUM_TRIADS; t++) {
			int firstBase = -1 * (triadIntervals[t][1] - 12);
			if (firstBase == interval1 && (triadIntervals[t][0] + firstBase) == interval2) {
				printNoteNoOct(packedNotes[1], &displayChord[0], showSharp);
				snprintf(&displayChord[4], 4, "%s", triadNames[t].c_str());
				int inversionCursor = 8;
				if (triadNumbers[t] != -1) {
					inversionCursor += 4;
					snprintf(&displayChord[8], 4, "%i", triadNumbers[t]);
				}
				else {
					displayChord[12] = 0;
				}
				printNoteNoOct(packedNotes[0], &displayChord[inversionCursor + 1], showSharp);
				displayChord[inversionCursor] = '/';
				return true;
			}
		}
		
		// Second inversion down (= first inversion up)
		for (int t = 0; t < NUM_TRIADS; t++) {
			if ((triadIntervals[t][1] - triadIntervals[t][0]) == interval1 && (12 - triadIntervals[t][0]) == interval2) {
				printNoteNoOct(packedNotes[2], &displayChord[0], showSharp);
				snprintf(&displayChord[4], 4, "%s", triadNames[t].c_str());
				int inversionCursor = 8;
				if (triadNumbers[t] != -1) {
					inversionCursor += 4;
					snprintf(&displayChord[8], 4, "%i", triadNumbers[t]);
				}
				else {
					displayChord[12] = 0;
				}
				printNoteNoOct(packedNotes[0], &displayChord[inversionCursor + 1], showSharp);
				displayChord[inversionCursor] = '/';
				return true;
			}
		}

		// At this point no triads were detected, so check for intervals
		if ( (((interval2 % 12) == 0) || (((interval2 - interval1) % 12) == 0)) && printInterval(packedNotes)) {
			return true;
		}
		
		return false;
	}
	
	
	bool print4Chord(int *packedNotes) {// prints to all of displayChord if match found 
		int interval1 = packedNotes[1] - packedNotes[0];			
		int interval2 = packedNotes[2] - packedNotes[0];
		int interval3 = packedNotes[3] - packedNotes[0];

		// No inversion
		for (int c = 0; c < NUM_CHORDS; c++) {
			if (chordIntervals[c][0] == interval1 && chordIntervals[c][1] == interval2 && chordIntervals[c][2] == interval3) {
				printNoteNoOct(packedNotes[0], &displayChord[0], showSharp);// root note
				snprintf(&displayChord[4], 4, "%s", chordNames[c].c_str());
				snprintf(&displayChord[8], 4, "%i", chordNumbers[c]);
				displayChord[12] = 0;// no inversion
				return true;
			}
		}
		// First inversion down (= third inversion up)
		for (int c = 0; c < NUM_CHORDS; c++) {
			int firstBase = -1 * (chordIntervals[c][2] - 12);
			if (firstBase == interval1 && (chordIntervals[c][0] + firstBase) == interval2 && (chordIntervals[c][1] + firstBase) == interval3) {
				printNoteNoOct(packedNotes[1], &displayChord[0], showSharp);// root note
				snprintf(&displayChord[4], 4, "%s", chordNames[c].c_str());
				snprintf(&displayChord[8], 4, "%i", chordNumbers[c]);
				printNoteNoOct(packedNotes[0], &displayChord[12 + 1], showSharp);// base note of inversion
				displayChord[12] = '/';
				return true;
			}
		}
		// Second inversion
		for (int c = 0; c < NUM_CHORDS; c++) {
			int firstBase = -1 * (chordIntervals[c][1] - 12);
			if ((chordIntervals[c][2] - 12 + firstBase) == interval1 && firstBase == interval2 && (chordIntervals[c][0] + firstBase) == interval3) {
				printNoteNoOct(packedNotes[2], &displayChord[0], showSharp);// root note
				snprintf(&displayChord[4], 4, "%s", chordNames[c].c_str());
				snprintf(&displayChord[8], 4, "%i", chordNumbers[c]);
				printNoteNoOct(packedNotes[0], &displayChord[12 + 1], showSharp);// base note of inversion
				displayChord[12] = '/';
				return true;
			}
		}
		// Third inversion down (= first inversion up)
		for (int c = 0; c < NUM_CHORDS; c++) {
			if ((chordIntervals[c][1] - chordIntervals[c][0]) == interval1 && (chordIntervals[c][2] - chordIntervals[c][0]) == interval2 && (12 - chordIntervals[c][0]) == interval3) {
				printNoteNoOct(packedNotes[3], &displayChord[0], showSharp);// root note
				snprintf(&displayChord[4], 4, "%s", chordNames[c].c_str());
				snprintf(&displayChord[8], 4, "%i", chordNumbers[c]);
				printNoteNoOct(packedNotes[0], &displayChord[12 + 1], showSharp);// base note of inversion
				displayChord[12] = '/';
				return true;
			}
		}
		
		// At this point no 4-note chords were detected, so check for triads with and extra octave-related root note
		// Since the triad's biggest interval is < 12, the octave-related note is either the first note or the 
		//   fourth note since packedNotes are sorted
		
		// check if second note is octave-related to first note; if so, check for triad in last three notes
		if (((interval1 % 12) == 0) && printTriad(&packedNotes[1])) {
			return true;
		}
		
		// check if last note is octave-related to first note; if so, check for triad in first three notes
		if (((interval3 % 12) == 0) && printTriad(packedNotes)) {
			return true;
		}
		
		// At this point no triads were detected, so check for intervals
		if (((interval2 % 12) == 0) && (((interval3 - interval1) % 12) == 0) && printInterval(packedNotes)) {
			return true;
		}
				
		return false;
	}
};// module


struct FourViewWidget : ModuleWidget {
	int lastPanelTheme = -1;
	float lastPanelContrast = -1.0f;
	
	struct NotesDisplayWidget : TransparentWidget {
		FourView* module;
		int baseIndex;
		std::shared_ptr<Font> font;
		std::string fontPath;
		char text[4];

		NotesDisplayWidget(Vec _pos, Vec _size, FourView* _module, int _baseIndex) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			baseIndex = _baseIndex;
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void cvToStr() {
			if (module == NULL) {
				snprintf(text, 4, " - ");
			}	
			else if (module->params[FourView::MODE_PARAM].getValue() >= 0.5f) {// chord mode
				snprintf(text, 4, "%s", &module->displayChord[baseIndex<<2]);
			}
			else {// note mode
				if (module->displayValues[baseIndex] != module->unusedValue) {
					float cvVal = module->displayValues[baseIndex];
					printNote(cvVal, text, module->showSharp);
				}
				else {
					snprintf(text, 4, " - ");
				}
			}
		}

		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (!(font = APP->window->loadFont(fontPath))) {
					return;
				}
				nvgFontSize(args.vg, 17);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, -1.5);

				Vec textPos = VecPx(7.0f, 23.4f);
				nvgFillColor(args.vg, nvgTransRGBA(displayColOn, 23));
				nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
				
				nvgFillColor(args.vg, displayColOn);
				cvToStr();
				nvgText(args.vg, textPos.x, textPos.y, text, NULL);
			}
		}
	};

	struct InteropSeqItem : MenuItem {
		struct InteropCopySeqItem : MenuItem {
			FourView *module;
			void onAction(const event::Action &e) override {
				module->interopCopySeq();
			}
		};
		struct InteropCopyChordItem : MenuItem {
			FourView *module;
			void onAction(const event::Action &e) override {
				module->interopCopyChord();
			}
		};

		FourView *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			InteropCopyChordItem *interopCopyChordItem = createMenuItem<InteropCopyChordItem>("Copy chord", "");
			interopCopyChordItem->module = module;
			menu->addChild(interopCopyChordItem);		
			
			InteropCopySeqItem *interopCopySeqItem = createMenuItem<InteropCopySeqItem>("Copy chord as sequence", "");
			interopCopySeqItem->module = module;
			menu->addChild(interopCopySeqItem);		

			return menu;
		}
	};	
	void appendContextMenu(Menu *menu) override {
		FourView *module = dynamic_cast<FourView*>(this->module);
		assert(module);
		
		menu->addChild(new MenuSeparator());
		
		createPanelThemeMenu(menu, &(module->panelTheme), &(module->panelContrast), (SvgPanel*)getPanel());

		InteropSeqItem *interopSeqItem = createMenuItem<InteropSeqItem>(portableSequenceID, RIGHT_ARROW);
		interopSeqItem->module = module;
		menu->addChild(interopSeqItem);		

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings"));
		
		menu->addChild(createCheckMenuItem("Allow poly in 1 to override", "",
			[=]() {return module->allowPolyOverride == 1;},
			[=]() {module->allowPolyOverride ^= 0x1;}
		));
		
		menu->addChild(createBoolPtrMenuItem("Sharp (unchecked is flat)", "", &module->showSharp));
	}	
	
	
	FourViewWidget(FourView *module) {
		setModule(module);
		int* mode = module ? &module->panelTheme : NULL;
		float* cont = module ? &module->panelContrast : NULL;
		
		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/FourView.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		svgPanel->fb->addChildBottom(new PanelBaseWidget(svgPanel->box.size, cont));
		svgPanel->fb->addChild(new InverterWidget(svgPanel->box.size, mode));	

		// Screws
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 0), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(15, 365), mode));
		svgPanel->fb->addChild(createDynamicScrew<IMScrew>(VecPx(box.size.x-30, 365), mode));

		const float centerX = box.size.x / 2;
		static const int rowRulerTop = 66;
		static const int spacingY = 44;
		static const float offsetXL = 30;
		static const float offsetXR = 20;
		
		// Notes displays and inputs
		NotesDisplayWidget* displayNotes[4];
		for (int i = 0; i < 4; i++) {
			displayNotes[i] = new NotesDisplayWidget(VecPx(centerX + offsetXR, rowRulerTop + i * spacingY), VecPx(52, 29), module, i);
			addChild(displayNotes[i]);
			svgPanel->fb->addChild(new DisplayBackground(displayNotes[i]->box.pos, displayNotes[i]->box.size, mode));

			addInput(createDynamicPortCentered<IMPort>(VecPx(centerX - offsetXL, rowRulerTop + i * spacingY), true, module, FourView::CV_INPUTS + i, mode));	
		}


		// Display mode switch
		addParam(createDynamicSwitchCentered<IMSwitch2H>(VecPx(centerX, 234), module, FourView::MODE_PARAM, mode, svgPanel));		


		static const int spacingY2 = 46;
		static const float offsetX = 26;
		static const int posY2 = 285;

		// Thru outputs
		addOutput(createDynamicPortCentered<IMPort>(VecPx(centerX - offsetX, posY2), false, module, FourView::CV_OUTPUTS + 0, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(centerX + offsetX, posY2), false, module, FourView::CV_OUTPUTS + 1, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(centerX - offsetX, posY2 + spacingY2), false, module, FourView::CV_OUTPUTS + 2, mode));
		addOutput(createDynamicPortCentered<IMPort>(VecPx(centerX + offsetX, posY2 + spacingY2), false, module, FourView::CV_OUTPUTS + 3, mode));
	}
	
	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			if (e.key == GLFW_KEY_C) {
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					((FourView*)module)->interopCopyChord();
					e.consume(this);
					return;
				}
				else if ((e.mods & RACK_MOD_MASK) == (GLFW_MOD_SHIFT | GLFW_MOD_ALT)) {
					((FourView*)module)->interopCopySeq();
					e.consume(this);
					return;
				}						
			}
		}
		ModuleWidget::onHoverKey(e);
	}
	
	void step() override {
		if (module) {
			int panelTheme = (((FourView*)module)->panelTheme);
			float panelContrast = (((FourView*)module)->panelContrast);
			if (panelTheme != lastPanelTheme || panelContrast != lastPanelContrast) {
				SvgPanel* svgPanel = (SvgPanel*)getPanel();
				svgPanel->fb->dirty = true;
				lastPanelTheme = panelTheme;
				lastPanelContrast = panelContrast;
			}
		}
		Widget::step();
	}

};

Model *modelFourView = createModel<FourView, FourViewWidget>("Four-View");
