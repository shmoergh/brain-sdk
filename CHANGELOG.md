# Changelog

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