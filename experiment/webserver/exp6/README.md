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
  struct _ele {
      char *key, *file_cache;
      long cost;
      struct timeval pre_t;
      unsigned int cnt;
      struct _ele *nxt, *pre;
  };
  
  struct hash_set {
      hashpair table[HASHTABELSIZE], *first_table;      /// hash table
      set_ret (*f)(set_t, const char *);                /// function pointer
      pthread_mutex_t mutex;
      unsigned int capacity, _cur, first_cur;
  };/// *set_t
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
  Total requests:	1000
  Total page fault:	994
  read socket:	0.05ms/time
  read web:	3.89ms/time
  post data:	222.09ms/time
  write log:	0.03ms/tim
  ```

  - Client:

  ```text
  http status:	 {200: 1000}
  ms/time:	 {200: 0.706}
  Total use time: 706.1137 ms
  ```

- LFU
  - Server:

  ```text
  Total requests:	1000
  Total page fault:	995
  read socket:	0.05ms/time
  read web:	4.55ms/time
  post data:	203.66ms/time
  write log:	0.03ms/time
  ```

  - Client:

  ```text
  http status:	 {200: 1000}
  ms/time:	 {200: 0.628}
  Total use time: 628.0187 ms
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
  http status:	 {200: 1000}
  ms/time:	 {200: 0.595}
  Total use time: 594.5521 ms
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
  http status:	 {200: 1000}
  ms/time:	 {200: 0.731}
  Total use time: 731.1264 ms
  ```

- GD

  - Server:

  ```text
  Total requests:	1000
  Total page fault:	996
  read socket:	0.05ms/time
  read web:	2.32ms/time
  post data:	275.70ms/time
  write log:	0.03ms/time
  ```

  - Client:

  ```text
  http status:	 {200: 1000}
  ms/time:	 {200: 0.84}
  Total use time: 839.9940 ms
  ```

- GDSF

  - Server:

  ```text
  Total requests:	1000
  Total page fault:	995
  read socket:	0.05ms/time
  read web:	11.06ms/time
  post data:	129.04ms/time
  write log:	0.03ms/time
  ```

  - Client:

  ```text
  http status:	 {200: 1000}
  ms/time:	 {200: 0.443}
  Total use time: 442.4360 ms
  ```

## 题目3

### 分析

根据实验数据，在当前压力下（20线程，进行总计1000次访问），表现最佳的是`GDSF`算法。GDSF算法的页面替换策略综合文件的大小、访问频率和在缓存中的时间长度三个因素。在均匀随机访问测试中，相比于其他单因素策略算法，GDSF引入访问次数因子能更加准确地量化页面在缓存中的分数，页面的替换策略更加优秀。

