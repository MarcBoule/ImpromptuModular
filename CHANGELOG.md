### 1.1.3 (in development)

- Fixed random in tied steps in PhraseSeq16/32, SemiModularSynth and Foundry, i.e. if head step has a probability, make succesive tied steps follow this probability also such that it is one big tied step with one single probability
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
- Redo and improve layout of Four-View


### 1.1.0 (2019-07-04)

- Added option in right-click menu to inverty velocity range in TwelveKey
- Reinstated right-click of keys to perform autostep when entering notes in Foundry, PhraseSeq16/32 and SemiModularSynth (ctrl-right-click does the same but also copies the note over to new step)


### 1.0.1 (2019-06-23)

- Added velocity to TwelveKey
- Fixed mouse painting bug in GateSeq64
- Reninstated expander launching in right-click menu of modules that have expanders


### 1.0.0 (2019-05-31)

- Initial release for Rack v1
