#ifndef KL_JSON_NODE_H
#define KL_JSON_NODE_H

#include <stdint.h>
#include <stdbool.h>
#include <kl/string/string.h>
#include <kl/templates/list/list.h>
#include <kl/templates/hashmap/hashmap.h>

typedef enum {
  KL_JSON_NODE_STRING,
  KL_JSON_NODE_INT,
  KL_JSON_NODE_FLOAT,
  KL_JSON_NODE_BOOLEAN,
  KL_JSON_NODE_NULL,
  KL_JSON_NODE_OBJECT,
  KL_JSON_NODE_ARRAY,
  KL_JSON_NODE_PARSER_ERROR
} KL_JSONNodeType;

typedef struct KL_JSONNode KL_JSONNode;

KL_DEFINE_LIST(json_node_ptr, KL_JSONNode*);
KL_DEFINE_LIST(json_node, KL_JSONNode);
KL_DEFINE_HASHMAP(json_node_ptr, KL_JSONNode*);

struct KL_JSONNode {
  KL_JSONNodeType type;

  union {
    KL_StrConst string;
    double number_float;
    int64_t number_int;
    bool boolean;
    KL_json_node_ptr_List array;
    KL_json_node_ptr_Hashmap object;
  } as;
};


bool kl_json_node_stringify(const KL_JSONNode* node, KL_StrBuffer* out);

double kl_json_node_as_float(const KL_JSONNode* node);

int64_t kl_json_node_as_int(const KL_JSONNode* node);

bool kl_json_node_as_boolean(const KL_JSONNode* node);

KL_JSONNode* kl_json_node_select(KL_JSONNode* node, KL_StrConst path);

int64_t kl_json_node_select_int(KL_JSONNode* node, KL_StrConst path, int64_t fallback);

KL_json_node_ptr_List* kl_json_node_select_array(KL_JSONNode* node, KL_StrConst path);

#endif
