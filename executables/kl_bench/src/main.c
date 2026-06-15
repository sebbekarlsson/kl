#include <kl/memory/allocator.h>
#include <kl/time/time.h>
#include <kl/json/parser.h>
#include <kl/structures/kv_store.h>
#include <kl/http/http.h>
#include <kl/io/fs.h>
#include <kl/cli/args.h>
#include <stdio.h>
#include <assert.h>


#define DO_VERIFY_LAST_OBJECT false


int main(int argc, char* argv[]) {

  KL_StrConst filepath = kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("file"), true);
  
  KL_Str content = {0};
  if (!kl_fs_read_text_file(filepath, &content)) {
    fprintf(stderr, "Failed to read %s\n", filepath.value);
  }


  KL_JSONParser parser = {0};
  kl_json_parser_init(&parser);

  double before_parse = kl_time_now_ms__monotonic();

  KL_JSONNode* node = kl_json_parser_parse(&parser, (KL_StrConst){
    .value = content.value,
    .length = content.length
  });
  assert(node != 0);

  double after_parse = kl_time_now_ms__monotonic();
  double elapsed = after_parse - before_parse;

  printf("Finished parsing in %g ms\n", elapsed);


  #if DO_VERIFY_LAST_OBJECT
  // --- verify the last object in the root array was fully parsed ---
  assert(node->type == KL_JSON_NODE_ARRAY);
  int64_t n = node->as.array.length;
  assert(n > 0);
  KL_JSONNode* last = node->as.array.items[n - 1];
  assert(last != 0);
  assert(last->type == KL_JSON_NODE_OBJECT);

  KL_JSONNode* name     = kl_json_node_object_get(last, KL_STR_CONST_LIT("name"));
  KL_JSONNode* language = kl_json_node_object_get(last, KL_STR_CONST_LIT("language"));
  KL_JSONNode* id       = kl_json_node_object_get(last, KL_STR_CONST_LIT("id"));
  KL_JSONNode* bio      = kl_json_node_object_get(last, KL_STR_CONST_LIT("bio"));
  KL_JSONNode* version  = kl_json_node_object_get(last, KL_STR_CONST_LIT("version"));

  assert(name != 0 && name->type == KL_JSON_NODE_STRING);
  assert(language != 0 && language->type == KL_JSON_NODE_STRING);
  assert(id != 0 && id->type == KL_JSON_NODE_STRING);
  assert(bio != 0 && bio->type == KL_JSON_NODE_STRING);
  assert(version != 0 && version->type == KL_JSON_NODE_FLOAT);

  KL_StrConst expected_bio = KL_STR_CONST_LIT(
    "Maecenas tempus neque ut porttitor malesuada. Phasellus massa ligula, "
    "hendrerit eget efficitur eget, tincidunt in ligula. Quisque mauris ligula, "
    "efficitur porttitor sodales ac, lacinia non ex. Maecenas quis nisi nunc.");

  assert(kl_str_const_eq(name->as.string,     KL_STR_CONST_LIT("Bhupesh Menon")));
  assert(kl_str_const_eq(language->as.string, KL_STR_CONST_LIT("Hindi")));
  assert(kl_str_const_eq(id->as.string,       KL_STR_CONST_LIT("0CEPNRDV98KT3ORP")));
  assert(kl_str_const_eq(bio->as.string,      expected_bio));
  assert(version->as.number_float > 2.689 && version->as.number_float < 2.691);

  printf("Verified last object (array length %ld): name=\"%.*s\" language=\"%.*s\" "
         "id=\"%.*s\" version=%g bio_len=%ld\n",
         n,
         (int)name->as.string.length,     name->as.string.value,
         (int)language->as.string.length, language->as.string.value,
         (int)id->as.string.length,       id->as.string.value,
         version->as.number_float,
         bio->as.string.length);

  #endif

  free(content.value);
  content.value = 0;
  
  return 0;
}
