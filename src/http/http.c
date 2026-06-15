#include <kl/http/http.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

void kl_http_request_init(KL_HTTPRequest* req, KL_HTTPRequestConfig cfg) {
  if (req->initialized) return;
  assert(cfg.url.value != 0);
  assert(cfg.url.length > 0);
  assert(cfg.method.value != 0);
  assert(cfg.method.length > 0);

  if (cfg.allocator == 0) {
    cfg.allocator = &req->_allocator;
  }

  KL_Allocator* allocator = cfg.allocator;
  assert(allocator != 0);

  if (!allocator->initialized) {
    kl_allocator_init(allocator, (KL_AllocatorConfig){
      .capacity = 1024
    });
  }

  if (cfg.timeout_ms <= 0.00000000000000001) cfg.timeout_ms = 10000.0;
  if (cfg.max_redirects <= 0) cfg.max_redirects = 10;

  req->config = cfg;

  kl_kv_store_init(&req->headers, (KL_KVStoreConfig){
    .capacity = 16,
    .allocator = allocator
  });

  req->req_text.allocator = allocator;
  req->res_text.allocator = allocator;
  
  req->initialized = true;
}

void kl_http_request_destroy(KL_HTTPRequest* req) {
  assert(req->initialized);

  kl_kv_store_destroy(&req->headers);

  if (req->_allocator.initialized) {
    kl_allocator_destroy(&req->_allocator);
  }

  memset(req, 0, sizeof(KL_HTTPRequest));
}

void kl_http_request_set_header(KL_HTTPRequest* req, KL_StrConst key, KL_StrConst value) {
  assert(req->initialized);
  assert(key.value != 0);
  assert(value.value != 0);

  kl_kv_store_set(&req->headers, key, (KL_KVNode){
    .type = KL_KV_NODE_STRING,
    .as.string = value
  });
}

typedef struct {
  KL_StrConst host;   /* bare hostname, e.g. "example.org" (no scheme/port/path) */
  KL_StrConst path;   /* request-target, e.g. "/" or "/foo?bar" */
  int port;           /* resolved port (explicit, or scheme default) */
  bool is_https;
} KL_ParsedURL;

static KL_ParsedURL parse_url(KL_StrConst url) {
  KL_ParsedURL p = {
    .path = KL_STR_CONST_LIT("/"),
    .port = 0,
    .is_https = false,
  };

  KL_StrConst rest = url;

  /* strip "scheme://" */
  KL_StrConst scheme_sep = KL_STR_CONST_LIT("://");
  int64_t scheme_idx = kl_str_const_first_index_of(rest, scheme_sep);
  if (scheme_idx >= 0) {
    KL_StrConst scheme = kl_str_const_slice(rest, 0, scheme_idx);
    p.is_https = kl_str_const_eq(scheme, KL_STR_CONST_LIT("https"));
    int64_t after = scheme_idx + scheme_sep.length;
    rest = kl_str_const_slice(rest, after, rest.length - after);
  }

  /* split authority ("host[:port]") from path ("/...") */
  KL_StrConst authority = rest;
  int64_t slash = kl_str_const_first_index_of(rest, KL_STR_CONST_LIT("/"));
  if (slash >= 0) {
    authority = kl_str_const_slice(rest, 0, slash);
    p.path = kl_str_const_slice(rest, slash, rest.length - slash);
  }

  /* split optional ":port" off the authority */
  p.host = authority;
  int64_t colon = kl_str_const_first_index_of(authority, KL_STR_CONST_LIT(":"));
  if (colon >= 0) {
    p.host = kl_str_const_slice(authority, 0, colon);
    KL_StrConst port_str = kl_str_const_slice(authority, colon + 1, authority.length - colon - 1);
    char buf[16] = {0};
    int64_t n = port_str.length < (int64_t)sizeof(buf) - 1 ? port_str.length : (int64_t)sizeof(buf) - 1;
    if (n > 0) memcpy(buf, port_str.value, (size_t)n);
    p.port = atoi(buf);
  }

  if (p.port == 0) p.port = p.is_https ? 443 : 80;

  return p;
}

static struct timeval ms_to_timeval(double ms) {
  if (ms < 0) ms = 0;
  struct timeval tv;
  tv.tv_sec = (time_t)(ms / 1000.0);
  tv.tv_usec = (suseconds_t)((ms - (double)tv.tv_sec * 1000.0) * 1000.0);
  return tv;
}

/* connect() that gives up after timeout_ms (<= 0 means block indefinitely).
 * Uses a non-blocking connect + poll, since a blocking connect ignores any
 * socket-level timeout. Restores blocking mode on success. */
