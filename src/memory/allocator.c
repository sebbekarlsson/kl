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

static inline void add_allocation(KL_Allocator* allocator, KL_Allocation allocation) {
  allocation.freed = false;
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

static inline void remove_allocation(KL_Allocator* allocator, KL_Allocation allocation) {
  if (allocator->allocations.items == 0 || allocator->allocations.n_allocated <= 0 || allocator->allocations.length <= 0) return;

  int64_t idx = -1;

  for (int64_t i = 0; i < allocator->allocations.length; i++) {
    KL_Allocation a = allocator->allocations.items[i];
    if (a.page == allocation.page && a.offset == allocation.offset && a.size == allocation.size) {
      idx = i;
      break;
    }
  }

  if (idx < 0) return;

  assert(idx < allocator->allocations.length);
  assert(idx >= 0);

  for (int64_t i = idx; i < allocator->allocations.length - 1; i++) {
    allocator->allocations.items[i] = allocator->allocations.items[i + 1];
  }

  allocator->allocations.length -= 1;
  assert(allocator->allocations.length >= 0);
}

static inline int free_allocation(KL_Allocator* allocator, KL_Allocation allocation) {
  if (allocator->allocations.items == 0 || allocator->allocations.n_allocated <= 0 || allocator->allocations.length <= 0) return 0;
  if (allocation.freed || allocation.size <= 0) return 0;

  for (int64_t i = 0; i < allocator->allocations.length; i++) {
    KL_Allocation* a = &allocator->allocations.items[i];
    if (a->page == allocation.page && a->offset == allocation.offset && a->size == allocation.size) {
      a->freed = true;

      if (a->size > 0 && a->page >= 0 && a->page < allocator->pages.length && allocator->pages.items != 0) {
        KL_AllocatorPage* page = &allocator->pages.items[a->page];
        if (page->index == a->page && page->data != 0 && page->size >= a->size && a->offset < page->size) {
          memset(&page->data[a->offset], 0, a->size);
        }
      }
      return 1;
    }
  }

  return 0;
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

static inline KL_AllocatorPage* find_avail_page(KL_Allocator* allocator, int64_t size, int64_t* out_allocation_index) {
  *out_allocation_index = -1;
  
  if (allocator->pages.length <= 0) {
    assert(allocator->pages.items == 0);
    return new_page(allocator, size);
  }

  assert(allocator->pages.items != 0);
  assert(allocator->pages.length > 0);

  KL_AllocatorPage* first = &allocator->pages.items[0];
  if (page_fits(first, size)) return first;
  KL_AllocatorPage* last = &allocator->pages.items[KL_MAX(0, allocator->pages.length-1)];
  if (page_fits(last, size)) return last;

  if (allocator->allocations.n_freed > 0 && allocator->allocations.items != 0 && allocator->allocations.length > 0) {
    for (int64_t i = 0; i < allocator->allocations.length; i++) {
      KL_Allocation a = allocator->allocations.items[i];
      if (a.freed && a.page >= 0 && a.page < allocator->pages.length) {
        KL_AllocatorPage* page = &allocator->pages.items[a.page];
        if (page->size >= size && a.size >= size && page->data != 0 && page->index == a.page) {
          *out_allocation_index = i;
          return page;
        }
      } 
    }
  }
  
  return new_page(allocator, size);
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
  KL_Allocation allocation = allocator->allocations.items[allocation_index];
  assert(allocation.offset >= 0);
  assert(allocation.offset < page->size);
  uint8_t* data = &page->data[allocation.offset];
  remove_allocation(allocator, allocation);
  allocator->allocations.n_freed -= 1;
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
  
  if (!is_realloc && allocator->config.track_allocations) {
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
  
  int64_t offset = page->bytes_written;
  size_t total_size_needed = size + alignment - 1;
  bool fits = page_fits(page, total_size_needed);
  assert(fits == true);

  uintptr_t current_ptr = (uintptr_t)page->data + page->bytes_written;
  uintptr_t aligned_ptr = (current_ptr + alignment - 1) & ~(alignment - 1);
  uint8_t padding = (uint8_t)(aligned_ptr - current_ptr);

  //*(uint8_t*)(aligned_ptr) = padding;
  page->bytes_written += size + padding;

  KL_Allocation allocation = {0};
  allocation.offset = offset;
  allocation.size = size + padding;
  allocation.page = page->index;
  allocation.alignment = alignment;

  if (!is_realloc && allocator->config.track_allocations) {
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
  KL_AllocatorPage* page = find_avail_page(allocator, total_size_needed, &allocation_index);
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
  
  if (allocation->size <= 0) return generic_malloc(allocator, new_size, alignment, true, allocation);
  
  kl_mutex_lock(&allocator->lock);
  assert(allocator->initialized == true);
  assert(allocator->config.track_allocations == false); // realloc not supported if this is true
  
  assert(new_size > 0);

  int64_t old_size = allocation->size;
  uint8_t* old_data = (allocation->size > 0) ? find_old_data(allocator, allocation) : 0;

  if (new_size <= allocation->size) {
    kl_mutex_unlock(&allocator->lock);
    return old_data;
  }
  
  uint8_t* data = generic_malloc(allocator, new_size, alignment, true, allocation);
  assert(data != 0);

  if (old_data != 0 && data != 0 && allocation->size > 0) {
    memcpy(&data[0], &old_data[0], old_size);
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

  free(mark.page_bytes_written);

  kl_mutex_unlock(&allocator->lock);
}

void kl_allocator_print_diagnostics(KL_Allocator* allocator, FILE* fp) {
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
