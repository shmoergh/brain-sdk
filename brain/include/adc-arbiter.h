#pragma once

#include <hardware/sync.h>

// Shared ADC guard for IRQ-safe and multicore-safe arbitration.
// All Brain SDK ADC operations should hold this guard while touching
// ADC registers/FIFO or running ADC DMA transactions.
class BrainAdcLockGuard {
public:
	BrainAdcLockGuard()
		: lock_(spin_lock_instance(PICO_SPINLOCK_ID_STRIPED_FIRST)),
		  saved_irq_(spin_lock_blocking(lock_)) {}

	~BrainAdcLockGuard() {
		spin_unlock(lock_, saved_irq_);
	}

	BrainAdcLockGuard(const BrainAdcLockGuard&) = delete;
	BrainAdcLockGuard& operator=(const BrainAdcLockGuard&) = delete;
	BrainAdcLockGuard(BrainAdcLockGuard&&) = delete;
	BrainAdcLockGuard& operator=(BrainAdcLockGuard&&) = delete;

private:
	spin_lock_t* lock_;
	uint32_t saved_irq_;
};
