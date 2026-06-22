#include <kl/color/color.h>
#include <kl/common/number.h>
#include <math.h>


static inline float gamma_to_linear(float c) {
  return c >= 0.04045f ? powf((c + 0.055f) / 1.055f, 2.4f) : c / 12.92f;
}

static inline float linear_to_gamma(float c) {
  return c >= 0.0031308f ? 1.055f * powf(c, 1.0f / 2.4f) - 0.055f : 12.92f * c ;
}

/* Convert any color into OKLab, carrying the alpha channel in items[3].
 * OKLab acts as the hub through which every translation routes. */
static KL_Color to_oklab(KL_Color color) {
  switch (color.type) {
    case KL_COLOR_OKLABF32:
      // already in the hub representation
      return color;

    case KL_COLOR_OKLCHF32: {
      float L = color.values.items[0];
      float C = color.values.items[1];
      float h = color.values.items[2];
      return (KL_Color){
        .type = KL_COLOR_OKLABF32,
        .values.items = {
          L,
          C * cosf(h),
          C * sinf(h),
          color.values.items[3],
        },
      };
    }

    case KL_COLOR_RGBF32:
    case KL_COLOR_RGBAF32: {
      float r = gamma_to_linear(color.values.r);
      float g = gamma_to_linear(color.values.g);
      float b = gamma_to_linear(color.values.b);

      float l = cbrtf(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
      float m = cbrtf(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
      float s = cbrtf(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);

      float alpha = color.type == KL_COLOR_RGBAF32 ? color.values.a : 1.0f;

      return (KL_Color){
        .type = KL_COLOR_OKLABF32,
        .values.items = {
          l * +0.2104542553f + m * +0.7936177850f + s * -0.0040720468f,
          l * +1.9779984951f + m * -2.4285922050f + s * +0.4505937099f,
          l * +0.0259040371f + m * +0.7827717662f + s * -0.8086757660f,
          alpha,
        },
      };
    }
  }

  return color;
}

/* Convert an OKLab color (alpha in items[3]) into the requested type. */
static KL_Color from_oklab(KL_Color color, KL_ColorType wanted_type) {
  float L = color.values.items[0];
  float a = color.values.items[1];
  float b = color.values.items[2];
  float alpha = color.values.items[3];

  switch (wanted_type) {
    case KL_COLOR_OKLABF32:
      return color;

    case KL_COLOR_OKLCHF32: {
      float C = sqrtf(a * a + b * b);
      float h = atan2f(b, a);
      return (KL_Color){
        .type = KL_COLOR_OKLCHF32,
        .values.items = { L, C, h, alpha },
      };
    }

    case KL_COLOR_RGBF32:
    case KL_COLOR_RGBAF32: {
      float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
      float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
      float s_ = L - 0.0894841775f * a - 1.2914855480f * b;

      float l = l_ * l_ * l_;
      float m = m_ * m_ * m_;
      float s = s_ * s_ * s_;

      float r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
      float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
      float bl = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

      return (KL_Color){
        .type = wanted_type,
        .values.items = {
          kl_clamp_f32(linear_to_gamma(r), 0.0f, 1.0f),
          kl_clamp_f32(linear_to_gamma(g), 0.0f, 1.0f),
          kl_clamp_f32(linear_to_gamma(bl), 0.0f, 1.0f),
          kl_clamp_f32(alpha, 0.0f, 1.0f),
        },
      };
    }
  }

  return color;
}

KL_Color kl_color_translate(KL_Color color, KL_ColorType wanted_type) {
  if (color.type == wanted_type) return color;
  return from_oklab(to_oklab(color), wanted_type);
}
