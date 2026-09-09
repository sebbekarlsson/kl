#include <kl/templates/list/str_const_list_extra.h>
#include <assert.h>


bool kl_str_const_list_includes_str(const KL_str_const_List* list, KL_StrConst str) {
  assert(list != 0);
  assert(list->initialized);

  for (int64_t i = 0; i < list->length; i++) {
    KL_StrConst b = list->items[i];
    if (kl_str_const_eq(str, b)) return true;
  }
  
  return false;
}


bool kl_str_const_list_push_unique(KL_str_const_List* list, KL_StrConst str) {
  assert(list != 0);
  assert(list->initialized);

  if (kl_str_const_list_includes_str(list, str)) return false;

  kl_str_const_list_push(list, str);

  return true;
}


bool kl_str_const_list_push_unique_strdup(KL_str_const_List* list, KL_StrConst str, KL_Allocator* allocator) {
  assert(list != 0);
  assert(list->initialized);

  if (kl_str_const_list_includes_str(list, str)) return false;

  kl_str_const_list_push(list, kl_str_const_strdup(str, allocator));

  return true;
}
