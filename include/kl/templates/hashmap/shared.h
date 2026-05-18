#ifndef KL_HASHMAP__SHARED
#define KL_HASHMAP__SHARED
#include <stdint.h>
#include <stdbool.h>
/* v2: slot layout shrunk (intrusive single-link chain via `next`); collision
   slots no longer carry per-slot KL_int64_List next_ids and the redundant id
   field is gone. Wire format breaks accordingly. */
#define KL_HASHMAP_MAGIC__SHARED 0x3FA3CEC1

typedef struct {
  int64_t capacity;
  int64_t collision_capacity;
  int64_t max_key_size;
  bool track_used_slots;
} KL_HashmapConfig;

typedef struct {
  uint64_t hash;
  int64_t index;
} KL_HashmapHash;
#endif
