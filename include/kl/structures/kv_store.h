#ifndef KL_KV_STORE_H
#define KL_KV_STORE_H

#include <stdint.h>
#include <stdbool.h>

#include <kl/memory/allocator.h>
#include <kl/string/string.h>
#include <kl/templates/list/list.h>
#include <kl/templates/list/int64_list.h>
#include <kl/templates/list/str_const_list.h>
#include <kl/templates/sparse_set/sparse_set.h>
#include <kl/templates/hashmap/hashmap.h>


typedef enum {
  KL_KV_NODE_NULL,
  KL_KV_NODE_UINT8,
  KL_KV_NODE_INT8,
  KL_KV_NODE_BOOLEAN,
  KL_KV_NODE_UINT32,
  KL_KV_NODE_INT32,
  KL_KV_NODE_UINT64,
  KL_KV_NODE_INT64,
  KL_KV_NODE_STRING
} KL_KVNodeType;

typedef struct KL_KVNode KL_KVNode;
KL_DEFINE_LIST(kv_node, KL_KVNode);
KL_DEFINE_SPARSE_SET(kv_node, KL_KVNode);

typedef struct KL_KVStore KL_KVStore;

struct KL_KVNode {
  KL_KVNodeType type;

  union {
    uint8_t uint8;
    int8_t int8;
    bool boolean;
    uint32_t uint32;
    int32_t int32;
    uint64_t uint64;
    int64_t int64;
    KL_StrConst string;
  } as;
};

KL_DEFINE_HASHMAP(kv_node, KL_KVNode);

typedef struct {
  int64_t capacity;
  KL_Allocator* allocator; // optional
} KL_KVStoreConfig;

struct KL_KVStore {
  bool initialized;
  KL_KVStoreConfig config;
  KL_kv_node_Hashmap nodes;
};


void kl_kv_store_init(KL_KVStore* store, KL_KVStoreConfig cfg);

void kl_kv_store_destroy(KL_KVStore* store);

bool kl_kv_store_has(KL_KVStore* store, KL_StrConst key);

KL_KVNode kl_kv_store_get(KL_KVStore* store, KL_StrConst key);

void kl_kv_store_set(KL_KVStore* store, KL_StrConst key, KL_KVNode node);

#endif
