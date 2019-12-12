#include "cache.h"
#include <zconf.h>
#include "stdlib.h"
#include <sys/mman.h>
#include "pthread.h"
#include "string.h"
#include <sys/time.h>
#include <fcntl.h>
#include "thpool.h"

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

struct _ele {
    char *key, *file_cache;
    long cost;
    struct timeval pre_t;
    unsigned int cnt;
    struct _ele *nxt, *pre;
};

hashpair*new_hashpair(){
    MALLOC(res, hashpair, 1);
    res->key = res->file_cache = NULL;
    res->cnt = res->cost = 0;
    res->nxt = res->pre = NULL;
    return res;
}

void del_hashpair(hashpair *del) {
    free(del->key);
    free(del->file_cache);
    free(del);
}

struct hash_set {
    hashpair table[HASHTABELSIZE], *first_table;      /// hash table
    set_ret (*f)(set_t, const char *);                /// function pointer
    pthread_mutex_t mutex;
    unsigned int capacity, _cur, first_cur;
};/// *set_t

set_t new_set(unsigned capacity, set_ret(*f)(set_t, const char *)) {
    if (f) {
        MALLOC(ret, hash_set, 1);
        pthread_mutex_init(&ret->mutex, NULL);
        ret->capacity = capacity;
        ret->first_cur = ret->_cur = 0;
        memset(ret->table, 0, sizeof(hashpair) * HASHTABELSIZE);
        if (f == ARC)ret->first_table = RMALLOC(hashpair, HASHTABELSIZE);
        else if(f == MQ)ret->first_table = RMALLOC(hashpair, HASHTABELSIZE * 3);
        else ret->first_table = NULL;
        ret->f = f;
        return ret;
    }
    return NULL;
}

set_ret read_set(set_t set, const char *filename) {
    pthread_mutex_lock(&set->mutex);
    set_ret res = set->f(set, filename);
    pthread_mutex_unlock(&set->mutex);
    return res;
}

void del_set(set_t set) {
    pthread_mutex_destroy(&set->mutex);
    for (int i = 0; i < HASHTABELSIZE; ++i) {
        hashpair *p = set->table[i].nxt;
        while (p) {
            hashpair *tmp = p;
            p = p->nxt;
            del_hashpair(tmp);
        }
    }
    if (set->first_table) {
        for (int i = 0; i < HASHTABELSIZE; ++i) {
            hashpair *p = set->first_table[i].nxt;
            while (p) {
                hashpair *tmp = p;
                p = p->nxt;
                del_hashpair(tmp);
            }
        }
        free(set->first_table);
    }
    free(set);
}

hashpair *is_in_table(hashpair *table, const char *request, int *ret) {
    unsigned int index = string_hash(request) % HASHTABELSIZE;
    hashpair *src = table + index;
    if (!src->nxt) {
        *ret = 0;
        return src;
    }
    src = src->nxt;
    while (strcmp(src->key, request)) {
        hashpair *pre = src;
        src = src->nxt;
        if (!src) { /// not in table: return pre node
            *ret = 0;
            return pre;
        }
    }
    *ret = 1;
    return src;
}

