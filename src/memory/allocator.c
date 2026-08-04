#include <kl/macros.h>
#include <assert.h>
#include <kl/memory/allocator.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


// static inline void print_page(KL_AllocatorPage* page, FILE* fp) {
//   fprintf(fp, "page: {\n");
//   fprintf(fp, "  index: %ld\n", page->index);
//   fprintf(fp, "  size: %ld\n", page->size);
//   fprintf(fp, "  bytes_written: %ld\n", page->bytes_written);
//   fprintf(fp, "  data: %p\n", page->data);
//   fprintf(fp, "}\n");
// }

static inline int64_t alignup(int64_t x, int64_t alignment)  { return (x + alignment) & ~alignment; }

// floor(log2(size)) — the bucket a freed slot of this size lives in.
static inline int size_class_floor(int64_t size) {
  assert(size > 0);
  return 63 - __builtin_clzll((uint64_t)size);
}

// How far into the request's own (floor) size class to search before giving
// up on it. That class mixes slots smaller and larger than the request —
// unlike every higher class, where the first valid slot always fits — so an
// unbounded walk there could degenerate; a small cap keeps take() O(1) while
// still catching the dominant exact-size-reuse case.
#define KL_ALLOCATOR_FLOOR_CLASS_SCAN_CAP 64

static inline void free_list_reset(KL_Allocator* allocator) {
  for (int i = 0; i < KL_ALLOCATOR_SIZE_CLASS_COUNT; i++) {
    allocator->allocations.free_heads[i] = -1;
  }
}

static inline void free_list_push(KL_Allocator* allocator, int64_t slot_index) {
  KL_Allocation* a = &allocator->allocations.items[slot_index];
  assert(a->freed);
  int c = size_class_floor(a->size);
  a->next_free = allocator->allocations.free_heads[c];
  allocator->allocations.free_heads[c] = slot_index;
}

// Pop the first freed slot that satisfies (size, alignment, valid page),
// searching the ceiling size class upward. Returns the slot index or -1.
// First-fit within a class keeps a hit within 2x of the request, matching
// the fragmentation behavior of the old O(n) best-fit scan.
static inline int64_t free_list_take(KL_Allocator* allocator, int64_t size, size_t alignment) {
  if (allocator->allocations.items == 0 || allocator->allocations.length <= 0) return -1;

  // Start at the request's own (floor) class: it holds every freed slot of
  // similar size, including exact fits — the dominant reuse case for churny
  // same-shape workloads. Slots there may be smaller than the request (the
  // size check below filters); in every higher class the first valid slot is
  // guaranteed to fit.
  int floor_class = size_class_floor(size);
  for (int c = floor_class; c < KL_ALLOCATOR_SIZE_CLASS_COUNT; c++) {
    int64_t scanned = 0;
    int64_t prev = -1;
    int64_t idx = allocator->allocations.free_heads[c];
    while (idx >= 0) {
      if (c == floor_class && scanned++ >= KL_ALLOCATOR_FLOOR_CLASS_SCAN_CAP) break;
      KL_Allocation* a = &allocator->allocations.items[idx];
      assert(a->freed);
      if (a->page >= 0 && a->page < allocator->pages.length && a->size >= size) {
        KL_AllocatorPage* page = &allocator->pages.items[a->page];
        if (page->data != 0 && page->index == a->page &&
            (alignment == 0 || (((uintptr_t)page->data + (uintptr_t)a->offset) & (alignment - 1)) == 0)) {
          // Unlink; the caller resurrects the slot via reuse_allocation.
          if (prev < 0) allocator->allocations.free_heads[c] = a->next_free;
          else allocator->allocations.items[prev].next_free = a->next_free;
          a->next_free = -1;
          return idx;
        }
      }
      prev = idx;
      idx = a->next_free;
    }
  }

  return -1;
}

