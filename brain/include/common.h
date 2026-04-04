#pragma once

#include <cstdint>

#include "gpio-setup.h"

#define BRAIN_LED_1 GPIO_BRAIN_LED_1
#define BRAIN_LED_2 GPIO_BRAIN_LED_2
#define BRAIN_LED_3 GPIO_BRAIN_LED_3
#define BRAIN_LED_4 GPIO_BRAIN_LED_4
#define BRAIN_LED_5 GPIO_BRAIN_LED_5
#define BRAIN_LED_6 GPIO_BRAIN_LED_6

#define BRAIN_BUTTON_1 GPIO_BRAIN_BUTTON_1
#define BRAIN_BUTTON_2 GPIO_BRAIN_BUTTON_2
#define BRAIN_BUTTON_1_LED GPIO_BRAIN_BUTTON_1_LED

constexpr uint32_t kMicrosPerSecond = 1000000;
constexpr uint32_t kMillisPerSecond = 1000;

constexpr uint16_t kAdcMaxValue = 4095;
constexpr float kAdcVoltageRef = 3.3f;

constexpr uint32_t kDefaultAudioSampleRate = 44100;
constexpr uint32_t kDefaultControlRate = 1000;

constexpr float kAudioCvInVoltageAtMinus5V = 0.240f;
constexpr float kAudioCvInVoltageAtPlus5V = 3.000f;

constexpr float kAudioCvInMinVoltage = -5.0f;
constexpr float kAudioCvInMaxVoltage = 5.0f;

namespace brain {
namespace constants {
constexpr uint32_t kMicrosPerSecond = ::kMicrosPerSecond;
constexpr uint32_t kMillisPerSecond = ::kMillisPerSecond;
constexpr uint16_t kAdcMaxValue = ::kAdcMaxValue;
constexpr float kAdcVoltageRef = ::kAdcVoltageRef;
constexpr uint32_t kDefaultAudioSampleRate = ::kDefaultAudioSampleRate;
constexpr uint32_t kDefaultControlRate = ::kDefaultControlRate;
constexpr float kAudioCvInVoltageAtMinus5V = ::kAudioCvInVoltageAtMinus5V;
constexpr float kAudioCvInVoltageAtPlus5V = ::kAudioCvInVoltageAtPlus5V;
constexpr float kAudioCvInMinVoltage = ::kAudioCvInMinVoltage;
constexpr float kAudioCvInMaxVoltage = ::kAudioCvInMaxVoltage;
}  // namespace constants
}  // namespace brain
