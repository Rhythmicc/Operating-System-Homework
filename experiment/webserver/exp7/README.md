# 操作系统课程设计报告七

### 连浩丞 计算机二班 2017011344

## 题目1

### TCmalloc组织和管理内存结构

Tcmalloc是Google gperftools里的组件之一。全名是 thread cache malloc（线程缓存分配器）其内存管理分为**线程内存**和**中央堆**两部分。

#### 小内存分配

对于小块内存分配，其内部维护了**60个不同大小的分配器**（实际源码中看到的是86个），和ptmalloc不同的是，它的每个分配器的大小差是不同的，依此按8字节、16字节、32字节等间隔开。在内存分配的时候，会找到最小复合条件的，比如833字节到1024字节的内存分配请求都会分配一个1024大小的内存块。如果这些分配器的剩余内存不够了，会向中央堆申请一些内存，打碎以后填入对应分配器中。同样，如果中央堆也没内存了，就向中央内存分配器申请内存。

在线程缓存内的60个分配器（_文档上说60个，但是我在2.0的代码里看到得是86个_）分别维护了一个大小固定的自由空间链表，直接由这些链表分配内存的时候是不加锁的。但是中央堆是所有线程共享的，在由其分配内存的时候会加自旋锁(spin lock)。

在线程内存池每次从中央堆申请内存的时候，分配多少内存也直接影响分配性能。申请地太少会导致频繁访问中央堆，也就会频繁加锁，而申请地太多会导致内存浪费。在tcmalloc里，这个**每次申请的内存量是动态调整的**，调整方式使用了类似把tcp窗口反过来用的**慢启动（slow start）**算法调整max_length， 每次申请内存是申请max_length和每个分配器对应的num_objects_to_move中取小的值的个数的内存块。

**num_objects_to_move**取值比较简单，是**以64K为基准**，并且**最小不低于2，最大不高于32的值**。也就是说，对于大于等于32K的分配器这个值为2（好像没有这样的分配器 class），对于小于2K的分配器，统一为32。其他的会把数据调整到64K / size 的个数。（可能是经验数值，目前2.0版本里的代码是这么写的）

对于max_length就比较复杂了，而且其更多是用于释放内存。max_length由1开始，在其小于num_objects_to_move的时候每次累加1，大于等于的时候累加num_objects_to_move。释放内存的时候，首先max_length会对齐到num_objects_to_move，然后在大于num_objects_to_move的释放次数超过一定阀值，则会按num_objects_to_move缩减大小。

#### 大内存分配

对于大内存分配(大于8个分页, 即32K)，tcmalloc直接在中央堆里分配。**中央堆的内存管理是以分页为单位的**，同样按大小维护了256个空闲空间链表，前255个分别是1个分页、2个分页到255个分页的空闲空间，最后一个是更多分页的小的空间。这里的空间如果不够用，就会直接从系统申请了。

#### 分页管理 – span

Tcmalloc使用一种叫**span**的东东来管理内存分页，一个span可以包含几个连续分页。一个span的状态只有**未分配**(这时候在空闲链表中)，**作为大对象分配**，或**作为小对象分配**（这时候span内记录了小对象的class size）。

在32位系统中，span分为两级由中央分配器管理。第一级有2^5个节点，第二级是2^15个。32位总共只能有2^20个分页（每个分页4KB = 2^12）。(骗纸，我在代码里明明看到的是2^7和2^13，定义了TCMALLOC_LARGE_PAGES宏之后才是 2^5和2^15，可是所有的代码或者编辑脚本里都没定义这个宏，可能是文档没更新)

在64为系统中，有三级。

在**资源释放的时候**，首先计算其分页编号，然后再查找出它对应的span，如果它是一个小对象，则直接归入小对象分配器的空闲链表。等到空闲空间足够大以后划入中央堆。如果是大对象，则会把物理地址连续的前后的span也找出来，如果空闲则合并，并归入中央堆中。