static int connect_with_timeout(int fd, const struct sockaddr* addr,
                                socklen_t addrlen, double timeout_ms) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;

  int rc = connect(fd, addr, addrlen);
  if (rc != 0 && errno == EINPROGRESS) {
    struct pollfd pfd = { .fd = fd, .events = POLLOUT };
    int timeout = timeout_ms > 0 ? (int)timeout_ms : -1;
    int pr = poll(&pfd, 1, timeout);
    if (pr <= 0) return -1; /* timed out (0) or poll error (-1) */

    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1 || err != 0) return -1;
  } else if (rc != 0) {
    return -1; /* immediate failure */
  }

  /* restore the original (blocking) mode for the send/recv phase */
  if (fcntl(fd, F_SETFL, flags) == -1) return -1;
  return 0;
}

static int send_all(int fd, const char *buf, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send(fd, buf + sent, len - sent, 0);
    if (n <= 0) return -1;
    sent += (size_t)n;
  }
  return 0;
}

/* case-insensitive substring search */
static bool ci_includes(KL_StrConst hay, KL_StrConst needle) {
  if (needle.length <= 0 || hay.length < needle.length) return false;
  for (int64_t i = 0; i + needle.length <= hay.length; i++) {
    int64_t j = 0;
    for (; j < needle.length; j++) {
      if (tolower((unsigned char)hay.value[i + j]) != tolower((unsigned char)needle.value[j])) break;
    }
    if (j == needle.length) return true;
  }
  return false;
}

/* read the Content-Length header value, or -1 if absent/unparseable */
static int64_t parse_content_length(KL_StrConst headers) {
  KL_StrConst key = KL_STR_CONST_LIT("content-length:");

  int64_t idx = -1;
  for (int64_t i = 0; i + key.length <= headers.length; i++) {
    int64_t j = 0;
    for (; j < key.length; j++) {
      if (tolower((unsigned char)headers.value[i + j]) != tolower((unsigned char)key.value[j])) break;
    }
    if (j == key.length) { idx = i + key.length; break; }
  }
  if (idx < 0) return -1;

  while (idx < headers.length && (headers.value[idx] == ' ' || headers.value[idx] == '\t')) idx++;

  int64_t val = 0;
  int digits = 0;
  while (idx < headers.length && headers.value[idx] >= '0' && headers.value[idx] <= '9') {
    val = val * 10 + (headers.value[idx] - '0');
    idx++;
    digits++;
  }
  return digits > 0 ? val : -1;
}

/* parse the numeric status code out of a status line like "HTTP/1.1 200 OK" */
static int32_t parse_status_code(KL_StrConst headers) {
  int64_t sp = kl_str_const_first_index_of(headers, KL_STR_CONST_LIT(" "));
  if (sp < 0) return -1;

  int32_t code = 0;
  int digits = 0;
  for (int64_t i = sp + 1; i < headers.length && digits < 3; i++) {
    char c = headers.value[i];
    if (c < '0' || c > '9') break;
    code = code * 10 + (c - '0');
    digits++;
  }
  return digits == 3 ? code : -1;
}

/* decode a chunked-transfer body in place; returns the decoded length.
 * read position is always ahead of write position, so compacting to the
 * front of the same buffer is safe. */
static int64_t decode_chunked(char* body, int64_t len) {
  int64_t r = 0, w = 0;
  while (r < len) {
    /* read the hex chunk-size line (stop at CR or a ';' chunk-extension) */
    int64_t size = 0;
    while (r < len && body[r] != '\r') {
      char c = body[r];
      int d;
      if (c >= '0' && c <= '9') d = c - '0';
      else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
      else break; /* extension or stray byte: skip to end of line */
      size = size * 16 + d;
      r++;
    }
    /* advance past the rest of the size line and its trailing CRLF */
    while (r < len && body[r] != '\n') r++;
    if (r < len) r++; /* skip '\n' */

    if (size == 0) break; /* terminating chunk */
    if (r + size > len) size = len - r; /* guard against truncation */

    memmove(body + w, body + r, (size_t)size);
    w += size;
    r += size;

    /* skip the CRLF that follows the chunk data */
    if (r < len && body[r] == '\r') r++;
    if (r < len && body[r] == '\n') r++;
  }
  return w;
}