static inline void add_allocation(KL_Allocator* allocator, KL_Allocation allocation) {
  allocation.freed = false;
  allocation.next_free = -1;
  assert(allocation.tracked_index == allocator->allocations.length);
  int64_t avail = allocator->allocations.n_allocated - allocator->allocations.length;
  if (avail <= 0 || allocator->allocations.n_allocated <= 0) {
    allocator->allocations.items = (KL_Allocation*)realloc(allocator->allocations.items, (allocator->allocations.n_allocated + allocator->config.tracked_allocations_capacity) * sizeof(KL_Allocation));
    assert(allocator->allocations.items != 0);
    memset(&allocator->allocations.items[allocator->allocations.n_allocated], 0, allocator->config.tracked_allocations_capacity * sizeof(KL_Allocation));
    allocator->allocations.n_allocated += allocator->config.tracked_allocations_capacity;
  }
  assert(allocator->allocations.items != 0);
  assert(allocator->allocations.n_allocated > 0);
  assert(allocator->allocations.length < allocator->allocations.n_allocated);
  allocator->allocations.items[allocator->allocations.length++] = allocation;
}
// - DO NOT REMOVE -
//static inline void remove_allocation(KL_Allocator* allocator, KL_Allocation allocation) {
//  if (allocator->allocations.items == 0 || allocator->allocations.n_allocated <= 0 || allocator->allocations.length <= 0) return;
//
//  int64_t idx = -1;
//
//  for (int64_t i = 0; i < allocator->allocations.length; i++) {
//    KL_Allocation a = allocator->allocations.items[i];
//    if (a.page == allocation.page && a.offset == allocation.offset && a.size == allocation.size) {
//      idx = i;
//      break;
//    }
//  }
//
//  if (idx < 0) return;
//
//  assert(idx < allocator->allocations.length);
//  assert(idx >= 0);
//
//  for (int64_t i = idx; i < allocator->allocations.length - 1; i++) {
//    allocator->allocations.items[i] = allocator->allocations.items[i + 1];
//  }
//
//  allocator->allocations.length -= 1;
//  assert(allocator->allocations.length >= 0);
//}

static inline int free_allocation(KL_Allocator* allocator, KL_Allocation allocation) {
  if (allocator->allocations.items == 0 || allocator->allocations.n_allocated <= 0 || allocator->allocations.length <= 0) return 0;
  if (allocation.size <= 0) return 0;

  // O(1): the handle carries its slot index; the (page, offset, size) match
  // guards against stale handles (e.g. copies that predate a restore()).
  KL_Allocation* a = 0;
  int64_t idx = allocation.tracked_index;
  if (idx >= 0 && idx < allocator->allocations.length) {
    KL_Allocation* cand = &allocator->allocations.items[idx];
    if (cand->page == allocation.page && cand->offset == allocation.offset && cand->size == allocation.size) {
      a = cand;
    }
  }

  // Fallback for handles without a valid index (zero-initialized or
  // pre-tracking structs) — rare, so the scan cost doesn't matter.
  if (a == 0) {
    for (int64_t i = 0; i < allocator->allocations.length; i++) {
      KL_Allocation* cand = &allocator->allocations.items[i];
      if (cand->page == allocation.page && cand->offset == allocation.offset && cand->size == allocation.size) {
        a = cand;
        idx = i;
        break;
      }
    }
  }

  if (a == 0) return 0;

  // The slot's freed flag is the source of truth, not the caller's copy:
  // a stale handle must neither double-free nor block a legitimate free.
  if (a->freed) return 0;
  a->freed = true;

  if (a->size > 0 && a->page >= 0 && a->page < allocator->pages.length && allocator->pages.items != 0) {
    KL_AllocatorPage* page = &allocator->pages.items[a->page];
    if (page->index == a->page && page->data != 0 && page->size >= a->size && a->offset < page->size) {
      memset(&page->data[a->offset], 0, a->size);
    }
  }

  free_list_push(allocator, idx);
  return 1;
}


int kl_allocator_init(KL_Allocator* allocator, KL_AllocatorConfig cfg) {
  assert(cfg.capacity > 0);
  if (allocator->initialized) return 1;
  cfg.capacity = alignup(cfg.capacity, 4);
  cfg.tracked_allocations_capacity = cfg.tracked_allocations_capacity > 0 ? cfg.tracked_allocations_capacity : 64;
  allocator->config = cfg;
  allocator->pages.length = 0;
  allocator->pages.items = 0;
  allocator->allocations.length = 0;
  allocator->allocations.items = 0;
  allocator->allocations.n_allocated = 0;
  allocator->allocations.n_freed = 0;
  allocator->allocations.n_reused = 0;
  free_list_reset(allocator);
  allocator->current_capacity = cfg.capacity;

  kl_mutex_init(&allocator->lock);
  
  allocator->initialized = true;
  return 1;
}

