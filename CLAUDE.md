# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`kl` ("Karlsson's Library") — a static C library of utilities (arena allocator, strings, mutex, generic containers via template macros). Built as a single static lib `libkl.a` plus subprojects under `executables/` that link against it.

## Build & Test

- `./compile.sh` — runs `cmake` + `make -j$(nproc)` into `./build/`. Always invoke from repo root; the script preserves cwd.
- `./run.sh <target>` — compiles, then runs `./build/<target>`. The `test` target is the test runner: `./run.sh test`.
- Single test: `./build/test --test <test_name>` (or `./run.sh test --test <test_name>`). The `--test` flag matches the exact function name as stringified in `KL_TEST_ENTRY`.
- `./build/test --report <file>` writes a per-test timing report instead of running normally; `--filter <substr>` narrows which tests it includes (substring match; only used in report mode).
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
  - `KL_Allocator` free-memory semantics: with `config.track_allocations` **off** (the default), `kl_allocator_free_allocation` is a no-op — freed regions are never reused and pages only die at destroy, so alloc/free churn grows the arena without bound. With tracking **on**, frees and reuse are ~O(1): handles carry their tracked-slot index (`KL_Allocation.tracked_index`) and freed slots live in per-size-class free lists (`free_heads[]` + intrusive `next_free`); a reused block is always within 2× of the request. Rule of thumb: arena-without-tracking for build-once data, tracking for high-churn workloads (e.g. g3's BSP build).
- `executables/<name>/{src,include}/` — programs that link against `libkl.a`.
- `scripts/` — Python tooling; has its own `pyproject.toml` (uv-managed, Python ≥ 3.13).

## Test framework

The test framework is part of `libkl.a` and available to any application that links against the library via `#include <kl/test/framework.h>`. Macros: `KL_TEST_BEGIN/END`, `KL_TEST_ASSERT(expr, msg)`, `KL_TEST_EXPECT_EQ_INT(a, b)`, `KL_TEST_EXPECT_EXPR(expr)`, plus `KL_TEST_IT/IT_END` for named sub-tests. Tests are plain functions registered in a `KL_TestEntry` array via `KL_TEST_ENTRY(fn_name)`. The convenience function `kl_test_run_entry(KL_TestEntry*)` handles context setup and BEGIN/END for a single entry.

Framework layout (in the main library):
- `include/kl/test/framework.h` — macros, types, `extern` declarations of the run-wide counters and helpers.
- `src/test/framework.c` — the single definition of those counters/helpers plus `kl_test_run_entry`. They **must not** be `static` in the header: each test `.c` file is its own translation unit, so per-TU copies would desync the pass/fail totals.

KL's own test suites live under `executables/test/`:
- `include/test/tests.h` — umbrella header. Pulls in `<kl/test/framework.h>` + every library header a suite needs, and declares every test function.
- `include/test/entries.h` — the `TEST_ENTRIES[]` registry of `KL_TEST_ENTRY(...)` lines.
- `src/test_<suite>.c` — test functions, one file per suite (`allocator`, `list`, `hashmap`, `string`, `json`, `noise`, `vec`, ... — see the directory for the full set). Each `#include <test/tests.h>`. Suite-local helpers stay `static`.
- `src/main.c` — CLI handling only (`--test`, `--filter`, `--report`) plus the run loop over `TEST_ENTRIES`.

No discovery. Adding a test is three steps: write the function in the relevant `src/test_<suite>.c`, declare it in `include/test/tests.h`, and add a `KL_TEST_ENTRY(...)` line to `TEST_ENTRIES[]` in `include/test/entries.h`. `add_subproject` globs `src/*.c`, so a brand-new suite file is picked up on the next `./compile.sh` (cmake reruns) without editing `CMakeLists.txt`.