void replace_after_src(hashpair *src, const char *request) {
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

void add_after_src(hashpair *src, const char *request) {
    src->nxt = new_hashpair();
    src->nxt->pre = src;
    replace_after_src(src, request);
}

void replace_copy(hashpair *src, hashpair *aim) {
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

void add_copy(hashpair *src, hashpair *aim) {
    src->nxt = new_hashpair();
    src->nxt->pre = src;
    replace_copy(src, aim);
}

hashpair *LRU_CHOOSE(hashpair *table) {
    double mn = -1;
    hashpair *res = NULL;
    for (int i = 0; i < HASHTABELSIZE; ++i)
        if (table[i].nxt) {
            hashpair *ptr = table + i;
            while (ptr->nxt) {
                hashpair *pre = ptr;
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

hashpair *LFU_CHOOSE(hashpair *table) {
    int mn = -1;
    hashpair *res = NULL;
    for (int i = 0; i < HASHTABELSIZE; ++i)
        if (table[i].nxt) {
            hashpair *ptr = table + i;
            while (ptr->nxt) {
                hashpair *pre = ptr;
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

hashpair *GD_CHOOSE(hashpair *table) {
    double mn = -1;
    hashpair *res = NULL;
    for (int i = 0; i < HASHTABELSIZE; ++i)
        if (table[i].nxt) {
            hashpair *ptr = table + i;
            while (ptr->nxt) {
                hashpair *pre = ptr;
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

hashpair *GDSF_CHOOSE(hashpair *table) {
    double mn = -1;
    hashpair *res = NULL;
    for (int i = 0; i < HASHTABELSIZE; ++i)
        if (table[i].nxt) {
            hashpair *ptr = table + i;
            while (ptr->nxt) {
                hashpair *pre = ptr;
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

set_ret LRU(set_t set, const char *request) {
    int flag;
    hashpair *src = is_in_table(set->table, request, &flag);
    if (flag) { /// real node
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else { /// pre node
        if (set->_cur == set->capacity) { /// choose and replace
            src = LRU_CHOOSE(set->table);
            replace_after_src(src, request);
        } else { /// add node
            add_after_src(src, request);
            ++set->_cur;
        }
        src = src->nxt;
    }
    return (set_ret) {src->cost, src->file_cache};
}

set_ret LFU(set_t set, const char *request) {
    int flag;
    hashpair *src = is_in_table(set->table, request, &flag);
    if (flag) {
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        if (set->_cur == set->capacity) {
            src = LFU_CHOOSE(set->table);
            replace_after_src(src, request);
        } else {
            add_after_src(src, request);
            ++set->_cur;
        }
        src = src->nxt;
    }
    return (set_ret) {src->cost, src->file_cache};
}

set_ret ARC(set_t set, const char *request) {
    int flag;
    hashpair *first_table = set->first_table;
    hashpair *src = is_in_table(set->table, request, &flag);
    if (flag) { /// in second table
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        hashpair *first_src = is_in_table(first_table, request, &flag);
        if (flag) { /// in first table
            ++first_src->cnt;
            if (set->_cur == set->capacity) { /// choose and replace
                src = LRU_CHOOSE(set->table);
                replace_copy(src, first_src); /// copy data to nxt src and delete first_src
            } else { /// add node
                add_copy(src, first_src); /// create node and replace
                ++set->_cur;
            }
            src = src->nxt;
        } else { /// not in first table
            if (set->first_cur == set->capacity) {
                first_src = LRU_CHOOSE(first_table);
                replace_after_src(first_src, request);
            } else {
                add_after_src(first_src, request);
                ++set->first_cur;
            }
            src = first_src->nxt;
        }
    }
    return (set_ret) {src->cost, src->file_cache};
}

set_ret MQ(set_t set, const char *request) {
    int flag;
    hashpair *first_table = set->first_table;
    hashpair *src = is_in_table(set->table, request, &flag);
    if (flag) { /// in second table
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        hashpair *first_src = is_in_table(first_table, request, &flag);
        if (flag) { /// in first table
            ++first_src->cnt;
            if (set->_cur == set->capacity) { /// choose and replace
                src = LRU_CHOOSE(set->table);
                replace_copy(src, first_src); /// copy data to nxt src and delete first_src
            } else { /// add node
                add_copy(src, first_src); /// create node and replace
                ++set->_cur;
            }
            src = src->nxt;
        } else { /// not in first table
            if (set->first_cur == set->capacity) {
                first_src = LRU_CHOOSE(first_table);
                replace_after_src(first_src, request);
            } else {
                add_after_src(first_src, request);
                ++set->first_cur;
            }
            src = first_src->nxt;
        }
    }
    return (set_ret) {src->cost, src->file_cache};
}

set_ret GD(set_t set, const char *request) {
    int flag;
    hashpair *src = is_in_table(set->table, request, &flag);
    if (flag) {
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        if (set->_cur == set->capacity) {
            src = GD_CHOOSE(set->table);
            replace_after_src(src, request);
        } else {
            add_after_src(src, request);
            ++set->_cur;
        }
        src = src->nxt;
    }
    return (set_ret) {src->cost, src->file_cache};
}

set_ret GDSF(set_t set, const char *request) {
    int flag;
    hashpair *src = is_in_table(set->table, request, &flag);
    if (flag) {
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        if (set->_cur == set->capacity) {
            src = GDSF_CHOOSE(set->table);
            replace_after_src(src, request);
        } else {
            add_after_src(src, request);
            ++set->_cur;
        }
        src = src->nxt;
    }
    return (set_ret) {src->cost, src->file_cache};
}
