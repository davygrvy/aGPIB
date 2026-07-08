/* ----------------------------------------------------------------------
 *
 * aGPIBQueue.hpp --
 *
 * 	'Asynchronous General Purpose Interface Bus' for Tcl.
 *
 *	This extension adds a new channel type to Tool Command Language
 * 	that allows for easy communication with devices plugged into a
 *	GPIB bus.  Linux and Windows friendly.
 *
 *	This file contains the atomic single-producer single-consumer
 *	FIFO queue as a static sized ring buffer.
 *
 *	While operations of a device will be synchronous in nature -- it
 *	talks then waits for a reply, repeat -- This queue doesn't really
 *	*need* to have these features, I am doing it anyway for the
 *	experience as other Tcl channel drivers I work on run free such
 *	as TCP/IP sockets, etc., and need the full indication of push()
 *	for flow-control (TCP windowing feedback by not repost a
 *	replacement overlapped WSARecv).  IOW, when Tcl is being slow
 *	servicing the incoming data, reflect that back.
 *
 * ----------------------------------------------------------------------
 *
 * Copyright (c) 2026 David Gravereaux <davygrvy@pobox.com>
 *
 * See the file "license.terms" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL
 * WARRANTIES.
 *
 * ----------------------------------------------------------------------
 * RCS: @(#) $Id: $
 * ----------------------------------------------------------------------
 */

#include <atomic>
#include <cstddef>
#include <new>
#include <utility>

template <typename T, std::size_t Capacity>
class SPSCQueue {
    // Capacity must be a power of two for the fast bitwise mask optimization
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    SPSCQueue() : head_(0), tail_(0) {}

    ~SPSCQueue() {
	T discarded;
	while (pop(discarded)); // Clean up remaining elements
    }

    // Push an item into the queue (Producer thread only)
    bool push(const T& item) {
	const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
	const std::size_t current_head = head_.load(std::memory_order_acquire);

	if ((current_tail - current_head) == Capacity) {
	    return false; // Queue is full
	}

	buffer_[current_tail & BufferMask] = item;
	tail_.store(current_tail + 1, std::memory_order_release);
	return true;
    }

    // Move an item into the queue (Producer thread only)
    bool push(T&& item) {
	const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
	const std::size_t current_head = head_.load(std::memory_order_acquire);

	if ((current_tail - current_head) == Capacity) {
	    return false; // Queue is full
	}

	buffer_[current_tail & BufferMask] = std::move(item);
	tail_.store(current_tail + 1, std::memory_order_release);
	return true;
    }

    // Pop an item from the queue (Consumer thread only)
    bool pop(T& value) {
	const std::size_t current_head = head_.load(std::memory_order_relaxed);
	const std::size_t current_tail = tail_.load(std::memory_order_acquire);

	if (current_head == current_tail) {
	    return false; // Queue is empty
	}

	value = std::move(buffer_[current_head & BufferMask]);
	head_.store(current_head + 1, std::memory_order_release);
	return true;
    }

    // Returns the approximate or exact number of entries currently in the queue
    std::size_t size() const noexcept {
	// Load positions safely.
	// Use acquire to establish visibility if you need to synchronize data changes.
	const std::size_t current_tail = tail_.load(std::memory_order_acquire);
	const std::size_t current_head = head_.load(std::memory_order_acquire);

	// If a race causes head to appear greater than tail, the queue is effectively empty
	if (current_head >= current_tail) {
	    return 0;
	}

	// Pure mathematical distance between monotonically increasing indexes
	return current_tail - current_head;
    }

    bool empty() const noexcept {
	// Checking equality directly is faster and safer than evaluating size()
	return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

private:
    static constexpr std::size_t BufferMask = Capacity - 1;
    T buffer_[Capacity];

    // Hardware destructive interference size ensures variables sit on separate cache lines
#ifdef __cpp_lib_hardware_interference_size
    static constexpr std::size_t CacheLineSize = std::hardware_destructive_interference_size;
#else
    static constexpr std::size_t CacheLineSize = 64; // Fallback for most x86/ARM CPUs
#endif

    // Align variables to prevent False Sharing between producer and consumer cores
    alignas(CacheLineSize) std::atomic<std::size_t> head_;
    alignas(CacheLineSize) std::atomic<std::size_t> tail_;
};
