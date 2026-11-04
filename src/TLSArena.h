#ifndef TLSARENA_H_
#define TLSARENA_H_

#include <Arduino.h>

/*
 * Gives TLS memory from a buffer that isn't needed for a while, instead of from the heap. A TLS
 * connection takes about 50 KB, most of the heap the clock has left, so while the weather was
 * fetched the web server and Wi-Fi ran out of memory.
 *
 * Between begin() and end(), memory that the calling task asks TLS for comes from the buffer, and
 * from the heap if the buffer is full. Everything else still comes from the heap.
 */
namespace TLSArena {
	void init();	// Before anything uses TLS
	void begin(void *buffer, size_t size);
	bool end();	// False if TLS still has some of the buffer, which it would then share with its owner
}

#endif /* TLSARENA_H_ */
