#include <kl/io/fs.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool kl_fs_read_text_file(KL_StrConst filepath, KL_Str* out) {
  assert(filepath.value != 0);
  assert(filepath.length > 0);
  assert(out != 0);
  
  FILE* fp = fopen(filepath.value, "r");
  if (fp == 0) return false;

  int64_t len = 0;
  fseek(fp, 0, SEEK_END);
  len = ftell(fp);
  rewind(fp);

  char* buff = (char*)calloc(len + 1, sizeof(char));
  assert(buff != 0);
  fread(&buff[0], sizeof(char), len, fp);

  fclose(fp);
  fp = 0;


  out->value = buff;
  out->length = len;
  
  return true;
}

bool kl_fs_file_exists(const char *filepath) {
  if (filepath == 0)
    return false;
  return access(filepath, F_OK) == 0;
}

bool kl_fs_is_directory(const char *filepath) {
  if (filepath == 0)
    return false;
  struct stat statbuf;
  if (stat(filepath, &statbuf) != 0)
    return false;
  return S_ISDIR(statbuf.st_mode);
}

bool kl_fs_get_homedir(KL_StrBuffer *out) {
  const char *homedir = getenv("HOME");
  if (homedir != 0)
    return kl_str_buffer_append(out, KL_STR_CONST(homedir));

  long initial = sysconf(_SC_GETPW_R_SIZE_MAX);
  size_t bufsize = (initial > 0) ? (size_t)initial : 1024;

  char *buf = (char *)malloc(bufsize);
  assert(buf != 0);

  struct passwd pwd;
  struct passwd *result = 0;
  int err;
  while ((err = getpwuid_r(getuid(), &pwd, buf, bufsize, &result)) == ERANGE) {
    bufsize *= 2;
    char *new_buf = (char *)realloc(buf, bufsize);
    assert(new_buf != 0);
    buf = new_buf;
  }
  assert(err == 0 && result != 0);

  bool ok = kl_str_buffer_append(out, KL_STR_CONST(result->pw_dir));
  free(buf);
  return ok;
}

static bool read_directory_traverse(KL_StrConst dirpath, bool recursive,
                                    KL_Allocator *allocator,
                                    KL_file_List *out_files) {
  DIR *dir = opendir(dirpath.value);
  if (dir == 0) {
    return false;
  }

  struct dirent *entry = 0;
  while ((entry = readdir(dir)) != NULL) {
    // Skip "." and ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    int64_t d_name_length = strlen(entry->d_name);
    int64_t full_path_size =
        (dirpath.length + d_name_length + 4);
    char *full_path =
        (char *)kl_allocator_malloc(allocator, full_path_size * sizeof(char));
    assert(full_path != 0);
    snprintf(full_path, full_path_size - 1, "%s/%s", dirpath.value,
             entry->d_name);

    KL_StrConst fpath = KL_STR_CONST(full_path);

    struct stat st = {0};
    if (lstat(fpath.value, &st) == -1) {
      continue;
    }

    bool is_dir = S_ISDIR(st.st_mode);

    kl_file_list_push(out_files, (KL_File){
      .is_directory = is_dir,
      .name = (KL_StrConst){
        .value = kl_allocator_strdup(allocator, entry->d_name, d_name_length),
        .length = d_name_length,
      },
      .parent_path = dirpath,
    });

    if (is_dir && recursive) {
      read_directory_traverse(fpath, recursive, allocator, out_files);
    }
  }

  closedir(dir);

  return out_files->length > 0 && out_files->items != 0;
}

bool kl_fs_read_directory(KL_StrConst dirpath, bool recursive,
                          KL_Allocator *allocator, KL_file_List *out_files) {
  if (dirpath.value == 0)
    return false;
  if (dirpath.length <= 0)
    return false;
  if (out_files == 0)
    return false;
  if (allocator == 0)
    return false;

  if (!out_files->initialized) {
    kl_file_list_init(out_files,
                      (KL_ListConfig){.capacity = 32, .allocator = allocator});
  }

  return read_directory_traverse(dirpath, recursive, allocator, out_files);
}

bool kl_fs_mkdir(const char* dirpath) {
  if (kl_fs_file_exists(dirpath)) return true;
  return mkdir(dirpath, 0755) == 0;
}
