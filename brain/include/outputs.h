#pragma once

#include "audio-cv-out.h"
#include "pulse.h"

class Outputs {
public:
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

	AudioCvOut audio_cv;
	Pulse pulse;
};
