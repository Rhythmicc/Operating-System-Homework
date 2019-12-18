#pragma once

#include <sys/time.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _cache *cache_t, _cache;
typedef struct _cache_ele cache_pair;
typedef struct _cache_ret {
    long cost;
    const char*cache;
}cache_ret;
/**
 * @API
 * create, delete, search, read
 */
cache_t   new_cache        (unsigned capacity, cache_ret(*model)(cache_t, const char*));
void      del_cache        (cache_t cache);
unsigned  cache_page_fault (cache_t cache);
cache_ret read_cache       (cache_t cache, const char*filename);

/**
 * @Cache_Algorithm_Model
 * cache_ret(*)(cache_t, const char*)
 */
cache_ret LRU (cache_t cache, const char*request);
cache_ret LFU (cache_t cache, const char*request);
cache_ret ARC (cache_t cache, const char*request);
cache_ret MQ  (cache_t cache, const char*request);
cache_ret GD  (cache_t cache, const char*request);
cache_ret GDSF(cache_t cache, const char*request);

#ifdef __cplusplus
}
#endif