而线程缓存内的分配器，会根据max_length、num_objects_to_move和上一次垃圾收集到现在为止的最小链表长度，按一定的策略回收资源到中央堆中，具体的算法不再复述tcmalloc的文档写得比较清楚。同样可以在需要时减少某一个线程的max_length来转移内存，但是要等到那个线程下一次执行free，触发垃圾回收之后才会真正把内存返还中央堆。

### ptmalloc缺陷

- 后分配的内存先释放,因为 ptmalloc 收缩内存是从 top chunk 开始,如果与 top chunk 相邻的 chunk 不能释放, top chunk 以下的 chunk 都无法释放。
- 多线程锁开销大， 需要避免多线程频繁分配释放。
- 内存从thread的areana中分配， 内存不能从一个arena移动到另一个arena， 就是说如果多线程使用内存不均衡，容易导致内存的浪费。 比如说线程1使用了300M内存，完成任务后glibc没有释放给操作系统，线程2开始创建了一个新的arena， 但是线程1的300M却不能用了。
- 每个chunk至少8字节的开销很大
- 不定期分配长生命周期的内存容易造成内存碎片，不利于回收。 64位系统最好分配32M以上内存，这是使用mmap的阈值。

## 题目2

### 源代码

- Memorypool.h

```c
#ifndef _Z_MEMORYPOOL_H_
#define _Z_MEMORYPOOL_H_

#define _Z_MEMORYPOOL_THREAD_
#ifdef _Z_MEMORYPOOL_THREAD_
#include <pthread.h>
#endif

#include <stdlib.h>
#include <string.h>

#define mem_size_t unsigned long long
#define KB (mem_size_t)(1 << 10)
#define MB (mem_size_t)(1 << 20)
#define GB (mem_size_t)(1 << 30)

typedef struct _mp_chunk {
    mem_size_t alloc_mem;
    struct _mp_chunk *prev, *next;
    int is_free;
} _MP_Chunk;

typedef struct _mp_mem_pool_list {
    char* start;
    unsigned int id;
    mem_size_t mem_pool_size;
    mem_size_t alloc_mem;
    mem_size_t alloc_prog_mem;
    _MP_Chunk *free_list, *alloc_list;
    struct _mp_mem_pool_list* next;
} _MP_Memory;

typedef struct _mp_mem_pool {
    unsigned int last_id;
    int auto_extend;
    mem_size_t mem_pool_size, total_mem_pool_size, max_mem_pool_size;
    struct _mp_mem_pool_list* mlist;
#ifdef _Z_MEMORYPOOL_THREAD_
    pthread_mutex_t lock;
#endif
} MemoryPool;

/*
 *  内存池API
 */
MemoryPool* MemoryPoolInit(mem_size_t maxmempoolsize, mem_size_t mempoolsize);
void* MemoryPoolAlloc(MemoryPool* mp, mem_size_t want_size);
int MemoryPoolFree(MemoryPool* mp, void* p);
MemoryPool* MemoryPoolClear(MemoryPool* mp);
int MemoryPoolDestroy(MemoryPool* mp);
int MemoryPoolSetThreadSafe(MemoryPool* mp, int thread_safe);

/*
 *  内存池信息API
 */
mem_size_t GetTotalMemory(MemoryPool* mp);
mem_size_t GetUsedMemory(MemoryPool* mp);
float MemoryPoolGetUsage(MemoryPool* mp);
mem_size_t GetProgMemory(MemoryPool* mp);
float MemoryPoolGetProgUsage(MemoryPool* mp);

#endif  // !_Z_MEMORYPOOL_H_

```

- Memorypool.c