void kl_allocator_destroy(KL_Allocator* allocator) {
  assert(allocator->initialized == true);
  if (allocator->pages.items != 0) {
    for (int64_t i = 0; i < allocator->pages.length; i++) {
      KL_AllocatorPage* page = &allocator->pages.items[i];
      if (page->data != 0) {
        free(page->data);
      }
      page->bytes_written = 0;
      page->size = 0;
    }
    free(allocator->pages.items);
  }
  if (allocator->allocations.items != 0) {
    free(allocator->allocations.items);
  }
  allocator->allocations.items = 0;
  allocator->allocations.n_allocated = 0;
  allocator->allocations.length = 0;
  kl_mutex_destroy(&allocator->lock);
  allocator->pages.items = 0;
  allocator->pages.length = 0;
  allocator->config = (KL_AllocatorConfig){0};
  allocator->initialized = false;
}

void kl_allocator_destroy_all_pages(KL_Allocator* allocator) {
  assert(allocator->initialized == true);

  kl_mutex_lock(&allocator->lock);
  if (allocator->pages.items != 0) {
    for (int64_t i = 0; i < allocator->pages.length; i++) {
      KL_AllocatorPage* page = &allocator->pages.items[i];
      if (page->data != 0) {
        free(page->data);
      }
      page->bytes_written = 0;
      page->size = 0;
    }
    free(allocator->pages.items);
  }
  allocator->pages.items = 0;
  allocator->pages.length = 0;
  if (allocator->allocations.items != 0) {
    free(allocator->allocations.items);
  }
  allocator->allocations.items = 0;
  allocator->allocations.n_allocated = 0;
  allocator->allocations.length = 0;
  allocator->allocations.n_freed = 0;
  free_list_reset(allocator);
  kl_mutex_unlock(&allocator->lock);
}

void kl_allocator_reset(KL_Allocator* allocator) {
  assert(allocator->initialized == true);
  size_t tot_bytes_written = 0;
  
  for (int64_t i = 0; i < allocator->pages.length; i++) {
    KL_AllocatorPage* page = &allocator->pages.items[i];
    if (page->data != 0 && page->bytes_written > 0) {
      assert(page->size > 0);
      memset(&page->data[0], 0, page->size);
    }
    tot_bytes_written += page->bytes_written;
    page->bytes_written = 0;
  }

  // increase capacity for next round
  if (tot_bytes_written > allocator->current_capacity) {
    kl_allocator_destroy_all_pages(allocator);
    allocator->current_capacity = tot_bytes_written;
  }

  allocator->allocations.length = 0;
  allocator->allocations.n_freed = 0;
  free_list_reset(allocator);
}

void kl_allocator_lock(KL_Allocator* allocator) {
  assert(allocator->initialized);
  kl_mutex_lock(&allocator->lock);
}

void kl_allocator_unlock(KL_Allocator* allocator) {
  assert(allocator->initialized);
  kl_mutex_unlock(&allocator->lock);
}

static inline bool page_fits(KL_AllocatorPage* page, int64_t size) {
  if (page->bytes_written >= page->size || page->size < size) return false;
  assert(page->data != 0);
  int64_t avail = page->size - page->bytes_written;
  return avail >= size;
}

static inline KL_AllocatorPage* new_page(KL_Allocator* allocator, int64_t size) {
  allocator->pages.items = (KL_AllocatorPage*)realloc(allocator->pages.items, (allocator->pages.length + 1) * sizeof(KL_AllocatorPage));
  assert(allocator->pages.items != 0);
  memset(&allocator->pages.items[allocator->pages.length], 0, sizeof(KL_AllocatorPage));
  KL_AllocatorPage* p = &allocator->pages.items[allocator->pages.length];
  assert(allocator->current_capacity >= 0);
  p->size = KL_MAX(size, allocator->current_capacity);
  assert(p->size >= size);
  assert(p->size >= allocator->current_capacity);
  p->data = (uint8_t*)calloc(p->size, sizeof(uint8_t));
  assert(p->data != 0);
  p->bytes_written = 0;
  p->index = allocator->pages.length;
  allocator->pages.length += 1;
  return p;
}

