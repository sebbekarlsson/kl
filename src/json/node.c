#include <kl/json/node.h>
#include <kl/string/scanner.h>
#include <ctype.h>
#include <stdalign.h>

KL_IMPL_LIST(json_node_ptr, KL_JSONNode*);
KL_IMPL_LIST(json_node, KL_JSONNode);
KL_IMPL_HASHMAP(json_node_ptr, KL_JSONNode*);

KL_JSONNode* kl_json_node_create(KL_Allocator* allocator, KL_JSONNode cfg) {
  assert(allocator != 0);

  KL_JSONNode* node = (KL_JSONNode*)kl_allocator_malloc_aligned(allocator, sizeof(KL_JSONNode), alignof(KL_JSONNode));
  assert(node != 0);
  memcpy(node, &cfg, sizeof(KL_JSONNode));
  return node;
}

KL_JSONNode* kl_json_node_create_string(KL_Allocator* allocator, KL_StrConst value) {
  KL_JSONNode* node = kl_json_node_create(allocator, (KL_JSONNode){ .type = KL_JSON_NODE_STRING });
  node->as.string = kl_str_const_strdup(value, allocator);
  return node;
}

KL_JSONNode* kl_json_node_create_object(KL_Allocator* allocator) {
  KL_JSONNode* node = kl_json_node_create(allocator, (KL_JSONNode){ .type = KL_JSON_NODE_OBJECT });
  kl_json_node_ptr_hashmap_init(&node->as.object, (KL_HashmapConfig){
    .capacity = 73,
    .collision_capacity = 4,
    .allocator = allocator,
    .track_used_slots = true
  });
  return node;
}

bool kl_json_node_object_set(KL_JSONNode* obj, KL_StrConst key, KL_JSONNode* value) {
  if (obj == 0 || obj->type != KL_JSON_NODE_OBJECT || !obj->as.object.initialized) return false;
  if (key.value == 0 || key.length <= 0 || value == 0) return false;
  return kl_json_node_ptr_hashmap_set(&obj->as.object, KL_STR_CONST_TO_BLOB(key), value) >= 0;
}

bool kl_json_node_object_unset(KL_JSONNode* obj, KL_StrConst key) {
  if (obj == 0 || obj->type != KL_JSON_NODE_OBJECT || !obj->as.object.initialized) return false;
  if (key.value == 0 || key.length <= 0) return false;
  return kl_json_node_ptr_hashmap_unset(&obj->as.object, KL_STR_CONST_TO_BLOB(key)) != 0;
}

KL_JSONNode* kl_json_node_object_get(KL_JSONNode* obj, KL_StrConst key) {
  if (obj == 0 || obj->type != KL_JSON_NODE_OBJECT || !obj->as.object.initialized) return 0;
  if (key.value == 0 || key.length <= 0) return 0;
  KL_JSONNode* value = 0;
  if (!kl_json_node_ptr_hashmap_get(&obj->as.object, KL_STR_CONST_TO_BLOB(key), &value)) return 0;
  return value;
}



