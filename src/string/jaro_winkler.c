#include <assert.h>
#include <kl/macros.h>
#include <kl/string/jaro_winkler.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define JARO_STACK_MAX_LEN 256

static inline float jaro_sim(KL_StrConst a, KL_StrConst b, bool* a_matches, bool* b_matches) {
  int64_t match_window = KL_MAX((KL_MAX(a.length, b.length) / 2) - 1, 0);

  int64_t matches = 0;
  int64_t transpositions = 0;

  for (int64_t i = 0; i < a.length; i++) {
    int64_t start = KL_MAX(0, i - match_window);
    int64_t end = KL_MIN(i + match_window + 1, b.length);

    for (int64_t j = start; j < end; j++) {
      if (b_matches[j] || (a.value[i] != b.value[j])) continue;
      a_matches[i] = true;
      b_matches[j] = true;
      matches += 1;
      break;
    }
  }

  if (matches <= 0) return 0.0f;

  int64_t k = 0;
  for (int64_t i = 0; i < a.length; i++) {
    if (!a_matches[i]) continue;
    while (k < b.length && !b_matches[k]) k++;
    if (k >= b.length) break;
    if (a.value[i] != b.value[k]) transpositions++;
    k++;
  }

  return (
    ((float)matches / (float)a.length +
     (float)matches / (float)b.length +
     ((float)matches - (float)transpositions / 2.0f) / (float)matches) /
    3.0f
  );
}

float kl_str_const_jaro_winkler(KL_StrConst a, KL_StrConst b, float scaling_factor) {
  if (kl_str_const_eq(a, b)) return 1.0f;
  if (a.length <= 0 || b.length <= 0 || a.value == 0 || b.value == 0) return 0.0f;

  float jaro;
  if (a.length <= JARO_STACK_MAX_LEN && b.length <= JARO_STACK_MAX_LEN) {
    bool stack_buf[JARO_STACK_MAX_LEN * 2];
    memset(stack_buf, 0, (a.length + b.length) * sizeof(bool));
    jaro = jaro_sim(a, b, stack_buf, stack_buf + a.length);
  } else {
    bool* heap_buf = (bool*)calloc(a.length + b.length, sizeof(bool));
    assert(heap_buf != 0);
    jaro = jaro_sim(a, b, heap_buf, heap_buf + a.length);
    free(heap_buf);
  }

  float prefix_length = 0.0f;
  int64_t max_prefix = KL_MIN(KL_MIN(a.length, b.length), (int64_t)4);
  for (int64_t i = 0; i < max_prefix; i++) {
    if (a.value[i] == b.value[i]) {
      prefix_length += 1.0f;
    } else {
      break;
    }
  }

  return jaro + prefix_length * scaling_factor * (1.0f - jaro);
}
