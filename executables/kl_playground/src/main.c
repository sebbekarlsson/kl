#define STB_IMAGE_IMPLEMENTATION
#include <kl_playground/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <kl_playground/stb_image_write.h>

#include <kl/cli/program.h>
#include <kl/string/string.h>
#include <kl/hash/hash.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <kl/tui/table.h>
#include <kl/bitmap/bitmap.h>
#include <kl/math/macros.h>
#include <kl/math/vec3.h>
#include <kl/math/vec4.h>
#include <kl/math/vecn.h>
#include <kl/common/number.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>



static void kl_bitmap_load_image(KL_Bitmap* bm, KL_StrConst path) {
  int w = 0;
  int h = 0;
  int comp = 0;
  uint8_t* data = stbi_load(path.value, &w, &h, &comp, 0);
  assert(data != 0);
  assert(w > 0);
  assert(h > 0);
  bm->width = w;
  bm->height = h;
  bm->stride = bm->width;
  bm->data = data;
  bm->initialized = true;
}

static void kl_bitmap_save_to_file(KL_Bitmap* bm, KL_StrConst path) {
  assert(bm->initialized);

  if (kl_str_const_ends_with(path, KL_STR_CONST_LIT(".png"))) {
    stbi_write_png(path.value, bm->width, bm->height, 4, bm->data, 0);
  } else if (kl_str_const_ends_with(path, KL_STR_CONST_LIT(".jpg"))) {
    stbi_write_jpg(path.value, bm->width, bm->height, 4, bm->data, 100);
  } else if (kl_str_const_ends_with(path, KL_STR_CONST_LIT(".bmp"))) {
    stbi_write_bmp(path.value, bm->width, bm->height, 4, bm->data);
  } else {
    stbi_write_png(path.value, bm->width, bm->height, 4, bm->data, 0);
  }
}



// ---------------------------------------------------------------------------
// Ray-marched sphere on a ground plane.
// ---------------------------------------------------------------------------

#define RM_MAX_STEPS 128
#define RM_MAX_DIST 40.0f
#define RM_SURF_EPS 0.001f

#define RM_SPHERE_CENTER kl_vec3(0.0f, 1.0f, 0.0f)
#define RM_SPHERE_RADIUS 1.0f

static inline float rm_sdf_sphere(KL_Vec3 p, KL_Vec3 c, float r) {
  return kl_vec3_mag(kl_vec3_sub(p, c)) - r;
}

static inline float rm_sdf_plane(KL_Vec3 p, float height) {
  return p.y - height;
}

// Signed distance to the whole scene. Also reports which object is closest.
static inline float rm_scene(KL_Vec3 p, int* material) {
  float d_sphere = rm_sdf_sphere(p, RM_SPHERE_CENTER, RM_SPHERE_RADIUS);
  float d_plane = rm_sdf_plane(p, 0.0f);

  if (d_sphere < d_plane) {
    if (material) *material = 1;
    return d_sphere;
  }
  if (material) *material = 2;
  return d_plane;
}

static inline KL_Vec3 rm_normal(KL_Vec3 p) {
  const float e = 0.0005f;
  float dx = rm_scene(kl_vec3(p.x + e, p.y, p.z), 0) - rm_scene(kl_vec3(p.x - e, p.y, p.z), 0);
  float dy = rm_scene(kl_vec3(p.x, p.y + e, p.z), 0) - rm_scene(kl_vec3(p.x, p.y - e, p.z), 0);
  float dz = rm_scene(kl_vec3(p.x, p.y, p.z + e), 0) - rm_scene(kl_vec3(p.x, p.y, p.z - e), 0);
  return kl_vec3_unit(kl_vec3(dx, dy, dz));
}

// Sphere-trace along ro + rd*t. Returns t, or a negative value on miss.
static inline float rm_march(KL_Vec3 ro, KL_Vec3 rd, int* material) {
  float t = 0.0f;
  for (int i = 0; i < RM_MAX_STEPS; i++) {
    KL_Vec3 p = kl_vec3_add(ro, kl_vec3_scale(rd, t));
    float d = rm_scene(p, material);
    if (d < RM_SURF_EPS) return t;
    t += d;
    if (t > RM_MAX_DIST) break;
  }
  return -1.0f;
}

