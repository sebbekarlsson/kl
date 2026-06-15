#include <kl/structures/kv_store.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


KL_IMPL_LIST(kv_node, KL_KVNode);
KL_IMPL_SPARSE_SET(kv_node, KL_KVNode);
KL_IMPL_HASHMAP(kv_node, KL_KVNode);

void kl_kv_store_init(KL_KVStore* store, KL_KVStoreConfig cfg) {
  if (store->initialized) return;
  assert(cfg.capacity > 0);
  store->config = cfg;

  kl_kv_node_hashmap_init(&store->nodes, (KL_HashmapConfig){
    .capacity = cfg.capacity,
    .collision_capacity = 4,
    .allocator = cfg.allocator,
    .track_used_slots = true
  });

  store->initialized = true;
}

void kl_kv_store_destroy(KL_KVStore* store) {
  assert(store->initialized);

  kl_kv_node_hashmap_destroy(&store->nodes);

  memset(store, 0, sizeof(KL_KVStore));
}


bool kl_kv_store_has(KL_KVStore* store, KL_StrConst key) {
  assert(store->initialized);

  return kl_kv_node_hashmap_has(&store->nodes, KL_STR_CONST_TO_BLOB(key));
}

#define KL_KV_NODE_MAKE_NULL() ((KL_KVNode){ .type = KL_KV_NODE_NULL })


KL_KVNode kl_kv_store_get(KL_KVStore* store, KL_StrConst key) {
  assert(store->initialized);
  
  KL_KVNode node = {0};

  if (!kl_kv_node_hashmap_get(&store->nodes, KL_STR_CONST_TO_BLOB(key), &node)) return KL_KV_NODE_MAKE_NULL();

  return node;
}

void kl_kv_store_set(KL_KVStore* store, KL_StrConst key, KL_KVNode node) {
  assert(store->initialized);
  assert(key.value != 0);
  assert(key.length > 0);

  kl_kv_node_hashmap_set(&store->nodes, KL_STR_CONST_TO_BLOB(key), node);
}
