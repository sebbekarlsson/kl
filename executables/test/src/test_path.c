#include <test/tests.h>

/* ---- path module test helpers ---- */

static KL_StrConst path_buf_view(const KL_StrBuffer* b) {
  return (KL_StrConst){.value = b->data, .length = b->length};
}

static void expect_path(KL_StrConst got, const char* expected) {
  int64_t exp_len = (int64_t)strlen(expected);
  bool eq;
  if (exp_len == 0) {
    eq = got.length == 0;
  } else {
    eq = got.length == exp_len && got.value != 0 &&
         memcmp(got.value, expected, (size_t)exp_len) == 0;
  }
  if (!eq) {
    fprintf(stderr, "[path test] expected \"%s\" but got \"%.*s\"\n",
            expected, (int)got.length, got.value != 0 ? got.value : "");
    KL_TEST_FAILED();
  }
}

static void expect_normalize(KL_StrBuffer* buf, const char* in, const char* out) {
  KL_TEST_ASSERT(kl_path_normalize(buf, KL_STR_CONST(in)) == true, "normalize unexpectedly failed");
  expect_path(path_buf_view(buf), out);
}

void test_path_is_absolute(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "detects absolute vs relative paths");
  {
    KL_TEST_EXPECT_EXPR(kl_path_is_absolute(KL_STR_CONST_LIT("")) == false);
    KL_TEST_EXPECT_EXPR(kl_path_is_absolute(KL_STR_CONST_LIT("/")) == true);
    KL_TEST_EXPECT_EXPR(kl_path_is_absolute(KL_STR_CONST_LIT("/foo")) == true);
    KL_TEST_EXPECT_EXPR(kl_path_is_absolute(KL_STR_CONST_LIT("foo")) == false);
    KL_TEST_EXPECT_EXPR(kl_path_is_absolute(KL_STR_CONST_LIT("./foo")) == false);
    KL_TEST_EXPECT_EXPR(kl_path_is_absolute(KL_STR_CONST_LIT(".")) == false);
    KL_TEST_EXPECT_EXPR(kl_path_is_absolute((KL_StrConst){0}) == false);
  }
  KL_TEST_IT_END(ctx);
}

void test_path_dirname(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "returns the directory portion of a path");
  {
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("")), ".");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("/")), "/");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("foo")), ".");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("foo/bar")), "foo");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("/foo/bar")), "/foo");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("/foo/bar/")), "/foo");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("/foo")), "/");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("a//b")), "a/");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT("foo/..")), "foo");
    expect_path(kl_path_dirname(KL_STR_CONST_LIT(".hidden")), ".");
  }
  KL_TEST_IT_END(ctx);
}

void test_path_basename(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "returns the last component of a path");
  {
    expect_path(kl_path_basename(KL_STR_CONST_LIT("")), "");
    expect_path(kl_path_basename(KL_STR_CONST_LIT("/")), "");
    expect_path(kl_path_basename(KL_STR_CONST_LIT("/foo/bar")), "bar");
    expect_path(kl_path_basename(KL_STR_CONST_LIT("/foo/bar/")), "bar");
    expect_path(kl_path_basename(KL_STR_CONST_LIT("foo")), "foo");
    expect_path(kl_path_basename(KL_STR_CONST_LIT("a//b//")), "b");
    expect_path(kl_path_basename(KL_STR_CONST_LIT("..")), "..");
    expect_path(kl_path_basename(KL_STR_CONST_LIT("index.tar.gz")), "index.tar.gz");
  }
  KL_TEST_IT_END(ctx);
}

void test_path_extname(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "returns the extension of a path");
  {
    expect_path(kl_path_extname(KL_STR_CONST_LIT("index.html")), ".html");
    expect_path(kl_path_extname(KL_STR_CONST_LIT("index.")), ".");
    expect_path(kl_path_extname(KL_STR_CONST_LIT("index")), "");
    expect_path(kl_path_extname(KL_STR_CONST_LIT(".hidden")), "");
    expect_path(kl_path_extname(KL_STR_CONST_LIT("index.tar.gz")), ".gz");
    expect_path(kl_path_extname(KL_STR_CONST_LIT("/p/to/f.txt")), ".txt");
    expect_path(kl_path_extname(KL_STR_CONST_LIT("a/b/.gitignore")), "");
    expect_path(kl_path_extname(KL_STR_CONST_LIT("..")), "");
    expect_path(kl_path_extname(KL_STR_CONST_LIT("...txt")), ".txt");
    expect_path(kl_path_extname(KL_STR_CONST_LIT("noext/")), "");
  }
  KL_TEST_IT_END(ctx);
}

