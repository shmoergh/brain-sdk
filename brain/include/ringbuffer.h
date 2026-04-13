#ifndef BRAIN_RINGBUFFER_H_
#define BRAIN_RINGBUFFER_H_

#include "pico/stdlib.h"

namespace brain::utils {

class RingBuffer {
	public:
			/**
			 * @brief Binds this `RingBuffer` instance to caller-owned storage.
			 * @param data_buffer Pointer to the byte array used as ring storage.
			 * @param buffer_size Size of `data_buffer` in bytes (maximum bytes storable before full).
			 */
			void init(uint8_t* data_buffer, uint16_t buffer_size);

			/**
			 * @brief Pushes one byte into the ring buffer.
			 * @param data Byte value to enqueue.
			 * @return `true` if the byte was written; `false` if the buffer was full.
			 */
			bool write_byte(uint8_t data);

			/**
			 * @brief Pops one byte from the ring buffer.
			 * @param data Output reference that receives the dequeued byte when available.
			 * @return `true` if a byte was read; `false` if the buffer was empty.
			 */
			bool read_byte(uint8_t& data);

			/**
			 * @brief Returns the next buffered byte without consuming it.
			 * @param data Output reference that receives the next byte when available.
			 * @return `true` if a byte is available to peek; `false` if the buffer is empty.
			 */
			bool peek(uint8_t& data) const;

			/**
			 * @brief Checks whether no additional bytes can be written.
			 * @return `true` when write operations would overflow because the buffer is full.
			 */
			bool is_full() const;

			/**
			 * @brief Checks whether there are no bytes available to read.
			 * @return `true` when the buffer contains zero unread bytes.
			 */
			bool is_empty() const;

	private:
		uint16_t buffer_size_;
		uint8_t* data_buffer_;
		volatile uint16_t read_index_;
		volatile uint16_t write_index_;
};

}	// namespace brain::utils

using RingBuffer = brain::utils::RingBuffer;

#endif
