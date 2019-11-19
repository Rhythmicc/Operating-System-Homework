## 三种页面置换算法

### 数据结构

```c
int sz, page_num; /// 内存大小、页面数
int *analog_mem; /// 模拟内存，sz * page_num
int *page_data; /// 页面数据
```

### 前置说明

首先要生成适用于算法运算的数据，这里我设置所有的访问页面数为1~10之间的随机数，内存大小要严格小于10。在程序运行起来时，先请求输入内存大小和页面数。然后生成随机数据并输出，再进行三种算法的模拟。

- 相关算法:

```c
void generate_data(){ /// 根据输入配置数据
    analog_mem = RMALLOC(int, sz * page_num);
    page_data = RMALLOC(int, page_num);
    int *p = page_data;
    for(int i=0;i<page_num;++i)*(p++) = rand()%10+1;
    puts("Auto generate data:");
    for(int i=0;i<page_num;++i)printf("%d%c",page_data[i], i==page_num-1?'\n':' ');
}

int is_in_mem(int*cur_mem, int id){ /// 判断id是否在内存中，如果在返回one base坐标，不在返回0
    for(int i=0;i<sz;++i)if(cur_mem[i] == id)return i+1;
    return 0;
}
```

- 运行：`run -br`

### FIFO

- 顾名思义，FIFO算法就是在有页面冲突时，淘汰最先进入内存的页面。

```c
void FIFO() {
    memset(analog_mem, 0, sizeof(int)*sz*page_num); /// 清空模拟内存
    int row = 0, cnt = 0; /// 当前要替换的行、总值。
    int*cur_mem = analog_mem, *pre_mem = cur_mem; /// 当前内存和上次的内存
    for (int i = 0; i < page_num; ++i) {
        memcpy(cur_mem, pre_mem, sizeof(int)*sz);
        if (!is_in_mem(cur_mem, page_data[i])) { /// 如果发生缺页就轮着替换
            for(int j=0;j<sz;++j)printf("%d ", pre_mem[j]);
            printf("%d [%d]\n", page_data[i], row);
            cur_mem[row++] = page_data[i];
            row %= sz;
            ++cnt;
        }
        pre_mem = cur_mem;
        cur_mem += sz;
    }
    printf("[FIFO] Total page fault:%d\n", cnt);
}
```

### LRU

- LRU与FIFO不同，它是在页面冲突时，选择最近没有使用过的页面淘汰。
- 利用一个计数器数组维护要替换的行，实现LRU算法。

```c
int LRU_choose_row(int*counter){ /// 选择一个最近没有使用的行。
    int row=0, cur = counter[0];
    for(int i=1;i<sz;++i)if(counter[i]>cur){
        cur = counter[i];
        row = i;
    }
    return row;
}

void LRU(){
    int counter[sz];
    memset(counter,0, sizeof(counter));
    memset(analog_mem,0, sizeof(int)*sz*page_num);
    int*cur_mem = analog_mem, *pre_mem = cur_mem, cnt = 0; /// 初始化
    for(int i=0;i<page_num;++i){
        memcpy(cur_mem, pre_mem, sizeof(int)*sz);
        int idx = is_in_mem(cur_mem, page_data[i]); /// 判断页面数是否在内存
        if(!idx){
            int row = LRU_choose_row(counter); /// 不在就选择合适的行插入
            for(int j=0;j<sz;++j)printf("%d ", pre_mem[j]);
            printf("%d [%d]\n", page_data[i], row);
            counter[row] = -1;
            cur_mem[row] = page_data[i];
            ++cnt;
        } else counter[idx - 1] = -1; /// 如果在就更新counter
        for(int j=0;j<sz;++j)++counter[j]; /// 每次循环刷新counter
        pre_mem = cur_mem;
        cur_mem+=sz;
    }
    printf("[LRU] Total page fault:%d\n", cnt);
}
```

### OPT

- OPT是一种理想算法，实际上不能实现，因为你无法预测后续要访问的页面数是什么。
- OPT的算法思想是在页面冲突时，选择未来最长时间不被访问的或者以后永不使用的页面进行淘汰。

```CQL
int OPT_choose_row(int*cur_mem, int idx){ /// 计算最长不被访问的行，这个算法很低效。
    int row = 0, cnt = 0;
    for(int i=idx+1;i<page_num;++i){
        if(page_data[i] == cur_mem[0])break;
        else ++cnt;
    }
    for(int i=1;i<sz;++i){
        int tmp_cnt = 0;
        for(int j=idx+1;j<page_num;++j){
            if(page_data[j] == cur_mem[i])break;
            else ++tmp_cnt;
        }
        if(tmp_cnt > cnt){
            row = i;
            cnt = tmp_cnt;
        }
    }
    return row;
}

void OPT(){
    memset(analog_mem,0, sizeof(int)*sz*page_num);
    int*cur_mem = analog_mem, *pre_mem = cur_mem, cnt = 0; /// 初始化
    for(int i=0;i<page_num;++i){
        memcpy(cur_mem, pre_mem, sizeof(int)*sz);
        if(!is_in_mem(cur_mem, page_data[i])){ /// 缺页就选择行淘汰
            ++cnt;
            int row = OPT_choose_row(cur_mem, i);
            for(int j=0;j<sz;++j)printf("%d ", pre_mem[j]);
            printf("%d [%d]\n", page_data[i], row);
            cur_mem[row] = page_data[i];
        }
        pre_mem = cur_mem;
        cur_mem += sz;
    }
    printf("[OPT] Total page fault:%d\n", cnt);
}
```

