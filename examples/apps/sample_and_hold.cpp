#include "sample_and_hold.h"

void SampleAndHold::init() {
	// Init required modules (inputs & outputs)
	bool initSucceeded = true;

	initSucceeded &= brain_init_succeeded(brain_.init_inputs());
	initSucceeded &= brain_init_succeeded(brain_.init_outputs());

	if (!initSucceeded) {
		printf("Init failed.");
		return;
	}

	// Set output range (simulate AC coupling)
	brain_.outputs.set_output_range(
		kOutputsChannelA,
		kOutputsRangeMinus5To5V
	);

	// Read Input A and set Output A to the same value on pulse rise edge
	// You need to register callback functions only once
	brain_.inputs.pulse_on_rise([this]() {
		int32_t channelAMilliVolts = brain_.inputs.get_voltage_millivolts_channel_a();
		brain_.outputs.set_voltage_millivolts(kOutputsChannelA, channelAMilliVolts);
	});

	initialized_ = true;
}

void SampleAndHold::update() {
	if (!initialized_) return;

	// Update analog inputs + pulse edge processing
	brain_.update_inputs();
}