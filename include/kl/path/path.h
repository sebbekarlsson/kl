#ifndef KL_PATH_H
#define KL_PATH_H

#include <kl/string/string.h>
#include <stdint.h>
#include <stdbool.h>

/**
* @brief POSIX path utilities modeled on the Node.js `path` module.
*
* String-producing functions write their result into a caller-owned
* `KL_StrBuffer` - this module never allocates. Lexical query functions return
* zero-copy `KL_StrConst` views into their input.
**/

#define KL_PATH_SEPARATOR '/'

/* ---- Lexical queries: zero-copy, no allocation, no "."/".." resolution ---- */

/**
* @brief returns true if `path` begins with '/'
**/
bool kl_path_is_absolute(KL_StrConst path);

/**
* @brief the directory portion of `path`
*
* Returns a view into `path`, or the literals "." / "/". Purely lexical -
* does not resolve "." or "..".
**/
KL_StrConst kl_path_dirname(KL_StrConst path);

/**
* @brief the last component of `path`
*
* Returns a view into `path`, or an empty string when `path` has no component.
**/
KL_StrConst kl_path_basename(KL_StrConst path);

/**
* @brief the extension of `path`, including the leading '.'
*
* Returns a view into `path`, or an empty string when there is no extension.
**/
KL_StrConst kl_path_extname(KL_StrConst path);

/* ---- String producers: write into caller-owned `out` ----
*
* Each resets `out->length` to 0, then writes the result into
* `out->data[0 .. out->length)` and NUL-terminates it at `out->data[out->length]`.
* The buffer must have room for that terminator - `KL_STR_BUFFER_STACK_ALLOC`
* reserves it (the allocation is `capacity + 1` bytes).
* They return false on capacity overflow, leaving `out` in an undefined state.
* `parts` / `path` / `cwd` must not alias `out->data`.
*/

/**
* @brief joins `n_parts` path segments together and normalizes the result
**/
bool kl_path_join(KL_StrBuffer* out, const KL_StrConst* parts, int64_t n_parts);

/**
* @brief normalizes `path`, collapsing "." / ".." and duplicate separators
**/
bool kl_path_normalize(KL_StrBuffer* out, KL_StrConst path);

/**
* @brief resolves `parts` against `cwd` into an absolute, normalized path
*
* `cwd` must be absolute - if it is not, and no part is absolute, this returns
* false. Trailing slashes are stripped (except the root).
**/
bool kl_path_resolve(KL_StrBuffer* out, KL_StrConst cwd, const KL_StrConst* parts, int64_t n_parts);

/**
* @brief fills `out` with the process working directory (getcwd)
*
* The result is NUL-terminated by getcwd; `out->length` excludes the
* terminator. Returns false on error (e.g. the buffer is too small).
**/
bool kl_path_cwd(KL_StrBuffer* out);

/**
* @brief ergonomic variadic wrapper around kl_path_join
*
* Pass KL_StrConst arguments directly, e.g.
*   KL_PATH_JOIN(&buf, KL_STR_CONST("a"), KL_STR_CONST("b"));
* Requires at least one path argument.
**/
#define KL_PATH_JOIN(OUT, ...) \
  kl_path_join((OUT), \
    (const KL_StrConst[]){ __VA_ARGS__ }, \
    (int64_t)(sizeof((const KL_StrConst[]){ __VA_ARGS__ }) / sizeof(KL_StrConst)))

/**
* @brief ergonomic variadic wrapper around kl_path_resolve
*
* Requires at least one path argument.
**/
#define KL_PATH_RESOLVE(OUT, CWD, ...) \
  kl_path_resolve((OUT), (CWD), \
    (const KL_StrConst[]){ __VA_ARGS__ }, \
    (int64_t)(sizeof((const KL_StrConst[]){ __VA_ARGS__ }) / sizeof(KL_StrConst)))

#endif