// Soft shadow: march toward the light and track how close we came to geometry.
static inline float rm_soft_shadow(KL_Vec3 ro, KL_Vec3 rd, float k) {
  float res = 1.0f;
  float t = 0.02f;
  for (int i = 0; i < 48 && t < 12.0f; i++) {
    float d = rm_scene(kl_vec3_add(ro, kl_vec3_scale(rd, t)), 0);
    res = fminf(res, k * d / t);
    if (res < 0.001f) break;
    t += fmaxf(d, 0.01f);
  }
  return fmaxf(res, 0.0f);
}

static inline KL_Vec3 rm_sky(KL_Vec3 rd) {
  float t = kl_clamp_f32(rd.y * 0.5f + 0.5f, 0.0f, 1.0f);
  return kl_vec3_lerp(kl_vec3(0.85f, 0.90f, 1.00f), kl_vec3(0.25f, 0.45f, 0.90f), t);
}

static void my_shader(KL_BitmapProcessContext* ctx) {
  KL_Bitmap* bm = ctx->output;
  float aspect = (float)bm->width / (float)bm->height;

  // uv has y pointing down (row 0 is the top). Flip to a y-up screen space in [-1, 1].
  float sx = (ctx->uv.x * 2.0f - 1.0f) * aspect;
  float sy = 1.0f - ctx->uv.y * 2.0f;

  // Camera looking from slightly above toward the sphere.
  KL_Vec3 ro = kl_vec3(0.0f, 1.6f, 4.5f);
  KL_Vec3 target = kl_vec3(0.0f, 0.8f, 0.0f);
  KL_Vec3 fwd = kl_vec3_unit(kl_vec3_sub(target, ro));
  KL_Vec3 right = kl_vec3_unit(kl_vec3(-fwd.z, 0.0f, fwd.x)); // cross(fwd, world_up)
  KL_Vec3 up = kl_vec3(
    right.y * fwd.z - right.z * fwd.y,
    right.z * fwd.x - right.x * fwd.z,
    right.x * fwd.y - right.y * fwd.x
  );
  const float focal = 1.8f;
  KL_Vec3 rd = kl_vec3_unit(kl_vec3_add(kl_vec3_add(kl_vec3_scale(right, sx), kl_vec3_scale(up, sy)), kl_vec3_scale(fwd, focal)));

  KL_Vec3 light_dir = kl_vec3_unit(kl_vec3(-0.6f, 0.9f, 0.5f));
  KL_Vec3 color = rm_sky(rd);

  int material = 0;
  float t = rm_march(ro, rd, &material);

  if (t >= 0.0f) {
    KL_Vec3 p = kl_vec3_add(ro, kl_vec3_scale(rd, t));
    KL_Vec3 n = rm_normal(p);

    KL_Vec3 albedo;
    float shininess;
    if (material == 1) {
      albedo = kl_vec3(0.90f, 0.30f, 0.25f);
      shininess = 48.0f;
    } else {
      // checkerboard floor
      int cx = (int)floorf(p.x);
      int cz = (int)floorf(p.z);
      bool odd = ((cx + cz) & 1) != 0;
      albedo = odd ? kl_vec3(0.75f, 0.75f, 0.75f) : kl_vec3(0.30f, 0.30f, 0.32f);
      shininess = 12.0f;
    }

    float diffuse = fmaxf(kl_vec3_dot(n, light_dir), 0.0f);
    float shadow = rm_soft_shadow(kl_vec3_add(p, kl_vec3_scale(n, 0.01f)), light_dir, 16.0f);

    // Blinn-Phong specular
    KL_Vec3 view_dir = kl_vec3_scale(rd, -1.0f);
    KL_Vec3 half_dir = kl_vec3_unit(kl_vec3_add(light_dir, view_dir));
    float spec = powf(fmaxf(kl_vec3_dot(n, half_dir), 0.0f), shininess) * (material == 1 ? 0.6f : 0.15f);

    // Cheap ambient occlusion-ish term from the normal's upness and a sky tint.
    KL_Vec3 ambient = kl_vec3_scale(rm_sky(n), 0.25f);

    KL_Vec3 lit = kl_vec3_scale(albedo, diffuse * shadow);
    lit = kl_vec3_add(lit, kl_vec3_mul(albedo, ambient));
    lit = kl_vec3_add(lit, kl_vec3_scale(kl_vec3(1.0f, 0.95f, 0.9f), spec * shadow));

    // Fog toward the sky color with distance so the floor fades out.
    float fog = 1.0f - expf(-0.02f * t * t);
    color = kl_vec3_lerp(lit, rm_sky(rd), fog);
  }

  // Gamma correction
  color = kl_vec3(powf(color.x, 1.0f / 2.2f), powf(color.y, 1.0f / 2.2f), powf(color.z, 1.0f / 2.2f));

  kl_bitmap_set(bm, ctx->uv, kl_vec4(color.x, color.y, color.z, 1.0f));
}

