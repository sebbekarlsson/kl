#include <kl/common/number.h>
#include <math.h>

bool kl_is_unsafe_float32(float x) {
  return isnanf(x) || isinff(x);
}

bool kl_is_safe_float32(float x) {
  return !kl_is_unsafe_float32(x);
}
