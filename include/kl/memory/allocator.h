#ifndef KL_ALLOCATOR_H
#define KL_ALLOCATOR_H
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <kl/threads/mutex.h>
#include <stdio.h>

typedef struct {
  int64_t capacity;
  int64_t tracked_allocations_capacity;
  bool track_allocations;
} KL_AllocatorConfig;

typedef struct {
  uint8_t* data;
  int64_t bytes_written;
  int64_t size;
  int64_t index;
} KL_AllocatorPage;

typedef struct {
  int64_t page;
  int64_t offset;
  int64_t size;
  size_t alignment;
  bool freed;
  // Index of this allocation's slot in the allocator's tracked array, carried
  // in every handle so free is an O(1) validated lookup instead of a scan.
  // -1 when the allocator doesn't track allocations.
  int64_t tracked_index;
  // Intrusive freed-list link (index of the next freed slot in the same size
  // class, -1 = end). Only meaningful on the allocator's own slot entries
  // while freed; junk on caller-held handle copies.
  int64_t next_free;
} KL_Allocation;

// Freed slots are kept in power-of-two size-class buckets (bucket i holds
// slots with 2^i <= size < 2^(i+1)), linked through KL_Allocation.next_free.
// Allocation pops from the ceiling class upward, so a reused block is always
// within 2x of the request — the same fragmentation guarantee the old
// best-fit scan gave, without the O(n) walk.
#define KL_ALLOCATOR_SIZE_CLASS_COUNT 64

typedef struct KL_ALLOCATOR_STRUCT {
  KL_Mutex lock;
  bool initialized;
  KL_AllocatorConfig config;
  int64_t current_capacity;

  struct {
    KL_AllocatorPage* items;
    int64_t length;
  } pages;

  struct {
    KL_Allocation* items;
    int64_t length;
    int64_t n_allocated;
    int64_t n_freed;
    int64_t n_reused;
    // Heads of the per-size-class freed-slot lists (indices into items,
    // -1 = empty). See KL_ALLOCATOR_SIZE_CLASS_COUNT.
    int64_t free_heads[KL_ALLOCATOR_SIZE_CLASS_COUNT];
  } allocations;

} KL_Allocator;

typedef struct {
  int64_t pages_length;
  int64_t* page_bytes_written;
  int64_t allocations_length;
} KL_AllocatorMark;

int kl_allocator_init(KL_Allocator* allocator, KL_AllocatorConfig cfg);

void kl_allocator_destroy(KL_Allocator* allocator);

void kl_allocator_destroy_all_pages(KL_Allocator* allocator);

void kl_allocator_reset(KL_Allocator* allocator);

void kl_allocator_lock(KL_Allocator* allocator);

void kl_allocator_unlock(KL_Allocator* allocator);

uint8_t* kl_allocator_malloc(KL_Allocator* allocator, int64_t size);

uint8_t* kl_allocator_malloc_aligned(KL_Allocator* allocator, int64_t size, size_t alignment);

uint8_t* kl_allocator_malloc_tracked(KL_Allocator* allocator, int64_t size, KL_Allocation* out);

uint8_t* kl_allocator_malloc_tracked_aligned(KL_Allocator* allocator, int64_t size, size_t alignment, KL_Allocation* out);

uint8_t* kl_allocator_realloc(KL_Allocator* allocator, int64_t new_size, KL_Allocation* allocation);

uint8_t* kl_allocator_realloc_aligned(KL_Allocator* allocator, int64_t new_size, size_t alignment, KL_Allocation* allocation);

char* kl_allocator_strdup(KL_Allocator* allocator, const char* str, int64_t length);

char* kl_allocator_sprintf(KL_Allocator* allocator, const char* fmt, ...);

int kl_allocator_free_allocation(KL_Allocator* allocator, KL_Allocation allocation);

void kl_allocator_print_diagnostics(const KL_Allocator* allocator, FILE* fp);

void kl_allocator_print_diagnostics_summary(const KL_Allocator* allocator, FILE* fp);

KL_AllocatorMark kl_allocator_save(KL_Allocator* allocator);

void kl_allocator_restore(KL_Allocator* allocator, KL_AllocatorMark mark);

#endif