static int cmd_bitmap_fn(KL_CLICommand *cmd) {
  (void) cmd;

  KL_Bitmap bm = {0};
  kl_bitmap_alloc(&bm, (KL_BitmapConfig){ .width = 1024, .height = 768 });


  KL_BitmapProcessor proc = {0};
  kl_bitmap_processor_init(&proc);

  kl_bitmap_processor_run(&proc, (KL_BitmapProcessConfig){
    .output = &bm,
    .fn = my_shader,
    .num_workers = 16
  });

  kl_bitmap_processor_destroy(&proc);


  const char* outpath = "bitmap.ppm";

  FILE* fp = fopen(outpath, "w+");
  kl_bitmap_save__ppm(&bm, fp);
  fclose(fp);
  fp = 0;
  printf("wrote to: %s\n", outpath);
  
  kl_bitmap_destroy(&bm);

  return 0;
}

//static int cmd_bitmap_fn2(KL_CLICommand *cmd) {
//  (void) cmd;
//
//  KL_Bitmap bm = {0};
//  kl_bitmap_alloc(&bm, (KL_BitmapConfig){ .width = 1024, .height = 768 });
//
//
//  kl_bitmap_draw_text(&bm, (KL_IVec2){ .x = bm.width / 2, .y = bm.height / 2 }, (KL_IVec2){ .x = 64, .y = 64 }, KL_STR_CONST_LIT("hello"), (KL_Vec4){ .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f });
//
//
//  const char* outpath = "bitmap.ppm";
//
//  FILE* fp = fopen(outpath, "w+");
//  kl_bitmap_save__ppm(&bm, fp);
//  fclose(fp);
//  fp = 0;
//  printf("wrote to: %s\n", outpath);
//  
//  kl_bitmap_destroy(&bm);
//
//  return 0;
//}


// ------ PBR playground


static inline float luma3(KL_Vec3 c) {
  return kl_vec3_dot(c, kl_vec3(0.299f, 0.587f, 0.114f));
}

static inline float luma4(KL_Vec4 c) {
  return luma3(kl_vec3(c.x, c.y, c.z));
}

static void shade__lowpass(KL_BitmapProcessContext* ctx) {
  KL_Bitmap* input = &ctx->config.input.bitmaps.items[0];
  KL_Vec4 color = kl_bitmap_get(input, ctx->uv);

  float w = fminf(color.w, kl_smoothstep_f32(0.5f, 0.0f, luma4(color)));

  kl_bitmap_set(ctx->output, ctx->uv, kl_vec4(color.x, color.y, color.z, w));
}

// ---------------------------------------------------------------------------
// PBR approximation from a single impact texture.
//
// Everything is derived from one scalar field: the *depth* of the damage.
// shade__lowpass already computes that in its alpha channel (dark => deep),
// so height = 1 - depth. Normals are the gradient of that height, AO is how
// much the surrounding height rises above a texel, and roughness comes from
// local high-frequency detail plus depth.
//
// textures.inputs[0] = original image, textures.inputs[1] = lowpass output.
// ---------------------------------------------------------------------------

#define PBR_NORMAL_STRENGTH  6.0f   // gradient scale before normalisation
#define PBR_NORMAL_STEP      2.0f   // Sobel tap spacing in texels; widens the crater lip
#define PBR_AO_DIRS          8
#define PBR_AO_RADII         5      // rings at 2,4,8,16,32 texels
#define PBR_AO_HEIGHT_SCALE  24.0f  // how tall a height of 1.0 is, in texels
#define PBR_AO_DEPTH_WEIGHT  0.55f  // extra darkening just for being deep
#define PBR_ROUGH_BASE       0.55f  // intact plaster / concrete
#define PBR_ROUGH_DETAIL     0.35f
#define PBR_ROUGH_DEPTH      0.25f
#define PBR_DETAIL_GAIN      6.0f   // luma std-dev -> [0,1]
#define PBR_METALLIC_MAX     0.15f

