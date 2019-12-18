#include "cache.h"
#include <zconf.h>
#include "stdlib.h"
#include <sys/mman.h>
#include "pthread.h"
#include "string.h"
#include <sys/time.h>
#include <fcntl.h>

#define RMALLOC(type,n) (type*)malloc(sizeof(type)*(n))
#define MALLOC(p,type,n) type*p = RMALLOC(type, n)
#define MAX_BUFFER_LEN 1024ll * 1024
#ifndef HASHTABLESZIE
#define HASHTABELSIZE 10005
#endif

unsigned int string_hash(const char *str) {
    unsigned int hash = 0;
    int i;
    for (i = 0; *str; i++) {
        if ((i & 1) == 0)hash ^= ((hash << 7) ^ (*str++) ^ (hash >> 3));
        else hash ^= (~((hash << 11) ^ (*str++) ^ (hash >> 5)));
    }
    return (hash & 0x7FFFFFFF);
}

struct _cache_ele {
    char *key, *file_cache;
    long cost;
    struct timeval pre_t;
    unsigned int cnt;
    struct _cache_ele *nxt, *pre;
};

cache_pair*new_cache_pair(){
    MALLOC(res, cache_pair, 1);
    res->key = res->file_cache = NULL;
    res->cnt = res->cost = 0;
    res->nxt = res->pre = NULL;
    return res;
}

void del_cache_pair(cache_pair *del) {
    free(del->key);
    free(del->file_cache);
    free(del);
}

struct _cache {
    cache_pair table[HASHTABELSIZE], *first_table;      /// hash table
    cache_ret (*f)(cache_t, const char *);              /// function pointer
    pthread_mutex_t mutex;
    unsigned int capacity, _cur, first_cur, page_fault;
};/// *cache_t

cache_t new_cache(unsigned capacity, cache_ret(*model)(cache_t, const char *)) {
    if (model) {
        MALLOC(ret, _cache, 1);
        pthread_mutex_init(&ret->mutex, NULL);
        ret->capacity = capacity;
        ret->page_fault = ret->first_cur = ret->_cur = 0;
        memset(ret->table, 0, sizeof(cache_pair) * HASHTABELSIZE);
        if (model == ARC)ret->first_table = RMALLOC(cache_pair, HASHTABELSIZE);
        else if(model == MQ)ret->first_table = RMALLOC(cache_pair, HASHTABELSIZE * 3);
        else ret->first_table = NULL;
        ret->f = model;
        return ret;
    }
    return NULL;
}

cache_ret read_cache(cache_t cache, const char *filename) {
    pthread_mutex_lock(&cache->mutex);
    cache_ret res = cache->f(cache, filename);
    pthread_mutex_unlock(&cache->mutex);
    return res;
}

unsigned cache_page_fault(cache_t cache){
    return cache->page_fault;
}

void del_cache(cache_t cache) {
    pthread_mutex_destroy(&cache->mutex);
    for (int i = 0; i < HASHTABELSIZE; ++i) {
        cache_pair *p = cache->table[i].nxt;
        while (p) {
            cache_pair *tmp = p;
            p = p->nxt;
            del_cache_pair(tmp);
        }
    }
    if (cache->first_table) {
        for (int i = 0; i < HASHTABELSIZE; ++i) {
            cache_pair *p = cache->first_table[i].nxt;
            while (p) {
                cache_pair *tmp = p;
                p = p->nxt;
                del_cache_pair(tmp);
            }
        }
        free(cache->first_table);
    }
    free(cache);
}

cache_pair *is_in_table(cache_pair *table, const char *request, int *ret) {
    unsigned int index = string_hash(request) % HASHTABELSIZE;
    cache_pair *src = table + index;
    if (!src->nxt) {
        *ret = 0;
        return src;
    }
    src = src->nxt;
    while (strcmp(src->key, request)) {
        cache_pair *pre = src;
        src = src->nxt;
        if (!src) { /// not in table: return pre node
            *ret = 0;
            return pre;
        }
    }
    *ret = 1;
    return src;
}

