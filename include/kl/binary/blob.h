#ifndef KL_BLOB_H
#define KL_BLOB_H

#include <stdint.h>

typedef struct {
  uint8_t* value;
  int64_t size;
} KL_Blob;

#define KL_BLOB_T(T, X) ((KL_Blob){ .value = (uint8_t*)&(X), .size = sizeof(T) })
#define KL_BLOB_AUTO(X) KL_BLOB_T(typeof(X), X)

#define KL_BLOB_NULL() ((KL_Blob){0})

#endif
