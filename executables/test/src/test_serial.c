#include <test/tests.h>

static FILE* serial_tmpfile() {
  FILE* fp = tmpfile();
  assert(fp != 0);
  return fp;
}

void test_serial_size_of_component() {
  KL_TEST_EXPECT_EQ_INT((int)kl_serial_size_of_component(KL_SERIAL_COMPONENT_NIL), 0);
  KL_TEST_EXPECT_EQ_INT((int)kl_serial_size_of_component(KL_SERIAL_COMPONENT_INT8), 1);
  KL_TEST_EXPECT_EQ_INT((int)kl_serial_size_of_component(KL_SERIAL_COMPONENT_UINT16), 2);
  KL_TEST_EXPECT_EQ_INT((int)kl_serial_size_of_component(KL_SERIAL_COMPONENT_INT24), 4);
  KL_TEST_EXPECT_EQ_INT((int)kl_serial_size_of_component(KL_SERIAL_COMPONENT_UINT32), 4);
  KL_TEST_EXPECT_EQ_INT((int)kl_serial_size_of_component(KL_SERIAL_COMPONENT_INT64), 8);
  KL_TEST_EXPECT_EQ_INT((int)kl_serial_size_of_component(KL_SERIAL_COMPONENT_F32), 4);
  KL_TEST_EXPECT_EQ_INT((int)kl_serial_size_of_component(KL_SERIAL_COMPONENT_F64), 8);
}

