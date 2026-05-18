#ifndef KL_FS_H
#define KL_FS_H

#include <stdbool.h>
#include <kl/string/string.h>

bool kl_fs_file_exists(const char* filepath);

bool kl_fs_is_directory(const char* filepath);

bool kl_fs_get_homedir(KL_StrBuffer* out);

#endif