static bool stringify(const KL_JSONNode* node, KL_StrBuffer* out, int32_t pad) {
  switch (node->type) {
    case KL_JSON_NODE_STRING: {
      if (!kl_str_buffer_append_char(out, '"')) return false;
      if (!kl_str_buffer_append(out, node->as.string)) return false;
      if (!kl_str_buffer_append_char(out, '"')) return false;
      return true;
    };
    case KL_JSON_NODE_FLOAT: {
      char tmp[64] = {0};
      snprintf(tmp, 64-1, "%1.9f", node->as.number_float);
      return kl_str_buffer_append(out, KL_STR_CONST(tmp));
    };
    case KL_JSON_NODE_INT: {
      char tmp[64] = {0};
      snprintf(tmp, 64-1, "%ld", node->as.number_int);
      return kl_str_buffer_append(out, KL_STR_CONST(tmp));
    };
    case KL_JSON_NODE_BOOLEAN: return node->as.boolean == true ?
    kl_str_buffer_append(out, KL_STR_CONST_LIT("true")) :
    kl_str_buffer_append(out, KL_STR_CONST_LIT("false")) ;
    case KL_JSON_NODE_NULL: return kl_str_buffer_append(out, KL_STR_CONST_LIT("null"));
    case KL_JSON_NODE_ARRAY: {
      if (!kl_str_buffer_append_char(out, '[')) return false;
      if (!kl_str_buffer_append_char(out, '\n')) return false;

      for (int64_t i = 0; i < node->as.array.length; i++) {
        KL_JSONNode* item = node->as.array.items[i];
        kl_str_buffer_repeat_char(out, ' ', pad+2);
        if (!stringify(item, out, pad+2)) return false;
        if (i < (node->as.array.length - 1)) {
          if (!kl_str_buffer_append_char(out, ',')) return false;
          if (!kl_str_buffer_append_char(out, '\n')) return false;
        }
      }
      
      if (!kl_str_buffer_append_char(out, '\n')) return false;
      kl_str_buffer_repeat_char(out, ' ', pad);
      if (!kl_str_buffer_append_char(out, ']')) return false;

      return true;
    };

    case KL_JSON_NODE_OBJECT: {
      if (!kl_str_buffer_append_char(out, '{')) return false;
      if (!kl_str_buffer_append_char(out, '\n')) return false;

      for (int64_t i = 0; i < node->as.object.used_slots.length; i++) {
        kl_str_buffer_repeat_char(out, ' ', pad+2);
        int64_t idx = node->as.object.used_slots.items[i]; 
        KL_JSONNode* item = node->as.object.slots.items[idx].value;
        assert(item != 0);
        if (!kl_str_buffer_append_char(out, '"')) return false;
        if(!kl_str_buffer_append(out, (KL_StrConst){
          .value = (const char*)node->as.object.slots.items[idx].key.value,
          .length = node->as.object.slots.items[idx].key.size
        })) return false;
        if (!kl_str_buffer_append_char(out, '"')) return false;
        if (!kl_str_buffer_append_char(out, ':')) return false;
        if (!kl_str_buffer_append_char(out, ' ')) return false;
        if (!stringify(item, out, pad+2)) return false;

        if (i < (node->as.object.used_slots.length - 1)) {
          if (!kl_str_buffer_append_char(out, ',')) return false;
        }
        if (!kl_str_buffer_append_char(out, '\n')) return false;
      }

      kl_str_buffer_repeat_char(out, ' ', pad);
      if (!kl_str_buffer_append_char(out, '}')) return false;

      return true;
    };
    case KL_JSON_NODE_PARSER_ERROR: return kl_str_buffer_append(out, KL_STR_CONST_LIT("[PARSER ERROR]"));
  }

  return false;

}

bool kl_json_node_stringify(const KL_JSONNode* node, KL_StrBuffer* out) {
  return stringify(node, out, 0);
}

double kl_json_node_as_float(const KL_JSONNode* node) {
  switch (node->type) {
    case KL_JSON_NODE_FLOAT: return node->as.number_float;
    case KL_JSON_NODE_INT: return (double)node->as.number_int;
    default: return 0;
  }
}

int64_t kl_json_node_as_int(const KL_JSONNode* node) {
  switch (node->type) {
    case KL_JSON_NODE_INT: return node->as.number_int;
    case KL_JSON_NODE_FLOAT: return (int64_t)node->as.number_float;
    default: return 0;
  }
}

bool kl_json_node_as_boolean(const KL_JSONNode* node) {
  switch (node->type) {
    case KL_JSON_NODE_BOOLEAN: return node->as.boolean;
    case KL_JSON_NODE_INT: return node->as.number_int > 0;
    case KL_JSON_NODE_FLOAT: return node->as.number_float > 0;
    default: return false;
  }
}

static inline bool is_traversable(const KL_JSONNode* node) {
  return node->type == KL_JSON_NODE_ARRAY ||
         node->type == KL_JSON_NODE_OBJECT;
}

static inline KL_JSONNode* select_shallow(KL_JSONNode* node, KL_StrConst path) {
  switch (node->type) {
    case KL_JSON_NODE_ARRAY: {
      if (!kl_str_const_is_integer(path)) return 0;
      int64_t idx = atoll(path.value);
      if (idx < 0 || idx >= node->as.array.length) return 0;
      assert(node->as.array.items != 0);
      return node->as.array.items[idx];
    };
    case KL_JSON_NODE_OBJECT: {
      if (!node->as.object.initialized) return 0;
      KL_JSONNode* selected = 0;
      if (!kl_json_node_ptr_hashmap_get(&node->as.object, KL_STR_CONST_TO_BLOB(path), &selected)) return 0;
      assert(selected != 0);
      return selected;
    };
    default: return 0;
  }
  return 0;
}

