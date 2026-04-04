#pragma once

#include "audio-cv-in.h"
#include "midi-parser.h"
#include "pulse.h"

class Inputs {
public:
	explicit Inputs(Pulse* shared_pulse = nullptr)
		: audio_cv(),
		  owned_pulse_(),
		  pulse(shared_pulse ? *shared_pulse : owned_pulse_) {}

	bool init_audio_cv() {
		return audio_cv.init();
	}

	void init_pulse() {
		pulse.begin();
	}

	bool init() {
		init_pulse();
		return init_audio_cv();
	}

	void update_audio_cv() {
		audio_cv.update();
	}

	void poll_pulse() {
		pulse.poll();
	}

	void update() {
		update_audio_cv();
		poll_pulse();
	}

	AudioCvIn audio_cv;
	Pulse& pulse;

private:
	Pulse owned_pulse_;
};
