### 2.0.8 (in development)

- Swap accidental and octave characters in displays that can print notes (FourView, Part, CVPad, WriteSeq64, Foundry, but not WriteSeq32 to keep its display more readable)


### 2.0.7 (2021-12-26)

- Expose red and green led colors in ImpromptuModular.json for improved accessibility tweaking
- Re-code menus so that cmd/ctrl-click can be used to change options without closing the menus
- Clkd/Clocked: add menu option to force BPM out to be a BPM CV when using an external clock on the BPM input (sync mode)


### 2.0.6 (2021-11-27)

- First v2 library candidate
- Many changes that are not listed here


### 1.1.12 (2021-x-x)

- ProbKey: Bumped the lock buffer' maximum length to 32 steps
- ProbKey: Implemented randomization in module's menu, which randomizes the lock buffer according to the current settings
- ProbKey: Changed the pGain knob so that it now works like a density knob
- ProbKey: Added poly capabilities to the offset, density and squash cv inputs (when using a poly gate in)
- ProbKey: Added tracer lights in the probability edit mode, to show the note that is playing
- ProbKey: Added a manual step-lock menu to allow locking individual steps
- PhraseSeq16/32, SMS16, Foundry: fixed obscur bug with octave changes after transpositions (float dust)


### 1.1.11 (2021-06-26)

- Implemented portable sequence copy/paste in WriteSeq32/64
- Fixed TwelveKey bug with tracer key not saved/loaded with patch
- Added new module called ProbKey


### 1.1.10 (2021-02-07)

- Fixed parameter entry in Clkd and Clocked ratio knobs, and improved tooltip on main BPM knob when external BPM
- Added lock option to GateSeq64 to prevent steps, gate types and gate p from being clicked
- Added x96 clock ratio to Clocked and Clkd
- Added light version of Blank panel and touched up dark theme


### 1.1.9 (2020-09-17)

- Added a new module called TactG, which is a Tact1 with a gate output and two offset controls
- Reduced width of blank panel from 10 HP to 9 HP


### 1.1.8 (2020-07-19)

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
