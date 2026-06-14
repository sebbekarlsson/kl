#ifndef KL_UNIQUE_LIST_NAME_H
#define KL_UNIQUE_LIST_NAME_H
#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>
#include <kl/memory/allocator.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/unique_list/shared.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <kl/templates/list/list.h>

KL_DEFINE_LIST(NAME__u, T_X);

typedef struct {
  KL_UniqueListConfig config;
  KL_NAME__u_List _list;
  KL_int64_Hashmap lookup;
  bool initialized;
} KL_NAME_UniqueList;


int kl_NAME_unique_list_init(KL_NAME_UniqueList* list, KL_UniqueListConfig cfg);

void kl_NAME_unique_list_destroy(KL_NAME_UniqueList* list);

void kl_NAME_unique_list_reset(KL_NAME_UniqueList* list);

bool kl_NAME_unique_list_push(KL_NAME_UniqueList *list, T_X item);

bool kl_NAME_unique_list_pop(KL_NAME_UniqueList* list, T_X* out);

bool kl_NAME_unique_list_pop_at(KL_NAME_UniqueList* list, int64_t idx, T_X* out);

void kl_NAME_unique_list_grow(KL_NAME_UniqueList *list, int64_t n_slots);

bool kl_NAME_unique_list_includes(KL_NAME_UniqueList* list, T_X item);

int64_t kl_NAME_unique_list_index_of(KL_NAME_UniqueList* list, T_X item);

#endif
