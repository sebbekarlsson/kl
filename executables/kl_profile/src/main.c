#include <stdio.h>
#include <kl/debug/profiler.h>
#include <kl/hash/hash.h>


static int64_t hash_increment(uint32_t* seed, int64_t count) {
  int64_t x = 0;

  for (int64_t i = 0; i < count; i++) {
    x += kl_hash_uint32(*seed + (uint32_t)i);
  }

  *seed = (uint32_t)x;
  return x;
}


int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  KL_Profiler profiler = {0};
  if (!kl_profiler_init(&profiler, (KL_ProfilerConfig){
    .filepath = KL_STR_CONST_LIT("./profile.txt"),
  })) {
    fprintf(stderr, "failed to open profile output\n");
    return 1;
  }

  kl_profiler_start(&profiler);

  uint32_t seed = 18935U;

  for (int64_t i = 0; i < 64; i++) {
    // An outer block wrapping the whole iteration: nested blocks are written
    // with their depth, so the inner ones show up as depth 1.
    kl_profiler_begin_block(&profiler, KL_STR_CONST_LIT("iteration"));

    {
      kl_profiler_begin_block(&profiler, KL_STR_CONST_LIT("hash_increment_1"));
      int64_t result = hash_increment(&seed, 512);
      printf("result: %ld\n", result);
      kl_profiler_end_block(&profiler);
    }

    {
      kl_profiler_begin_block(&profiler, KL_STR_CONST_LIT("hash_increment_2"));
      int64_t result = hash_increment(&seed, 1024);
      printf("result: %ld\n", result);
      kl_profiler_end_block(&profiler);
    }

    {
      kl_profiler_begin_block(&profiler, KL_STR_CONST_LIT("hash_increment_3"));
      int64_t result = hash_increment(&seed, 1024*4);
      printf("result: %ld\n", result);
      kl_profiler_end_block(&profiler);
    }

    {
      kl_profiler_begin_block(&profiler, KL_STR_CONST_LIT("hash_increment_4"));
      int64_t result = hash_increment(&seed, 1024*4*4);
      printf("result: %ld\n", result);
      kl_profiler_end_block(&profiler);
    }

    kl_profiler_end_block(&profiler);
  }

  kl_profiler_destroy(&profiler);
  return 0;
}