// size is the user-requested size, total_size_needed the bump-allocation
// footprint (size + alignment slack). Freed-block matching must use the user
// size: freed records store the user size, so matching against
// total_size_needed makes a freed block unable to satisfy a new request of
// the same size whenever alignment > 0.
//
// Freed-slot reuse is tried BEFORE bump allocation: a free/alloc churn
// workload otherwise never reuses while a page has bump space left, so both
// bytes_written and the tracked-allocations list grow without bound. Reuse
// pops from the per-size-class freed lists (see free_list_take) — a hit is
// always within 2x of the request, so a large freed block isn't burned on a
// tiny one; blocks don't split, the remainder would be lost until the block
// is freed again.
static inline KL_AllocatorPage* find_avail_page(KL_Allocator* allocator, int64_t size, size_t alignment, int64_t total_size_needed, int64_t* out_allocation_index) {
  *out_allocation_index = -1;

  if (allocator->pages.length <= 0) {
    assert(allocator->pages.items == 0);
    return new_page(allocator, total_size_needed);
  }

  assert(allocator->pages.items != 0);
  assert(allocator->pages.length > 0);

  if (allocator->allocations.n_freed > 0) {
    int64_t taken = free_list_take(allocator, size, alignment);
    if (taken >= 0) {
      *out_allocation_index = taken;
      return &allocator->pages.items[allocator->allocations.items[taken].page];
    }
  }

  KL_AllocatorPage* first = &allocator->pages.items[0];
  if (page_fits(first, total_size_needed)) return first;
  KL_AllocatorPage* last = &allocator->pages.items[KL_MAX(0, allocator->pages.length-1)];
  if (page_fits(last, total_size_needed)) return last;

  return new_page(allocator, total_size_needed);
}

static inline uint8_t* find_old_data(KL_Allocator* allocator, KL_Allocation* allocation) {
  if (allocation->size <= 0 || allocation->page < 0 || allocation->page >= allocator->pages.length || allocation->offset < 0 || allocator->pages.items == 0 || allocator->pages.length <= 0) return 0;
  KL_AllocatorPage* og_page = &allocator->pages.items[allocation->page];
  if (og_page->size <= 0 || allocation->offset > og_page->size) return 0;
  return &og_page->data[allocation->offset];
}

static inline uint8_t* reuse_allocation(KL_Allocator* allocator, KL_AllocatorPage* page, int64_t allocation_index, KL_Allocation* out) {
  assert(allocation_index < allocator->allocations.length);
  assert(allocator->allocations.items != 0);
  KL_Allocation* slot = &allocator->allocations.items[allocation_index];
  assert(slot->offset >= 0);
  assert(slot->offset < page->size);
  // Resurrect the slot in place so kl_allocator_free_allocation can find it
  // again. Removing the entry on reuse would leave the live region untracked.
  // Clear freed BEFORE copying into the caller's handle: a handle carrying
  // freed=true would make the next free_allocation reject it and leak the
  // block as live forever.
  slot->freed = false;
  KL_Allocation allocation = *slot;
  uint8_t* data = &page->data[allocation.offset];
  allocator->allocations.n_freed -= 1;
  allocator->allocations.n_reused += 1;
  if (out != 0) {
    *out = allocation;
  }
  return data;
}

static inline uint8_t* page_alloc(KL_Allocator* allocator, KL_AllocatorPage* page, int64_t size, bool is_realloc, KL_Allocation* out) {
  int64_t offset = page->bytes_written;
  uint8_t* data = &page->data[page->bytes_written];
  page->bytes_written += size;
  

  KL_Allocation allocation = {0};
  allocation.offset = offset;
  allocation.size = size;
  allocation.page = page->index;
  allocation.tracked_index = -1;
  allocation.next_free = -1;

  if (!is_realloc && allocator->config.track_allocations) {
    allocation.tracked_index = allocator->allocations.length;
    add_allocation(allocator, allocation);
  }

  if (out != 0) {
    *out = allocation;
  }
  
  return data;
}


