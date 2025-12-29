/**
 * @file main.cpp
 * @brief SDK Test Program - Verifies all Brain SDK libraries compile and link correctly
 *
 * This program exercises all components of the Brain SDK to ensure the build system
 * is configured correctly. It initializes all modules and demonstrates basic usage.
 */

#include <stdio.h>
#include "pico/stdlib.h"

// Include Brain SDK headers
#include "brain-common/brain-common.h"
#include "brain-utils/ringbuffer.h"
#include "brain-utils/midi-to-cv.h"

// Include specific components to verify they're accessible
#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-io/pulse.h"
#include "brain-io/midi-parser.h"
#include "brain-ui/button.h"
#include "brain-ui/led.h"
#include "brain-ui/pots.h"

int main() {
    // Initialize standard I/O
    stdio_init_all();

    printf("Brain SDK Test Program\n");
    printf("======================\n\n");

    printf("All libraries compiled and linked successfully.\n\n");

    // Test accessing SDK constants
    printf("SDK Constants Test:\n");
    printf("- Audio sample rate: %lu Hz\n", brain::constants::kDefaultAudioSampleRate);
    printf("- ADC max value: %u\n", brain::constants::kAdcMaxValue);
    printf("- ADC voltage ref: %.2fV\n", brain::constants::kAdcVoltageRef);

    printf("\nSDK test program running. Press Ctrl+C to exit.\n");

	brain::utils::MidiToCV midiToCV;
	midiToCV.init(brain::io::AudioCvOutChannel::kChannelA, 11);

    // Main loop - blink LED to show it's running
    while (true) {
		midiToCV.update();
        sleep_ms(1000);
    }

    return 0;
}
