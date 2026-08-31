#ifndef KL_BLOB_H
#define KL_BLOB_H

#include <stdint.h>
#include <stdalign.h>

typedef struct {
  uint8_t* value;
  int64_t size;
} KL_Blob;

#define KL_BLOB_T(T, X) ((KL_Blob){ .value = (uint8_t*)&((T[]){ X }), .size = sizeof(T) })
#define KL_BLOB_RAW_T(T, X) ((KL_Blob){ .value = (uint8_t*)&X, .size = sizeof(T) })
#define KL_BLOB_AUTO(X) KL_BLOB_T(typeof(X), X)
#define KL_BLOB_RAW_AUTO(X) KL_BLOB_RAW_T(typeof(X), X)
#define KL_BLOB_NULL() ((KL_Blob){0})

typedef struct {
  uint8_t* value;
  int64_t size;
  int64_t alignment;
} KL_BlobAligned;

#define KL_BLOB_ALIGNED_T(T, X) ((KL_BlobAligned){ .value = (uint8_t*)&((T[]){ X }), .size = sizeof(T), .alignment = alignof(T) })
#define KL_BLOB_ALIGNED_RAW_T(T, X) ((KL_BlobAligned){ .value = (uint8_t*)&X, .size = sizeof(T), .alignment = alignof(T) })
#define KL_BLOB_ALIGNED_AUTO(X) KL_BLOB_ALIGNED_T(typeof(X), X)
#define KL_BLOB_ALIGNED_RAW_AUTO(X) KL_BLOB_ALIGNED_RAW_T(typeof(X), X)
#define KL_BLOB_ALIGNED_NULL() ((KL_BlobAligned){0})

#endif