```c
#include "memorypool.h"

#define MP_CHUNKHEADER sizeof(struct _mp_chunk)
#define MP_CHUNKEND sizeof(struct _mp_chunk*)


#define MP_ALIGN_SIZE(_n) (_n + sizeof(long) - ((sizeof(long) - 1) & _n))

#define MP_INIT_MEMORY_STRUCT(mm, mem_sz)       \
    do {                                        \
        mm->mem_pool_size = mem_sz;             \
        mm->alloc_mem = 0;                      \
        mm->alloc_prog_mem = 0;                 \
        mm->free_list = (_MP_Chunk*) mm->start; \
        mm->free_list->is_free = 1;             \
        mm->free_list->alloc_mem = mem_sz;      \
        mm->free_list->prev = NULL;             \
        mm->free_list->next = NULL;             \
        mm->alloc_list = NULL;                  \
    } while (0)

#define MP_DLINKLIST_INS_FRT(head, x) \
    do {                              \
        x->prev = NULL;               \
        x->next = head;               \
        if (head) head->prev = x;     \
        head = x;                     \
    } while (0)

#define MP_DLINKLIST_DEL(head, x)                 \
    do {                                          \
        if (!x->prev) {                           \
            head = x->next;                       \
            if (x->next) x->next->prev = NULL;    \
        } else {                                  \
            x->prev->next = x->next;              \
            if (x->next) x->next->prev = x->prev; \
        }                                         \
    } while (0)

static _MP_Memory *extend_memory_list(MemoryPool *mp, mem_size_t new_mem_sz) {
    char *s = (char *) malloc(sizeof(_MP_Memory) + new_mem_sz * sizeof(char));
    if (!s) return NULL;
    _MP_Memory *mm = (_MP_Memory *) s;
    mm->start = s + sizeof(_MP_Memory);
    MP_INIT_MEMORY_STRUCT(mm, new_mem_sz);
    mm->id = mp->last_id++;
    mm->next = mp->mlist;
    mp->mlist = mm;
    return mm;
}

static _MP_Memory *find_memory_list(MemoryPool *mp, void *p) {
    _MP_Memory *tmp = mp->mlist;
    while (tmp) {
        if (tmp->start <= (char *) p &&
            tmp->start + mp->mem_pool_size > (char *) p)
            break;
        tmp = tmp->next;
    }
    return tmp;
}

static int merge_free_chunk(MemoryPool *mp, _MP_Memory *mm, _MP_Chunk *c) {
    _MP_Chunk *p0 = c, *p1 = c;
    while (p0->is_free) {
        p1 = p0;
        if ((char *) p0 - MP_CHUNKEND - MP_CHUNKHEADER <= mm->start) break;
        p0 = *(_MP_Chunk **) ((char *) p0 - MP_CHUNKEND);
    }
    p0 = (_MP_Chunk *) ((char *) p1 + p1->alloc_mem);
    while ((char *) p0 < mm->start + mp->mem_pool_size && p0->is_free) {
        MP_DLINKLIST_DEL(mm->free_list, p0);
        p1->alloc_mem += p0->alloc_mem;
        p0 = (_MP_Chunk *) ((char *) p0 + p0->alloc_mem);
    }
    *(_MP_Chunk **) ((char *) p1 + p1->alloc_mem - MP_CHUNKEND) = p1;
    pthread_mutex_unlock(&mp->lock);
    return 0;
}

MemoryPool *MemoryPoolInit(mem_size_t maxmempoolsize, mem_size_t mempoolsize) {
    if (mempoolsize > maxmempoolsize)return NULL;
    MemoryPool *mp = (MemoryPool *) malloc(sizeof(MemoryPool));
    if (!mp) return NULL;
    mp->last_id = 0;
    if (mempoolsize < maxmempoolsize) mp->auto_extend = 1;
    mp->max_mem_pool_size = maxmempoolsize;
    mp->total_mem_pool_size = mp->mem_pool_size = mempoolsize;
    pthread_mutex_init(&mp->lock, NULL);
    char *s = (char *) malloc(sizeof(_MP_Memory) + sizeof(char) * mp->mem_pool_size);
    if (!s) return NULL;
    mp->mlist = (_MP_Memory *) s;
    mp->mlist->start = s + sizeof(_MP_Memory);
    MP_INIT_MEMORY_STRUCT(mp->mlist, mp->mem_pool_size);
    mp->mlist->next = NULL;
    mp->mlist->id = mp->last_id++;
    return mp;
}

void *MemoryPoolAlloc(MemoryPool *mp, mem_size_t wantsize) {
    if (wantsize <= 0) return NULL;
    mem_size_t total_needed_size = MP_ALIGN_SIZE(wantsize + MP_CHUNKHEADER + MP_CHUNKEND);
    if (total_needed_size > mp->mem_pool_size) return NULL;
    _MP_Memory *mm = NULL, *mm1 = NULL;
    _MP_Chunk *_free = NULL, *_not_free = NULL;
    pthread_mutex_lock(&mp->lock);
    FIND_FREE_CHUNK:
    mm = mp->mlist;
    while (mm) {
        if (mp->mem_pool_size - mm->alloc_mem < total_needed_size) {
            mm = mm->next;
            continue;
        }
        _free = mm->free_list;
        while (_free) {
            if (_free->alloc_mem >= total_needed_size) {
                if (_free->alloc_mem - total_needed_size >
                    MP_CHUNKHEADER + MP_CHUNKEND) {
                    _not_free = _free;
                    _free = (_MP_Chunk *) ((char *) _not_free + total_needed_size);
                    *_free = *_not_free;
                    _free->alloc_mem -= total_needed_size;
                    *(_MP_Chunk **) ((char *) _free + _free->alloc_mem - MP_CHUNKEND) = _free;
                    if (!_free->prev) mm->free_list = _free;
                    else _free->prev->next = _free;
                    if (_free->next) _free->next->prev = _free;
                    _not_free->is_free = 0;
                    _not_free->alloc_mem = total_needed_size;

                    *(_MP_Chunk **) ((char *) _not_free + total_needed_size - MP_CHUNKEND) = _not_free;
                } else {
                    _not_free = _free;
                    MP_DLINKLIST_DEL(mm->free_list, _not_free);
                    _not_free->is_free = 0;
                }
                MP_DLINKLIST_INS_FRT(mm->alloc_list, _not_free);
                mm->alloc_mem += _not_free->alloc_mem;
                mm->alloc_prog_mem += (_not_free->alloc_mem - MP_CHUNKHEADER - MP_CHUNKEND);
                pthread_mutex_unlock(&mp->lock);
                return (void *) ((char *) _not_free + MP_CHUNKHEADER);
            }
            _free = _free->next;
        }
        mm = mm->next;
    }
    if (mp->auto_extend) {
        if (mp->total_mem_pool_size >= mp->max_mem_pool_size) {
            pthread_mutex_unlock(&mp->lock);
            return NULL;
        }
        mem_size_t add_mem_sz = mp->max_mem_pool_size - mp->mem_pool_size;
        add_mem_sz = add_mem_sz >= mp->mem_pool_size ? mp->mem_pool_size : add_mem_sz;
        mm1 = extend_memory_list(mp, add_mem_sz);
        if (!mm1) {
            pthread_mutex_unlock(&mp->lock);
            return NULL;
        }
        mp->total_mem_pool_size += add_mem_sz;
        goto FIND_FREE_CHUNK;
    }
    pthread_mutex_unlock(&mp->lock);
    return NULL;
}

int MemoryPoolFree(MemoryPool *mp, void *p) {
    if (p == NULL || mp == NULL) return 1;
    pthread_mutex_lock(&mp->lock);
    _MP_Memory *mm = mp->mlist;
    if (mp->auto_extend) mm = find_memory_list(mp, p);
    _MP_Chunk *ck = (_MP_Chunk *) ((char *) p - MP_CHUNKHEADER);
    MP_DLINKLIST_DEL(mm->alloc_list, ck);
    MP_DLINKLIST_INS_FRT(mm->free_list, ck);
    ck->is_free = 1;
    mm->alloc_mem -= ck->alloc_mem;
    mm->alloc_prog_mem -= (ck->alloc_mem - MP_CHUNKHEADER - MP_CHUNKEND);
    return merge_free_chunk(mp, mm, ck);
}

MemoryPool *MemoryPoolClear(MemoryPool *mp) {
    if (!mp) return NULL;
    pthread_mutex_lock(&mp->lock);
    _MP_Memory *mm = mp->mlist;
    while (mm) {
        MP_INIT_MEMORY_STRUCT(mm, mm->mem_pool_size);
        mm = mm->next;
    }
    pthread_mutex_unlock(&mp->lock);
    return mp;
}

int MemoryPoolDestroy(MemoryPool *mp) {
    if (mp == NULL) return 1;
    pthread_mutex_lock(&mp->lock);
    _MP_Memory *mm = mp->mlist, *mm1 = NULL;
    while (mm) {
        mm1 = mm;
        mm = mm->next;
        free(mm1);
    }
    pthread_mutex_unlock(&mp->lock);
    pthread_mutex_destroy(&mp->lock);
    free(mp);
    return 0;
}

mem_size_t GetTotalMemory(MemoryPool *mp) {
    return mp->total_mem_pool_size;
}

mem_size_t GetUsedMemory(MemoryPool *mp) {
    pthread_mutex_lock(&mp->lock);
    mem_size_t total_alloc = 0;
    _MP_Memory *mm = mp->mlist;
    while (mm) {
        total_alloc += mm->alloc_mem;
        mm = mm->next;
    }
    pthread_mutex_unlock(&mp->lock);
    return total_alloc;
}

mem_size_t GetProgMemory(MemoryPool *mp) {
    pthread_mutex_lock(&mp->lock);
    mem_size_t total_alloc_prog = 0;
    _MP_Memory *mm = mp->mlist;
    while (mm) {
        total_alloc_prog += mm->alloc_prog_mem;
        mm = mm->next;
    }
    pthread_mutex_unlock(&mp->lock);
    return total_alloc_prog;
}

float MemoryPoolGetUsage(MemoryPool *mp) {
    return (float) GetUsedMemory(mp) / GetTotalMemory(mp);
}

float MemoryPoolGetProgUsage(MemoryPool *mp) {
    return (float) GetProgMemory(mp) / GetTotalMemory(mp);
}


#undef MP_CHUNKHEADER
#undef MP_CHUNKEND
#undef MP_ALIGN_SIZE
#undef MP_INIT_MEMORY_STRUCT
#undef MP_DLINKLIST_INS_FRT
#undef MP_DLINKLIST_DEL
```

