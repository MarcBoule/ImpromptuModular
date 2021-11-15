//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc BoulĂ©
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "PianoKey.hpp"


// ******** PianoKey ********

void PianoKey::onButton(const event::Button &e) {
	if ( (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT) && pkInfo) {
		if (e.action == GLFW_PRESS) {
			pkInfo->isRightClick = e.button == GLFW_MOUSE_BUTTON_RIGHT;
			pkInfo->key = keyNumber;
			pkInfo->gate = true;// this should be last because multithreading
			e.consume(this);
			return;
		}
		// else if (e.action == GLFW_RELEASE) {// subsumed by onDragEnd() which is more general
		// }
	}
	OpaqueWidget::onButton(e);
}


void PianoKey::onDragEnd(const event::DragEnd &e) {// required since if mouse button release happens outside the box of the opaque widget, it will not trigger onButton (to detect GLFW_RELEASE)
	if ( (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT) && pkInfo) {
		pkInfo->gate = false;
	}
	e.consume(this);
}

// void PianoKey::draw(const DrawArgs& args) {
	// nvgBeginPath(args.vg);
	// nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
	// nvgStrokeColor(args.vg, nvgRGBA(255,0,0,127));
	// nvgStrokeWidth(args.vg, 0.5f);
	// nvgStroke(args.vg);
// }

// ******** PianoKeyWithVel ********

void PianoKeyWithVel::draw(const DrawArgs &args) {
	static const float xSize = 10.0f;
	if (pkInfo && pkInfo->showMarks != 0) {
		const float xPos = (box.size.x - xSize) / 2.0f;
		float col = isBlackKey ? 0.4f : 0.5f;
		NVGcolor borderColor = nvgRGBf(col, col, col);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, xPos, 0.5f); nvgLineTo(args.vg, xPos + xSize, 0.5f);// top
		for (int y = 1; y < pkInfo->showMarks; y++) {
			float yPos = (box.size.y * (float)y) / (float)pkInfo->showMarks;
			nvgMoveTo(args.vg, xPos, yPos); nvgLineTo(args.vg, xPos + xSize, yPos);// mids
		}
		if (isBlackKey) {
			nvgMoveTo(args.vg, xPos, box.size.y - 0.5f); nvgLineTo(args.vg, xPos + xSize, box.size.y - 0.5f);// bot
		}
		nvgStrokeColor(args.vg, borderColor);
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);
	}
	PianoKey::draw(args);
}

void PianoKeyWithVel::onButton(const event::Button &e) {
	onButtonMouseY = APP->scene->rack->getMousePos().y;
	onButtonPosY = e.pos.y;
	if ( (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT) && pkInfo) {
		if (e.action == GLFW_PRESS) {
			float newVel = rescale(e.pos.y, box.size.y, 0.0f, 1.0f, 0.0f);// do this before parent's onButton()
			pkInfo->vel = clamp(newVel, 0.0f, 1.0f);// should not be needed, but best to keep in case click precision is not perfect	
		}
	}
	PianoKey::onButton(e);
}

void PianoKeyWithVel::onDragMove(const event::DragMove &e) {
	if ( (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT) && pkInfo) {
		float dragMouseY = APP->scene->rack->getMousePos().y;
		float newVel = rescale(onButtonPosY + dragMouseY - onButtonMouseY, box.size.y, 0.0f, 1.0f, 0.0f);
		pkInfo->vel = clamp(newVel, 0.0f, 1.0f);
	}
	e.consume(this);
}
