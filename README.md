# kl (Karlsson's Library)
A library of useful C utilities

## Core features
- allocator: `#include <kl/memory/allocator.h>`
- dynamic generic list: `#include <kl/templates/list/list.h>`
- generic sparse set: `#include <kl/templates/sparse_set/sparse_set.h>`
- "binary" sparse set: `#include <kl/templates/sparse_set/binary_sparse_set.h>`
- generic hashmap: `#include <kl/templates/hashmap/hashmap.h>`
- pool allocator with stable handles / ID's: `#include <kl/templates/pool/binary_pool.h>`
- string utilities: `#include <kl/string/string.h>`
- JSON parser: `#include <kl/json/parser.h>`


## Compiling
```bash
./compile.sh
```

## Running tests
```bash
./run.sh test
```
