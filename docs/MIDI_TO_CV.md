# MidiToCV Utility

## Overview
The MidiToCV utility provides a complete MIDI-to-CV converter implementation, translating MIDI note messages into 1V/octave CV pitch output and gate signals. It integrates the MIDI parser, DAC output, and pulse output with note priority handling.

## Features
- Converts MIDI note on/off to CV pitch (1V/octave) and gate
- Last-note priority with note stack (up to 25 notes)
- Configurable MIDI channel (1-16)
- Configurable CV output channel (A or B)
- Optional custom note on/off callbacks
- Integrated MIDI parser with UART
- Automatic gate timing
- Note stealing and priority handling
- Easy-to-use single utility class

## Usage

### Basic Setup
1. **Initialization**: Create a `MidiToCV` instance and call `init()`
2. **Update Loop**: Call `update()` regularly in your main loop
3. **Optional**: Register callbacks for custom note handling

### Example - Simple MIDI to CV
```cpp
#include "brain-utils/midi-to-cv.h"

brain::utils::MidiToCV midi_to_cv;

// Initialize: CV on channel A, listen to MIDI channel 1
if (!midi_to_cv.init(brain::io::AudioCvOutChannel::kChannelA, 1)) {
    // Initialization failed
    return;
}

while (true) {
    midi_to_cv.update();  // Process MIDI and update CV/gate
    sleep_ms(1);
}
```

### Example - With Custom Callbacks
```cpp
#include "brain-utils/midi-to-cv.h"

brain::utils::MidiToCV midi_to_cv;

void my_note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
    printf("Note On: %d, velocity: %d\n", note, velocity);
    // Custom handling here (LED, trigger, etc.)
}

void my_note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
    printf("Note Off: %d\n", note);
    // Custom handling here
}

midi_to_cv.init(brain::io::AudioCvOutChannel::kChannelA, 1);

// Set optional callbacks
midi_to_cv.set_note_on_callback(my_note_on);
midi_to_cv.set_note_off_callback(my_note_off);

while (true) {
    midi_to_cv.update();
    sleep_ms(1);
}
```

### Example - Changing Configuration
```cpp
#include "brain-utils/midi-to-cv.h"

brain::utils::MidiToCV midi_to_cv;
midi_to_cv.init(brain::io::AudioCvOutChannel::kChannelA, 1);

// Later, change MIDI channel
midi_to_cv.set_midi_channel(5);  // Listen to MIDI channel 5

// Or change CV output channel
midi_to_cv.set_pitch_channel(brain::io::AudioCvOutChannel::kChannelB);

while (true) {
    midi_to_cv.update();
    sleep_ms(1);
}
```

### Example - Note Detection
```cpp
#include "brain-utils/midi-to-cv.h"

brain::utils::MidiToCV midi_to_cv;
midi_to_cv.init(brain::io::AudioCvOutChannel::kChannelA, 1);

while (true) {
    midi_to_cv.update();

    // Check if a note is currently playing
    if (midi_to_cv.is_note_playing()) {
        // Gate is high, a note is active
        printf("Note is playing\n");
    }

    sleep_ms(10);
}
```

## API Reference

### Initialization
```cpp
bool init(brain::io::AudioCvOutChannel cv_channel, uint8_t midi_channel)
```
- Initialize MIDI parser, DAC, and gate output
- `cv_channel`: Which DAC channel to use for pitch CV (kChannelA or kChannelB)
- `midi_channel`: MIDI channel to listen on (1-16)
- Returns `true` if successful, `false` on error

### Runtime Configuration
```cpp
void set_midi_channel(uint8_t midi_channel)
```
- Change MIDI channel to listen on (1-16)

```cpp
void set_pitch_channel(brain::io::AudioCvOutChannel cv_channel)
```
- Change which DAC channel outputs pitch CV

### Update
```cpp
void update()
```
- Process incoming MIDI and update CV/gate outputs
- Call regularly in main loop (at least every few milliseconds)

