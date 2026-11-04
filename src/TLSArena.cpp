#include "TLSArena.h"

#include <esp_heap_caps.h>
#include <multi_heap.h>
#include <mbedtls/platform.h>

namespace {
	multi_heap_handle_t arena = NULL;
	TaskHandle_t arenaTask = NULL;
	uint8_t *arenaStart = NULL;
	uint8_t *arenaEnd = NULL;
	size_t arenaFree = 0;	// When nothing has been taken from it

	// What ESP-IDF gives mbedTLS otherwise
	void *heapCalloc(size_t n, size_t size) {
		return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	}

	void *arenaCalloc(size_t n, size_t size) {
		if (arena != NULL && xTaskGetCurrentTaskHandle() == arenaTask && size != 0 && n <= SIZE_MAX / size) {
			void *p = multi_heap_malloc(arena, n * size);
			if (p != NULL) {
				memset(p, 0, n * size);
				return p;
			}
		}

		return heapCalloc(n, size);
	}

	void arenaFreeBlock(void *p) {
		if ((uint8_t*)p >= arenaStart && (uint8_t*)p < arenaEnd) {
			// After end() the buffer belongs to its owner again, so a block left over is just forgotten
			if (arena != NULL) {
				multi_heap_free(arena, p);
			}
		} else {
			heap_caps_free(p);
		}
	}
}

void TLSArena::init() {
	mbedtls_platform_set_calloc_free(arenaCalloc, arenaFreeBlock);
}

void TLSArena::begin(void *buffer, size_t size) {
	arenaStart = (uint8_t*)buffer;
	arenaEnd = arenaStart + size;
	arena = multi_heap_register(buffer, size);
	arenaFree = multi_heap_free_size(arena);
	arenaTask = xTaskGetCurrentTaskHandle();
}

bool TLSArena::end() {
	bool allReturned = multi_heap_free_size(arena) == arenaFree;
	arenaTask = NULL;
	arena = NULL;

	return allReturned;
}