void test_path_join(KL_TestFunctionContext* ctx) {
  char data[1024];
  KL_StrBuffer buf = {.data = data, .length = 0, .capacity = sizeof(data)};

  KL_TEST_IT(ctx, "joins and normalizes path segments");
  {
    KL_TEST_EXPECT_EXPR(kl_path_join(&buf, NULL, 0) == true);
    expect_path(path_buf_view(&buf), ".");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT(""), KL_STR_CONST_LIT("")) == true);
    expect_path(path_buf_view(&buf), ".");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("bar")) == true);
    expect_path(path_buf_view(&buf), "foo/bar");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("/bar")) == true);
    expect_path(path_buf_view(&buf), "foo/bar");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT(""), KL_STR_CONST_LIT("/foo")) == true);
    expect_path(path_buf_view(&buf), "/foo");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("/"), KL_STR_CONST_LIT("foo")) == true);
    expect_path(path_buf_view(&buf), "/foo");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("bar/")) == true);
    expect_path(path_buf_view(&buf), "foo/bar/");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT(".."), KL_STR_CONST_LIT("bar")) == true);
    expect_path(path_buf_view(&buf), "bar");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("bar"), KL_STR_CONST_LIT("..")) == true);
    expect_path(path_buf_view(&buf), "foo");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("../../bar")) == true);
    expect_path(path_buf_view(&buf), "../bar");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("/foo"), KL_STR_CONST_LIT("..")) == true);
    expect_path(path_buf_view(&buf), "/");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("a//b"), KL_STR_CONST_LIT("c")) == true);
    expect_path(path_buf_view(&buf), "a/b/c");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT(".."), KL_STR_CONST_LIT("..")) == true);
    expect_path(path_buf_view(&buf), "../..");

    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&buf, KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("bar/"), KL_STR_CONST_LIT("..")) == true);
    expect_path(path_buf_view(&buf), "foo");
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "reports capacity overflow");
  {
    char small[4];
    KL_StrBuffer sb = {.data = small, .length = 0, .capacity = sizeof(small)};
    KL_TEST_EXPECT_EXPR(KL_PATH_JOIN(&sb, KL_STR_CONST_LIT("aaaa"), KL_STR_CONST_LIT("bbbb")) == false);
  }
  KL_TEST_IT_END(ctx);
}

void test_path_normalize(KL_TestFunctionContext* ctx) {
  char data[1024];
  KL_StrBuffer buf = {.data = data, .length = 0, .capacity = sizeof(data)};

  KL_TEST_IT(ctx, "collapses '.', '..' and duplicate separators");
  {
    expect_normalize(&buf, "", ".");
    expect_normalize(&buf, ".", ".");
    expect_normalize(&buf, "..", "..");
    expect_normalize(&buf, "/", "/");
    expect_normalize(&buf, "//", "/");
    expect_normalize(&buf, "foo/", "foo/");
    expect_normalize(&buf, "a//b//c", "a/b/c");
    expect_normalize(&buf, "foo/./bar", "foo/bar");
    expect_normalize(&buf, "foo/../bar", "bar");
    expect_normalize(&buf, "/foo/../bar", "/bar");
    expect_normalize(&buf, "foo/bar/..", "foo");
    expect_normalize(&buf, "foo/bar/../", "foo/");
    expect_normalize(&buf, "a/..", ".");
    expect_normalize(&buf, "a/../", "./");
    expect_normalize(&buf, "foo/bar/../../", "./");
    expect_normalize(&buf, "/..", "/");
    expect_normalize(&buf, "/../foo", "/foo");
    expect_normalize(&buf, "../../foo", "../../foo");
    expect_normalize(&buf, "foo/../../bar", "../bar");
    expect_normalize(&buf, "a/b/../../../..", "../..");
  }
  KL_TEST_IT_END(ctx);
}

void test_path_resolve(KL_TestFunctionContext* ctx) {
  char data[1024];
  KL_StrBuffer buf = {.data = data, .length = 0, .capacity = sizeof(data)};
  KL_StrConst cwd = KL_STR_CONST_LIT("/home/user");

  KL_TEST_IT(ctx, "resolves parts against an absolute cwd");
  {
    KL_TEST_EXPECT_EXPR(kl_path_resolve(&buf, cwd, NULL, 0) == true);
    expect_path(path_buf_view(&buf), "/home/user");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("foo")) == true);
    expect_path(path_buf_view(&buf), "/home/user/foo");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("/foo")) == true);
    expect_path(path_buf_view(&buf), "/foo");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("/foo"), KL_STR_CONST_LIT("/bar"), KL_STR_CONST_LIT("baz")) == true);
    expect_path(path_buf_view(&buf), "/bar/baz");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("/foo"), KL_STR_CONST_LIT("..")) == true);
    expect_path(path_buf_view(&buf), "/");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("/foo/bar"), KL_STR_CONST_LIT("../baz")) == true);
    expect_path(path_buf_view(&buf), "/foo/baz");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("/foo/bar/")) == true);
    expect_path(path_buf_view(&buf), "/foo/bar");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("..")) == true);
    expect_path(path_buf_view(&buf), "/home");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("../../..")) == true);
    expect_path(path_buf_view(&buf), "/");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT("/a"), KL_STR_CONST_LIT(""), KL_STR_CONST_LIT("/b")) == true);
    expect_path(path_buf_view(&buf), "/b");

    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, cwd, KL_STR_CONST_LIT(".")) == true);
    expect_path(path_buf_view(&buf), "/home/user");
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "fails when neither cwd nor any part is absolute");
  {
    KL_TEST_EXPECT_EXPR(KL_PATH_RESOLVE(&buf, KL_STR_CONST_LIT("relative"), KL_STR_CONST_LIT("foo")) == false);
  }
  KL_TEST_IT_END(ctx);
}

void test_path_cwd(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "fills the buffer with an absolute working directory");
  {
    char data[4096];
    KL_StrBuffer buf = {.data = data, .length = 0, .capacity = sizeof(data)};
    KL_TEST_EXPECT_EXPR(kl_path_cwd(&buf) == true);
    KL_TEST_EXPECT_EXPR(buf.length > 0);
    KL_TEST_EXPECT_EXPR(kl_path_is_absolute(path_buf_view(&buf)) == true);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "fails for an undersized or empty buffer");
  {
    char tiny[1];
    KL_StrBuffer small = {.data = tiny, .length = 0, .capacity = sizeof(tiny)};
    KL_TEST_EXPECT_EXPR(kl_path_cwd(&small) == false);

    KL_StrBuffer none = {0};
    KL_TEST_EXPECT_EXPR(kl_path_cwd(&none) == false);
  }
  KL_TEST_IT_END(ctx);
}