## 题目3

### 设计方案

- 分别使用在内存池和不使用内存池情况下进行测试

### 源代码

- Mptest1.c

```c
#include "memorypool.h"
#include <sys/time.h>
#include <stdio.h>

double time_val(struct timeval t1, struct timeval t2){
    return (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
}
char* ls[(int)3e5];
int main() {
    MemoryPool*pool = MemoryPoolInit(500*MB, 100*MB);
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    for(int i=0;i<300000;++i)ls[i] = (char*)MemoryPoolAlloc(pool, sizeof(char)*(i%1000));
    gettimeofday(&t2, NULL);
    printf("Memory pool alloc:\t%.3fms\n", time_val(t1, t2));
    gettimeofday(&t1, NULL);
    for(int i=0;i<300000;++i)MemoryPoolFree(pool, ls[i]);
    gettimeofday(&t2, NULL);
    printf("Memory pool free:\t%.3fms\n", time_val(t1, t2));
    MemoryPoolClear(pool);
    MemoryPoolDestroy(pool);
    return 0;
}
```

- Mptest2.c

```c
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

double time_val(struct timeval t1, struct timeval t2){
    return (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
}
char* ls[(int)3e5];
int main() {
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    for(int i=0;i<300000;++i)ls[i] = (char*)malloc(sizeof(char)*(i%1000));
    gettimeofday(&t2, NULL);
    printf("Malloc:\t%.3fms\n", time_val(t1, t2));
    gettimeofday(&t1, NULL);
    for(int i=0;i<300000;++i)free(ls[i]);
    gettimeofday(&t2, NULL);
    printf("Free:\t%.3fms\n", time_val(t1, t2));
    return 0;
}
```

### 实验过程

- 分别对两份代码编译运行，得到时间对比

### 运行结果与分析

- mptest1

```text
Memory pool alloc:      65.027ms
Memory pool free:       32.972ms
```

- mptest2

```text
Malloc: 21.909ms
Free:   116.667ms
```

## 题目4

### 设计方案

### 源代码

### 实验过程

### 运行结果与分析



## 题目5

### 设计方案

### 源代码

### 实验过程

### 运行结果与分析