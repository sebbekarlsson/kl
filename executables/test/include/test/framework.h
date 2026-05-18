#ifndef KL_TEST_FRAMEWORK_H
#define KL_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*KL_TestFunction)();

typedef struct {
  KL_TestFunction fn;
  const char* name;
} KL_TestEntry;

#define KL_TEST_STR(A) #A

#define KL_TEST_ENTRY(FN) ((KL_TestEntry){ .fn = (FN), .name = KL_TEST_STR(FN) })

#define KL_TEST_BEGIN(NAME) do { printf("## Running test '%s'\n", NAME); } while (0)

#define KL_TEST_END(NAME) do { printf("- '%s' passed ✓\n", NAME); } while (0)

#define KL_TEST_PRINT_ASSERT_FAILURE(MSG) do { fprintf(stderr, "[%s:%d]: Assertion failure: %s\n", __FILE_NAME__, __LINE__, MSG); } while (0)

#define KL_TEST_FAILED() do { fprintf(stderr, "Terminating.\n"); exit(1); } while (0)

#define KL_TEST_ASSERT(EXPR, MSG) do { if (!(EXPR)) { KL_TEST_PRINT_ASSERT_FAILURE(MSG); KL_TEST_FAILED(); } } while (0)

#define KL_TEST_EXPECT_EQ_ERROR_MESSAGE(A, B) "expected " KL_TEST_STR(A) " to be equal " KL_TEST_STR(B)
#define KL_TEST_EXPECT_EXPR(EXPR) KL_TEST_ASSERT(EXPR, "expected " KL_TEST_STR(EXPR) " to be truthy")
#define KL_TEST_EXPECT_EQ_INT(A, B) KL_TEST_ASSERT((A) == (B), KL_TEST_EXPECT_EQ_ERROR_MESSAGE(A, B))

#endif
