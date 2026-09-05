/**
* KL_Bitmap - an interface for manipulating or writing images / bitmaps.
* conventions:
* - the data is always allocated as 4 channels
**/
#ifndef KL_BITMAP_H
#define KL_BITMAP_H

#include <kl/memory/allocator.h>
#include <kl/threads/mutex.h>
#include <kl/threads/worker.h>
#include <kl/threads/pool.h>
#include <kl/templates/list/list.h>
#include <kl/math/vec2.h>
#include <kl/math/vec3.h>
#include <kl/math/vec4.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

typedef struct {
  int32_t width;
  int32_t height;
  KL_Allocator* allocator; // optional
} KL_BitmapConfig;

typedef struct {
  bool initialized;
  bool is_view; // it's a cropped view, and should not be free'd
  int32_t width;
  int32_t height;
  int32_t stride; // pixels per row in the backing buffer (== width unless is_view)
  uint8_t* data; // always 4 channels
  KL_Allocator* allocator; // optional
  KL_Allocation allocation; // if an allocator was used
} KL_Bitmap;

KL_DEFINE_LIST(bitmap, KL_Bitmap);

void kl_bitmap_alloc(KL_Bitmap* bm, KL_BitmapConfig cfg);

void kl_bitmap_destroy(KL_Bitmap* bm);

KL_Bitmap kl_bitmap_get_crop(KL_Bitmap* bm, int32_t x, int32_t y, int32_t width, int32_t height);

int64_t kl_bitmap_index_at(KL_Bitmap* bm, int32_t x, int32_t y);

void kl_bitmap_read_raw(KL_Bitmap* bm, int32_t x, int32_t y, uint8_t out[4]);

void kl_bitmap_write_raw(KL_Bitmap* bm, int32_t x, int32_t y, uint8_t pixel[4]);

void kl_bitmap_fill_raw(KL_Bitmap* bm, uint8_t pixel[4]);

void kl_bitmap_read_f32(KL_Bitmap* bm, int32_t x, int32_t y, float out[4]);

void kl_bitmap_write_f32(KL_Bitmap* bm, int32_t x, int32_t y, float pixel[4]);

void kl_bitmap_fill_f32(KL_Bitmap* bm, float pixel[4]);

KL_Vec4 kl_bitmap_get(KL_Bitmap* bm, KL_Vec2 uv);

KL_Vec3 kl_bitmap_get_rgb(KL_Bitmap* bm, KL_Vec2 uv);

KL_Vec4 kl_bitmap_set(KL_Bitmap* bm, KL_Vec2 uv, KL_Vec4 pixel);


/**
* Save as ppm file
**/
void kl_bitmap_save__ppm(KL_Bitmap* bm, FILE* fp);


// ----------- multi-threading
struct KL_BitmapProcessContext;

typedef void (*KL_BitmapProcessFunction)(struct KL_BitmapProcessContext* ctx);

typedef struct KL_BitmapProcessConfig {
  KL_Bitmap* bitmap;
  int32_t num_workers;
  KL_BitmapProcessFunction fn;
  void* user_ptr; // passed through untouched to fn (per-run parameters)
  struct {
    KL_bitmap_List inputs; // read-only inputs
    KL_bitmap_List outputs; // write-only outputs
  } textures;
} KL_BitmapProcessConfig;

typedef struct KL_BitmapProcessContext {
  KL_Bitmap* self; // the bitmap we're curently processing (not cropped)
  KL_BitmapProcessConfig config;
  KL_Vec2 uv;
  KL_IVec2 fragcoord;  // pixel coordinate in the full bitmap (config.bitmap)

  int32_t tile_x;
  int32_t tile_y;
  int32_t tile_w;
  int32_t tile_h;
} KL_BitmapProcessContext;




typedef struct {
  bool initialized;
  KL_WorkerPool worker_pool;
} KL_BitmapProcessor;


void kl_bitmap_processor_init(KL_BitmapProcessor* proc);

void kl_bitmap_processor_destroy(KL_BitmapProcessor* proc);

void kl_bitmap_processor_run(KL_BitmapProcessor* proc, KL_BitmapProcessConfig cfg);

#endif
