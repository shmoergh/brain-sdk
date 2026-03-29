# Test Apps (`test`)

Purpose: reusable manual hardware test apps for Brain SDK components.

## Available Test Apps
- `LedsTest` (`apps/leds_test.cpp`)
- `MidiToCvTest` (`apps/midi_to_cv_test.cpp`)
- `MultipotTest` (`apps/multipot_test.cpp`)
- `StorageTest` (`apps/storage_test.cpp`)

## Switching Active App

The test firmware entrypoint is in `test/main.cpp`.

To switch tests:
1. Update the include in `test/main.cpp` to the desired app header from `test/apps/`.
2. Update the instantiated class in `main()`.
3. Rebuild with:
```bash
cmake --build build --target test -j4
```

## Development

When adding new test apps, follow the same conventions used by `LedsTest`:

1. Communicate through UART/terminal.
2. Clear the terminal screen at startup.
3. Print a startup menu/list of available tests.
4. Wait for user selection before running tests.
5. Before running each test step, wait for explicit user prompt/confirmation (hardware interaction requires focus on LEDs/controls).
6. Execute the selected test(s) and report outcomes as `PASS` / `FAIL` / `SKIP`.
7. Print a final summary of results at the end of a run.

These conventions keep manual hardware validation consistent and easier to operate.
