#pragma once

#include <hardware/sync.h>

// Shared ADC guard for IRQ-safe and multicore-safe arbitration.
// All Brain SDK ADC operations should hold this guard while touching
// ADC registers/FIFO or running ADC DMA transactions.
class BrainAdcLockGuard {
public:
	/**
	 * @brief Acquires the shared ADC spinlock and stores previous IRQ state for automatic release.
	 */
	BrainAdcLockGuard()
		: lock_(spin_lock_instance(PICO_SPINLOCK_ID_STRIPED_FIRST)),
		  saved_irq_(spin_lock_blocking(lock_)) {}

	/**
	 * @brief Releases the ADC spinlock and restores IRQ state captured in the constructor.
	 */
	~BrainAdcLockGuard() {
		spin_unlock(lock_, saved_irq_);
	}

	/**
	 * @brief Copy construction is disabled for this type.
	 */
	BrainAdcLockGuard(const BrainAdcLockGuard&) = delete;

	/**
	 * @brief Copy assignment is disabled for this type.
	 */
	BrainAdcLockGuard& operator=(const BrainAdcLockGuard&) = delete;

	/**
	 * @brief Move construction is disabled for this type.
	 */
	BrainAdcLockGuard(BrainAdcLockGuard&&) = delete;

	/**
	 * @brief Move assignment is disabled for this type.
	 */
	BrainAdcLockGuard& operator=(BrainAdcLockGuard&&) = delete;

private:
	spin_lock_t* lock_;
	uint32_t saved_irq_;
};
