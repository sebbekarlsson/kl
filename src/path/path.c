#include <kl/path/path.h>
#include <unistd.h>
#include <stddef.h>

bool kl_path_is_absolute(KL_StrConst path) {
  return path.value != 0 && path.length > 0 && path.value[0] == '/';
}

/* ---- Lexical queries (ported from Node.js path.posix) ---- */

KL_StrConst kl_path_dirname(KL_StrConst path) {
  if (path.value == 0 || path.length <= 0) return KL_STR_CONST_LIT(".");

  bool has_root = path.value[0] == '/';
  int64_t end = -1;
  bool matched_slash = true;

  for (int64_t i = path.length - 1; i >= 1; i--) {
    if (path.value[i] == '/') {
      if (!matched_slash) {
        end = i;
        break;
      }
    } else {
      matched_slash = false;
    }
  }

  if (end == -1) return has_root ? KL_STR_CONST_LIT("/") : KL_STR_CONST_LIT(".");
  return kl_str_const_slice(path, 0, end);
}

KL_StrConst kl_path_basename(KL_StrConst path) {
  if (path.value == 0 || path.length <= 0) return (KL_StrConst){0};

  int64_t start = 0;
  int64_t end = -1;
  bool matched_slash = true;

  for (int64_t i = path.length - 1; i >= 0; i--) {
    if (path.value[i] == '/') {
      if (!matched_slash) {
        start = i + 1;
        break;
      }
    } else if (end == -1) {
      matched_slash = false;
      end = i + 1;
    }
  }

  if (end == -1) return (KL_StrConst){0};
  return kl_str_const_slice(path, start, end - start);
}

KL_StrConst kl_path_extname(KL_StrConst path) {
  if (path.value == 0 || path.length <= 0) return (KL_StrConst){0};

  int64_t start_dot = -1;
  int64_t start_part = 0;
  int64_t end = -1;
  bool matched_slash = true;
  /* 0: nothing yet, 1: a dot ran before the last dot, -1: a non-dot did */
  int pre_dot_state = 0;

  for (int64_t i = path.length - 1; i >= 0; i--) {
    char c = path.value[i];
    if (c == '/') {
      if (!matched_slash) {
        start_part = i + 1;
        break;
      }
      continue;
    }
    if (end == -1) {
      matched_slash = false;
      end = i + 1;
    }
    if (c == '.') {
      if (start_dot == -1) {
        start_dot = i;
      } else if (pre_dot_state != 1) {
        pre_dot_state = 1;
      }
    } else if (start_dot != -1) {
      pre_dot_state = -1;
    }
  }

  if (start_dot == -1 || end == -1 || pre_dot_state == 0 ||
      (pre_dot_state == 1 && start_dot == end - 1 && start_dot == start_part + 1)) {
    return (KL_StrConst){0};
  }

  return kl_str_const_slice(path, start_dot, end - start_dot);
}

/* ---- Core path builder ----
*
* A KL_PathBuilder is a stack-local value owned by each public function; it is
* passed by pointer to the helpers below. There are no static struct instances
* in this module, so it stays reentrant and thread-safe.
*/

typedef struct {
  KL_StrBuffer* out;
  int64_t root_len;      /* 1 for an absolute path ("/"), else 0 */
  bool is_abs;
  bool ok;               /* cleared if any append overflows the buffer */
  bool wants_trailing;   /* the last non-empty part ended with '/' */
} KL_PathBuilder;

static void kl_path_builder_begin(KL_PathBuilder* b, KL_StrBuffer* out, bool is_abs) {
  b->out = out;
  b->is_abs = is_abs;
  b->ok = true;
  b->wants_trailing = false;
  out->length = 0;

  if (is_abs) {
    if (!kl_str_buffer_append_char(out, '/')) b->ok = false;
    b->root_len = 1;
  } else {
    b->root_len = 0;
  }
}

/* index where the last path segment in `out` begins (>= root_len) */
static int64_t kl_path_builder_last_seg_start(KL_PathBuilder* b) {
  KL_StrBuffer* out = b->out;
  int64_t i = out->length - 1;
  while (i >= b->root_len && out->data[i] != '/') i--;
  return i + 1;
}

static bool kl_path_builder_last_seg_is_dotdot(KL_PathBuilder* b) {
  int64_t start = kl_path_builder_last_seg_start(b);
  int64_t len = b->out->length - start;
  return len == 2 && b->out->data[start] == '.' && b->out->data[start + 1] == '.';
}

static void kl_path_builder_pop(KL_PathBuilder* b) {
  int64_t start = kl_path_builder_last_seg_start(b);
  b->out->length = (start > b->root_len) ? (start - 1) : b->root_len;
}