static inline KL_Vec2 pbr_texel(KL_Bitmap* bm) {
  return kl_vec2(1.0f / (float)bm->width, 1.0f / (float)bm->height);
}

static inline KL_Vec2 pbr_offset(KL_Vec2 uv, KL_Vec2 texel, float dx, float dy) {
  return kl_vec2_add(uv, kl_vec2(texel.x * dx, texel.y * dy));
}

// 0 = intact surface, 1 = bottom of the hole.
static inline float pbr_depth(KL_Bitmap* lowpass, KL_Vec2 uv) {
  // Outside the image the wall is intact.
  if (uv.x < 0.0f || uv.y < 0.0f || uv.x >= 1.0f || uv.y >= 1.0f) return 0.0f;
  KL_Vec4 c = kl_bitmap_get(lowpass, uv);
  // Alpha is the darkness mask. Keep a little of the raw luma so the inside
  // of the hole isn't a perfectly flat floor.
  float interior = 1.0f - luma4(c);
  return c.w * (0.75f + 0.25f * interior);
}

static inline float pbr_height(KL_Bitmap* lowpass, KL_Vec2 uv) {
  return 1.0f - pbr_depth(lowpass, uv);
}

// 5x5 box-filtered height. The crater edge is noisy at texel scale and
// normals from raw pixel differences turn into sparkle; the blur also widens
// the lip so the slope spans a few texels instead of one.
static inline float pbr_height_smooth(KL_Bitmap* lowpass, KL_Vec2 uv, KL_Vec2 texel) {
  float sum = 0.0f;
  for (int y = -2; y <= 2; y++) {
    for (int x = -2; x <= 2; x++) {
      sum += pbr_height(lowpass, pbr_offset(uv, texel, (float)x, (float)y));
    }
  }
  return sum / 25.0f;
}

/**
* Local high-frequency detail: alpha-weighted standard deviation of luma over
* a 5x5 window, normalised to [0,1]. Unlike the colour cosine distance this is
* well defined on black texels, and weighting by alpha stops the source
* image's transparent border from registering as a hard edge.
**/
static inline float approx_detail(KL_Bitmap* source, KL_Vec2 uv) {
  KL_Vec2 t = pbr_texel(source);
  float wsum = 0.0f;
  float sum = 0.0f;
  float sum2 = 0.0f;
  for (int y = -2; y <= 2; y++) {
    for (int x = -2; x <= 2; x++) {
      KL_Vec4 c = kl_bitmap_get(source, pbr_offset(uv, t, (float)x, (float)y));
      float l = luma4(c);
      wsum += c.w;
      sum += c.w * l;
      sum2 += c.w * l * l;
    }
  }
  if (wsum < 1e-4f) return 0.0f;
  float mean = sum / wsum;
  float var = fmaxf(sum2 / wsum - mean * mean, 0.0f);
  return kl_clamp_f32(sqrtf(var) * PBR_DETAIL_GAIN, 0.0f, 1.0f);
}

/**
* Tangent-space normal: Sobel gradient of the smoothed height field.
* OpenGL convention (+X right, +Y up). Image rows run downward, so the
* y derivative keeps its sign while x is negated: n = (-dh/dx, -dh/dy_up, 1).
* Returned in [-1,1]; the shader remaps to [0,1] for storage.
**/
static inline KL_Vec3 approx_normal(KL_Bitmap* lowpass, KL_Vec2 uv) {
  KL_Vec2 t = pbr_texel(lowpass);

  float h[3][3]; // [row = y down][col = x right]
  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      h[y + 1][x + 1] = pbr_height_smooth(lowpass, pbr_offset(uv, t, (float)x * PBR_NORMAL_STEP, (float)y * PBR_NORMAL_STEP), t);
    }
  }

  float dx = ((h[0][2] + 2.0f * h[1][2] + h[2][2]) - (h[0][0] + 2.0f * h[1][0] + h[2][0])) / (8.0f * PBR_NORMAL_STEP);
  float dy = ((h[2][0] + 2.0f * h[2][1] + h[2][2]) - (h[0][0] + 2.0f * h[0][1] + h[0][2])) / (8.0f * PBR_NORMAL_STEP);

  return kl_vec3_unit(kl_vec3(-dx * PBR_NORMAL_STRENGTH, dy * PBR_NORMAL_STRENGTH, 1.0f));
}

