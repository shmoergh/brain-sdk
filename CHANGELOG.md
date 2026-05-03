# Changelog

### 2.1

Internal refactor of the ADC and DAC paths. Public API stays source-compatible with 2.0; most firmwares need no changes. See [`docs/2.1_MIGRATION.md`](docs/2.1_MIGRATION.md) for the migration walk-through.

- New internal `AdcEngine` (sole ADC owner) running continuous DMA-paced round-robin sampling over `(POT, IN1, IN2)`. `Inputs` and `Pots` become thin readers over its snapshots.
- New internal `OutputEngine` (sole DAC owner) running a single ring-DMA channel paced by a hardware timer. On RP2350 the channel runs in `ENDLESS` mode — zero IRQs, zero CPU work, zero rate drift between input and output sample clocks. On RP2040 it runs autonomously for ~14 hours per cycle.
- `AudioProcessor` reimplemented as a thin shim over the shared engines. The audio loop runs from the ADC's per-sample IRQ at the configured rate. Public `init`, `stop`, `get_stats`, `get_pot_raw_u8`, `AudioProcessorConfig`, `AudioProcessorFrame`, and `AudioProcessorStats` are preserved unchanged.
- New v2 stereo audio API: `init_audio_processor_v2`, `AudioProcessorConfigV2`, `ProcessFrameFnV2`, `AudioProcessorFrameV2`. Stereo (IN1+IN2 → OUT A+OUT B) DSP in one callback with per-channel claim flags. Additive — legacy mono API keeps working.
- `AudioProcessor` no longer conflicts with `Inputs`, `Pots`, or `PotMultiFunction`. All combinations coexist on one `Brain` instance.
- Per-channel DAC ownership: `Outputs::set_voltage_*` is rejected on channels currently claimed by `AudioProcessor`. `Outputs::get_channel_owner` / `set_channel_owner` exposed for inspection.
- `AudioProcessorConfig::max_dma_drain_samples_per_tick` and `spi_baud_hz` ignored (preserved for source compat).
- `brain/include/adc-arbiter.h` and `BrainAdcLockGuard` removed (was used only by the legacy `AudioProcessor`'s ADC DMA path).

### 2.0

**Release date:** Apr 10 2025
Complete refactor for simplicity.

- Getting rid of complex include directories and namespaces (`brain::io`, `brain::ui` etc.)
- Monorepo structure changed from multi-library `lib/*` to one `brain` module
- Single `Brain` class as the top level wrapper class for all SDK component classes (e.g. `Button`, `Inputs`, `Outputs` etc.). Provides compile-time feature selection and centralized init/update helpers.
- Component classes can be instantiated in main `Brain` classes using `BRAIN_USE_*` defines for better memory usage
- Reorganized inputs and outputs to `Inputs` and `Outputs` classes, which include Audio/CV and Pulse I/O
- Centralized Brain utilities are available in main class
- New `AudioProcessor` class for DMA based ADC read (combining Audio/CV input and pot multiplexed reading)
- `AudioCvOut` voltage API changed from float volts to integer millivolts
- Output coupling API (`AC/DC`) was replaced by explicit output range API
- `AudioCvIn` voltage getters changed from float volts to integer millivolts
- `Led` and `ButtonLed` were folded into `Leds`

### 1.0

-