bool kl_http_request_fetch(KL_HTTPRequest* req, KL_HTTPResponse* out) {
  assert(req->initialized);
  out->ok = false;
  out->status_code = -1;

  struct addrinfo hints, *res, *rp;
  int sockfd = -1;
 
  memset(&hints, 0, sizeof hints);
  hints.ai_family   = AF_UNSPEC;     /* IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM;   /* TCP */


  KL_ParsedURL url = parse_url(req->config.url);

  /* config.port (if explicitly set) overrides the scheme/URL default */
  int resolved_port = req->config.port > 0 ? req->config.port : url.port;

  if (url.is_https) {
    fprintf(stderr,
      "warning: https requested but TLS is not supported; "
      "attempting a plaintext connection on port %d\n", resolved_port);
  }

  /* getaddrinfo needs a NUL-terminated bare host, not the full URL */
  char host[256] = {0};
  if (url.host.length <= 0 || url.host.length >= (int64_t)sizeof(host)) {
    fprintf(stderr, "invalid host in url\n");
    return false;
  }
  memcpy(host, url.host.value, (size_t)url.host.length);

  char port[16] = {0};
  snprintf(port, sizeof(port), "%d", resolved_port);

  int gai = getaddrinfo(host, port, &hints, &res);
  if (gai != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
    return false;
  }

  double timeout_ms = req->config.timeout_ms;

  for (rp = res; rp != NULL; rp = rp->ai_next) {
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1) continue;
    if (connect_with_timeout(sockfd, rp->ai_addr, rp->ai_addrlen, timeout_ms) == 0) break;
    close(sockfd);
    sockfd = -1;
  }
  freeaddrinfo(res);

  if (sockfd == -1) {
    fprintf(stderr, "could not connect to %s:%s\n", host, port);
    return false;
  }

  /* bound the send/recv phase by the same timeout */
  struct timeval tv = ms_to_timeval(timeout_ms);
  if (timeout_ms > 0) {
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  }

  KL_StrBuffer* req_text = &req->req_text;
  kl_str_buffer_clear(req_text);

  kl_str_buffer_append(req_text, req->config.method);
  kl_str_buffer_append(req_text, KL_STR_CONST_LIT(" "));
  kl_str_buffer_append(req_text, url.path);
  kl_str_buffer_append(req_text, KL_STR_CONST_LIT(" HTTP/1.1"));
  kl_str_buffer_append(req_text, KL_STR_CONST_LIT("\r\n"));

  kl_str_buffer_append(req_text, KL_STR_CONST_LIT("Host: "));
  kl_str_buffer_append(req_text, url.host);
  kl_str_buffer_append(req_text, KL_STR_CONST_LIT("\r\n"));

  kl_str_buffer_append(req_text, KL_STR_CONST_LIT("Accept: */*"));
  kl_str_buffer_append(req_text, KL_STR_CONST_LIT("\r\n"));

  kl_str_buffer_append(req_text, KL_STR_CONST_LIT("Connection: close"));
  kl_str_buffer_append(req_text, KL_STR_CONST_LIT("\r\n"));

  /* emit any headers the call-site added via kl_http_request_set_header */
  KL_kv_node_Hashmap* req_headers = &req->headers.nodes;
  for (int64_t i = 0; i < req_headers->used_slots.length; i++) {
    int64_t slot_index = req_headers->used_slots.items[i];
    KL_kv_node_HashmapSlot* slot = &req_headers->slots.items[slot_index];
    if (!slot->is_set) continue;
    if (slot->value.type != KL_KV_NODE_STRING) continue;

    KL_StrConst key = { .value = (const char*)slot->key.value, .length = slot->key.size };

    kl_str_buffer_append(req_text, key);
    kl_str_buffer_append(req_text, KL_STR_CONST_LIT(": "));
    kl_str_buffer_append(req_text, slot->value.as.string);
    kl_str_buffer_append(req_text, KL_STR_CONST_LIT("\r\n"));
  }

  /* blank line terminates the header section */
  kl_str_buffer_append(req_text, KL_STR_CONST_LIT("\r\n"));

  if (send_all(sockfd, req_text->data, (size_t)req_text->length) != 0) {
    close(sockfd);
    return false;
  }


  KL_StrBuffer* res_text = &req->res_text;
  kl_str_buffer_clear(res_text);

  ssize_t n = -1;
  char chunk[4096] = {0};
  while ((n = recv(sockfd, chunk, sizeof(chunk), 0)) > 0) {
    kl_str_buffer_append(res_text, (KL_StrConst){ .value = chunk, .length = (int64_t)n });
  }

  if (n < 0) {
    close(sockfd);
    return false;
  }

  close(sockfd);

  /* split the status line + headers from the body at the blank line */
  KL_StrConst raw = { .value = res_text->data, .length = res_text->length };
  KL_StrConst sep = KL_STR_CONST_LIT("\r\n\r\n");
  int64_t hdr_end = kl_str_const_first_index_of(raw, sep);

  KL_StrConst headers = hdr_end >= 0 ? kl_str_const_slice(raw, 0, hdr_end) : raw;
  out->status_code = parse_status_code(headers);

  int64_t body_start = hdr_end >= 0 ? hdr_end + sep.length : raw.length;
  char* body = res_text->data + body_start;
  int64_t body_len = res_text->length - body_start;

  if (ci_includes(headers, KL_STR_CONST_LIT("Transfer-Encoding")) &&
      ci_includes(headers, KL_STR_CONST_LIT("chunked"))) {
    body_len = decode_chunked(body, body_len);
  } else {
    /* trust Content-Length when present, but never read past what arrived */
    int64_t content_length = parse_content_length(headers);
    if (content_length >= 0 && content_length < body_len) {
      body_len = content_length;
    }
  }

  body[body_len] = '\0'; /* NUL-terminate so callers can use it as a C string */

  out->text = (KL_StrConst){ .value = body, .length = body_len };
  out->ok = true;

  return true;
}