void replace_after_src(cache_pair *src, const char *request) {
    src = src->nxt;
    src->cnt = 1;
    gettimeofday(&src->pre_t, NULL);
    src->key = src->key ? (char *) realloc(src->key, strlen(request) + 1) : RMALLOC(char, strlen(request) + 1);
    strcpy(src->key, request);
    int fd = open(request, O_RDONLY);
    if (fd > 0) {
        char *fp = mmap(NULL, MAX_BUFFER_LEN, PROT_READ, MAP_SHARED, fd, 0);
        src->cost = strlen(fp) + 1;
        src->file_cache = src->file_cache ? (char *) realloc(src->file_cache, src->cost) : RMALLOC(char, src->cost);
        strcpy(src->file_cache, fp);
        munmap(fp, MAX_BUFFER_LEN);
        close(fd);
    } else {
        src->cost = -1;
        if (src->file_cache)free(src->file_cache);
        src->file_cache = NULL;
    }
}

void add_after_src(cache_pair *src, const char *request) {
    src->nxt = new_cache_pair();
    src->nxt->pre = src;
    replace_after_src(src, request);
}

void replace_copy(cache_pair *src, cache_pair *aim) {
    src = src->nxt;
    src->cnt = aim->cnt;
    gettimeofday(&src->pre_t, NULL);
    src->cost = aim->cost;
    free(src->key);
    free(src->file_cache);
    src->key = aim->key;
    src->file_cache = aim->file_cache;
    aim->pre->nxt = aim->nxt;
    free(aim);
}

void add_copy(cache_pair *src, cache_pair *aim) {
    src->nxt = new_cache_pair();
    src->nxt->pre = src;
    replace_copy(src, aim);
}

cache_pair *LRU_CHOOSE(cache_pair *table) {
    double mn = -1;
    cache_pair *res = NULL;
    for (int i = 0; i < HASHTABELSIZE; ++i)
        if (table[i].nxt) {
            cache_pair *ptr = table + i;
            while (ptr->nxt) {
                cache_pair *pre = ptr;
                ptr = ptr->nxt;
                double cur = ptr->pre_t.tv_sec * 1000.0 + ptr->pre_t.tv_usec / 1000.0;
                if (mn < 0 || cur < mn) {
                    mn = cur;
                    res = pre;
                }
            }
        }
    return res;
}

cache_pair *LFU_CHOOSE(cache_pair *table) {
    int mn = -1;
    cache_pair *res = NULL;
    for (int i = 0; i < HASHTABELSIZE; ++i)
        if (table[i].nxt) {
            cache_pair *ptr = table + i;
            while (ptr->nxt) {
                cache_pair *pre = ptr;
                ptr = ptr->nxt;
                int cur = ptr->cnt;
                if (mn < 0 || cur < mn) {
                    mn = cur;
                    res = pre;
                }
            }
        }
    return res;
}

cache_pair *GD_CHOOSE(cache_pair *table) {
    double mn = -1;
    cache_pair *res = NULL;
    for (int i = 0; i < HASHTABELSIZE; ++i)
        if (table[i].nxt) {
            cache_pair *ptr = table + i;
            while (ptr->nxt) {
                cache_pair *pre = ptr;
                ptr = ptr->nxt;
                double cur = ptr->cost + ptr->pre_t.tv_sec;
                if (mn < 0 || cur < mn) {
                    mn = cur;
                    res = pre;
                }
            }
        }
    return res;
}

cache_pair *GDSF_CHOOSE(cache_pair *table) {
    double mn = -1;
    cache_pair *res = NULL;
    for (int i = 0; i < HASHTABELSIZE; ++i)
        if (table[i].nxt) {
            cache_pair *ptr = table + i;
            while (ptr->nxt) {
                cache_pair *pre = ptr;
                ptr = ptr->nxt;
                double cur = ptr->cnt * ptr->cost + ptr->pre_t.tv_sec;
                if (mn < 0 || cur < mn) {
                    mn = cur;
                    res = pre;
                }
            }
        }
    return res;
}

