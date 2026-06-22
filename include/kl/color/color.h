#ifndef KL_COLOR_H
#define KL_COLOR_H

typedef enum {
  KL_COLOR_OKLABF32,
  KL_COLOR_OKLCHF32,
  KL_COLOR_RGBF32,
  KL_COLOR_RGBAF32
} KL_ColorType;

typedef struct {
  KL_ColorType type;

  union {
    float items[4];
    struct { float x; float y; float z; float w; };
    struct { float r; float g; float b; float a; };
  } values;
} KL_Color;


KL_Color kl_color_translate(KL_Color color, KL_ColorType wanted_type);

#endif
