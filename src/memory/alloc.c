#include <kl/memory/alloc.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#include <malloc.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif


void *kl_aligned_alloc(size_t size, size_t alignment) {
  if (alignment <= 0) return calloc(1, size);
  void* data = aligned_alloc(alignment, size);
  if (data == 0) return data;
  memset(data, 0, size);
  return data;
}

void *kl_aligned_realloc(void* ptr, size_t new_size, size_t alignment) {
  if (ptr == 0) return kl_aligned_alloc(new_size, alignment);

  if (new_size <= 0) {
    free(ptr);
    return 0;
  }

  size_t old_size = 0;
#if defined(_WIN32)
  old_size = _msize(ptr);
#elif defined(__APPLE__)
  old_size = malloc_size(ptr);
#else
  old_size = malloc_usable_size(ptr);
#endif

  void *new_ptr = aligned_alloc(alignment, new_size);
  if (new_ptr == 0) return 0;

  size_t copy_size = (new_size < old_size) ? new_size : old_size;
  memcpy(new_ptr, ptr, copy_size);

  free(ptr);
  return new_ptr;
}
