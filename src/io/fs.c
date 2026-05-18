#include <kl/io/fs.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

bool kl_fs_file_exists(const char* filepath) {
  if (filepath == 0) return false;
  return access(filepath, F_OK) == 0;
}

bool kl_fs_is_directory(const char* filepath) {
  if (filepath == 0) return false;
    struct stat statbuf;
  if (stat(filepath, &statbuf) != 0)
    return false;
  return S_ISDIR(statbuf.st_mode);
}

bool kl_fs_get_homedir(KL_StrBuffer* out) {
  const char* homedir = getenv("HOME");
  if (homedir != 0) return kl_str_buffer_append(out, KL_STR_CONST(homedir));

  long initial = sysconf(_SC_GETPW_R_SIZE_MAX);
  size_t bufsize = (initial > 0) ? (size_t)initial : 1024;

  char* buf = (char*)malloc(bufsize);
  assert(buf != 0);

  struct passwd pwd;
  struct passwd* result = 0;
  int err;
  while ((err = getpwuid_r(getuid(), &pwd, buf, bufsize, &result)) == ERANGE) {
    bufsize *= 2;
    char* new_buf = (char*)realloc(buf, bufsize);
    assert(new_buf != 0);
    buf = new_buf;
  }
  assert(err == 0 && result != 0);

  bool ok = kl_str_buffer_append(out, KL_STR_CONST(result->pw_dir));
  free(buf);
  return ok;
}
