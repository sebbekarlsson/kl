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


/**
* @brief allocates a node from `allocator` and copies `cfg` into it
**/
KL_JSONNode* kl_json_node_create(KL_Allocator* allocator, KL_JSONNode cfg);

/**
* @brief creates a string node; `value` is duplicated into `allocator`
**/
KL_JSONNode* kl_json_node_create_string(KL_Allocator* allocator, KL_StrConst value);

/**
* @brief creates an empty object node with an initialized member hashmap
**/
KL_JSONNode* kl_json_node_create_object(KL_Allocator* allocator);

/**
* @brief sets `obj[key] = value`; the key is copied into the object's hashmap
**/
bool kl_json_node_object_set(KL_JSONNode* obj, KL_StrConst key, KL_JSONNode* value);

/**
* @brief removes `key` from `obj`; returns false if it was not present
**/
bool kl_json_node_object_unset(KL_JSONNode* obj, KL_StrConst key);

/**
* @brief returns `obj[key]`, or 0 when `obj` is not an object or has no such key
**/
KL_JSONNode* kl_json_node_object_get(KL_JSONNode* obj, KL_StrConst key);

bool kl_json_node_stringify(const KL_JSONNode* node, KL_StrBuffer* out);

double kl_json_node_as_float(const KL_JSONNode* node);

int64_t kl_json_node_as_int(const KL_JSONNode* node);

bool kl_json_node_as_boolean(const KL_JSONNode* node);

KL_JSONNode* kl_json_node_select(KL_JSONNode* node, KL_StrConst path);

int64_t kl_json_node_select_int(KL_JSONNode* node, KL_StrConst path, int64_t fallback);

double kl_json_node_select_float(KL_JSONNode* node, KL_StrConst path, double fallback);

KL_StrConst kl_json_node_select_string(KL_JSONNode* node, KL_StrConst path, KL_StrConst fallback);

KL_json_node_ptr_List* kl_json_node_select_array(KL_JSONNode* node, KL_StrConst path);

#endif