KL_JSONNode* kl_json_node_select(KL_JSONNode* node, KL_StrConst path) {
  if (path.length <= 0 || path.value == 0) return node;
  if (!is_traversable(node)) return 0;

  KL_JSONNode* cur = node;

  KL_StrScanner scanner = {0};
  kl_str_scanner_init(&scanner, path);
  kl_str_scanner_skip_whitespace(&scanner);

  KL_StrBuffer tmp = KL_STR_BUFFER_STACK_ALLOC(256);
  kl_str_buffer_clear(&tmp);


  while (kl_str_scanner_has_more(&scanner) && cur != 0) {

    kl_str_scanner_skip_whitespace(&scanner);
    if (!kl_str_scanner_has_more(&scanner)) break;

    if (scanner.c == '"') {
      if (!kl_str_scanner_eat_char(&scanner, '"')) return 0;
      kl_str_buffer_clear(&tmp);
      if (!kl_str_scanner_eat_until_char(&scanner, '"', &tmp)) return 0;
      assert(tmp.length > 0);
      assert(tmp.data != 0);
      cur = select_shallow(cur, (KL_StrConst){ .value = tmp.data, .length = tmp.length });
      if (!kl_str_scanner_eat_char(&scanner, '"')) return 0;
      continue;
    }

    if (kl_str_scanner_has_identifier_start(&scanner)) {
      kl_str_buffer_clear(&tmp);
      if (!kl_str_scanner_eat_identifier(&scanner, &tmp)) return 0;
      assert(tmp.length > 0);
      assert(tmp.data != 0);
      cur = select_shallow(cur, (KL_StrConst){ .value = tmp.data, .length = tmp.length });
      continue;
    }

    if (isdigit(scanner.c)) {
      kl_str_buffer_clear(&tmp);
      if (!kl_str_scanner_eat_integer(&scanner, &tmp)) return 0;
      assert(tmp.length > 0);
      assert(tmp.data != 0);
      cur = select_shallow(cur, (KL_StrConst){ .value = tmp.data, .length = tmp.length });
      continue;
    }

    switch (scanner.c) {
      case '.': {
        if (!kl_str_scanner_eat_char(&scanner, '.')) return 0;
      } break;
      case '[': {
        if (!kl_str_scanner_eat_char(&scanner, '[')) return 0;
        kl_str_buffer_clear(&tmp);
        if (!kl_str_scanner_eat_until_char(&scanner, ']', &tmp)) return 0;
        assert(tmp.length > 0);
        assert(tmp.data != 0);
        cur = select_shallow(cur, (KL_StrConst){ .value = tmp.data, .length = tmp.length });
        if (!kl_str_scanner_eat_char(&scanner, ']')) return 0;
      } break;
      default: return 0;
    }
  }


  return cur;
}

int64_t kl_json_node_select_int(KL_JSONNode* node, KL_StrConst path, int64_t fallback) {
  if (path.length <= 0 || path.value == 0) return fallback;
  if (!is_traversable(node)) return fallback;

  KL_JSONNode* selected = kl_json_node_select(node, path);
  if (selected == 0) return fallback;

  if (selected->type == KL_JSON_NODE_FLOAT) {
    return (int64_t)selected->as.number_float;
  }
  
  if (selected->type != KL_JSON_NODE_INT) return fallback;

  return selected->as.number_int;
}


bool kl_json_node_select_boolean(KL_JSONNode* node, KL_StrConst path, bool fallback) {
  if (path.length <= 0 || path.value == 0) return fallback;
  if (!is_traversable(node)) return fallback;

  KL_JSONNode* selected = kl_json_node_select(node, path);
  if (selected == 0) return fallback;

  switch (selected->type) {
    case KL_JSON_NODE_BOOLEAN: return selected->as.boolean;
    case KL_JSON_NODE_INT: return selected->as.number_int > 0;
    case KL_JSON_NODE_FLOAT: return selected->as.number_float > 0.0;
    case KL_JSON_NODE_STRING: return kl_str_const_eq(selected->as.string, KL_STR_CONST_LIT("true")) || kl_str_const_eq(selected->as.string, KL_STR_CONST_LIT("yes")) || kl_str_const_eq(selected->as.string, KL_STR_CONST_LIT("1")) || kl_str_const_eq(selected->as.string, KL_STR_CONST_LIT("on"));
    default: return fallback;
  } 
}

double kl_json_node_select_float(KL_JSONNode* node, KL_StrConst path, double fallback) {
  if (path.length <= 0 || path.value == 0) return fallback;
  if (!is_traversable(node)) return fallback;

  KL_JSONNode* selected = kl_json_node_select(node, path);
  if (selected == 0) return fallback;

  if (selected->type == KL_JSON_NODE_INT) {
    return (double)selected->as.number_int;
  }
  
  if (selected->type != KL_JSON_NODE_FLOAT) return fallback;

  return selected->as.number_float;
}


KL_StrConst kl_json_node_select_string(KL_JSONNode* node, KL_StrConst path, KL_StrConst fallback) {
  if (path.length <= 0 || path.value == 0) return fallback;
  if (!is_traversable(node)) return fallback;

  KL_JSONNode* selected = kl_json_node_select(node, path);
  if (selected == 0) return fallback;

  if (selected->type != KL_JSON_NODE_STRING) {
    return fallback; 
  }
  
  return selected->as.string;
}

KL_json_node_ptr_List* kl_json_node_select_array(KL_JSONNode* node, KL_StrConst path) {
  if (path.length <= 0 || path.value == 0) return 0;
  if (!is_traversable(node)) return 0;
  KL_JSONNode* selected = kl_json_node_select(node, path);
  if (selected == 0) return 0;
  if (selected->type != KL_JSON_NODE_ARRAY) return 0;
  return &selected->as.array;
}
