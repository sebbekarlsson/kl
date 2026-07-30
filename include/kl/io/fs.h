#ifndef KL_FS_H
#define KL_FS_H

#include <stdbool.h>
#include <kl/string/string.h>
#include <kl/io/file.h>
#include <kl/memory/allocator.h>

bool kl_fs_read_text_file(KL_StrConst filepath, KL_Str* out);

bool kl_fs_read_text_file_into_buffer(KL_StrConst filepath, KL_StrBuffer* out);

bool kl_fs_write_text_file(KL_StrConst filepath, KL_StrConst content);

bool kl_fs_file_exists(const char* filepath);

bool kl_fs_is_directory(const char* filepath);

bool kl_fs_get_homedir(KL_StrBuffer* out);

bool kl_fs_read_directory(KL_StrConst dirpath, bool recursive, KL_Allocator* allocator, KL_file_List* out_files);

bool kl_fs_mkdir(const char* dirpath);

#endif