### CLOCK

- CLOCK算法是给每一帧关联一个附加位，称为使用位。当某一页首次装入主存时，该帧的使用位设置为1;当该页随后再被访问到时，它的使用位也被置为1。对于页替换算法，用于替换的候选帧集合看做一个循环缓冲区，并且有一个指针与之相关联。当某一页被替换时，该指针被设置成指向缓冲区中的下一帧。当需要替换一页时，操作系统扫描缓冲区，以查找使用位被置为0的一帧。每当遇到一个使用位为1的帧时，操作系统就将该位重新置为0；如果在这个过程开始时，缓冲区中所有帧的使用位均为0，则选择遇到的第一个帧替换；如果所有帧的使用位均为1,则指针在缓冲区中完整地循环一周，把所有使用位都置为0，并且停留在最初的位置上，替换该帧中的页。

```c
void CLOCK(){
    int cur_mem[sz], use[sz], cnt = 0, row = 0;
    memset(cur_mem,0, sizeof(cur_mem));
    memset(use,0, sizeof(use));
    for(int i=0;i<page_num;++i){
        int idx = is_in_mem(cur_mem, page_data[i]);
        if(!idx){
            do{
                if(use[row] == 0){
                    for(int j=0;j<sz;++j)printf("%d ", cur_mem[j]);
                    printf("%d [%d]\n", page_data[i], row);
                    cur_mem[row] = page_data[i];
                    use[row] = 1;
                    idx = 1;
                    ++cnt;
                }
                else use[row]=0;
                ++row;
                if(row == sz)row=0;
            } while (!idx);
        } else use[idx-1] = 1;
    }
    printf("[CLOCK] Total page fault:%d\n", cnt);
}
```



###测试与输出

```text
Input memory size and page number:3 10
Auto generate data:
4 2 7 10 6 6 1 5 8 9
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:1
0 0 0 4 [0]
4 0 0 2 [1]
4 2 0 7 [2]
4 2 7 10 [0]
10 2 7 6 [1]
10 6 7 1 [2]
10 6 1 5 [0]
5 6 1 8 [1]
5 8 1 9 [2]
[FIFO] Total page fault:9
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:2
0 0 0 4 [0]
4 0 0 2 [1]
4 2 0 7 [2]
4 2 7 10 [0]
10 2 7 6 [1]
10 6 7 1 [2]
10 6 1 5 [0]
5 6 1 8 [1]
5 8 1 9 [2]
[LRU] Total page fault:9
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:3
5 8 9 4 [2]
5 8 4 2 [2]
5 8 2 7 [2]
5 8 7 10 [2]
5 8 10 6 [2]
5 8 6 1 [2]
5 8 1 9 [0]
[OPT] Total page fault:7
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:4
0 0 0 4 [0]
4 0 0 2 [1]
4 2 0 7 [2]
4 2 7 10 [0]
10 2 7 6 [1]
10 6 7 1 [2]
10 6 1 5 [0]
5 6 1 8 [1]
5 8 1 9 [2]
[CLOCK] Total page fault:9
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:0
```

```text
Input memory size and page number:3 10
Auto generate data:
9 3 6 6 6 7 8 9 10 4
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:1
0 0 0 9 [0]
9 0 0 3 [1]
9 3 0 6 [2]
9 3 6 7 [0]
7 3 6 8 [1]
7 8 6 9 [2]
7 8 9 10 [0]
10 8 9 4 [1]
[FIFO] Total page fault:8
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:2
0 0 0 9 [0]
9 0 0 3 [1]
9 3 0 6 [2]
9 3 6 7 [0]
7 3 6 8 [1]
7 8 6 9 [2]
7 8 9 10 [0]
10 8 9 4 [1]
[LRU] Total page fault:8
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:3
0 0 0 9 [0]
9 0 0 3 [1]
9 3 0 6 [1]
9 6 0 7 [1]
9 7 0 8 [1]
9 8 0 10 [0]
10 8 0 4 [0]
[OPT] Total page fault:7
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:4
0 0 0 9 [0]
9 0 0 3 [1]
9 3 0 6 [2]
9 3 6 7 [0]
7 3 6 8 [1]
7 8 6 9 [2]
7 8 9 10 [0]
10 8 9 4 [1]
[CLOCK] Total page fault:8
----------------------------------------
1. FIFO
2. LRU
3. OPT
4. CLOCK
0. exit
----------------------------------------
Input your choice:0
```

##优化方案

- 显然，三种计算过程并不要申请`sz * page_num`大小的内存，只需要申请一个`sz`大小的内存就足以模拟所有情况。
- source code:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RMALLOC(type,n) (type*)malloc(sizeof(type)*(n))
#define MALLOC(p,type,n) type*p = RMALLOC(type, n)

