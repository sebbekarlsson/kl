#ifndef KL_HASHMAP__SHARED
#define KL_HASHMAP__SHARED
#include <stdint.h>
#include <stdbool.h>
#include <kl/binary/blob.h>
#include <kl/string/string.h>
/* v2: slot layout shrunk (intrusive single-link chain via `next`); collision
   slots no longer carry per-slot KL_int64_List next_ids and the redundant id
   field is gone. Wire format breaks accordingly.
   v3: max_key_size removed from KL_HashmapConfig — per-slot key buffers now
   grow on demand, so keys are no longer length-capped. Config layout shrank. */
#define KL_HASHMAP_MAGIC__SHARED 0x3FA3CEC2

struct KL_ALLOCATOR_STRUCT;


typedef uint64_t (*KL_HashmapHashFn)(KL_Blob key, void* user_ptr);

typedef void (*KL_HashmapValueToStringFn)(void* value, KL_StrBuffer* out);

typedef struct {
  int64_t capacity;
  int64_t collision_capacity;
  bool track_used_slots;

  /**
  * @brief allocator: optional
  **/
  struct KL_ALLOCATOR_STRUCT* allocator;

  struct {
    KL_HashmapHashFn fn;
    void* user_ptr;
  } hash;


  KL_HashmapValueToStringFn value_to_string;
} KL_HashmapConfig;

typedef struct {
  uint64_t hash;
  int64_t index;
} KL_HashmapHash;
#endif
