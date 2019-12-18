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
