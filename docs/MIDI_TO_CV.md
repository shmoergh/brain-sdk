# MidiToCV

## Overview
`MidiToCV` is a turnkey MIDI-to-CV helper built on `MidiParser` + `Outputs`.

- pitch CV is 1V/octave
- gate uses pulse output
- second CV output is mode-dependent (`kDefault`, `kModWheel`, `kUnison`, `kDuo`)

## Include
```cpp
#include "brain/include/midi-to-cv.h"
```

You can use either:
- `MidiToCV` (global alias)
- `brain::utils::MidiToCV`

## Quick Start
```cpp
MidiToCV midi_to_cv;

if (!midi_to_cv.init(AudioCvOutChannel::kChannelA, 1)) {
	return;
}

while (true) {
	midi_to_cv.update();
	sleep_ms(1);
}
```

## API Highlights
- `bool init(AudioCvOutChannel cv_channel, uint8_t midi_channel)`
- `void update()`
- `void set_midi_channel(uint8_t midi_channel)`
- `void set_pitch_channel(AudioCvOutChannel cv_channel)`
- `void set_mode(Mode mode)` / `Mode get_mode() const`
- `bool is_note_playing()`

Callbacks:
- `set_note_on_callback(...)`
- `set_note_off_callback(...)`
- `set_control_change_callback(...)`

Calibration:
- `enable_calibrated_output(bool load_from_flash = true)`
- `disable_calibrated_output()`
- `set_cv_calibration(const CvCalibrationV1&)`
- `is_calibrated_output_enabled() const`

## CV Mapping
- MIDI note 24 (C1) maps to 0V
- 12 semitones = 1000mV
- 1 semitone ≈ 83.33mV

Internal output writes are millivolt-based.
