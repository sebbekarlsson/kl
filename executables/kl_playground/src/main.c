#include <kl/cli/program.h>
#include <kl/string/string.h>
#include <kl/hash/hash.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <kl/tui/table.h>
#include <kl/bitmap/bitmap.h>
#include <kl/math/macros.h>
#include <kl/math/vec3.h>
#include <kl/math/vec4.h>
#include <kl/common/number.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>


static inline void int64_to_string(void* value, KL_StrBuffer* out) {
  kl_str_buffer_append_fmt(out, "%ld", *(int64_t*)value);
}

static int cmd_hash_fn(KL_CLICommand *cmd) {

  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, (KL_HashmapConfig){
    .capacity = 5013,
    .collision_capacity = 4,
    .value_to_string = int64_to_string
  });

  kl_int64_hashmap_set(&map, KL_STR_CONST_TO_BLOB(KL_STR_CONST_LIT("x")), 3);
  kl_int64_hashmap_set(&map, KL_STR_CONST_TO_BLOB(KL_STR_CONST_LIT("y")), -4);
  kl_int64_hashmap_set(&map, KL_STR_CONST_TO_BLOB(KL_STR_CONST_LIT("z")), 10);

  kl_int64_hashmap_print_json(&map, stdout);
  
  return 0;
}

static int cmd_table_fn(KL_CLICommand *cmd) {
  (void) cmd;


  KL_Allocator allocator = {0};
  kl_allocator_init(&allocator, (KL_AllocatorConfig){
    .capacity = 2*1024,
    .track_allocations = true,
    .tracked_allocations_capacity = 16
  });

  KL_TUI_Table table = {0};
  kl_tui_table_init(&table, (KL_TUI_TableConfig){
    .allocator = &allocator,
  });

  kl_tui_table_add_row(&table, (KL_StrConst[]){ KL_STR_CONST_LIT("firstname"), KL_STR_CONST_LIT("lastname"), KL_STR_CONST_LIT("age"), KL_STR_CONST_LIT("score") }, 4);

  kl_tui_table_begin_row(&table);
  kl_tui_table_add_column(&table, "john");
  kl_tui_table_add_column(&table, "doe");
  kl_tui_table_add_column(&table, "%d", 45);
  kl_tui_table_add_column(&table, "%1.6f", 3.5f);
  kl_tui_table_end_row(&table);


  kl_tui_table_begin_row(&table);
  kl_tui_table_add_column(&table, "sarah");
  kl_tui_table_add_column(&table, "sharp");
  kl_tui_table_add_column(&table, "%d", 46);
  kl_tui_table_add_column(&table, "%1.6f", 7.2f);
  kl_tui_table_end_row(&table);

  kl_tui_table_begin_row(&table);
  kl_tui_table_add_column(&table, "boris");
  kl_tui_table_add_column(&table, "kent");
  kl_tui_table_add_column(&table, "%d", 33);
  kl_tui_table_add_column(&table, "%1.6f", 0.583f);
  kl_tui_table_end_row(&table);

  for (int i = 0; i < 1000; i++) {
    kl_tui_table_begin_row(&table);
    kl_tui_table_add_column(&table, "first_%d", i);
    kl_tui_table_add_column(&table, "last_%d", i * 7);
    kl_tui_table_add_column(&table, "%d", i % 100);
    kl_tui_table_add_column(&table, "%1.6f", (float)i * 0.31f);
    kl_tui_table_end_row(&table);
  }

  kl_tui_table_print(&table, stdout);

  kl_allocator_destroy(&allocator);
  
  return 0;
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
  KL_Bitmap* bm = ctx->self;
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
    .bitmap = &bm,
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
  
  KL_CLICommand* cmd_hash = kl_cli_program_define_command(
    &program,
    (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("hash"),
      .fn = cmd_hash_fn,
    }
  );
  (void)cmd_hash;
  //kl_cli_command_add_option(cmd_hash, (KL_CLIArgConfig){
  //  .name = KL_STR_CONST_LIT("x"),
  //  .type = KL_CLI_ARG_BOOLEAN
  //});

  kl_cli_program_define_command(
    &program,
    (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("table"),
      .fn = cmd_table_fn,
    }
  );

  int ret = kl_cli_program_run(&program, argc, argv);

  kl_cli_program_destroy(&program);
  return ret;
}
