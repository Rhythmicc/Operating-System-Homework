#pragma once

#include <sys/time.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct hash_set *set_t, hash_set;
typedef struct _ele {
    char *key, *file_cache;
    long cost;
    struct timeval pre_t;
    unsigned int cnt;
    struct _ele *nxt, *pre;
}hashpair;

set_t new_set(int num_thread, unsigned capacity, void(*f)(void *));
void del_set(set_t set);
hashpair* read_set(set_t set, const char*filename);
void LRU(void*st);
void LFU(void*st);
void ARC(void*st);
void MQ(void*st);
void GD(void*st);
void GDSF(void*st);

#ifdef __cplusplus
}
#endif