#include <kl/bitmap/bitmap.h>
#include <kl/macros.h>
#include <kl/common/number.h>
#include <kl/threads/util.h>
#include <kl/math/macros.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


KL_IMPL_LIST(bitmap, KL_Bitmap);

void kl_bitmap_alloc(KL_Bitmap* bm, KL_BitmapConfig cfg) {
  if (bm->initialized) return;

  assert(cfg.width > 0);
  assert(cfg.height > 0);

  int64_t size = cfg.width * cfg.height * 4;

  if (cfg.allocator != 0) {
    bm->data = (uint8_t*)kl_allocator_malloc_tracked(cfg.allocator, size, &bm->allocation);
  } else {
    bm->data = (uint8_t*)calloc(size, sizeof(uint8_t));
  }

  assert(bm->data != 0);

  bm->width = cfg.width;
  bm->height = cfg.height;
  bm->stride = cfg.width;
  bm->is_view = false;
  bm->allocator = cfg.allocator;
  bm->initialized = true;
}

void kl_bitmap_destroy(KL_Bitmap* bm) {
  assert(bm->initialized);
  assert(bm->data != 0);
  assert(!bm->is_view);
  
  if (bm->allocator != 0) {
    kl_allocator_free_allocation(bm->allocator, bm->allocation);
  } else {
    free(bm->data);
  }


  memset(bm, 0, sizeof(KL_Bitmap));
}

KL_Bitmap kl_bitmap_get_crop(KL_Bitmap* bm, int32_t x, int32_t y, int32_t width, int32_t height) {
  assert(bm->initialized);
  assert(bm->data != 0);


  x = KL_MIN(KL_MAX(x, 0), bm->width);
  y = KL_MIN(KL_MAX(y, 0), bm->height);

  KL_Bitmap cropped = *bm;
  cropped.is_view = true;
  cropped.stride = bm->stride; // rows are still as far apart as in the parent
  cropped.width = KL_MAX(KL_MIN(width, bm->width - x), 0);
  cropped.height = KL_MAX(KL_MIN(height, bm->height - y), 0);
  // data is bytes, index is pixels -> 4 bytes per pixel
  cropped.data = &bm->data[(int64_t)(y * bm->stride + x) * 4];

  return cropped;
}

int64_t kl_bitmap_index_at(KL_Bitmap* bm, int32_t x, int32_t y) {
  assert(bm->initialized);
  assert(bm->data != 0);

  x = KL_MIN(x, bm->width-1);
  y = KL_MIN(y, bm->height-1);
  x = KL_MAX(x, 0);
  y = KL_MAX(y, 0);

  int64_t idx = (int64_t)y * bm->stride + x;
  assert(idx >= 0);
  assert(idx < ((int64_t)bm->stride * bm->height));

  return idx;
}

typedef union {
  uint8_t raw[4];
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  };
} KL_BitmapColor;

static inline KL_BitmapColor* get_color_ptr(KL_Bitmap* bm, int32_t x, int32_t y) {
  KL_BitmapColor* data = (KL_BitmapColor*)(bm->data);
  return &data[kl_bitmap_index_at(bm, x, y)];
}

void kl_bitmap_read_raw(KL_Bitmap* bm, int32_t x, int32_t y, uint8_t out[4]) {
  assert(bm->initialized);
  assert(bm->data != 0);

  KL_BitmapColor* data = get_color_ptr(bm, x, y);
  memcpy(out, data->raw, 4 * sizeof(uint8_t));
}

void kl_bitmap_write_raw(KL_Bitmap* bm, int32_t x, int32_t y, uint8_t pixel[4])  {
  assert(bm->initialized);
  assert(bm->data != 0);

  KL_BitmapColor* data = get_color_ptr(bm, x, y);
  memcpy(data->raw, pixel, 4 * sizeof(uint8_t));
}

