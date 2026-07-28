#ifndef KL_SERIAL_H
#define KL_SERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <kl/binary/blob.h>
#include <kl/memory/allocator.h>
#include <kl/string/string.h>
#include <stdio.h>

typedef int32_t KL_Serial_int24;
typedef uint32_t KL_Serial_uint24;

typedef enum {
  KL_SERIAL_COMPONENT_NIL,
  KL_SERIAL_COMPONENT_INT8,
  KL_SERIAL_COMPONENT_UINT8,
  KL_SERIAL_COMPONENT_INT16,
  KL_SERIAL_COMPONENT_UINT16,
  KL_SERIAL_COMPONENT_INT24,
  KL_SERIAL_COMPONENT_UINT24,
  KL_SERIAL_COMPONENT_INT32,
  KL_SERIAL_COMPONENT_UINT32,
  KL_SERIAL_COMPONENT_INT64,
  KL_SERIAL_COMPONENT_UINT64,
  KL_SERIAL_COMPONENT_F32,
  KL_SERIAL_COMPONENT_F64
} KL_SerialComponentType;


typedef enum {
  KL_SERIAL_ELEMENT_SCALAR,
  KL_SERIAL_ELEMENT_STRING,
  KL_SERIAL_ELEMENT_BLOCK
} KL_SerialElementType;


typedef struct {
  KL_SerialComponentType component_type;
  KL_SerialElementType element_type;
  size_t alignment;
  size_t length; // how many elements
  size_t size; // how big the buffer is
  uint8_t* buffer;
} KL_SerialBlock;

typedef struct KL_SerialDecodedBlock KL_SerialDecodedBlock;

struct KL_SerialDecodedBlock {
  KL_SerialComponentType component_type;
  KL_SerialElementType element_type;

  size_t alignment;
  size_t length;
  size_t size;

  union {
    struct { uint8_t _unused; } nil;
    int8_t* int8;
    uint8_t* uint8;
    int16_t* int16;
    uint16_t* uint16;
    KL_Serial_int24* int24;
    KL_Serial_uint24* uint24;
    int32_t* int32;
    uint32_t* uint32;
    int64_t* int64;
    uint64_t* uint64;
    float* f32;
    double* f64;
    KL_StrConst string;
    KL_SerialDecodedBlock* blocks;
  } as;
};


size_t kl_serial_size_of_component(KL_SerialComponentType component);

unsigned long kl_serial_block_write_to_file(KL_SerialBlock block, FILE* fp);
unsigned long kl_serial_block_read_from_file(KL_SerialBlock* block_out, FILE* fp, KL_Allocator* allocator);

bool kl_serial_decode_blocks_from_file(KL_SerialDecodedBlock* block_out, FILE* fp, KL_Allocator* allocator);

#endif
