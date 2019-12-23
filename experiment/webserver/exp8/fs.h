#ifndef _FS_H_
#define _FS_H_
#define FS_TABLE_SIZE 10005

#include <stdio.h>
typedef struct Hash_FS_pair{
    char*filename;
    unsigned len;
    unsigned long long excursion;
    struct Hash_FS_pair*nxt;
}Hash_FS_pair, *fs_pair_t;

typedef struct {
    Hash_FS_pair table[FS_TABLE_SIZE];
    FILE*start_fp;
    unsigned long long total;
}Hash_FS, *fs_t;

fs_pair_t fs_read(fs_t fs, const char*filename);
void fs_load(fs_t fs, const char*filename, const char*as_filename);
void fs_export(fs_t fs, const char*fs_filename);
fs_t fs_lead(const char*fs_filename, const char*fs_dt);
fs_t empty_fs(const char*fs_name);
void delete_fs(fs_t fs);
#endif