/**
* Decal coverage shared by the normal and ARM maps: the darkness mask, widened
* to include the crater rim wherever the normal tilts. The albedo stays
* transparent there, so a renderer keeps the wall colour but bends its normal,
* which reads as the dent around the hole.
**/
static inline float pbr_mask(KL_Bitmap* lowpass, KL_Vec2 uv, KL_Vec3 n) {
  float depth_mask = kl_bitmap_get(lowpass, uv).w;
  float tilt = kl_clamp_f32((1.0f - n.z) * 2.0f, 0.0f, 1.0f);
  return fmaxf(depth_mask, tilt);
}

static void shade__normal(KL_BitmapProcessContext* ctx) {
  KL_Bitmap* lowpass = &ctx->config.input.bitmaps.items[1];

  KL_Vec3 n = approx_normal(lowpass, ctx->uv);
  float mask = pbr_mask(lowpass, ctx->uv, n);

  kl_bitmap_set(ctx->output, ctx->uv, kl_vec4(n.x * 0.5f + 0.5f, n.y * 0.5f + 0.5f, n.z * 0.5f + 0.5f, mask));
}

/**
* Height-based ambient occlusion. For each direction we look outward along
* rings of increasing radius and keep the steepest horizon (how much the
* surroundings rise above this texel, rise over run). A texel walled in on
* all sides is occluded; one on an open rim is not. Depth is also penalised
* directly because the rings can't span the whole crater from its centre.
**/
static inline float approx_ao(KL_Bitmap* lowpass, KL_Vec2 uv) {
  KL_Vec2 t = pbr_texel(lowpass);
  float hc = pbr_height(lowpass, uv);
  float depth = 1.0f - hc;

  float occlusion = 0.0f;
  for (int i = 0; i < PBR_AO_DIRS; i++) {
    float ang = ((float)i / (float)PBR_AO_DIRS) * (float)M_PI * 2.0f;
    KL_Vec2 dir = kl_vec2(cosf(ang), sinf(ang));

    float horizon = 0.0f;
    float r = 2.0f;
    for (int k = 0; k < PBR_AO_RADII; k++, r *= 2.0f) {
      float hs = pbr_height(lowpass, pbr_offset(uv, t, dir.x * r, dir.y * r));
      float slope = (hs - hc) * PBR_AO_HEIGHT_SCALE / r;
      horizon = fmaxf(horizon, slope);
    }
    occlusion += horizon / (1.0f + horizon); // steep walls -> 1
  }
  occlusion /= (float)PBR_AO_DIRS;

  float ao = (1.0f - occlusion) * (1.0f - PBR_AO_DEPTH_WEIGHT * depth);
  return kl_clamp_f32(ao, 0.0f, 1.0f);
}

/**
* Roughness: intact masonry is already fairly rough, crumbly detail makes it
* rougher, and the pulverised material inside the hole is roughest of all.
**/
static inline float approx_roughness(KL_Bitmap* source, KL_Bitmap* lowpass, KL_Vec2 uv) {
  float detail = approx_detail(source, uv);
  float depth = pbr_depth(lowpass, uv);
  return kl_clamp_f32(PBR_ROUGH_BASE + PBR_ROUGH_DETAIL * detail + PBR_ROUGH_DEPTH * depth, 0.0f, 1.0f);
}

/**
* Metallic: metals are grey *and* smooth *and* not buried in dust. Note the
* old "grayish" was really chroma (large = colourful) so it rewarded the
* opposite of grey. Even fixed, colour alone cannot separate bare metal from
* grey concrete, so the result is capped at PBR_METALLIC_MAX: masonry impacts
* should sit near zero, and a PBR renderer makes even 0.3 look wrong.
**/
static inline float approx_metallic(KL_Bitmap* source, KL_Bitmap* lowpass, KL_Vec2 uv) {
  KL_Vec3 c = kl_bitmap_get_rgb(source, uv);
  float chroma = (fabsf(c.x - c.y) + fabsf(c.x - c.z) + fabsf(c.y - c.z)) / 3.0f;
  float grey = 1.0f - kl_clamp_f32(chroma * 8.0f, 0.0f, 1.0f);
  float smooth = 1.0f - approx_detail(source, uv);
  float exposed = 1.0f - pbr_depth(lowpass, uv);
  return PBR_METALLIC_MAX * grey * smooth * exposed;
}

