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
