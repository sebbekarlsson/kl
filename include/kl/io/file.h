#ifndef KL_FILE_H
#define KL_FILE_H

#include <kl/string/string.h>
#include <kl/templates/list/list.h>
#include <stdbool.h>

typedef struct {
  KL_StrConst name;
  KL_StrConst parent_path;
  bool is_directory;
} KL_File;

KL_DEFINE_LIST(file, KL_File);

#endif
