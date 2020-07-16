### 1.1.8 (in development)

- Made LED displays show when using the Lights Out module (Modular Fungi plugin)
- Add "CV input viewer" option to TwelveKey


### 1.1.7 (2020-07-01)

- Converted all SVGs from px to mm
- Alphabetized modules in module browser and manual
- Added option in WriteSeq32/64 to make arrow controls rotate instead of step (step is default)
- Added auto-return options to Tact and Tact1
- Added tracer option to TwelveKey
- Added auto-patching Clocked and Clkd to a designated clock master


### 1.1.6 (2020-05-04)

- Fixed bug in Foundry CUST selection (custom length selection for SEL button and copy-paste)
- Added +/- suffix in Part's split point display when showing notes (and fixed a note display bug)
- Added CV through in Part for chaining (or else in some cases a 1-sample glitch can occur because of signal delays)
- Added option to Clkd to send triggers instead of gates (in Clocked, setting PW at minimum can be used to accomplish this)


### 1.1.5 (2020-03-25)

- Changed plugin name from "Impromptu Modular" to "Impromptu" (slug unchanged to keep patch compatibility)
- Added new module called Part, which splits a gate signal into two cables according to a split point and a CV input
- Added expander for ChordKey which offers 4 poly quantizers that quantize to the chord's notes
- Added chord mode to FourView to show chord names
- Modified Clkd knobs to change display when clicking the knob (even if it doesn't turn), and fixed the tooltips
- Improved reset options in Clocked/Clkd for internal reset vs emitted reset pulse
- Implemented the Portable Sequence copy/paste in BigButtonSeq2
- Made BigButtonSeq2 not overwrite CVs when CV input is unconnected during big button hits and fills to memory
- Increased refresh rate of signals going through expander mechanism


### 1.1.4 (2020-02-28)

- Allowed FourView to function as an expander for ChordKey and CVPad
- Added new module called ChordKey: a keyboard-based chord generator 
- Implemented Portable sequence copy/paste in Foundry, PhraseSeq16/32 and SemiModularSynth
- Added new module called Clkd: a half-sized version of Clocked without PW, Swing nor Delay
- Reduced width of CVPad by 2 HP
- Fixed bug in Foundry when using expander to sync select sequences with different run modes
- Fixed bug in divided clocks in Clocked when launching Rack


### 1.1.3 (2019-11-27)

- Added a new module called CV-Pad
- Fixed random in tied steps in PhraseSeq16/32, SemiModularSynth and Foundry, i.e. if head step has a probability, make successive tied steps follow this probability also such that it is one big tied step with one single probability
- Reduced range of randomization of notes in all sequencers when module is randomized
- Added an option in Clocked to make the Run CV input level sensitive, default is trigger sensitive as before


### 1.1.2 (2019-10-27)

- Changed "Reset when run is turned off" to "Restart when run is:" with a submenu with choices "turned off", "turned on" and "neither"; also offer a separate menu choice for emitting the reset pulse when a restart is activated
- Added option to Foundry to poly merge other tracks into track A outputs
- Added keyboard shortcuts to SEQ displays in all PhraseSeq, GateSeq and Foundry sequencers: when mouse is over the display, sequence number can be directly typed in; when in song mode, space bar moves to the next phrase in the song; key presses within the span of one second will register as a two digit sequence number
- Change to GPLv3 license and restart github repository fresh (now back to master branch)


### 1.1.1 (2019-08-03)

- Fixed WriteSeq32/64 gate outputs when not running (they are now off but will give a pulse on every write or movement of the active step)
- Disallow random sequence length of 1 in all Phrase/Gate sequencers, and Foundry
- Added option for chaining velocity settings in TwelveKey via right-click menu
- Redo and improve layout of 4-View


### 1.1.0 (2019-07-04)

- Added option in right-click menu to inverty velocity range in TwelveKey
- Reinstated right-click of keys to perform autostep when entering notes in Foundry, PhraseSeq16/32 and SemiModularSynth (ctrl-right-click does the same but also copies the note over to new step)


### 1.0.1 (2019-06-23)

- Added velocity to TwelveKey
- Fixed mouse painting bug in GateSeq64
- Reninstated expander launching in right-click menu of modules that have expanders


### 1.0.0 (2019-05-31)

- Initial release for Rack v1
