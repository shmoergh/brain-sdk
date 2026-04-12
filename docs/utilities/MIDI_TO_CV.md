# MIDI to CV Utility (`MidiToCV`)

## Overview: what is it for?
`MidiToCV` converts MIDI note/control data into Brain output voltages and gate behavior.
It sits on top of `MidiParser` + `Outputs`, so you get a ready-to-use MIDI-to-CV signal path instead of wiring everything manually.

It handles:

- note/gate behavior
- pitch CV generation
- mode-dependent secondary CV behavior
- optional calibrated output writes

## MIDI-to-CV behavior
The class listens to MIDI events and drives output channels based on selected mode.

Supported modes:

- `kMidiToCVModeDefault`
  Pitch on selected CV channel, velocity on the other channel.
- `kMidiToCVModeModWheel`
  Pitch on selected channel, mod wheel on the other channel.
- `kMidiToCVModeUnison`
  Pitch mirrored to both channels.
- `kMidiToCVModeDuo`
  Duophonic behavior with note-priority/latched fallback handling.

## Constants

- `kMidiToCVModeDefault`, `kMidiToCVModeModWheel`, `kMidiToCVModeUnison`, `kMidiToCVModeDuo`
  Mode selectors for `set_mode(...)`.
- `kOutputsChannelA`, `kOutputsChannelB`
  Output channel selectors for `init(...)` and `set_pitch_channel(...)`.

## Example for MIDI-to-CV (Brain wrapper)
```cpp
#define BRAIN_USE_STORAGE 1
#define BRAIN_USE_OUTPUTS 1
#define BRAIN_USE_MIDI_PARSER 1
#define BRAIN_USE_MIDI_TO_CV 1
#include "brain/brain.h"

#include <pico/stdlib.h>
#include <stdio.h>

Brain brain;

static void on_note_on(uint8_t note, uint8_t vel, uint8_t ch) {
	printf("Note On  ch=%u note=%u vel=%u\n", ch, note, vel);
}

static void on_note_off(uint8_t note, uint8_t vel, uint8_t ch) {
	printf("Note Off ch=%u note=%u vel=%u\n", ch, note, vel);
}

int main() {
	stdio_init_all();

	BrainInitStatus status = brain.init_midi_to_cv(
		kOutputsChannelA, // pitch channel
		1,                            // MIDI channel
		31250                         // baud
	);
	if (!brain_init_succeeded(status)) return 1;

	brain.midi_to_cv.set_mode(kMidiToCVModeDuo);
	brain.midi_to_cv.set_note_on_callback(on_note_on);
	brain.midi_to_cv.set_note_off_callback(on_note_off);

	while (true) {
		brain.update_midi_to_cv();
		sleep_ms(1);
	}
}
```

## MIDI-to-CV API methods categorized and explained

### Lifecycle and integration
- `void set_dependencies(Outputs* outputs, MidiParser* midi_parser)`
  Injects required dependencies for direct (non-wrapper) usage.
- `BrainInitStatus init(AudioCvOutChannel cv_channel, uint8_t midi_channel)`
  Initializes mapper with selected CV output channel + MIDI channel.
- `bool is_initialized() const`
  Returns initialization state.
- `void update()`
  Processes parser input and updates CV/gate outputs. Call in loop.

### Mode and channel configuration
- `void set_mode(Mode mode)`
  Selects conversion behavior mode.
- `Mode get_mode() const`
  Returns active mode.
- `void set_midi_channel(uint8_t midi_channel)`
  Changes MIDI channel target.
- `void set_pitch_channel(AudioCvOutChannel cv_channel)`
  Changes primary pitch output channel.

### Event callbacks
- `void set_note_on_callback(MidiParser::NoteOnCallback callback)`
  Sets user callback for note-on events.
- `void set_note_off_callback(MidiParser::NoteOffCallback callback)`
  Sets user callback for note-off events.
- `void set_control_change_callback(MidiParser::ControlChangeCallback callback)`
  Sets user callback for CC events.

### Gate / note state controls
- `void set_gate(bool state)`
  Forces gate state.
- `bool is_note_playing()`
  Returns whether note stack currently contains active note(s).
- `void reset_note_stack()`
  Clears internal note stack/tracking state.

### CV output behavior
- `void enable_cv()`
  Enables CV output writes.
- `void disable_cv()`
  Disables CV output writes.
- `void set_max_cc_voltage(uint8_t max_voltage)`
  Sets max voltage used for CC-to-CV scaling.

### Calibration-aware output controls
- `bool enable_calibrated_output(bool load_from_flash = true)`
  Enables calibrated output mode, optionally loading calibration from storage.
- `void disable_calibrated_output()`
  Disables calibrated output mode.
- `bool set_cv_calibration(const CvCalibrationV1& calibration)`
  Sets calibration payload explicitly.
- `bool is_calibrated_output_enabled() const`
  Returns calibrated output mode state.

### Protected extension hooks (for subclassing)
- `virtual void note_on(...)`
- `virtual void note_off(...)`
- `virtual void control_change(...)`
- `virtual void pitch_bend(...)`
  Override points for custom behavior in derived classes.

### Type alias
- `using MidiToCV = brain::utils::MidiToCV;`
  Global alias for convenience.

## Direct utility example (without `Brain`)
```cpp
#include "brain/include/midi-to-cv.h"
#include "brain/include/outputs.h"
#include "brain/include/midi-parser.h"

Outputs outputs;
MidiParser parser;
MidiToCV m2cv;

int main() {
	if (!outputs.init()) return 1;
	if (!parser.init_uart(31250)) return 1;

	m2cv.set_dependencies(&outputs, &parser);
	if (!brain_init_succeeded(m2cv.init(kOutputsChannelA, 1))) return 1;

	while (true) {
		m2cv.update();
	}
}
```

## API usage notes
- Wrapper path (`Brain::init_midi_to_cv`) is preferred for most apps.
- `update()` is mandatory; no updates means no MIDI processing and no CV changes.
- If behavior seems wrong, verify mode, MIDI channel, and output range/calibration settings first.
