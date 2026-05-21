#include <kl/json/node.h>

KL_IMPL_LIST(json_node_ptr, KL_JSONNode*);
KL_IMPL_LIST(json_node, KL_JSONNode);
KL_IMPL_HASHMAP(json_node_ptr, KL_JSONNode*);



static bool stringify(const KL_JSONNode* node, KL_StrBuffer* out, int32_t pad) {
  switch (node->type) {
    case KL_JSON_NODE_STRING: {
      if (!kl_str_buffer_append_char(out, '"')) return false;
      if (!kl_str_buffer_append(out, node->as.string)) return false;
      if (!kl_str_buffer_append_char(out, '"')) return false;
      return true;
    };
    case KL_JSON_NODE_NUMBER: {
      char tmp[64] = {0};
      snprintf(tmp, 64-1, "%1.9f\n", node->as.number);
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
          if (!kl_str_buffer_append_char(out, '\n')) return false;
        }
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