static inline uint8_t* page_alloc_aligned(KL_Allocator* allocator, KL_AllocatorPage* page, int64_t size, size_t alignment, bool is_realloc, KL_Allocation* out) {
  assert(alignment > 0 && "Alignment must be positive.");
  assert((alignment & (alignment - 1)) == 0 && "Alignment must be a power of two.");

  size_t total_size_needed = size + alignment - 1;
  bool fits = page_fits(page, total_size_needed);
  assert(fits == true);

  uintptr_t current_ptr = (uintptr_t)page->data + page->bytes_written;
  uintptr_t aligned_ptr = (current_ptr + alignment - 1) & ~(alignment - 1);
  uint8_t padding = (uint8_t)(aligned_ptr - current_ptr);

  page->bytes_written += size + padding;

  // Record the aligned offset and the user-requested size so that
  // find_old_data / realloc see the actual user data region, not the
  // padding bytes that precede it.
  KL_Allocation allocation = {0};
  allocation.offset = (int64_t)(aligned_ptr - (uintptr_t)page->data);
  allocation.size = size;
  allocation.page = page->index;
  allocation.alignment = alignment;
  allocation.tracked_index = -1;
  allocation.next_free = -1;

  if (!is_realloc && allocator->config.track_allocations) {
    allocation.tracked_index = allocator->allocations.length;
    add_allocation(allocator, allocation);
  }

  if (out != 0) {
    *out = allocation;
  }

  return (uint8_t*)aligned_ptr;
}

static inline uint8_t* generic_malloc(KL_Allocator* allocator, int64_t size, size_t alignment, bool is_realloc, KL_Allocation* out) {
  assert(allocator->initialized == true);
  assert(size > 0);
  
  kl_mutex_lock(&allocator->lock);

  uint8_t* data = 0;
  size_t total_size_needed = (alignment > 0) ?  (size + alignment - 1 + sizeof(uint8_t)) : (size);

  int64_t allocation_index = -1;
  KL_AllocatorPage* page = find_avail_page(allocator, size, alignment, total_size_needed, &allocation_index);
  assert(page != 0);
  assert(page->data != 0);
  
  if (allocation_index >= 0) {
    data = reuse_allocation(allocator, page, allocation_index, out);
  } else if (alignment > 0) {
    data = page_alloc_aligned(allocator, page, size, alignment, is_realloc, out);
  } else {
    data = page_alloc(allocator, page, size, is_realloc, out);
  }

  assert(data != 0);
  
  kl_mutex_unlock(&allocator->lock);

  return data;
}

uint8_t* kl_allocator_malloc(KL_Allocator* allocator, int64_t size) {
  return generic_malloc(allocator, size, 0, false, 0);
}

uint8_t* kl_allocator_malloc_aligned(KL_Allocator* allocator, int64_t size, size_t alignment) {
  return generic_malloc(allocator, size, alignment, false, 0);
}

uint8_t* kl_allocator_malloc_tracked(KL_Allocator* allocator, int64_t size, KL_Allocation* out) {
  return generic_malloc(allocator, size, 0, false, out);
}

uint8_t* kl_allocator_malloc_tracked_aligned(KL_Allocator* allocator, int64_t size, size_t alignment, KL_Allocation* out) {
  return generic_malloc(allocator, size, alignment, false, out);
}

static inline uint8_t* generic_realloc(KL_Allocator* allocator, int64_t new_size, size_t alignment, KL_Allocation* allocation) {
  assert(allocator->initialized == true);

  if (allocation->size <= 0) return generic_malloc(allocator, new_size, alignment, false, allocation);

  kl_mutex_lock(&allocator->lock);
  assert(allocator->initialized == true);
  assert(new_size > 0);

  int64_t old_size = allocation->size;
  KL_Allocation old_allocation = *allocation;
  uint8_t* old_data = find_old_data(allocator, allocation);

  if (new_size <= allocation->size) {
    kl_mutex_unlock(&allocator->lock);
    return old_data;
  }

  uint8_t* data = generic_malloc(allocator, new_size, alignment, false, allocation);
  assert(data != 0);

  if (old_data != 0 && data != 0 && old_size > 0) {
    memcpy(&data[0], &old_data[0], old_size);
  }

  // Return the old region to the freed pool so its space is reclaimable. Done
  // after the memcpy because free_allocation zeros the freed bytes.
  if (allocator->config.track_allocations) {
    if (free_allocation(allocator, old_allocation) == 1) {
      allocator->allocations.n_freed += 1;
    }
  }

  kl_mutex_unlock(&allocator->lock);

  return data;
}

