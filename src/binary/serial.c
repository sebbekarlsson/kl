#include <kl/binary/serial.h>
#include <string.h>


size_t kl_serial_size_of_component(KL_SerialComponentType component) {
  switch (component) {
    case KL_SERIAL_COMPONENT_NIL: return 0;
    case KL_SERIAL_COMPONENT_INT8: return sizeof(int8_t);
    case KL_SERIAL_COMPONENT_UINT8: return sizeof(uint8_t);
    case KL_SERIAL_COMPONENT_INT16: return sizeof(int16_t);
    case KL_SERIAL_COMPONENT_UINT16: return sizeof(uint16_t);
    case KL_SERIAL_COMPONENT_INT24: return sizeof(KL_Serial_int24);
    case KL_SERIAL_COMPONENT_UINT24: return sizeof(KL_Serial_uint24);
    case KL_SERIAL_COMPONENT_INT32: return sizeof(int32_t);
    case KL_SERIAL_COMPONENT_UINT32: return sizeof(uint32_t);
    case KL_SERIAL_COMPONENT_INT64: return sizeof(int64_t);
    case KL_SERIAL_COMPONENT_UINT64: return sizeof(uint64_t);
    case KL_SERIAL_COMPONENT_F32: return sizeof(float);
    case KL_SERIAL_COMPONENT_F64: return sizeof(double);
  }
  return 0;
}

#define KL_SERIAL_HEADER_SIZE ( \
    sizeof(KL_SerialComponentType) \
  + sizeof(KL_SerialElementType) \
  + sizeof(size_t) /* alignment */ \
  + sizeof(size_t) /* length */ \
  + sizeof(size_t) /* size */)

static bool kl_serial_write_header(const KL_SerialBlock* block, FILE* fp) {
  return fwrite(&block->component_type, sizeof(block->component_type), 1, fp) == 1
      && fwrite(&block->element_type, sizeof(block->element_type), 1, fp) == 1
      && fwrite(&block->alignment, sizeof(block->alignment), 1, fp) == 1
      && fwrite(&block->length, sizeof(block->length), 1, fp) == 1
      && fwrite(&block->size, sizeof(block->size), 1, fp) == 1;
}

static bool kl_serial_read_header(KL_SerialBlock* block, FILE* fp) {
  return fread(&block->component_type, sizeof(block->component_type), 1, fp) == 1
      && fread(&block->element_type, sizeof(block->element_type), 1, fp) == 1
      && fread(&block->alignment, sizeof(block->alignment), 1, fp) == 1
      && fread(&block->length, sizeof(block->length), 1, fp) == 1
      && fread(&block->size, sizeof(block->size), 1, fp) == 1;
}

unsigned long kl_serial_block_write_to_file(KL_SerialBlock block, FILE* fp) {
  if (fp == 0) return 0;
  if (!kl_serial_write_header(&block, fp)) return 0;

  unsigned long total = KL_SERIAL_HEADER_SIZE;
  if (block.size > 0) {
    if (block.buffer == 0) return 0;
    if (fwrite(block.buffer, block.size, 1, fp) != 1) return 0;
    total += block.size;
  }
  return total;
}

unsigned long kl_serial_block_read_from_file(KL_SerialBlock* block_out, FILE* fp, KL_Allocator* allocator) {
  if (block_out == 0 || fp == 0) return 0;

  memset(block_out, 0, sizeof(*block_out));
  if (!kl_serial_read_header(block_out, fp)) return 0;

  unsigned long total = KL_SERIAL_HEADER_SIZE;
  if (block_out->size > 0) {
    size_t alignment = block_out->alignment > 1 ? block_out->alignment : 1;
    block_out->buffer = kl_allocator_malloc_aligned(allocator, block_out->size, alignment);
    if (block_out->buffer == 0) return 0;
    if (fread(block_out->buffer, block_out->size, 1, fp) != 1) return 0;
    total += block_out->size;
  }
  return total;
}

bool kl_serial_decode_blocks_from_file(KL_SerialDecodedBlock* block_out, FILE* fp, KL_Allocator* allocator) {
  if (block_out == 0 || fp == 0) return false;

  memset(block_out, 0, sizeof(*block_out));

  KL_SerialBlock raw = {0};
  if (!kl_serial_read_header(&raw, fp)) return false;

  block_out->component_type = raw.component_type;
  block_out->element_type = raw.element_type;
  block_out->alignment = raw.alignment;
  block_out->length = raw.length;
  block_out->size = raw.size;

  switch (raw.element_type) {
    case KL_SERIAL_ELEMENT_SCALAR: {
      if (raw.component_type == KL_SERIAL_COMPONENT_NIL || raw.length == 0 || raw.size == 0) {
        block_out->component_type = KL_SERIAL_COMPONENT_NIL;
        // a nil block may still carry a payload we have to skip past
        if (raw.size > 0 && fseek(fp, (long)raw.size, SEEK_CUR) != 0) return false;
        return true;
      }

      size_t comp_size = kl_serial_size_of_component(raw.component_type);
      if (comp_size == 0 || raw.size != comp_size * raw.length) return false;

      size_t alignment = raw.alignment > 1 ? raw.alignment : comp_size;
      uint8_t* buffer = kl_allocator_malloc_aligned(allocator, raw.size, alignment);
      if (buffer == 0) return false;
      if (fread(buffer, raw.size, 1, fp) != 1) return false;

      // every typed member of the union aliases the same pointer
      block_out->as.uint8 = buffer;
    }; break;

    case KL_SERIAL_ELEMENT_STRING: {
      char* value = (char*)kl_allocator_malloc(allocator, raw.size + 1);
      if (value == 0) return false;
      if (raw.size > 0 && fread(value, raw.size, 1, fp) != 1) return false;
      value[raw.size] = 0;

      block_out->as.string = (KL_StrConst){ .value = value, .length = (int64_t)raw.size };
    }; break;

    case KL_SERIAL_ELEMENT_BLOCK: {
      // a block container has no payload of its own; its `length` children
      // follow sequentially in the stream
      if (raw.size > 0) return false;
      if (raw.length == 0) return true;

      KL_SerialDecodedBlock* children = (KL_SerialDecodedBlock*)
        kl_allocator_malloc(allocator, raw.length * sizeof(KL_SerialDecodedBlock));
      if (children == 0) return false;

      for (size_t i = 0; i < raw.length; i++) {
        if (!kl_serial_decode_blocks_from_file(&children[i], fp, allocator)) return false;
      }
      block_out->as.blocks = children;
    }; break;

    default: return false;
  }

  return true;
}
