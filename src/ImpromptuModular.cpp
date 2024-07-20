//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Pyer 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************


#include "ImpromptuModular.hpp"


Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	readThemeAndContrastFromDefault();

	p->addModel(modelAdaptiveQuantizer);
	p->addModel(modelBigButtonSeq);
	p->addModel(modelBigButtonSeq2);
	p->addModel(modelChordKey);
	p->addModel(modelChordKeyExpander);
	p->addModel(modelClocked);
	p->addModel(modelClockedExpander);
	p->addModel(modelClkd);
	p->addModel(modelClkd2);
	p->addModel(modelCvPad);
	p->addModel(modelFoundry);
	p->addModel(modelFoundryExpander);
	p->addModel(modelFourView);
	p->addModel(modelGateSeq64);
	p->addModel(modelGateSeq64Expander);
	p->addModel(modelHotkey);
	p->addModel(modelNoteEcho);
	p->addModel(modelNoteLoop);
	p->addModel(modelPart);
	p->addModel(modelPhraseSeq16);
	p->addModel(modelPhraseSeq32);
	p->addModel(modelPhraseSeqExpander);
	p->addModel(modelProbKey);
	p->addModel(modelSygen);
	p->addModel(modelTact);
	p->addModel(modelTact1);
	p->addModel(modelTactG);
	p->addModel(modelTwelveKey);
	p->addModel(modelVariations);
	p->addModel(modelWriteSeq32);
	p->addModel(modelWriteSeq64);
	p->addModel(modelBlankPanel);
}



// General objects

ClockMaster clockMaster;  




// General functions

static const char noteLettersSharp[12] = {'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B'};
static const char noteLettersFlat [12] = {'C', 'D', 'D', 'E', 'E', 'F', 'G', 'G', 'A', 'A', 'B', 'B'};
static const char isBlackKey      [12] = { 0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0 };

void printNoteNoOct(int note, char* text, bool sharp) {// text must be at least 3 chars long (three displayed chars plus end of string)
	// given note is a pitch CV multiplied by 12 and rounded to nearest integer
	note = eucMod(note, 12);
	text[0] = sharp ? noteLettersSharp[note] : noteLettersFlat[note];// note letter
	text[1] = (isBlackKey[note] == 1) ? (sharp ? '\"' : 'b' ) : ' ';// sharp/flat
	text[2] = 0;
}


int printNoteOrig(float cvVal, char* text, bool sharp) {// text must be at least 4 chars long (three displayed chars plus end of string)
	// return cursor position of eos
	
	int indexNote;
	int octave;
	calcNoteAndOct(cvVal, &indexNote, &octave);
	
	// note letter
	text[0] = sharp ? noteLettersSharp[indexNote] : noteLettersFlat[indexNote];
	int cursor = 1;
	
	// octave number
	octave += 4;
	if (octave >= 0 && octave <= 9) {
		text[cursor] = (char) ( 0x30 + octave);
		cursor++;
	}
	
	// sharp/flat
	if (isBlackKey[indexNote] == 1) {
		text[cursor] = (sharp ? '\"' : 'b' );
		cursor++;
	}
	
	text[cursor] = 0;
	return cursor;
}


int printNote(float cvVal, char* text, bool sharp) {// text must be at least 4 chars long (three displayed chars plus end of string)
	// return cursor position of eos
	
	int indexNote;
	int octave;
	calcNoteAndOct(cvVal, &indexNote, &octave);
	
	// note letter
	text[0] = sharp ? noteLettersSharp[indexNote] : noteLettersFlat[indexNote];
	int cursor = 1;
	
	// sharp/flat
	if (isBlackKey[indexNote] == 1) {
		text[cursor] = (sharp ? '\"' : 'b' );
		cursor++;
	}
	
	// octave number
	octave += 4;
	if (octave >= 0 && octave <= 9) {
		text[cursor] = (char) ( 0x30 + octave);
		cursor++;
	}
	
	text[cursor] = 0;
	return cursor;
}


int moveIndex(int index, int indexNext, int numSteps) {
	if (indexNext < 0)
		index = numSteps - 1;
	else
	{
		if (indexNext - index >= 0) { // if moving right or same place
			if (indexNext >= numSteps)
				index = 0;
			else
				index = indexNext;
		}
		else { // moving left 
			if (indexNext >= numSteps)
				index = numSteps - 1;
			else
				index = indexNext;
		}
	}
	return index;
}


void InstantiateExpanderItem::onAction(const event::Action &e) {
	// Create Module and ModuleWidget
	module = model->createModule();
	APP->engine->addModule(module);

	ModuleWidget* mw = model->createModuleWidget(module);
	// APP->scene->rack->addModuleAtMouse(mw);
	
	// ModuleWidget *mw = model->createModuleWidget();
	if (mw) {
		APP->scene->rack->setModulePosNearest(mw, posit);
		APP->scene->rack->addModule(mw);
		history::ModuleAdd *h = new history::ModuleAdd;
		h->name = "create expander module";
		h->setModule(mw);
		APP->history->push(h);
	}
}


void NormalizedFloat12Copy(float* float12) {
	json_t* normFloats12J = json_object();
	json_t *normFloats12ArrayJ = json_array();
	for (int i = 0; i < 12; i++) {
		json_array_insert_new(normFloats12ArrayJ, i, json_real(float12[i]));
	}
	json_object_set_new(normFloats12J, "normalizedFloats", normFloats12ArrayJ);

	json_t* clipboardJ = json_object();		
	json_object_set_new(clipboardJ, "Impromptu normalized float12", normFloats12J);
	char* float12Clip = json_dumps(clipboardJ, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
	json_decref(clipboardJ);
	glfwSetClipboardString(APP->window->win, float12Clip);
	free(float12Clip);
}

void NormalizedFloat12Paste(float* float12) { 
	const char* float12Clip = glfwGetClipboardString(APP->window->win);
	if (!float12Clip) {
		WARN("Normalized float12 error getting clipboard string");
	}
	else {
		json_error_t error;
		json_t* clipboardJ = json_loads(float12Clip, 0, &error);
		if (!clipboardJ) {
			WARN("Normalized float12 error json parsing clipboard");
		}
		else {
			DEFER({json_decref(clipboardJ);});
			json_t* normFloats12J = json_object_get(clipboardJ, "Impromptu normalized float12");
			if (!normFloats12J) {
				WARN("Error no Impromptu normalized float12 values present in clipboard");
			}
			else {
				json_t *normFloats12ArrayJ = json_object_get(normFloats12J, "normalizedFloats");
				if (normFloats12ArrayJ && json_is_array(normFloats12ArrayJ)) {
					for (int i = 0; i < 12; i++) {
						json_t *normFloatsItemJ = json_array_get(normFloats12ArrayJ, i);
						if (normFloatsItemJ) {
							float12[i] = json_number_value(normFloatsItemJ);
						}
					}
				}
			}
		}	
	}	
}