uint8_t* kl_allocator_realloc(KL_Allocator* allocator, int64_t new_size, KL_Allocation* allocation) {
  return generic_realloc(allocator, new_size, 0, allocation);
}

uint8_t* kl_allocator_realloc_aligned(KL_Allocator* allocator, int64_t new_size, size_t alignment, KL_Allocation* allocation) {
  return generic_realloc(allocator, new_size, alignment, allocation);
}

char* kl_allocator_strdup(KL_Allocator* allocator, const char* str, int64_t length) {
  assert(allocator->initialized == true);
  assert(length > 0);
  assert(str != 0);

  char* copy = (char*)kl_allocator_malloc(allocator, length + 1);
  assert(copy != 0);
  memccpy(&copy[0], &str[0], 0, length);

  return copy;
}


char* kl_allocator_sprintf(KL_Allocator* allocator, const char* fmt, ...) {
  va_list argptr = {0};
  va_start(argptr, fmt);
  int64_t fmt_len = strlen(fmt);
  int64_t fmt_count = 0;
  for (int64_t i = 0; i < fmt_len; i++) {
    if (fmt[i] == '%') {
      fmt_count += (fmt[(i+1) % fmt_len] == 's' ? 4 : 2);
    }
  }
  int64_t capacity = fmt_len * 2 + (fmt_count * (fmt_len / 3) + 16);
  char* formatted = (char*)kl_allocator_malloc(allocator, capacity + 1);
  assert(formatted != 0);
  vsnprintf(formatted, capacity, fmt, argptr);
  va_end(argptr);
  return formatted;
}

int kl_allocator_free_allocation(KL_Allocator* allocator, KL_Allocation allocation) {
  kl_mutex_lock(&allocator->lock);
  assert(allocator->initialized == true);

  if (free_allocation(allocator, allocation) == 1) {
    allocator->allocations.n_freed += 1;
  }
  
  kl_mutex_unlock(&allocator->lock);
  return 1;
}


KL_AllocatorMark kl_allocator_save(KL_Allocator* allocator) {
  assert(allocator->initialized == true);

  kl_mutex_lock(&allocator->lock);

  KL_AllocatorMark mark = {0};
  mark.pages_length = allocator->pages.length;
  mark.allocations_length = allocator->allocations.length;

  if (allocator->pages.length > 0) {
    mark.page_bytes_written = (int64_t*)malloc(allocator->pages.length * sizeof(int64_t));
    assert(mark.page_bytes_written != 0);
    for (int64_t i = 0; i < allocator->pages.length; i++) {
      mark.page_bytes_written[i] = allocator->pages.items[i].bytes_written;
    }
  } else {
    mark.page_bytes_written = 0;
  }

  kl_mutex_unlock(&allocator->lock);
  return mark;
}