void test_serial_raw_block_round_trip() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  FILE* fp = serial_tmpfile();

  uint32_t values[4] = { 1, 2, 0xdeadbeef, 4 };
  KL_SerialBlock block = {
    .component_type = KL_SERIAL_COMPONENT_UINT32,
    .element_type = KL_SERIAL_ELEMENT_SCALAR,
    .alignment = alignof(uint32_t),
    .length = 4,
    .size = sizeof(values),
    .buffer = (uint8_t*)values
  };

  unsigned long written = kl_serial_block_write_to_file(block, fp);
  KL_TEST_ASSERT(written > 0, "write reports bytes written");

  rewind(fp);
  KL_SerialBlock read_back = {0};
  unsigned long read = kl_serial_block_read_from_file(&read_back, fp, &a);
  KL_TEST_EXPECT_EQ_INT((int)read, (int)written);
  KL_TEST_EXPECT_EQ_INT((int)read_back.component_type, (int)KL_SERIAL_COMPONENT_UINT32);
  KL_TEST_EXPECT_EQ_INT((int)read_back.element_type, (int)KL_SERIAL_ELEMENT_SCALAR);
  KL_TEST_EXPECT_EQ_INT((int)read_back.length, 4);
  KL_TEST_EXPECT_EQ_INT((int)read_back.size, (int)sizeof(values));
  KL_TEST_ASSERT(read_back.buffer != 0, "buffer allocated");
  KL_TEST_EXPECT_EXPR(memcmp(read_back.buffer, values, sizeof(values)) == 0);

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_scalar_array() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  FILE* fp = serial_tmpfile();

  int64_t values[3] = { -1, INT64_MAX, 42 };
  KL_SerialBlock block = {
    .component_type = KL_SERIAL_COMPONENT_INT64,
    .element_type = KL_SERIAL_ELEMENT_SCALAR,
    .alignment = alignof(int64_t),
    .length = 3,
    .size = sizeof(values),
    .buffer = (uint8_t*)values
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(block, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&decoded, fp, &a), "decode succeeds");
  KL_TEST_EXPECT_EQ_INT((int)decoded.component_type, (int)KL_SERIAL_COMPONENT_INT64);
  KL_TEST_EXPECT_EQ_INT((int)decoded.element_type, (int)KL_SERIAL_ELEMENT_SCALAR);
  KL_TEST_EXPECT_EQ_INT((int)decoded.length, 3);
  KL_TEST_ASSERT(decoded.as.int64 != 0, "typed pointer set");
  KL_TEST_EXPECT_EXPR(decoded.as.int64[0] == -1);
  KL_TEST_EXPECT_EXPR(decoded.as.int64[1] == INT64_MAX);
  KL_TEST_EXPECT_EXPR(decoded.as.int64[2] == 42);
  KL_TEST_EXPECT_EXPR(((uintptr_t)decoded.as.int64 % alignof(int64_t)) == 0);

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_scalar_single_f64() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  FILE* fp = serial_tmpfile();

  double value = 3.14159;
  KL_SerialBlock block = {
    .component_type = KL_SERIAL_COMPONENT_F64,
    .element_type = KL_SERIAL_ELEMENT_SCALAR,
    .alignment = alignof(double),
    .length = 1,
    .size = sizeof(value),
    .buffer = (uint8_t*)&value
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(block, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&decoded, fp, &a), "decode succeeds");
  KL_TEST_EXPECT_EXPR(decoded.as.f64[0] == 3.14159);

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_string() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  FILE* fp = serial_tmpfile();

  const char* text = "hello serial";
  KL_SerialBlock block = {
    .component_type = KL_SERIAL_COMPONENT_UINT8,
    .element_type = KL_SERIAL_ELEMENT_STRING,
    .alignment = 1,
    .length = strlen(text),
    .size = strlen(text),
    .buffer = (uint8_t*)text
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(block, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&decoded, fp, &a), "decode succeeds");
  KL_TEST_EXPECT_EQ_INT((int)decoded.element_type, (int)KL_SERIAL_ELEMENT_STRING);
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(decoded.as.string, KL_STR_CONST_LIT("hello serial")));
  // decoded strings are NUL-terminated even though the payload is not
  KL_TEST_EXPECT_EQ_INT((int)decoded.as.string.value[decoded.as.string.length], 0);

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_empty_string() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  FILE* fp = serial_tmpfile();

  KL_SerialBlock block = {
    .component_type = KL_SERIAL_COMPONENT_UINT8,
    .element_type = KL_SERIAL_ELEMENT_STRING,
    .alignment = 1,
    .length = 0,
    .size = 0,
    .buffer = 0
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(block, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&decoded, fp, &a), "decode succeeds");
  KL_TEST_EXPECT_EQ_INT((int)decoded.as.string.length, 0);
  KL_TEST_ASSERT(decoded.as.string.value != 0, "empty string still has a value pointer");
  KL_TEST_EXPECT_EQ_INT((int)decoded.as.string.value[0], 0);

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_nil_block() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  FILE* fp = serial_tmpfile();

  KL_SerialBlock block = {
    .component_type = KL_SERIAL_COMPONENT_NIL,
    .element_type = KL_SERIAL_ELEMENT_SCALAR,
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(block, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&decoded, fp, &a), "decode succeeds");
  KL_TEST_EXPECT_EQ_INT((int)decoded.component_type, (int)KL_SERIAL_COMPONENT_NIL);

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_nested_blocks() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 8192 });
  FILE* fp = serial_tmpfile();

  // parent container with two children: a scalar array and a string
  KL_SerialBlock parent = {
    .component_type = KL_SERIAL_COMPONENT_NIL,
    .element_type = KL_SERIAL_ELEMENT_BLOCK,
    .length = 2,
    .size = 0,
    .buffer = 0
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(parent, fp) > 0);

  float floats[2] = { 1.5f, -2.5f };
  KL_SerialBlock child_scalar = {
    .component_type = KL_SERIAL_COMPONENT_F32,
    .element_type = KL_SERIAL_ELEMENT_SCALAR,
    .alignment = alignof(float),
    .length = 2,
    .size = sizeof(floats),
    .buffer = (uint8_t*)floats
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(child_scalar, fp) > 0);

  const char* text = "nested";
  KL_SerialBlock child_string = {
    .component_type = KL_SERIAL_COMPONENT_UINT8,
    .element_type = KL_SERIAL_ELEMENT_STRING,
    .alignment = 1,
    .length = strlen(text),
    .size = strlen(text),
    .buffer = (uint8_t*)text
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(child_string, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&decoded, fp, &a), "decode succeeds");
  KL_TEST_EXPECT_EQ_INT((int)decoded.element_type, (int)KL_SERIAL_ELEMENT_BLOCK);
  KL_TEST_EXPECT_EQ_INT((int)decoded.length, 2);
  KL_TEST_ASSERT(decoded.as.blocks != 0, "children allocated");

  KL_SerialDecodedBlock* c0 = &decoded.as.blocks[0];
  KL_TEST_EXPECT_EQ_INT((int)c0->component_type, (int)KL_SERIAL_COMPONENT_F32);
  KL_TEST_EXPECT_EXPR(c0->as.f32[0] == 1.5f);
  KL_TEST_EXPECT_EXPR(c0->as.f32[1] == -2.5f);

  KL_SerialDecodedBlock* c1 = &decoded.as.blocks[1];
  KL_TEST_EXPECT_EQ_INT((int)c1->element_type, (int)KL_SERIAL_ELEMENT_STRING);
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(c1->as.string, KL_STR_CONST_LIT("nested")));

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_empty_block_container() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  FILE* fp = serial_tmpfile();

  KL_SerialBlock parent = {
    .element_type = KL_SERIAL_ELEMENT_BLOCK,
    .length = 0,
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(parent, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&decoded, fp, &a), "decode succeeds");
  KL_TEST_EXPECT_EQ_INT((int)decoded.length, 0);
  KL_TEST_EXPECT_EXPR(decoded.as.blocks == 0);

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_truncated_file_fails() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });

  // empty file: header read fails
  FILE* fp = serial_tmpfile();
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_EXPECT_EXPR(!kl_serial_decode_blocks_from_file(&decoded, fp, &a));
  fclose(fp);

  // header present but payload truncated
  fp = serial_tmpfile();
  uint32_t values[4] = { 1, 2, 3, 4 };
  KL_SerialBlock block = {
    .component_type = KL_SERIAL_COMPONENT_UINT32,
    .element_type = KL_SERIAL_ELEMENT_SCALAR,
    .alignment = alignof(uint32_t),
    .length = 4,
    .size = sizeof(values),
    .buffer = (uint8_t*)values
  };
  unsigned long written = kl_serial_block_write_to_file(block, fp);
  KL_TEST_EXPECT_EXPR(written > 0);
  fflush(fp);
  KL_TEST_EXPECT_EXPR(ftruncate(fileno(fp), (off_t)(written - 8)) == 0);

  rewind(fp);
  KL_TEST_EXPECT_EXPR(!kl_serial_decode_blocks_from_file(&decoded, fp, &a));
  fclose(fp);

  kl_allocator_destroy(&a);
}

