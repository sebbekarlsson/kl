# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`kl` ("Karlsson's Library") — a static C library of utilities (arena allocator, strings, mutex, generic containers via template macros). Built as a single static lib `libkl.a` plus subprojects under `executables/` that link against it.

## Build & Test

- `./compile.sh` — runs `cmake` + `make -j$(nproc)` into `./build/`. Always invoke from repo root; the script preserves cwd.
- `./run.sh <target>` — compiles, then runs `./build/<target>`. The `test` target is the test runner: `./run.sh test`.
- Single test: `./build/test --test <test_name>` (or `./run.sh test --test <test_name>`). The `--test` flag matches by function name as stringified in `KL_TEST_ENTRY`.
- Build type is `RelWithDebInfo` with `-DNDEBUG` stripped so `assert()` stays live. Compile flags include `-mavx2 -mfma`; the codebase assumes an AVX2-capable x86_64 host.
- `compile_commands.json` is exported into `./build/` for clangd/LSPs.

## Adding executables

Use `add_subproject(executables <name>)` in `CMakeLists.txt`. The macro auto-creates `executables/<name>/src/main.c` with a stub if the directory is missing, globs `src/*.c`, links `kl`, and adds `executables/<name>/include` to the include path. The `test` executable is the existing example.

## Template-based generics

Containers (`list`, `queue`, `hashmap`, `sparse_set`) are implemented as macros that expand to a fully-typed struct + functions. The generation pipeline:

1. The "dummy" header/impl pair under `include/kl/templates/<container>/dummy.{h,c}` is the source-of-truth template, written using sentinel identifiers `NAME` and `T_X` (with `T_X` typedef'd to `int` in `x__shared.h` so the dummy compiles standalone for tooling).
2. `scripts/generate_macros.py` (invoked by CMake's `generate_macros` custom target before the `kl` target builds) reads each dummy, strips include-guards/includes/`<top>` blocks, rewrites `NAME` → `##NAME##` (with `(NAME` → `(NAME##` for token-paste at call sites) and `T_X` → `T`, then emits `KL_DEFINE_<CONTAINER>(NAME, T)` / `KL_IMPL_<CONTAINER>(NAME, T)` macros into `include/kl/templates/<container>/<container>.h`.
3. The same script also generates concrete instantiations (e.g. `int64_list.{h,c}`, `uint32_hashmap.{h,c}`, `blob_ptr_list.{h,c}`) by writing thin wrappers that `KL_DEFINE_<CONTAINER>` in the header and `KL_IMPL_<CONTAINER>` in the impl. These generated files end with `/* generated */` and have a "must not be modified directly" banner.

When adding a new container type instantiation: add a `generate_type(...)` call in `scripts/generate_macros.py` rather than hand-writing the `.c`/`.h`. When changing container behavior: edit the `dummy.{h,c}` and re-run the build (CMake reruns the generator automatically on dummy changes).

To author a *new* container family, mirror the existing structure: `dummy.h`/`dummy.c` using `NAME`/`T_X`, a `shared.h` for per-family config types, and a `generate(...)` call in the Python script wiring it up.

## Source layout

- `src/` + `include/kl/` — library code, mirrored layout. Subareas: `memory/` (arena `KL_Allocator` in `allocator.{h,c}` plus `alloc.{h,c}`), `string/`, `threads/` (`KL_Mutex`), `binary/` (header-only `KL_Blob`), `templates/` (see above).
- `executables/<name>/{src,include}/` — programs that link against `libkl.a`.
- `scripts/` — Python tooling; has its own `pyproject.toml` (uv-managed, Python ≥ 3.13).

## Test framework

In-tree, in `executables/test/include/test/framework.h`. Tests are plain `static void` functions registered in an array of `KL_TestEntry` in `main.c` via `KL_TEST_ENTRY(fn_name)`. Macros: `KL_TEST_BEGIN/END`, `KL_TEST_ASSERT(expr, msg)`, `KL_TEST_EXPECT_EQ_INT(a, b)`. No discovery — new tests must be added to the `tests[]` array in `executables/test/src/main.c`.