// arm = (ambient occlusion, roughness, metallic)
static void shade__arm(KL_BitmapProcessContext* ctx) {
  KL_Bitmap* source = &ctx->config.input.bitmaps.items[0];
  KL_Bitmap* lowpass = &ctx->config.input.bitmaps.items[1];

  float a = approx_ao(lowpass, ctx->uv);
  float r = approx_roughness(source, lowpass, ctx->uv);
  float m = approx_metallic(source, lowpass, ctx->uv);
  float mask = pbr_mask(lowpass, ctx->uv, approx_normal(lowpass, ctx->uv));

  kl_bitmap_set(ctx->output, ctx->uv, kl_vec4(a, r, m, mask));
}

// ---------------------------------------------------------------------------
// Preview: composite the generated decal over a flat wall and light it.
// textures.inputs = { albedo, normal, arm }, user_ptr = KL_PBRPreviewParams.
// ---------------------------------------------------------------------------

typedef struct {
  KL_Vec3 light_dir;   // direction *towards* the light, tangent space (+Y up, +Z out of the wall)
  KL_Vec3 light_color;
  KL_Vec3 wall_albedo;
  float wall_roughness;
  float ambient;
} KL_PBRPreviewParams;

static inline float pbr_srgb_encode(float v) {
  return powf(kl_clamp_f32(v, 0.0f, 1.0f), 1.0f / 2.2f);
}

static void shade__preview(KL_BitmapProcessContext* ctx) {
  const KL_PBRPreviewParams* prm = (const KL_PBRPreviewParams*)ctx->config.user_ptr;
  assert(prm != 0);
  KL_Bitmap* albedo_map = &ctx->config.input.bitmaps.items[0];
  KL_Bitmap* normal_map = &ctx->config.input.bitmaps.items[1];
  KL_Bitmap* arm_map = &ctx->config.input.bitmaps.items[2];

  KL_Vec4 alb = kl_bitmap_get(albedo_map, ctx->uv);
  KL_Vec4 nrm = kl_bitmap_get(normal_map, ctx->uv);
  KL_Vec4 arm = kl_bitmap_get(arm_map, ctx->uv);

  // Decal blend: each map fades to the bare wall by its own alpha.
  KL_Vec3 base = kl_vec3_lerp(prm->wall_albedo, kl_vec3(alb.x, alb.y, alb.z), alb.w);
  KL_Vec3 n_decoded = kl_vec3(nrm.x * 2.0f - 1.0f, nrm.y * 2.0f - 1.0f, nrm.z * 2.0f - 1.0f);
  KL_Vec3 n = kl_vec3_unit(kl_vec3_lerp(kl_vec3(0.0f, 0.0f, 1.0f), n_decoded, nrm.w));
  float ao = kl_clamp_f32(1.0f - (1.0f - arm.x) * arm.w, 0.0f, 1.0f);
  float roughness = kl_clamp_f32(prm->wall_roughness + (arm.y - prm->wall_roughness) * arm.w, 0.04f, 1.0f);
  float metallic = kl_clamp_f32(arm.z * arm.w, 0.0f, 1.0f);

  KL_Vec3 l = kl_vec3_unit(prm->light_dir);
  KL_Vec3 v = kl_vec3(0.0f, 0.0f, 1.0f); // camera looks straight at the wall
  KL_Vec3 h = kl_vec3_unit(kl_vec3_add(l, v));

  float n_dot_l = fmaxf(kl_vec3_dot(n, l), 0.0f);
  float n_dot_h = fmaxf(kl_vec3_dot(n, h), 0.0f);

  // Metals have no diffuse and a tinted specular; dielectrics ~4% white specular.
  KL_Vec3 diffuse_color = kl_vec3_scale(base, 1.0f - metallic);
  KL_Vec3 f0 = kl_vec3_lerp(kl_vec3(0.04f, 0.04f, 0.04f), base, metallic);

  // Blinn-Phong with roughness driving the lobe width and its normalisation.
  float shininess = 2.0f / (roughness * roughness) - 2.0f;
  float spec = powf(n_dot_h, shininess) * (shininess + 8.0f) / (8.0f * (float)M_PI);

  KL_Vec3 lit = kl_vec3_scale(diffuse_color, n_dot_l);
  lit = kl_vec3_add(lit, kl_vec3_scale(f0, spec * n_dot_l));
  lit = kl_vec3_mul(lit, prm->light_color);
  lit = kl_vec3_add(lit, kl_vec3_scale(diffuse_color, prm->ambient));
  lit = kl_vec3_scale(lit, ao);

  kl_bitmap_set(ctx->output, ctx->uv, kl_vec4(pbr_srgb_encode(lit.x), pbr_srgb_encode(lit.y), pbr_srgb_encode(lit.z), 1.0f));
}

