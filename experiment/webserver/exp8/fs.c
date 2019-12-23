#include "fs.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define RMALLOC(type,n) (type*)malloc(sizeof(type)*(n))
#define MALLOC(p,type,n) type*p = RMALLOC(type, n)
#define min(a,b) (a)<(b)?(a):(b)

unsigned int string_hash(const char *str) {
    unsigned int hash = 0;
    int i;
    for (i = 0; *str; i++) {
        if ((i & 1) == 0)hash ^= ((hash << 7) ^ (*str++) ^ (hash >> 3));
        else hash ^= (~((hash << 11) ^ (*str++) ^ (hash >> 5)));
    }
    return (hash & 0x7FFFFFFF);
}

fs_pair_t new_fs_pair(const char *filename) {
    MALLOC(ret, Hash_FS_pair, 1);
    ret->excursion = ret->len = 0;
    ret->filename = RMALLOC(char, strlen(filename));
    strcpy(ret->filename, filename);
    ret->nxt = NULL;
    return ret;
}

fs_pair_t new_empty_fs_pair() {
    MALLOC(ret, Hash_FS_pair, 1);
    ret->excursion = ret->len = 0;
    ret->filename = NULL;
    ret->nxt = NULL;
    return ret;
}

void delete_fs_pair(fs_pair_t fpt) {
    free(fpt->filename);
    free(fpt);
}

fs_pair_t fs_read(fs_t fs, const char *filename) {
    unsigned index = string_hash(filename) % FS_TABLE_SIZE;
    fs_pair_t p = fs->table + index;
    while (p->nxt) {
        p = p->nxt;
        if (!strcmp(p->filename, filename))return p;
    }
    return NULL;
}

void fs_load(fs_t fs, const char *filename, const char*as_filename) {
    unsigned index = string_hash(as_filename) % FS_TABLE_SIZE;
    fs_pair_t p = fs->table + index;
    while (p->nxt) {
        p = p->nxt;
        if (!strcmp(p->filename, as_filename))return;
    }
    p->nxt = new_fs_pair(as_filename);
    FILE *fp = fopen(filename, "r");
    unsigned len = 0;
    while (!feof(fp)) {
        fputc(fgetc(fp), fs->start_fp);
        ++len;
    }
    fclose(fp);
    printf("%s\t--%uKB\n", filename, len >> 10);
    p->nxt->excursion = fs->total;
    fflush(fs->start_fp);
    fs->total += len;
    p->nxt->len = len;
}

void fs_export(fs_t fs, const char *fs_filename) {
    FILE *fp = fopen(fs_filename, "w");
    fprintf(fp, "%llu\n", fs->total);
    fs_pair_t cur = fs->table;
    int cnt = FS_TABLE_SIZE;
    while (cnt--) {
        int line_cnt = 0;
        fs_pair_t p = cur;
        while (p->nxt) {
            p = p->nxt;
            ++line_cnt;
        }
        fprintf(fp, "%d\n", line_cnt);
        p = cur;
        while (p->nxt) {
            p = p->nxt;
            fprintf(fp, "%llu %u %s\n", p->excursion, p->len, p->filename);
        }
        ++cur;
    }
    fclose(fp);
}

fs_t fs_lead(const char *fs_filename, const char*fs_dt) {
    MALLOC(ret, Hash_FS, 1);
    FILE *fp = fopen(fs_filename, "r");
    int cnt = FS_TABLE_SIZE, line_cnt;
    fscanf(fp, "%llu", &ret->total);
    fs_pair_t cur = ret->table;
    while (cnt--) {
        fscanf(fp, "%d", &line_cnt);
        fs_pair_t p = cur;
        while (line_cnt--) {
            p->nxt = new_empty_fs_pair();
            p = p->nxt;
            p->filename = RMALLOC(char, 15);
            fscanf(fp, "%llu %u %s", &p->excursion, &p->len, p->filename);
        }
        ++cur;
    }
    ret->start_fp = fopen(fs_dt, "r");
    return ret;
}

fs_t empty_fs(const char *fs_name) {
    MALLOC(ret, Hash_FS, 1);
    memset(ret->table, 0, sizeof(ret->table));
    ret->total = 0;
    ret->start_fp = fopen(fs_name, "a");
    return ret;
}

void delete_fs(fs_t fs) {
    int cnt = FS_TABLE_SIZE;
    fs_pair_t cur = fs->table;
    while (cnt--) {
        fs_pair_t p = cur->nxt;
        while (p) {
            fs_pair_t tmp = p;
            p = p->nxt;
            delete_fs_pair(tmp);
        }
        ++cur;
    }
    fclose(fs->start_fp);
}
