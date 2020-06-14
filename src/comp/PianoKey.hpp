//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc BoulĂ©
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#ifndef IM_PIANOKEY_HPP
#define IM_PIANOKEY_HPP


#include "../ImpromptuModular.hpp"

using namespace rack;


struct PianoKeyInfo {
	bool gate = false;// use a dsp::BooleanTrigger to detect rising edge
	bool isRightClick = false;
	int key = 0;// key number that was pressed, typically 0 to 11 is stored here
	float vel = 0.0f;// normalized velocity from 0.0f to 1.0f (max value at lowest screen position on key)
	int showMarks = 0;// 0 is no, integer means separate in how many sections
};


template <class TWidget>
TWidget* createPianoKey(Vec pos, int _keyNumber, PianoKeyInfo* _pkInfo) {
	TWidget *pkWidget = createWidget<TWidget>(pos);
	pkWidget->pkInfo = _pkInfo;
	pkWidget->keyNumber = _keyNumber;
	pkWidget->isBlackKey = (_keyNumber == 1 || _keyNumber == 3 || _keyNumber == 6 || _keyNumber == 8 || _keyNumber == 10);
	return pkWidget;
}


struct PianoKey : OpaqueWidget {
	int keyNumber = 0;
	bool isBlackKey = false;
	PianoKeyInfo *pkInfo = NULL;

	void onButton(const event::Button &e) override;
	void onDragEnd(const event::DragEnd &e) override;
};


struct PianoKeyWithVel : PianoKey {
	float onButtonMouseY;
	float onButtonPosY;
	
	void draw(const DrawArgs &args) override;
	void onButton(const event::Button &e) override;
	void onDragMove(const event::DragMove &e) override;
};


struct PianoKeyBig : PianoKeyWithVel {
	PianoKeyBig() {
		box.size = VecPx(34, 70);
	}
};


struct PianoKeySmall : PianoKey {
	PianoKeySmall() {
		box.size = VecPx(23, 38);
	}
};


#endif