static int cmd_bitmap_pipeline_fn(KL_CLICommand *cmd) {

  KL_CLIArgParsed input_path = kl_cli_command_get_positional(cmd, 0);
  KL_Bitmap input_img = {0};
  kl_bitmap_load_image(&input_img, input_path.as.string);
  KL_StrConst out_path = kl_cli_command_get_string(cmd, KL_STR_CONST_LIT("out"), KL_STR_CONST_LIT("out"));


  KL_Bitmap out_lowpass = {0};
  kl_bitmap_alloc(&out_lowpass, (KL_BitmapConfig){ .width = input_img.width, .height = input_img.height });

  KL_Bitmap out_normal = {0};
  kl_bitmap_alloc(&out_normal, (KL_BitmapConfig){ .width = input_img.width, .height = input_img.height });

  KL_Bitmap out_arm = {0};
  kl_bitmap_alloc(&out_arm, (KL_BitmapConfig){ .width = input_img.width, .height = input_img.height });

  KL_Bitmap out_preview = {0};
  kl_bitmap_alloc(&out_preview, (KL_BitmapConfig){ .width = input_img.width, .height = input_img.height });

  KL_PBRPreviewParams preview_params = {
    .light_dir = kl_vec3(
      (float)kl_cli_command_get_float(cmd, KL_STR_CONST_LIT("light-x"), -1.0),
      (float)kl_cli_command_get_float(cmd, KL_STR_CONST_LIT("light-y"), 1.0),
      (float)kl_cli_command_get_float(cmd, KL_STR_CONST_LIT("light-z"), 0.8)
    ),
    .light_color = kl_vec3(1.0f, 0.97f, 0.92f),
    .wall_albedo = kl_vec3(0.62f, 0.60f, 0.56f),
    .wall_roughness = 0.6f,
    .ambient = 0.22f,
  };
  

  KL_BitmapProcessor proc = {0};
  kl_bitmap_processor_init(&proc);

  KL_BitmapProcessConfig cfg = {0};
  kl_bitmap_list_init(&cfg.input.bitmaps, (KL_ListConfig){ .capacity = 4 });

  {
    kl_bitmap_list_reset_fast(&cfg.input.bitmaps);
    kl_bitmap_list_push(&cfg.input.bitmaps, input_img);
    
    cfg.output = &out_lowpass;
    cfg.fn = shade__lowpass;
    kl_bitmap_processor_run(&proc, cfg);
    
    KL_StrBuffer buff = KL_STR_BUFFER_STACK_ALLOC(PATH_MAX);
    kl_str_buffer_append_fmt(&buff, "%s_albedo.png", out_path.value);
    kl_bitmap_save_to_file(&out_lowpass, KL_STR_BUFFER_TO_STR_CONST(buff));
    printf("wrote to: %s\n", buff.data);
  }

  {
    kl_bitmap_list_reset_fast(&cfg.input.bitmaps);
    kl_bitmap_list_push(&cfg.input.bitmaps, input_img);
    kl_bitmap_list_push(&cfg.input.bitmaps, out_lowpass);
    
    cfg.output = &out_normal;
    cfg.fn = shade__normal;
    kl_bitmap_processor_run(&proc, cfg);
    
    KL_StrBuffer buff = KL_STR_BUFFER_STACK_ALLOC(PATH_MAX);
    kl_str_buffer_append_fmt(&buff, "%s_normal.png", out_path.value);
    kl_bitmap_save_to_file(&out_normal, KL_STR_BUFFER_TO_STR_CONST(buff));
    printf("wrote to: %s\n", buff.data);
  }

  {
    kl_bitmap_list_reset_fast(&cfg.input.bitmaps);
    kl_bitmap_list_push(&cfg.input.bitmaps, input_img);
    kl_bitmap_list_push(&cfg.input.bitmaps, out_lowpass);
    
    cfg.output = &out_arm;
    cfg.fn = shade__arm;
    kl_bitmap_processor_run(&proc, cfg);
    
    KL_StrBuffer buff = KL_STR_BUFFER_STACK_ALLOC(PATH_MAX);
    kl_str_buffer_append_fmt(&buff, "%s_arm.png", out_path.value);
    kl_bitmap_save_to_file(&out_arm, KL_STR_BUFFER_TO_STR_CONST(buff));
    printf("wrote to: %s\n", buff.data);
  }

  {
    kl_bitmap_list_reset_fast(&cfg.input.bitmaps);
    kl_bitmap_list_push(&cfg.input.bitmaps, out_lowpass);
    kl_bitmap_list_push(&cfg.input.bitmaps, out_normal);
    kl_bitmap_list_push(&cfg.input.bitmaps, out_arm);

    cfg.output = &out_preview;
    cfg.fn = shade__preview;
    cfg.user_ptr = &preview_params;
    kl_bitmap_processor_run(&proc, cfg);
    cfg.user_ptr = 0;

    KL_StrBuffer buff = KL_STR_BUFFER_STACK_ALLOC(PATH_MAX);
    kl_str_buffer_append_fmt(&buff, "%s_preview.png", out_path.value);
    kl_bitmap_save_to_file(&out_preview, KL_STR_BUFFER_TO_STR_CONST(buff));
    printf("wrote to: %s\n", buff.data);
  }

  kl_bitmap_list_destroy(&cfg.input.bitmaps);
  kl_bitmap_destroy(&input_img);
  kl_bitmap_destroy(&out_lowpass);
  kl_bitmap_destroy(&out_normal);
  kl_bitmap_destroy(&out_arm);
  kl_bitmap_destroy(&out_preview);
  kl_bitmap_processor_destroy(&proc);


  // const char* outpath = "bitmap.ppm";

  // FILE* fp = fopen(outpath, "w+");
  // kl_bitmap_save__ppm(&bm, fp);
  // fclose(fp);
  // fp = 0;
  // printf("wrote to: %s\n", outpath);
  // 
  // kl_bitmap_destroy(&bm);

  return 0;
}

