# Sandbox Firmware (`sandbox`)

Purpose: quick manual testbed for Brain SDK components.

## Available apps
- `MidiToCvTest` (`apps/midi_to_cv_test.cpp`): MIDI-to-CV manual test app. Uses hardcoded MIDI channel/pitch channel/mode constants in source and prints note on/off events over USB serial.
- `MultipotTest` (`apps/multipot_test.cpp`): UI helper test app for `PotMultiFunction` mappings and button-selected functions with LED + serial feedback.

## Notes
- To switch apps, replace the include and instantiated class in `sandbox/main.cpp`.
- `sandbox/CMakeLists.txt` currently compiles both app source files into the `sandbox` target.

This firmware is intended as a reusable sandbox for validating Brain SDK building blocks before using them in product firmware.