void kl_allocator_restore(KL_Allocator* allocator, KL_AllocatorMark mark) {
  assert(allocator->initialized == true);

  kl_mutex_lock(&allocator->lock);

  // Restore pages that existed at save time
  for (int64_t i = 0; i < mark.pages_length; i++) {
    KL_AllocatorPage* page = &allocator->pages.items[i];
    int64_t saved = mark.page_bytes_written[i];
    if (page->data != 0 && page->bytes_written > saved) {
      memset(&page->data[saved], 0, page->bytes_written - saved);
    }
    page->bytes_written = saved;
  }

  // Free pages created after save
  for (int64_t i = mark.pages_length; i < allocator->pages.length; i++) {
    KL_AllocatorPage* page = &allocator->pages.items[i];
    if (page->data != 0) {
      free(page->data);
    }
    memset(page, 0, sizeof(KL_AllocatorPage));
  }

  allocator->pages.length = mark.pages_length;
  allocator->allocations.length = mark.allocations_length;

  // Truncating the slot array invalidates any freed-list links pointing at
  // (or through) removed slots — rebuild the lists from the surviving slots
  // and recount n_freed to match.
  free_list_reset(allocator);
  allocator->allocations.n_freed = 0;
  for (int64_t i = 0; i < allocator->allocations.length; i++) {
    if (allocator->allocations.items[i].freed) {
      free_list_push(allocator, i);
      allocator->allocations.n_freed += 1;
    }
  }

  // Maintain the invariant new_page()/find_avail_page() rely on (and that
  // destroy/destroy_all_pages uphold): an empty page list has a NULL backing
  // array. Restoring down to zero pages — e.g. a mark taken right after init,
  // before any allocation — must release the array too, otherwise the next
  // allocation trips `assert(pages.items == 0)`.
  if (allocator->pages.length == 0 && allocator->pages.items != 0) {
    free(allocator->pages.items);
    allocator->pages.items = 0;
  }

  free(mark.page_bytes_written);

  kl_mutex_unlock(&allocator->lock);
}

void kl_allocator_print_diagnostics(const KL_Allocator* allocator, FILE* fp) {
  assert(allocator->initialized == true);
  assert(fp != 0);

  fprintf(fp, "Allocations: [\n");
  for (int64_t i = 0; i < allocator->allocations.length; i++) {
    KL_Allocation a = allocator->allocations.items[i];
    fprintf(fp, "%*s{\n", 2, "");
    fprintf(fp, "%*sindex: %ld\n", 2+2, "", i);
    fprintf(fp, "%*spage: %ld\n", 2+2, "", a.page);
    fprintf(fp, "%*soffset: %ld\n", 2+2, "", a.offset);
    fprintf(fp, "%*ssize: %ld\n", 2+2, "", a.size);
    fprintf(fp, "%*salignment: %ld\n", 2+2, "", a.alignment);
    fprintf(fp, "%*sfreed: %d\n", 2+2, "", a.freed);
    fprintf(fp, "%*s}\n", 2, "");
  }
  fprintf(fp, "]\n");
  fprintf(fp, "allocations.length: %ld\n", allocator->allocations.length);
  fprintf(fp, "allocations.n_allocated: %ld\n", allocator->allocations.n_allocated);
  fprintf(fp, "allocations.n_freed: %ld\n", allocator->allocations.n_freed);
}


void kl_allocator_print_diagnostics_summary(const KL_Allocator* allocator, FILE* fp) {
  assert(allocator->initialized == true);
  assert(fp != 0);

  fprintf(fp, "---- allocator diag ----\n");
  fprintf(fp, "- overview\n");
  fprintf(fp, "  - current_capacity: %ld\n", allocator->current_capacity);
  fprintf(fp, "  - config.capacity: %ld\n", allocator->config.capacity);
  fprintf(fp, "  - config.tracked_allocations_capacity: %ld\n", allocator->config.tracked_allocations_capacity);
  fprintf(fp, "  - config.track_allocations: %s\n", allocator->config.track_allocations ? "true" : "false");
  fprintf(fp, "- allocations\n");
  fprintf(fp, "  - count: %ld\n", allocator->allocations.length);
  fprintf(fp, "  - n_allocated: %ld\n", allocator->allocations.n_allocated);
  fprintf(fp, "  - n_freed: %ld\n", allocator->allocations.n_freed);
  fprintf(fp, "  - n_reused: %ld\n", allocator->allocations.n_reused);

  fprintf(fp, "- pages\n");
  fprintf(fp, "  - count: %ld\n", allocator->pages.length);

  size_t page_size_sum = 0;
  size_t bytes_written_sum = 0;

  for (int64_t i = 0; i < allocator->pages.length; i++) {
    KL_AllocatorPage page = allocator->pages.items[i];
    page_size_sum += page.size;
    bytes_written_sum += page.bytes_written;
  }

  fprintf(fp, "  - page_size_sum: %zu\n", page_size_sum);
  fprintf(fp, "  - bytes_written_sum: %zu\n", bytes_written_sum);


}
