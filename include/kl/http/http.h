#ifndef KL_HTTP_H
#define KL_HTTP_H


/*
  Example usage:

  ```
  KL_HTTPRequest req = {0};
  kl_http_request_init(&req, (KL_HTTPRequestConfig){
    .url = KL_STR_CONST_LIT("http://example.org/"),
    .method = KL_STR_CONST_LIT("GET"),
  });
  
  kl_http_request_set_header(&req, KL_STR_CONST_LIT("X-Custom-Header"), KL_STR_CONST_LIT("hello-from-kl"));
  
  KL_HTTPResponse resp = {0};
  
  if (!kl_http_request_fetch(&req, &resp)) {
    fprintf(stderr, "Failed to fetch.\n");
    kl_http_request_destroy(&req);
    return 1;
  }
  
  assert(resp.ok);
  printf("Request ok!\n");
  printf("%s\n", resp.text.value);
  
  
  kl_http_request_destroy(&req);
  ```
*/


#include <stdint.h>

#include <kl/string/string.h>
#include <kl/structures/kv_store.h>
#include <kl/memory/allocator.h>

typedef struct {
  KL_StrConst url;
  KL_StrConst method;
  int32_t max_redirects;
  int32_t port;
  double timeout_ms;
  KL_Allocator* allocator;
} KL_HTTPRequestConfig;

typedef struct {
  bool initialized;
  KL_HTTPRequestConfig config;
  KL_KVStore headers;
  KL_StrBuffer req_text;
  KL_StrBuffer res_text;
  KL_Allocator _allocator;
} KL_HTTPRequest;

typedef struct {
  int32_t status_code;
  bool ok;
  KL_StrConst text;
} KL_HTTPResponse;


void kl_http_request_init(KL_HTTPRequest* req, KL_HTTPRequestConfig cfg);

void kl_http_request_destroy(KL_HTTPRequest* req);

void kl_http_request_set_header(KL_HTTPRequest* req, KL_StrConst key, KL_StrConst value);

bool kl_http_request_fetch(KL_HTTPRequest* req, KL_HTTPResponse* out);


#endif