static void kl_path_builder_maybe_sep(KL_PathBuilder* b) {
  KL_StrBuffer* out = b->out;
  if (out->length > 0 && out->data[out->length - 1] != '/') {
    if (!kl_str_buffer_append_char(out, '/')) b->ok = false;
  }
}

static void kl_path_builder_segment(KL_PathBuilder* b, const char* seg, int64_t seg_len) {
  if (seg_len <= 0) return;                                  /* empty -> collapse */
  if (seg_len == 1 && seg[0] == '.') return;                 /* "." -> skip */

  if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
    bool has_seg = b->out->length > b->root_len;
    if (has_seg && !kl_path_builder_last_seg_is_dotdot(b)) {
      kl_path_builder_pop(b);
    } else if (!b->is_abs) {
      kl_path_builder_maybe_sep(b);
      if (!kl_str_buffer_append(b->out, (KL_StrConst){.value = "..", .length = 2})) {
        b->ok = false;
      }
    }
    /* else: absolute path with nothing to pop -> drop the ".." */
    return;
  }

  kl_path_builder_maybe_sep(b);
  if (!kl_str_buffer_append(b->out, (KL_StrConst){.value = seg, .length = seg_len})) {
    b->ok = false;
  }
}

static void kl_path_builder_add(KL_PathBuilder* b, KL_StrConst part) {
  if (part.value == 0 || part.length <= 0) return;

  b->wants_trailing = part.value[part.length - 1] == '/';

  int64_t seg_start = 0;
  for (int64_t i = 0; i <= part.length; i++) {
    if (i == part.length || part.value[i] == '/') {
      kl_path_builder_segment(b, &part.value[seg_start], i - seg_start);
      seg_start = i + 1;
    }
  }
}

static bool kl_path_builder_finish(KL_PathBuilder* b, bool strip_trailing) {
  KL_StrBuffer* out = b->out;

  if (out->length == 0) {
    /* relative path that collapsed to nothing */
    if (!kl_str_buffer_append_char(out, '.')) b->ok = false;
    if (b->wants_trailing && !strip_trailing) {
      if (!kl_str_buffer_append_char(out, '/')) b->ok = false;
    }
  } else if (!strip_trailing && b->wants_trailing &&
             out->length > b->root_len && out->data[out->length - 1] != '/') {
    if (!kl_str_buffer_append_char(out, '/')) b->ok = false;
  }

  /* NUL-terminate the result. `pop` rewinds `out->length` without touching the
     bytes, so the terminator must be re-laid here once the length is final.
     The buffer reserves a byte past `capacity` for it. */
  if (out->data != 0) {
    out->data[out->length] = '\0';
  }

  return b->ok;
}

/* ---- String producers ---- */

bool kl_path_normalize(KL_StrBuffer* out, KL_StrConst path) {
  KL_PathBuilder b;
  kl_path_builder_begin(&b, out, kl_path_is_absolute(path));
  kl_path_builder_add(&b, path);
  return kl_path_builder_finish(&b, false);
}

bool kl_path_join(KL_StrBuffer* out, const KL_StrConst* parts, int64_t n_parts) {
  bool is_abs = false;
  for (int64_t i = 0; i < n_parts; i++) {
    if (parts[i].value != 0 && parts[i].length > 0) {
      is_abs = parts[i].value[0] == '/';
      break;
    }
  }

  KL_PathBuilder b;
  kl_path_builder_begin(&b, out, is_abs);
  for (int64_t i = 0; i < n_parts; i++) {
    kl_path_builder_add(&b, parts[i]);
  }
  return kl_path_builder_finish(&b, false);
}

bool kl_path_resolve(KL_StrBuffer* out, KL_StrConst cwd, const KL_StrConst* parts, int64_t n_parts) {
  int64_t k = -1;
  for (int64_t i = 0; i < n_parts; i++) {
    if (kl_path_is_absolute(parts[i])) k = i;
  }

  KL_PathBuilder b;
  if (k >= 0) {
    kl_path_builder_begin(&b, out, true);
    for (int64_t i = k; i < n_parts; i++) {
      kl_path_builder_add(&b, parts[i]);
    }
  } else {
    if (!kl_path_is_absolute(cwd)) return false;
    kl_path_builder_begin(&b, out, true);
    kl_path_builder_add(&b, cwd);
    for (int64_t i = 0; i < n_parts; i++) {
      kl_path_builder_add(&b, parts[i]);
    }
  }
  return kl_path_builder_finish(&b, true);
}

bool kl_path_cwd(KL_StrBuffer* out) {
  if (out == 0 || out->data == 0 || out->capacity <= 0) return false;

  out->length = 0;
  if (getcwd(out->data, (size_t)out->capacity) == NULL) return false;

  out->length = (int64_t)strlen(out->data);
  return true;
}
