#include "brain-utils/ringbuffer.h"

namespace brain::utils {

void RingBuffer::init(uint8_t* data_buffer, uint16_t buffer_size) {
	buffer_size_ = buffer_size;
	write_index_ = 0;
	read_index_ = 0;
	data_buffer_ = data_buffer;
}

/**
 * Since it's a circular/ringbuffer we keep increasing the write index and wrap
 * around once it exceeds the size of the buffer.
 */
bool RingBuffer::write_byte(uint8_t data) {
	if (!is_full()) {
		data_buffer_[write_index_] = data;
		write_index_++;
		if (write_index_ >= buffer_size_) write_index_ = 0;
		return true;
	}
	return false;
}

/**
 * Similar to the write function, we increase the read index. We don't actually
 * remove value from the array, it removes automatically as it wraps around.
 */
bool RingBuffer::read_byte(uint8_t& data) {
	if (!is_empty()) {
		data = data_buffer_[read_index_];
		read_index_++;
		if (read_index_ >= buffer_size_) read_index_ = 0;
		return true;
	}
	return false;
}

bool RingBuffer::peek(uint8_t& data) const {
	if (!is_empty()) {
		data = data_buffer_[read_index_];
		return true;
	}
	return false;
}

/**
 * Check if buffer is empty (no data to read).
 * When read and write indices are equal, the buffer is empty.
 * This is ISR-safe because we only read volatile indices (no modifications).
 */
bool RingBuffer::is_empty() const {
	return read_index_ == write_index_;
}

/**
 * Check if buffer is full (cannot write more data).
 * To distinguish "full" from "empty" (both would have read == write),
 * we sacrifice one buffer slot. The buffer is full when writing one more
 * byte would make write_index_ equal to read_index_ (which means empty).
 *
 * This implementation is ISR-safe for single-writer/single-reader scenarios:
 * - ISR writes (modifies write_index_)
 * - Main loop reads (modifies read_index_)
 * - isFull() only reads indices, never writes them
 *
 * Note: With buffer_size_ = N, you can store (N - 1) items maximum.
 */
bool RingBuffer::is_full() const {
	return ((write_index_ + 1) % buffer_size_) == read_index_;
}

} //namespace brain::common