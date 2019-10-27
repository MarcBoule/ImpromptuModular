This is the code structure used in an Impromptu sequencer. The code excerpt shows how resets, clocks and run states are implemented. The following concepts should be visible in the code below:
* 1ms-clock-ignore-on-reset (and initialize / power-up)
* Clock muting when run is off
* Gate retriggering on reset

```c++
void initRun() {
	// reposition run head to first step
	// ...
}


void MyModule::onReset() override {
	// initialize sequencer variables
	// ...
	running = true;
	initRun();
	clockIgnoreOnReset = (long) (0.001f * APP->engine->getSampleRate());// useful when Rack starts
}


void MyModule::fromJson(json_t *rootJ) override {
	// load sequencer variables
	// ...
	initRun();
}


void MyModule::process(const ProcessArgs &args) override {
	// Run button
	if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage())) {
		running = !running;
		if (running) {
			if (resetOnRun) {// this is an option offered in the right-click menu of the sequencer
				initRun();
				clockIgnoreOnReset = (long) (0.001f * args.sampleRate);
			}
		}
		// ...
	}

	// Process user interactions (buttons, knobs, etc)
	// ...
	
	// Clock
	if (running && clockIgnoreOnReset == 0) {// clock muting and 1ms-clock-ignore-on-reset
		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			// advance the sequencer
			// ...
		}
	}	
	
	// Reset
	if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
		initRun();
		clockIgnoreOnReset = (long) (0.001f * args.sampleRate);
		clockTrigger.reset();
	}
	
	// Outputs
	outputs[CV_OUTPUT].setVoltage( cv );
	outputs[GATE_OUTPUT].setVoltage( (gate && (clockIgnoreOnReset == 0)) ? 10.f : 0.f );// gate retriggering on reset
			
	// Set the lights of the sequencer 
	// ...
			
	if (clockIgnoreOnReset > 0l)
		clockIgnoreOnReset--;
}

```