void test_serial_decode_size_length_mismatch_fails() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  FILE* fp = serial_tmpfile();

  uint32_t values[4] = { 1, 2, 3, 4 };
  KL_SerialBlock block = {
    .component_type = KL_SERIAL_COMPONENT_UINT32,
    .element_type = KL_SERIAL_ELEMENT_SCALAR,
    .alignment = alignof(uint32_t),
    .length = 3, // lies: buffer holds 4 elements
    .size = sizeof(values),
    .buffer = (uint8_t*)values
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(block, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock decoded = {0};
  KL_TEST_EXPECT_EXPR(!kl_serial_decode_blocks_from_file(&decoded, fp, &a));

  fclose(fp);
  kl_allocator_destroy(&a);
}

void test_serial_decode_sequential_blocks() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 8192 });
  FILE* fp = serial_tmpfile();

  // two independent top-level blocks written back to back
  int32_t first = 7;
  KL_SerialBlock b0 = {
    .component_type = KL_SERIAL_COMPONENT_INT32,
    .element_type = KL_SERIAL_ELEMENT_SCALAR,
    .alignment = alignof(int32_t),
    .length = 1,
    .size = sizeof(first),
    .buffer = (uint8_t*)&first
  };
  const char* text = "second";
  KL_SerialBlock b1 = {
    .component_type = KL_SERIAL_COMPONENT_UINT8,
    .element_type = KL_SERIAL_ELEMENT_STRING,
    .alignment = 1,
    .length = strlen(text),
    .size = strlen(text),
    .buffer = (uint8_t*)text
  };
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(b0, fp) > 0);
  KL_TEST_EXPECT_EXPR(kl_serial_block_write_to_file(b1, fp) > 0);

  rewind(fp);
  KL_SerialDecodedBlock d0 = {0}, d1 = {0};
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&d0, fp, &a), "first decode succeeds");
  KL_TEST_ASSERT(kl_serial_decode_blocks_from_file(&d1, fp, &a), "second decode succeeds");
  KL_TEST_EXPECT_EQ_INT(d0.as.int32[0], 7);
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(d1.as.string, KL_STR_CONST_LIT("second")));

  // stream fully consumed: a third decode fails cleanly
  KL_SerialDecodedBlock d2 = {0};
  KL_TEST_EXPECT_EXPR(!kl_serial_decode_blocks_from_file(&d2, fp, &a));

  fclose(fp);
  kl_allocator_destroy(&a);
}