void draw_line(){
    for(int i=0;i<=40;++i)putchar(i==40?'\n':'-');
}

int sz, page_num;
int *page_data;

void generate_data(){
    page_data = RMALLOC(int, page_num);
    int *p = page_data;
    for(int i=0;i<page_num;++i)*(p++) = rand()%10+1;
    puts("Auto generate data:");
    for(int i=0;i<page_num;++i)printf("%d%c",page_data[i], i==page_num-1?'\n':' ');
}

int is_in_mem(int*cur_mem, int id){
    for(int i=0;i<sz;++i)if(cur_mem[i] == id)return i+1;
    return 0;
}

void FIFO() {
    int row = 0, cnt = 0;
    MALLOC(cur_mem, int ,sz);
    for (int i = 0; i < page_num; ++i) {
        if (!is_in_mem(cur_mem, page_data[i])) {
            for(int j=0;j<sz;++j)printf("%d ", cur_mem[j]);
            printf("%d [%d]\n", page_data[i], row);
            cur_mem[row++] = page_data[i];
            row %= sz;
            ++cnt;
        }
    }
    printf("[FIFO] Total page fault:%d\n", cnt);
    free(cur_mem);
}

int LRU_choose_row(int*counter){
    int row=0, cur = counter[0];
    for(int i=1;i<sz;++i)if(counter[i]>cur){
        cur = counter[i];
        row = i;
    }
    return row;
}

void LRU(){
    int counter[sz];
    memset(counter,0, sizeof(counter));
    MALLOC(cur_mem, int, sz);
    int cnt = 0;
    for(int i=0;i<page_num;++i){
        int idx = is_in_mem(cur_mem, page_data[i]);
        if(!idx){
            int row = LRU_choose_row(counter);
            for(int j=0;j<sz;++j)printf("%d ", cur_mem[j]);
            printf("%d [%d]\n", page_data[i], row);
            counter[row] = -1;
            cur_mem[row] = page_data[i];
            ++cnt;
        } else counter[idx - 1] = -1;
        for(int j=0;j<sz;++j)++counter[j];
    }
    printf("[LRU] Total page fault:%d\n", cnt);
    free(cur_mem);
}

int OPT_choose_row(int*cur_mem, int idx){
    int row = 0, cnt = 0;
    for(int i=idx+1;i<page_num;++i){
        if(page_data[i] == cur_mem[0])break;
        else ++cnt;
    }
    for(int i=1;i<sz;++i){
        int tmp_cnt = 0;
        for(int j=idx+1;j<page_num;++j){
            if(page_data[j] == cur_mem[i])break;
            else ++tmp_cnt;
        }
        if(tmp_cnt > cnt){
            row = i;
            cnt = tmp_cnt;
        }
    }
    return row;
}

void OPT(){
    MALLOC(cur_mem, int ,sz);
    int cnt = 0;
    for(int i=0;i<page_num;++i){
        if(!is_in_mem(cur_mem, page_data[i])){
            ++cnt;
            int row = OPT_choose_row(cur_mem, i);
            for(int j=0;j<sz;++j)printf("%d ", cur_mem[j]);
            printf("%d [%d]\n", page_data[i], row);
            cur_mem[row] = page_data[i];
        }
    }
    printf("[OPT] Total page fault:%d\n", cnt);
    free(cur_mem);
}

void CLOCK(){
    int cur_mem[sz], use[sz], cnt = 0, row = 0;
    memset(cur_mem,0, sizeof(cur_mem));
    memset(use,0, sizeof(use));
    for(int i=0;i<page_num;++i){
        int idx = is_in_mem(cur_mem, page_data[i]);
        if(!idx){
            do{
                if(use[row] == 0){
                    for(int j=0;j<sz;++j)printf("%d ", cur_mem[j]);
                    printf("%d [%d]\n", page_data[i], row);
                    cur_mem[row] = page_data[i];
                    use[row] = 1;
                    idx = 1;
                    ++cnt;
                }
                else use[row]=0;
                ++row;
                if(row == sz)row=0;
            } while (!idx);
        } else use[idx-1] = 1;
    }
    printf("[CLOCK] Total page fault:%d\n", cnt);
}

int menu(){
    draw_line();
    puts("1. FIFO");
    puts("2. LRU");
    puts("3. OPT");
    puts("4. CLOCK");
    puts("0. exit");
    draw_line();
    int ch = 1;
    do{
        printf("Input your choice:");
    }
    while (scanf("%d",&ch), ch<0 || ch>4);
    return ch;
}

int main(int argc, char **argv) {
    srand(time(NULL));
    printf("Input memory size and page number:");
    scanf("%d%d", &sz, &page_num);
    generate_data();
    int ch;
    while(ch=menu(), ch){
        switch (ch){
            case 1:
                FIFO();
                break;
            case 2:
                LRU();
                break;
            case 3:
                OPT();
                break;
            case 4:
                CLOCK();
                break;
            default:
                break;
        }
    }
    free(page_data);
    return 0;
}
```

