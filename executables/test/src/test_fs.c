#include <test/tests.h>

void test_fs_read_directory_rejects_bad_args() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  KL_file_List files = {0};

  // null value
  KL_TEST_EXPECT_EXPR(!kl_fs_read_directory((KL_StrConst){0}, false, &a, &files));
  // zero length
  KL_TEST_EXPECT_EXPR(!kl_fs_read_directory((KL_StrConst){ .value = "src", .length = 0 }, false, &a, &files));
  // null out_files
  KL_TEST_EXPECT_EXPR(!kl_fs_read_directory(KL_STR_CONST_LIT("src"), false, &a, 0));
  // null allocator
  KL_TEST_EXPECT_EXPR(!kl_fs_read_directory(KL_STR_CONST_LIT("src"), false, 0, &files));

  kl_allocator_destroy(&a);
}

void test_fs_read_directory_lists_project_src_non_recursive() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });

  KL_file_List files = {0};
  bool ok = kl_fs_read_directory(KL_STR_CONST_LIT("src"), false, &a, &files);
  KL_TEST_ASSERT(ok, "kl_fs_read_directory returned true for 'src'");
  KL_TEST_ASSERT(files.initialized, "out_files auto-initialized");
  KL_TEST_ASSERT(files.length > 0, "got at least one entry");
  KL_TEST_ASSERT(files.items != 0, "items buffer is non-null");

  // Every entry should have a non-null name and parent_path matching the input.
  bool found_io = false;
  bool found_memory = false;
  bool found_string = false;
  for (int64_t i = 0; i < files.length; i++) {
    KL_File f = files.items[i];
    KL_TEST_ASSERT(f.name.value != 0, "entry name.value is non-null");
    KL_TEST_ASSERT(f.name.length > 0 && f.name.length < 4096, "entry name.length is sane");
    KL_TEST_EXPECT_EQ_INT((int)f.name.length, (int)strlen(f.name.value));
    KL_TEST_EXPECT_EXPR(kl_str_const_eq(f.parent_path, KL_STR_CONST_LIT("src")));
    // The dummy "." and ".." entries must have been skipped.
    KL_TEST_EXPECT_EXPR(!kl_str_const_eq(f.name, KL_STR_CONST_LIT(".")));
    KL_TEST_EXPECT_EXPR(!kl_str_const_eq(f.name, KL_STR_CONST_LIT("..")));

    if (kl_str_const_eq(f.name, KL_STR_CONST_LIT("io"))) {
      KL_TEST_ASSERT(f.is_directory, "src/io is a directory");
      found_io = true;
    }
    if (kl_str_const_eq(f.name, KL_STR_CONST_LIT("memory"))) {
      KL_TEST_ASSERT(f.is_directory, "src/memory is a directory");
      found_memory = true;
    }
    if (kl_str_const_eq(f.name, KL_STR_CONST_LIT("string"))) {
      KL_TEST_ASSERT(f.is_directory, "src/string is a directory");
      found_string = true;
    }
  }
  KL_TEST_ASSERT(found_io, "non-recursive listing of 'src' contains 'io'");
  KL_TEST_ASSERT(found_memory, "non-recursive listing of 'src' contains 'memory'");
  KL_TEST_ASSERT(found_string, "non-recursive listing of 'src' contains 'string'");

  kl_allocator_destroy(&a);
}

void test_fs_read_directory_recursive_descends_into_subdirs() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 16384 });

  KL_file_List files = {0};
  bool ok = kl_fs_read_directory(KL_STR_CONST_LIT("src"), true, &a, &files);
  KL_TEST_ASSERT(ok, "recursive kl_fs_read_directory returned true for 'src'");

  // A recursive walk must surface strictly more entries than a non-recursive one,
  // because 'src' has nested subdirectories with files.
  KL_file_List shallow = {0};
  KL_TEST_EXPECT_EXPR(kl_fs_read_directory(KL_STR_CONST_LIT("src"), false, &a, &shallow));
  KL_TEST_ASSERT(files.length > shallow.length, "recursive yields more entries than non-recursive");

  // The recursive walk should reach a known leaf file: src/io/fs.c.
  bool found_fs_c = false;
  for (int64_t i = 0; i < files.length; i++) {
    KL_File f = files.items[i];
    if (!f.is_directory
        && kl_str_const_eq(f.name, KL_STR_CONST_LIT("fs.c"))
        && kl_str_const_ends_with(f.parent_path, KL_STR_CONST_LIT("io"))) {
      found_fs_c = true;
      break;
    }
  }
  KL_TEST_ASSERT(found_fs_c, "recursive walk reached src/io/fs.c");

  kl_allocator_destroy(&a);
}