void kl_bitmap_fill_raw(KL_Bitmap* bm, uint8_t pixel[4]) {
  assert(bm->initialized);
  assert(bm->data != 0);

  for (int32_t y = 0; y < bm->height; y++) {
    for (int32_t x = 0; x < bm->width; x++) {
      kl_bitmap_write_raw(bm, x, y, pixel);
    }
  }
}

void kl_bitmap_read_f32(KL_Bitmap* bm, int32_t x, int32_t y, float out[4]) {
  assert(bm->initialized);
  assert(bm->data != 0);

  uint8_t raw[4] = {0};
  kl_bitmap_read_raw(bm, x, y, raw);

  for (int32_t i = 0; i < 4; i++) {
    out[i] = kl_clamp_f32((float)raw[i] / 255.0f, 0.0f, 1.0f);
  }
}

void kl_bitmap_write_f32(KL_Bitmap* bm, int32_t x, int32_t y, float pixel[4]) {
  assert(bm->initialized);
  assert(bm->data != 0);
  
  uint8_t raw[4] = {0};

  for (int32_t i = 0; i < 4; i++) {
    raw[i] = (uint8_t)(kl_clamp_f32(pixel[i] * 255.0f, 0.0f, 255.0f));
  }

  kl_bitmap_write_raw(bm, x, y, raw);
}


void kl_bitmap_fill_f32(KL_Bitmap* bm, float pixel[4]) {
  assert(bm->initialized);
  assert(bm->data != 0);

  uint8_t raw[4] = {0};

  for (int32_t i = 0; i < 4; i++) {
    raw[i] = (uint8_t)(kl_clamp_f32(pixel[i] * 255.0f, 0.0f, 255.0f));
  }

  kl_bitmap_fill_raw(bm, raw);
}

KL_Vec4 kl_bitmap_get(KL_Bitmap* bm, KL_Vec2 uv) {
  assert(bm->initialized);
  assert(bm->data != 0);

  int32_t x = (int32_t)(uv.x * (float)bm->width);
  int32_t y = (int32_t)(uv.y * (float)bm->height);

  KL_Vec4 out = {0};
  kl_bitmap_read_f32(bm, x, y, out.items);

  return out;
}


KL_Vec3 kl_bitmap_get_rgb(KL_Bitmap* bm, KL_Vec2 uv) {
  assert(bm->initialized);
  assert(bm->data != 0);
  KL_Vec4 v4 = kl_bitmap_get(bm, uv);
  return kl_vec3(v4.x, v4.y, v4.z);
}

KL_Vec4 kl_bitmap_set(KL_Bitmap* bm, KL_Vec2 uv, KL_Vec4 pixel) {
  assert(bm->initialized);
  assert(bm->data != 0);

  int32_t x = (int32_t)(uv.x * (float)bm->width);
  int32_t y = (int32_t)(uv.y * (float)bm->height);

  kl_bitmap_write_f32(bm, x, y, pixel.items);

  return kl_bitmap_get(bm, uv);
}

void kl_bitmap_save__ppm(KL_Bitmap* bm, FILE* fp) {
  assert(bm->initialized);
  assert(bm->data != 0);
  assert(fp != 0);


  fprintf(fp, "P3\n");
  fprintf(fp, "%d %d\n", bm->width, bm->height);
  fprintf(fp, "255\n");

  for (int32_t y = 0; y < bm->height; y++) {
    for (int32_t x = 0; x < bm->width; x++) {
      uint8_t raw[4] = {0};
      kl_bitmap_read_raw(bm, x, y, raw);
      fprintf(fp, "%d %d %d", raw[0], raw[1], raw[2]);
      if (x < (bm->width-1)) fprintf(fp, " ");
    }
    fprintf(fp, "\n");
  }
}


// ----------- multi-threading


