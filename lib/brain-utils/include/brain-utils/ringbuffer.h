#ifndef BRAIN_RINGBUFFER_H_
#define BRAIN_RINGBUFFER_H_

#include "pico/stdlib.h"

namespace brain::utils {

class RingBuffer {
	public:
		void init(uint8_t* data_buffer, uint16_t buffer_size);

		bool writeByte(uint8_t data);
		bool readByte(uint8_t& data);
		bool peek(uint8_t& data) const;
		bool isFull() const;
		bool isEmpty() const;

	private:
		uint16_t buffer_size_;
		uint8_t* data_buffer_;
		volatile uint16_t read_index_;
		volatile uint16_t write_index_;
};

}	// namespace brain::utils

// class RingBuffer

#endif