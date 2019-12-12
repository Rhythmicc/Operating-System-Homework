#pragma once

#include <sys/time.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct hash_set *set_t, hash_set;
typedef struct _ele hashpair;
typedef struct _ret {
    long cost;
    const char*cache;
}set_ret;

set_t new_set(unsigned capacity, set_ret(*f)(set_t, const char*));
void del_set(set_t set);
set_ret read_set(set_t set, const char*filename);
set_ret LRU(set_t set, const char*request);
set_ret LFU(set_t set, const char*request);
set_ret ARC(set_t set, const char*request);
set_ret MQ(set_t set, const char*request);
set_ret GD(set_t set, const char*request);
set_ret GDSF(set_t set, const char*request);

#ifdef __cplusplus
}
#endif