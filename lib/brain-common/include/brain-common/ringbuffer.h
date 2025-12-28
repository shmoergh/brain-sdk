#ifndef BRAIN_COMMON_RINGBUFFER_H_
#define BRAIN_COMMON_RINGBUFFER_H_

#include "pico/stdlib.h"

namespace brain::common {

class RingBuffer {
	public:
	void init(uint16_t* data_buffer, uint16_t buffer_size);

	bool writeByte(uint16_t data);
	bool readByte(uint16_t& data);
	bool peek(uint16_t& data) const;
	bool isFull() const;
	bool isEmpty() const;

	private:
	uint16_t buffer_size_;
	uint16_t* data_buffer_;
	volatile uint16_t read_index_;
	volatile uint16_t write_index_;
};

}	// namespace brain::common

// class RingBuffer

#endif