### Status
```cpp
bool is_note_playing()
```
- Check if any notes are currently active
- Returns `true` if gate is high, `false` if no notes playing

### Callbacks
```cpp
void set_note_on_callback(NoteOnCallback callback)
```
- Register custom note-on handler
- Signature: `void callback(uint8_t note, uint8_t velocity, uint8_t channel)`

```cpp
void set_note_off_callback(NoteOffCallback callback)
```
- Register custom note-off handler
- Signature: `void callback(uint8_t note, uint8_t velocity, uint8_t channel)`

## How It Works

### CV Pitch Mapping (1V/Octave)
- MIDI note 24 (C1) = 0V
- Each semitone = 1/12 volt (~83.33mV)
- MIDI note 36 (C2) = 1V
- MIDI note 48 (C3) = 2V
- MIDI note 60 (C4) = 3V
- And so on...

Formula: `CV = (MIDI_NOTE - 24) / 12.0`

### Note Priority
- **Last-note priority**: Most recently played note takes precedence
- **Note stack**: Tracks up to 25 simultaneous notes
- **Note stealing**: When stack is full, oldest note is removed
- **Note release**: When a note is released, reverts to previous note if any

### Gate Output
- Gate goes HIGH when first note is pressed
- Gate stays HIGH while any notes are held
- Gate goes LOW when all notes are released
- Gate output uses the Pulse component on a dedicated GPIO

## Hardware Configuration
The MidiToCV utility uses:
- **MIDI Input**: UART1 on GPIO 5 (configured by MIDI parser)
- **CV Output**: One channel of the AudioCvOut DAC
- **Gate Output**: Pulse output GPIO (configured in brain-common.h)

## Advanced Usage

### Subclassing for Custom Behavior
You can subclass `MidiToCV` and override the protected methods:
```cpp
class MyCustomMidiToCV : public brain::utils::MidiToCV {
protected:
    void note_on(uint8_t note, uint8_t velocity, uint8_t channel) override {
        // Custom note-on logic
        // Don't forget to call parent if you want default CV/gate behavior
        MidiToCV::note_on(note, velocity, channel);
    }

    void note_off(uint8_t note, uint8_t velocity, uint8_t channel) override {
        // Custom note-off logic
        MidiToCV::note_off(note, velocity, channel);
    }
};
```

## Performance Notes
- UART-based MIDI input at 31250 baud
- Note stack uses static array (no dynamic allocation)
- CV updates are fast (SPI to DAC)
- Gate switching is instant (GPIO)
- Suitable for real-time performance
- Minimal latency from MIDI input to CV/gate output

## Common Use Cases
- **MIDI-to-CV Interface**: Convert MIDI keyboard/sequencer to CV
- **Eurorack MIDI Module**: Add MIDI control to modular synth
- **Pitch Control**: Drive VCO pitch from MIDI
- **Monophonic Synth**: Build a MIDI-controlled mono synth voice
- **Sequencer Interface**: Control sequences via MIDI

## Important Notes
- Uses DC-coupled DAC output for accurate CV
- 0V reference point is MIDI note 24 (C1)
- Maximum CV output is limited by DAC (10V = MIDI note 144)
- MIDI velocity is parsed but not used for CV (use callback if needed)
- Only responds to Note On/Off messages (not CC, pitch bend, etc.)
- Gate output is digital (high/low), not velocity-sensitive

## Integration with Other Components
MidiToCV internally uses:
- `brain::io::MidiParser` - MIDI message parsing
- `brain::io::AudioCvOut` - CV output via DAC
- `brain::io::Pulse` - Gate output

You can use these components independently for more control, or use MidiToCV for a turnkey solution.

## Troubleshooting
- **No CV output**: Check DAC initialization, verify MIDI channel matches
- **No gate**: Ensure Pulse GPIO is correct, check note stack
- **Wrong pitch**: Verify 1V/octave calibration, check MIDI note 24 = 0V
- **Stuck notes**: Call `update()` regularly, check MIDI cables
- **Polyphony issues**: MidiToCV is monophonic with last-note priority
