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

typedef struct {
    set_t set;
    const char*request;
    int*fd;
}set_param;

void del_hashpair(hashpair*del){
    free(del->key);
    free(del->file_cache);
    free(del);
}

struct hash_set {
    hashpair table[1000], *first_table;      /// hash table
    void(*f)(void*);                         /// function pointer
    threadpool pool;                         /// cache thread pool
    unsigned int capacity, _cur, first_cur;
};/// *set_t

unsigned int string_hash(const char *str) {
    unsigned int hash = 0;
    int i;
    for (i = 0; *str; i++) {
        if ((i & 1) == 0)hash ^= ((hash << 7) ^ (*str++) ^ (hash >> 3));
        else hash ^= (~((hash << 11) ^ (*str++) ^ (hash >> 5)));
    }
    return (hash & 0x7FFFFFFF);
}

set_t new_set(int num_thread, unsigned capacity, void(*f)(void *)) {
    if (f) {
        MALLOC(ret, hash_set, 1);
        ret->capacity = capacity;
        ret->first_cur = ret->_cur = 0;
        memset(ret->table, 0, sizeof(hashpair) * 1000);
        if(f == ARC || f == MQ)ret->first_table = RMALLOC(hashpair, 1000);
        else ret->first_table = NULL;
        ret->f = f;
        ret->pool = thpool_init(num_thread, "[cache]");
        return ret;
    }
    return NULL;
}

hashpair *read_set(set_t set, const char *filename) {
    hashpair *ret;
    int fd[2];
    pipe(fd);
    MALLOC(p, set_param, 1);
    p->fd = fd;
    p->request = filename;
    p->set = set;
    thpool_add_work(set->pool, set->f, p);
    read(fd[0], &ret, sizeof(hashpair*));
    return ret;
}

void del_set(set_t set) {
    for (int i = 0; i < 1000; ++i) {
        hashpair *p = set->table[i].nxt;
        while (p) {
            hashpair *tmp = p;
            p = p->nxt;
            del_hashpair(tmp);
        }
    }
    if(set->first_table){
        for(int i=0;i<1000;++i){
            hashpair *p = set->first_table[i].nxt;
            while (p) {
                hashpair *tmp = p;
                p = p->nxt;
                del_hashpair(tmp);
            }
        }
        free(set->first_table);
    }
    thpool_destroy(set->pool);
    free(set);
}

hashpair *is_in_table(hashpair *table, const char *request, int *ret) {
    unsigned int index = string_hash(request) % 1000;
    hashpair *src = table + index;
    if(!src->nxt){
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
    src->nxt = RMALLOC(hashpair, 1);
    src->nxt->pre = src;
    src->nxt->nxt = NULL;
    src->nxt->file_cache = NULL;
    src->nxt->key = NULL;
    replace_after_src(src, request);
}

void replace_copy(hashpair*src, hashpair*aim){
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

void add_copy(hashpair*src, hashpair*aim){
    src->nxt = RMALLOC(hashpair, 1);
    src->nxt->pre = src;
    src->nxt->nxt = NULL;
    replace_copy(src, aim);
}

hashpair*LRU_CHOOSE(hashpair*table){
    double mn = -1;
    hashpair*ret = NULL;
    for(int i=0;i<1000;++i)
        if(table[i].nxt){
            hashpair*ptr = table + i;
            while(ptr->nxt){
                hashpair*pre = ptr;
                ptr = ptr->nxt;
                double cur = ptr->pre_t.tv_sec * 1000.0 + ptr->pre_t.tv_usec / 1000.0;
                if(mn < 0 || cur < mn){
                    mn = cur;
                    ret = pre;
                }
            }
        }
    return ret;
}

hashpair*LFU_CHOOSE(hashpair*table){
    int mn = -1;
    hashpair*ret = NULL;
    for(int i=0;i<1000;++i)
        if(table[i].nxt){
            hashpair*ptr = table + i;
            while(ptr->nxt){
                hashpair*pre = ptr;
                ptr = ptr->nxt;
                int cur = ptr->cnt;
                if(mn < 0 || cur < mn){
                    mn = cur;
                    ret = pre;
                }
            }
        }
    return ret;
}

void LRU(void *st) {
    set_param *p = (set_param *) st;
    const char *request = p->request;
    int *fd = p->fd, flag;
    set_t set = p->set;
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
    write(fd[1], &src, sizeof(hashpair *));
}

void LFU(void *st) {
    set_param *p = (set_param *) st;
    const char *request = p->request;
    int *fd = p->fd, flag;
    set_t set = p->set;
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
    write(fd[1], &src, sizeof(hashpair *));
}

void ARC(void *st) {
    set_param *p = (set_param *) st;
    const char *request = p->request;
    int *fd = p->fd, flag;
    set_t set = p->set;
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
    write(fd[1], &src, sizeof(hashpair *)); /// OUT
}

void MQ(void *st) {
    /** TODO:*/
    set_param *p = (set_param *) st;
    const char *request = p->request;
    int *fd = p->fd, flag;
    set_t set = p->set;
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
    write(fd[1], &src, sizeof(hashpair *)); /// OUT
}

void GD(void *st) {
    set_param *p = (set_param *) st;
    const char *request = p->request;
    int *fd = p->fd, flag;
    set_t set = p->set;
    hashpair *src = is_in_table(set->table, request, &flag);
    if (flag) {
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        if (set->_cur == set->capacity) {

        } else {
            add_after_src(src, request);
            ++set->_cur;
        }
        src = src->nxt;
    }
    write(fd[1], &src, sizeof(hashpair *));
}

void GDSF(void *st) {
    set_param *p = (set_param *) st;
    const char *request = p->request;
    int *fd = p->fd, flag;
    set_t set = p->set;
    hashpair *src = is_in_table(set->table, request, &flag);
    if (flag) {
        src->cnt++;
        gettimeofday(&src->pre_t, NULL);
    } else {
        if (set->_cur == set->capacity) {

        } else {
            add_after_src(src, request);
            ++set->_cur;
        }
        src = src->nxt;
    }
    write(fd[1], &src, sizeof(hashpair *));
}
