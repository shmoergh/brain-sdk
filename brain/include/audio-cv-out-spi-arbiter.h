#pragma once

#include <hardware/sync.h>

// Shared SPI guard for the MCP4822 audio/CV output DAC.
//
// Both `AudioProcessor` (audio-rate sample writes from the audio ISR) and
// `Outputs` (CV writes from the main loop) drive the same physical SPI bus
// + chip-select line on the same MCP4822 chip. Without serialization a CV
// write from the main loop can collide with an audio sample write from the
// ISR, corrupting both transactions.
//
// Anything that drives the audio/CV output DAC must hold this guard for the
// duration of the CS-low → SPI write → CS-high sequence. The lock window is
// only a few microseconds at typical SPI baud rates.
class BrainAudioDacSpiLockGuard {
public:
	/**
	 * @brief Acquires the shared audio/CV DAC SPI spinlock and stores previous IRQ state for automatic release.
	 */
	BrainAudioDacSpiLockGuard()
		: lock_(spin_lock_instance(PICO_SPINLOCK_ID_STRIPED_FIRST + 1)),
		  saved_irq_(spin_lock_blocking(lock_)) {}

	/**
	 * @brief Releases the audio/CV DAC SPI spinlock and restores IRQ state captured in the constructor.
	 */
	~BrainAudioDacSpiLockGuard() {
		spin_unlock(lock_, saved_irq_);
	}

	BrainAudioDacSpiLockGuard(const BrainAudioDacSpiLockGuard&) = delete;
	BrainAudioDacSpiLockGuard& operator=(const BrainAudioDacSpiLockGuard&) = delete;
	BrainAudioDacSpiLockGuard(BrainAudioDacSpiLockGuard&&) = delete;
	BrainAudioDacSpiLockGuard& operator=(BrainAudioDacSpiLockGuard&&) = delete;

private:
	spin_lock_t* lock_;
	uint32_t saved_irq_;
};