void kl_bitmap_processor_init(KL_BitmapProcessor* proc) {
  if (proc->initialized) return;

  long avail_cores = kl_nprocs();
  assert(avail_cores > 0);

  kl_worker_pool_init(&proc->worker_pool, (KL_WorkerPoolConfig){
    .max_workers = KL_MAX(avail_cores-1, 1),
    .approx_payload_size = sizeof(KL_BitmapProcessContext),
  });

  proc->initialized = true;
}

void kl_bitmap_processor_destroy(KL_BitmapProcessor* proc) {
  assert(proc->initialized);

  kl_worker_pool_destroy(&proc->worker_pool);
}


static void* bitmap_process_fn(KL_Worker* worker) {
  assert(worker->payload.value != 0);
  assert(worker->payload.size == sizeof(KL_BitmapProcessContext));
  KL_BitmapProcessContext ctx = *(KL_BitmapProcessContext*)worker->payload.value;
  KL_Bitmap* bm = ctx.config.output;
  assert(bm != 0);
  assert(bm->width > 0);
  assert(bm->height > 0);
  assert(bm->data != 0);
  assert(ctx.config.fn != 0);

  for (int32_t y = 0; y < ctx.tile_h; y++) {
    for (int32_t x = 0; x < ctx.tile_w; x++) {

      int32_t ox = ctx.tile_x + x;
      int32_t oy = ctx.tile_y + y;
      
      ctx.fragcoord = (KL_IVec2){ .x = ox, .y = oy };
      // Sample at the pixel centre (GLSL convention). This also makes
      // kl_bitmap_set/get's truncation round-trip exactly for any width.
      ctx.uv = (KL_Vec2){
        .x = ((float)ox + 0.5f) / (float)bm->width,
        .y = ((float)oy + 0.5f) / (float)bm->height,
      };

      ctx.config.fn(&ctx);
    }
  }
  
  return 0;
}

void kl_bitmap_processor_run(KL_BitmapProcessor* proc, KL_BitmapProcessConfig cfg) {
  assert(proc->initialized);
  assert(cfg.fn != 0);
  assert(cfg.output != 0);
  assert(cfg.output->width > 0);
  assert(cfg.output->height > 0);
  assert(cfg.output->data != 0);

  int64_t max_workers = proc->worker_pool.config.max_workers;
  if (cfg.num_workers <= 0 || cfg.num_workers > max_workers) {
    cfg.num_workers = (int32_t)max_workers;
  }

  assert(cfg.num_workers > 0);

  // Drop workers from any previous run so their pool slots can be reused.
  kl_worker_pool_reset(&proc->worker_pool);

  // Split into an NxN grid of tiles; N*N must never exceed the pool size.
  int32_t tiles_per_axis = KL_MAX((int32_t)floorf(sqrtf((float)cfg.num_workers)), 1);
  int32_t crop_size_x = (int32_t)ceilf((float)cfg.output->width / (float)tiles_per_axis);
  int32_t crop_size_y = (int32_t)ceilf((float)cfg.output->height / (float)tiles_per_axis);
  assert(crop_size_x > 0);
  assert(crop_size_y > 0);


  for (int32_t y = 0; y < cfg.output->height; y += crop_size_y) {
    for (int32_t x = 0; x < cfg.output->width; x += crop_size_x) {
      KL_BitmapProcessContext ctx = {
        .output = cfg.output,
        .config = cfg,
        .tile_x = x,
        .tile_y = y,
        // clip edge tiles so no worker runs past the bitmap
        .tile_w = KL_MIN(crop_size_x, cfg.output->width - x),
        .tile_h = KL_MIN(crop_size_y, cfg.output->height - y),
      };
      kl_worker_pool__worker_register(&proc->worker_pool, (KL_WorkerConfig){
        .fn = bitmap_process_fn,
        .payload = KL_BLOB_ALIGNED_RAW_AUTO(ctx)
      });
    }
  }

  kl_worker_pool_dispatch_all_workers(&proc->worker_pool);
  kl_worker_pool_join(&proc->worker_pool);
}
