# 操作系统课程设计报告六

### 连浩丞 计算机二班 2017011344

## 题目1

### 设计方案

- 实现字符串hash表数据结构
- 实现在hash表上运行的LRU、LFU、ARC、MQ、GD和GDSF页面替换算法

### 源代码

- cache.h

```c
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
```

- cache.c

  - 字符串哈希算法：

  ```c
  unsigned int string_hash(const char *str) {
      unsigned int hash = 0;
      int i;
      for (i = 0; *str; i++) {
          if ((i & 1) == 0)hash ^= ((hash << 7) ^ (*str++) ^ (hash >> 3));
          else hash ^= (~((hash << 11) ^ (*str++) ^ (hash >> 5)));
      }
      return (hash & 0x7FFFFFFF);
  }
  ```

  - 哈希表存储单元、哈希表数据结构

  ```c
  struct _cache_ele {
      char *key, *file_cache;
      long cost;
      struct timeval pre_t;
      unsigned int cnt;
      struct _cache_ele *nxt, *pre;
  };
  
  struct _cache {
      cache_pair table[HASHTABELSIZE], *first_table;      /// hash table
      cache_ret (*f)(cache_t, const char *);              /// function pointer
      pthread_mutex_t mutex;
      unsigned int capacity, _cur, first_cur, page_fault;
  };/// *cache_t
  ```

```c
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
```

### 实验过程

- NONE

### 运行结果与分析

- LRU

  ```tex
  Total requests: 47
  Total page fault:       38
  read socket:    0.05ms/time
  read web:       0.38ms/time
  post data:      26.71ms/time
  write log:      0.04ms/time
  ```

- LFU

  ```tex
  Total requests: 63
  Total page fault:       44
  read socket:    0.04ms/time
  read web:       0.06ms/time
  post data:      28.36ms/time
  write log:      0.03ms/time
  ```

- ARC

  ```text
  Total requests: 49
  Total page fault:       40
  read socket:    0.06ms/time
  read web:       0.07ms/time
  post data:      21.49ms/time
  write log:      0.04ms/time
  ```

- MQ

  ```text
  Total requests: 53
  Total page fault:       40
  read socket:    0.04ms/time
  read web:       0.06ms/time
  post data:      27.20ms/time
  write log:      0.03ms/time
  ```

- GD

  ```text
  Total requests: 36
  Total page fault:       32
  read socket:    0.06ms/time
  read web:       0.07ms/time
  post data:      48.41ms/time
  write log:      0.04ms/time
  ```

- GDSF

  ```text
  Total requests:	43
  Total page fault:	34
  read socket:	0.05ms/time
  read web:	0.06ms/time
  post data:	35.20ms/time
  write log:	0.03ms/time
  ```

## 题目2

### 设计方案

- 针对每个model进行同样压力的测试，根据测试结果分析

### 源代码

- NONE

### 实验过程

### 运行结果与分析

- 运算结果受网络状态影响，可能产生403、404等错误状态。

- LRU

  - Server:

  ```text
  Total requests: 1000
  Total page fault:       991
  read socket:    0.17ms/time
  read web:       99.57ms/time
  post data:      13.38ms/time
  write log:      0.14ms/time
  ```

  - Client:

  ```text
  http status:     {200: 1000}
  ms/time:         {200: 143.154}
  ```
  
- LFU
  - Server:

  ```text
  Total requests: 1000
  Total page fault:       994
  read socket:    0.20ms/time
  read web:       98.00ms/time
  post data:      15.89ms/time
  write log:      0.16ms/time
  ```

  - Client:

  ```text
  http status:     {200: 1000}
  ms/time:         {200: 149.437}
  ```
  
- ARC

  - Server:

  ```text
  Total requests:	1000
  Total page fault:	996
  read socket:	0.08ms/time
  read web:	4.09ms/time
  post data:	224.70ms/time
  write log:	0.03ms/time
  ```

  - Client:

  ```text
  http status:     {200: 1000}
  ms/time:         {200: 146.139}
  ```
  
- MQ

  - Server:

  ```text
  Total requests:	1000
  Total page fault:	995
  read socket:	0.05ms/time
  read web:	3.58ms/time
  post data:	254.16ms/time
  write log:	0.04ms/time
  ```

  - Client:

  ```text
  http status:     {200: 1000}
  ms/time:         {200: 147.935}
  ```
  
- GD

  - Server:

  ```text
  Total requests: 1000
  Total page fault:       994
  read socket:    0.24ms/time
  read web:       94.91ms/time
  post data:      15.64ms/time
  write log:      0.17ms/time
  ```

  - Client:

  ```text
  http status:     {200: 1000}
  ms/time:         {200: 144.034}
  ```
  
- GDSF

  - Server:

  ```text
  Total requests: 1000
  Total page fault:       990
  read socket:    0.32ms/time
  read web:       106.63ms/time
  post data:      12.33ms/time
  write log:      0.19ms/time
  ```

  - Client:

  ```text
  http status:     {200: 1000}
  ms/time:         {200: 143.102}
  ```

## 题目3

### 分析

根据实验数据，在当前压力下（20线程，进行总计1000次访问），表现最佳的是`GDSF`算法。GDSF算法的页面替换策略综合文件的大小、访问频率和在缓存中的时间长度三个因素。在均匀随机访问测试中，相比于其他单因素策略算法，GDSF引入访问次数因子能更加准确地量化页面在缓存中的分数，页面的替换策略更加优秀。

