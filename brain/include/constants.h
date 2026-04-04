#pragma once

#include <cstdint>

#include "common.h"
#include "gpio-setup.h"

constexpr unsigned int kMidiRxPin = GPIO_BRAIN_MIDI_RX;

constexpr unsigned int kLedPin1 = GPIO_BRAIN_LED_1;
constexpr unsigned int kLedPin2 = GPIO_BRAIN_LED_2;
constexpr unsigned int kLedPin3 = GPIO_BRAIN_LED_3;
constexpr unsigned int kLedPin4 = GPIO_BRAIN_LED_4;
constexpr unsigned int kLedPin5 = GPIO_BRAIN_LED_5;
constexpr unsigned int kLedPin6 = GPIO_BRAIN_LED_6;

constexpr unsigned int kButtonPin1 = GPIO_BRAIN_BUTTON_1;
constexpr unsigned int kButtonPin2 = GPIO_BRAIN_BUTTON_2;
constexpr unsigned int kButtonLedPin1 = GPIO_BRAIN_BUTTON_1_LED;

constexpr unsigned int kPulseInputPin = GPIO_BRAIN_PULSE_INPUT;
constexpr unsigned int kPulseOutputPin = GPIO_BRAIN_PULSE_OUTPUT;

constexpr unsigned int kPotAdcPin = GPIO_BRAIN_POTMUX_ADC;
constexpr unsigned int kPotS0Pin = GPIO_BRAIN_POTMUX_S0;
constexpr unsigned int kPotS1Pin = GPIO_BRAIN_POTMUX_S1;

constexpr unsigned int kAudioCvOutSckPin = GPIO_BRAIN_AUDIO_CV_OUT_SCK;
constexpr unsigned int kAudioCvOutTxPin = GPIO_BRAIN_AUDIO_CV_OUT_TX;
constexpr unsigned int kAudioCvOutCsPin = GPIO_BRAIN_AUDIO_CV_OUT_CS;
constexpr unsigned int kAudioCvOutCouplingAPin = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A;
constexpr unsigned int kAudioCvOutCouplingBPin = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B;

constexpr unsigned int kAudioCvInAPin = GPIO_BRAIN_AUDIO_CV_IN_A;
constexpr unsigned int kAudioCvInBPin = GPIO_BRAIN_AUDIO_CV_IN_B;