cache_ret LRU(cache_t cache, const char *request) {
    int flag;
    cache_pair *src = is_in_table(cache->table, request, &flag);
    if (flag) { /// real node
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else { /// pre node
        ++cache->page_fault;
        if (cache->_cur == cache->capacity) { /// choose and replace
            src = LRU_CHOOSE(cache->table);
            replace_after_src(src, request);
        } else { /// add node
            add_after_src(src, request);
            ++cache->_cur;
        }
        src = src->nxt;
    }
    return (cache_ret) {src->cost, src->file_cache};
}

cache_ret LFU(cache_t cache, const char *request) {
    int flag;
    cache_pair *src = is_in_table(cache->table, request, &flag);
    if (flag) {
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        ++cache->page_fault;
        if (cache->_cur == cache->capacity) {
            src = LFU_CHOOSE(cache->table);
            replace_after_src(src, request);
        } else {
            add_after_src(src, request);
            ++cache->_cur;
        }
        src = src->nxt;
    }
    return (cache_ret) {src->cost, src->file_cache};
}

cache_ret ARC(cache_t cache, const char *request) {
    int flag;
    cache_pair *first_table = cache->first_table;
    cache_pair *src = is_in_table(cache->table, request, &flag);
    if (flag) { /// in second table
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        cache_pair *first_src = is_in_table(first_table, request, &flag);
        if (flag) { /// in first table
            ++first_src->cnt;
            if (cache->_cur == cache->capacity) { /// choose and replace
                src = LRU_CHOOSE(cache->table);
                replace_copy(src, first_src); /// copy data to nxt src and delete first_src
            } else { /// add node
                add_copy(src, first_src); /// create node and replace
                ++cache->_cur;
            }
            src = src->nxt;
        } else { /// not in first table
            ++cache->page_fault;
            if (cache->first_cur == cache->capacity) {
                first_src = LRU_CHOOSE(first_table);
                replace_after_src(first_src, request);
            } else {
                add_after_src(first_src, request);
                ++cache->first_cur;
            }
            src = first_src->nxt;
        }
    }
    return (cache_ret) {src->cost, src->file_cache};
}

cache_ret MQ(cache_t cache, const char *request) {
    int flag;
    cache_pair *first_table = cache->first_table;
    cache_pair *src = is_in_table(cache->table, request, &flag);
    if (flag) { /// in second table
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        cache_pair *first_src = is_in_table(first_table, request, &flag);
        if (flag) { /// in first table
            ++first_src->cnt;
            if (cache->_cur == cache->capacity) { /// choose and replace
                src = LRU_CHOOSE(cache->table);
                replace_copy(src, first_src); /// copy data to nxt src and delete first_src
            } else { /// add node
                add_copy(src, first_src); /// create node and replace
                ++cache->_cur;
            }
            src = src->nxt;
        } else { /// not in first table
            ++cache->page_fault;
            if (cache->first_cur == cache->capacity) {
                first_src = LRU_CHOOSE(first_table);
                replace_after_src(first_src, request);
            } else {
                add_after_src(first_src, request);
                ++cache->first_cur;
            }
            src = first_src->nxt;
        }
    }
    return (cache_ret) {src->cost, src->file_cache};
}

cache_ret GD(cache_t cache, const char *request) {
    int flag;
    cache_pair *src = is_in_table(cache->table, request, &flag);
    if (flag) {
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        ++cache->page_fault;
        if (cache->_cur == cache->capacity) {
            src = GD_CHOOSE(cache->table);
            replace_after_src(src, request);
        } else {
            add_after_src(src, request);
            ++cache->_cur;
        }
        src = src->nxt;
    }
    return (cache_ret) {src->cost, src->file_cache};
}

cache_ret GDSF(cache_t cache, const char *request) {
    int flag;
    cache_pair *src = is_in_table(cache->table, request, &flag);
    if (flag) {
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        ++cache->page_fault;
        if (cache->_cur == cache->capacity) {
            src = GDSF_CHOOSE(cache->table);
            replace_after_src(src, request);
        } else {
            add_after_src(src, request);
            ++cache->_cur;
        }
        src = src->nxt;
    }
    return (cache_ret) {src->cost, src->file_cache};
}