int main(int argc, char *argv[]) {

  KL_CLIProgram program = {0};
  kl_cli_program_init(&program, (KL_CLIProgramConfig){0});

  KL_CLICommand* cmd_bitmap = kl_cli_program_define_command(
    &program,
    (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("bitmap"),
      .fn = cmd_bitmap_fn,
    }
  );
  (void)cmd_bitmap;

  //KL_CLICommand* cmd_bitmap2 = kl_cli_program_define_command(
  //  &program,
  //  (KL_CLICommandConfig){
  //    .name = KL_STR_CONST_LIT("bitmap2"),
  //    .fn = cmd_bitmap_fn2,
  //  }
  //);
  //(void)cmd_bitmap2;

  KL_CLICommand* cmd_bitmap_pipeline = kl_cli_program_define_command(
    &program,
    (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("bitmap-pipeline"),
      .fn = cmd_bitmap_pipeline_fn,
    }
  );
  kl_cli_command_add_positional(cmd_bitmap_pipeline, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("in"),
    .required = true,
    .type = KL_CLI_ARG_STRING
  });

  kl_cli_command_add_option(cmd_bitmap_pipeline, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("out"),
    .required = false,
    .type = KL_CLI_ARG_STRING,
    .default_value = {
      .type = KL_CLI_ARG_STRING,
      .name = KL_STR_CONST_LIT("out"),
      .as.string = KL_STR_CONST_LIT("out")
    }
  });

  // Light direction for the preview render (tangent space: +X right, +Y up, +Z out of the wall).
  const char* light_names[3] = { "light-x", "light-y", "light-z" };
  const double light_defaults[3] = { -1.0, 1.0, 0.8 };
  for (int i = 0; i < 3; i++) {
    kl_cli_command_add_option(cmd_bitmap_pipeline, (KL_CLIArgConfig){
      .name = (KL_StrConst){ .value = light_names[i], .length = strlen(light_names[i]) },
      .description = KL_STR_CONST_LIT("preview light direction component"),
      .required = false,
      .type = KL_CLI_ARG_FLOAT,
      .default_value = {
        .type = KL_CLI_ARG_FLOAT,
        .name = (KL_StrConst){ .value = light_names[i], .length = strlen(light_names[i]) },
        .as.floating = light_defaults[i]
      }
    });
  }
  

  int ret = kl_cli_program_run(&program, argc, argv);

  kl_cli_program_destroy(&program);
  return ret;
}
