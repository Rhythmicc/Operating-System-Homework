# 操作系统课程设计报告八(文件系统)

### 连浩丞 计算机二班 2017011344

## 题目1

### 设计方案

- 爬取服务器上提供的html，用于制作文件系统。

### 源代码

```python
import requests

def get_html(url):
    html = requests.get(url)
    html.encoding = html.apparent_encoding
    return html.text


if __name__ == '__main__':
    root = 'http://202.204.194.17:9168'
    for i in range(100000):
        html = get_html(root + "%d.html" % i)
        with open('%d.html' % i, 'w') as f:
            f.write(html)

```

### 实验过程

### 运行结果与分析

- 可以利用上部分代码利用web服务爬取服务器上全部html文件
- 但是不如直接在服务器上直接读取html文件



## 题目2

### 设计方案

- 注意到，文件系统用于静态web服务时，文件并不会被修改和删除。因此，可以直接利用哈希表+连续性分配方案来制作简易文件系统。



## 题目3

### 设计方案

- 实现题目二的方案

### 源代码

- fs.h

```c
#ifndef _FS_H_
#define _FS_H_
#define FS_TABLE_SIZE 10005

#include <stdio.h>
typedef struct Hash_FS_pair{
    char*filename;
    unsigned len;
    unsigned long long excursion;
    struct Hash_FS_pair*nxt;
}Hash_FS_pair, *fs_pair_t;

typedef struct {
    Hash_FS_pair table[FS_TABLE_SIZE];
    FILE*start_fp;
    unsigned long long total;
}Hash_FS, *fs_t;

fs_pair_t fs_read(fs_t fs, const char*filename);
void fs_load(fs_t fs, const char*filename, const char*as_filename);
void fs_export(fs_t fs, const char*fs_filename);
fs_t fs_lead(const char*fs_filename, const char*fs_dt);
fs_t empty_fs(const char*fs_name);
void delete_fs(fs_t fs);
#endif
```

- fs.c

```c
#include "fs.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define RMALLOC(type,n) (type*)malloc(sizeof(type)*(n))
#define MALLOC(p,type,n) type*p = RMALLOC(type, n)
#define min(a,b) (a)<(b)?(a):(b)

unsigned int string_hash(const char *str) {
    unsigned int hash = 0;
    int i;
    for (i = 0; *str; i++) {
        if ((i & 1) == 0)hash ^= ((hash << 7) ^ (*str++) ^ (hash >> 3));
        else hash ^= (~((hash << 11) ^ (*str++) ^ (hash >> 5)));
    }
    return (hash & 0x7FFFFFFF);
}

fs_pair_t new_fs_pair(const char *filename) {
    MALLOC(ret, Hash_FS_pair, 1);
    ret->excursion = ret->len = 0;
    ret->filename = RMALLOC(char, strlen(filename));
    strcpy(ret->filename, filename);
    ret->nxt = NULL;
    return ret;
}

fs_pair_t new_empty_fs_pair() {
    MALLOC(ret, Hash_FS_pair, 1);
    ret->excursion = ret->len = 0;
    ret->filename = NULL;
    ret->nxt = NULL;
    return ret;
}

void delete_fs_pair(fs_pair_t fpt) {
    free(fpt->filename);
    free(fpt);
}

fs_pair_t fs_read(fs_t fs, const char *filename) {
    unsigned index = string_hash(filename) % FS_TABLE_SIZE;
    fs_pair_t p = fs->table + index;
    while (p->nxt) {
        p = p->nxt;
        if (!strcmp(p->filename, filename))return p;
    }
    return NULL;
}

void fs_load(fs_t fs, const char *filename, const char*as_filename) {
    unsigned index = string_hash(as_filename) % FS_TABLE_SIZE;
    fs_pair_t p = fs->table + index;
    while (p->nxt) {
        p = p->nxt;
        if (!strcmp(p->filename, as_filename))return;
    }
    p->nxt = new_fs_pair(as_filename);
    FILE *fp = fopen(filename, "r");
    unsigned len = 0;
    while (!feof(fp)) {
        fputc(fgetc(fp), fs->start_fp);
        ++len;
    }
    fclose(fp);
    printf("%s\t--%uKB\n", filename, len >> 10);
    p->nxt->excursion = fs->total;
    fflush(fs->start_fp);
    fs->total += len;
    p->nxt->len = len;
}

void fs_export(fs_t fs, const char *fs_filename) {
    FILE *fp = fopen(fs_filename, "w");
    fprintf(fp, "%llu\n", fs->total);
    fs_pair_t cur = fs->table;
    int cnt = FS_TABLE_SIZE;
    while (cnt--) {
        int line_cnt = 0;
        fs_pair_t p = cur;
        while (p->nxt) {
            p = p->nxt;
            ++line_cnt;
        }
        fprintf(fp, "%d\n", line_cnt);
        p = cur;
        while (p->nxt) {
            p = p->nxt;
            fprintf(fp, "%llu %u %s\n", p->excursion, p->len, p->filename);
        }
        ++cur;
    }
    fclose(fp);
}

fs_t fs_lead(const char *fs_filename, const char*fs_dt) {
    MALLOC(ret, Hash_FS, 1);
    FILE *fp = fopen(fs_filename, "r");
    int cnt = FS_TABLE_SIZE, line_cnt;
    fscanf(fp, "%llu", &ret->total);
    fs_pair_t cur = ret->table;
    while (cnt--) {
        fscanf(fp, "%d", &line_cnt);
        fs_pair_t p = cur;
        while (line_cnt--) {
            p->nxt = new_empty_fs_pair();
            p = p->nxt;
            p->filename = RMALLOC(char, 15);
            fscanf(fp, "%llu %u %s", &p->excursion, &p->len, p->filename);
        }
        ++cur;
    }
    ret->start_fp = fopen(fs_dt, "r");
    return ret;
}

fs_t empty_fs(const char *fs_name) {
    MALLOC(ret, Hash_FS, 1);
    memset(ret->table, 0, sizeof(ret->table));
    ret->total = 0;
    ret->start_fp = fopen(fs_name, "a");
    return ret;
}

void delete_fs(fs_t fs) {
    int cnt = FS_TABLE_SIZE;
    fs_pair_t cur = fs->table;
    while (cnt--) {
        fs_pair_t p = cur->nxt;
        while (p) {
            fs_pair_t tmp = p;
            p = p->nxt;
            delete_fs_pair(tmp);
        }
        ++cur;
    }
    fclose(fs->start_fp);
}
```

### 实验过程

- 利用文件系统提供的empty_fs, fs_load接口，将html导入文件系统
- 利用fs_export接口将文件系统导出

### 运行结果与分析

```shell
[2017011344@cupcs_ha2 webserver]$ du -sh *
12K     cache.c
4.0K    cache.h
40K     compress_web
36K     dist
728K    frontend
2.7M    fs
4.0K    fs.c
7.6G    fs_dt
4.0K    fs.h
4.0K    img
4.0K    Makefile
4.0K    project_configure.csv
28K     template
12K     thpool.c
4.0K    thpool.h
12K     webserver.c
```

- 成功将文件系统制作成功并导出。



## 题目4

### 设计方案

- 设计程序，比较两个文件系统在读一千次下的效率差距。

### 源代码

- test.c

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "../fs.h"

int main(int argc, char **argv) {
    srand(time(NULL));
    struct timeval t1, t2;
    char buffer[105];
    gettimeofday(&t1, NULL);
    for(int i=0;i<1000;++i){
        sprintf(buffer, "/usr/shiyou1/Crawler4jDate/%d.html", rand() % 100000);
        FILE*fp = fopen(buffer, "r");
        while(!feof(fp))fgetc(fp);
    }
    gettimeofday(&t2, NULL);
    printf("[Linux File System]:\t%.3lfms\n", (t2.tv_sec - t1.tv_sec) * 1000 + (t2.tv_usec - t1.tv_usec) / 1000.0);
    fs_t fs = fs_lead("../fs", "../fs_dt");
    gettimeofday(&t1, NULL);
    for(int i = 0; i<1000;++i){
        sprintf(buffer, "%d.html", rand() % 100000);
        fs_pair_t res = fs_read(fs, buffer);
        unsigned len = res->len;
        FILE*fp = fs->start_fp;
        fseek(fp, res->excursion, SEEK_SET);
        while (len--)fgetc(fp);
    }
    gettimeofday(&t2, NULL);
    printf("[My File System]:\t%.3lfms\n", (t2.tv_sec - t1.tv_sec) * 1000 + (t2.tv_usec - t1.tv_usec) / 1000.0);
    return 0;
}
```

### 实验过程

- 运行如上代码进行测试

### 运行结果与分析

```shell
[2017011344@cupcs_ha2 test]$ run -br
[Linux File System]:    4599.828ms
[My File System]:       24053.374ms
```

- 比对后，发现我写的文件系统比较慢。
- 原因是，我的文件系统是基于哈希表索引，通过移动文件指针来模拟实现的文件系统。其本质上在调用Linux文件系统API的基础上，还多进行了几步其他的耗时任务去模拟实现文件系统。从理论上讲必不能比Linux文件系统快。（如果我的文件系统还要同时支持修改和删除，基于一个大文件实现的文件系统将必然效率低于Linux文件系统）



## 题目5

### 设计方案

- 将文件系统适配进web服务器
- 测试

### 源代码

- webserver.c

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include "thpool.h"
#include "fs.h"

#define VERSION 23
#define BUFSIZE 8096
#define ERROR 42
#define LOG 44
#define FORBIDDEN 403
#define NOTFOUND 404
#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif
#define RMALLOC(type,n) (type*)malloc(sizeof(type)*(n))
#define MALLOC(p,type,n) type*p = RMALLOC(type, n)
#define min(a, b) (a)<(b)?(a):(b)

struct {
    char *ext;
    char *filetype;
} extensions[] = {
        {"gif",  "image/gif"},
        {"jpg",  "image/jpg"},
        {"jpeg", "image/jpeg"},
        {"png",  "image/png"},
        {"ico",  "image/ico"},
        {"zip",  "image/zip"},
        {"gz",   "image/gz"},
        {"tar",  "image/tar"},
        {"htm",  "text/html"},
        {"html", "text/html"},
        {0,      0}
};
double read_soc, post_dt, read_web, write_log;
unsigned int total, tol_log = 0;
pthread_mutex_t rs,wl;
threadpool deal_pool, data_pool, post_pool;
fs_t fs;

void time_to_str(char*res){
    time_t t;
    time (&t);
    struct tm *lt = localtime(&t);
    sprintf(res, "%4d-%02d-%02d %02d:%02d:%02d\n",lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
}

/* 日志函数，将运行过程中的提示信息记录到 webserver.log 文件中*/

void logger(int type, char *s1, char *s2, int socket_fd) {
    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    int fd;
    char logbuffer[BUFSIZE * 2];
    char TIME[105];
    time_to_str(TIME);
/*根据消息类型，将消息放入 logbuffer 缓存，或直接将消息通过 socket 通道返回给客户端*/
    switch (type) {
        case ERROR:
            (void) sprintf(logbuffer, "ERROR: %s:%s Errno=%d exiting pid=%d", s1, s2, errno, getpid());
            break;
        case FORBIDDEN:
            (void) write(socket_fd,
                         "HTTP/1.1 403 Forbidden\n"
                         "Content-Length: 185\n"
                         "Connection:close\n"
                         "Content-Type: text/html\n\n"
                         "<html><head>\n"
                         "<title>403 Forbidden</title>\n"
                         "</head><body>\n"
                         "<h1>Forbidden</h1>\n"
                         "The requested URL, file type or operationis not allowed on this simple static file webserver.\n"
                         "</body></html>\n",
                         271);
            (void) sprintf(logbuffer, "FORBIDDEN: %s:%s", s1, s2);
            break;
        case NOTFOUND:
            (void) write(socket_fd,
                         "HTTP/1.1 404 Not Found\n"
                         "Content-Length:136\n"
                         "Connection: close\n"
                         "Content-Type: text/html\n\n"
                         "<html><head>\n"
                         "<title>404 Not Found</title>\n"
                         "</head><body>\n"
                         "<h1>Not Found</h1>\n"
                         "The requested URL was not found on this server.\n"
                         "</body></html>\n",
                         224);
            (void) sprintf(logbuffer, "NOT FOUND: %s:%s", s1, s2);
            break;
        case LOG:
            (void) sprintf(logbuffer, " INFO: %s:%s:%d", s1, s2, socket_fd);
            break;
    }
/* 将 logbuffer 缓存中的消息存入 webserver.log 文件*/
    if ((fd = open("webserver.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
        (void) write(fd, TIME, strlen(TIME));
        (void) write(fd, logbuffer, strlen(logbuffer));
        (void) write(fd, "\n", 1);
        (void) close(fd);
    }
    gettimeofday(&t2,NULL);
    pthread_mutex_lock(&wl);
    ++tol_log;
    write_log += (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
    pthread_mutex_unlock(&wl);
}

void check_data(void*param);
void post_data(void*param);

typedef struct {
    int fd,hit;
}webparam;

typedef struct {
    int lim;
    char*buffer;
    int fd,hit;
    double rs;
}read_param;

typedef struct {
    int fd,hit;
    char*buffer,*fstr;
    double rs, rw;
}post_param;

void deal(void*data) {
    webparam *p = (webparam *) data;
    int fd = p->fd, hit = p->hit;
    long i, ret;
    MALLOC(buffer, char, BUFSIZE+1);
    ret = read(fd, buffer, BUFSIZE);
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    /* 从连接通道中读取客户端的请求消息 */
    if (ret == 0 || ret == -1)  //如果读取客户端消息失败，则向客户端发送 HTTP 失败响应信息
        logger(FORBIDDEN, "failed to read browser request", "", fd);
    if (ret > 0 && ret < BUFSIZE) buffer[ret] = 0;
    else buffer[0] = 0;
    for (i = 0; i < ret; i++) /* 移除消息字符串中的“CF”和“LF”字符*/
        if (buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i] = '*';
    logger(LOG, "request", buffer, hit);
/*判断客户端 HTTP 请求消息是否为 GET 类型，如果不是则给出相应的响应消息*/
    if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
        logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
    }
    for (i = 4; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
        if (buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
            buffer[i] = 0;
            break;
        }
    }
    gettimeofday(&t2, NULL);
    MALLOC(rdt, read_param, 1);
    rdt->buffer = buffer;
    rdt->fd = fd;
    rdt->lim = i;
    rdt->hit = hit;
    rdt->rs = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
    thpool_add_work(data_pool, check_data, (void *) rdt);
    free(p);
}

void check_data(void*param){
    read_param*p = (read_param*)param;
    struct timeval t1, t2;
    int i, j;
    gettimeofday(&t1, NULL);
    for (j = 0; j < p->lim - 1; j++)
/* 在消息中检测路径，不允许路径中出现“.” */
        if (p->buffer[j] == '.' && p->buffer[j + 1] == '.')
            logger(FORBIDDEN, "Parent directory (..) path names not supported", p->buffer, p->fd);
    if (!strncmp(p->buffer, "GET /\0", 6) || !strncmp(p->buffer, "get /\0", 6))
/* 如果请求消息中没有包含有效的文件名，则使用默认的文件名 index.html */
        (void) strcpy(p->buffer, "GET /index.html");
/* 根据预定义在 extensions 中的文件类型，检查请求的文件类型是否本服务器支持 */
    int buflen = strlen(p->buffer), len;
    char*fstr = (char *) 0;
    for (i = 0; extensions[i].ext != 0; i++) {
        len = strlen(extensions[i].ext);
        if (!strncmp(p->buffer + buflen - len, extensions[i].ext, len)) {
            fstr = extensions[i].filetype;
            break;
        }
    }
    if (fstr == 0)logger(FORBIDDEN, "file extension type not supported", p->buffer, p->fd);
    gettimeofday(&t2, NULL);
    MALLOC(pdt, post_param,1);
    pdt->fd = p->fd;
    pdt->buffer = p->buffer;
    pdt->fstr = fstr;
    pdt->hit = p->hit;
    pdt->rs = p->rs;
    pdt->rw = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
    thpool_add_work(post_pool, post_data, (void*)pdt);
    free(p);
}

void post_data(void*param){
    post_param*p = (post_param*)param;
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    fs_pair_t res = fs_read(fs, p->buffer + 5);
    if (!res) { /* 打开指定的文件名*/
        logger(NOTFOUND, "failed to open file", p->buffer+5, p->fd);
    } else {
        logger(LOG, "SEND", p->buffer + 5, p->hit);
        FILE*fp = fs->start_fp;
        fseek(fp, res->excursion, SEEK_SET);

        (void) sprintf(p->buffer,
                       "HTTP/1.1 200 OK\nServer:nweb/%d.0\nContent-Length:%ld\nConnection:close\nContent-Type: %s\n\n",
                       VERSION, (long)res->len, p->fstr); /* Header + a blank line */
        logger(LOG, "Header", p->buffer, p->hit);
        (void) write(p->fd, p->buffer, strlen(p->buffer));
        unsigned len = res->len;
        while(len) {
            int cur = min(BUFSIZE, len);
            for(int i=0;i<cur;++i)p->buffer[i] = fgetc(fp);
            write(p->fd, p->buffer, cur);
            len -= cur;
        }
        usleep(1000);
    }
    close(p->fd);
    gettimeofday(&t2, NULL);
    pthread_mutex_lock(&rs);
    read_soc += p->rs;
    read_web += p->rw;
    post_dt += (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
    pthread_mutex_unlock(&rs);
    free(p->buffer);
    free(p);
}


static void del_sig(int sig){
    thpool_destroy(deal_pool);
    thpool_destroy(data_pool);
    thpool_destroy(post_pool);
    delete_fs(fs);
    puts("\n");
    printf("Total requests:\t%u\n", total);
    printf("read socket:\t%4.2fms/time\n", read_soc / total);
    printf("read web:\t%4.2fms/time\n", read_web / total);
    printf("post data:\t%4.2fms/time\n", post_dt / total);
    printf("write log:\t%4.2fms/time\n", write_log / tol_log);
    exit(0);
}

int main(int argc, char **argv) {
    int i, port, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr; /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
/*解析命令参数*/
    if (argc != 2 || !strcmp(argv[1], "-?")) {
        (void) printf("hint: nweb Port-Number\t\tversion %d\n\n"
                      "\tnweb is a small and very safe mini web server\n"
                      "\tnweb only servers out file/web pages with extensions named below\n"
                      "\tThere is no fancy features = safe and secure.\n\n"
                      "\tExample:webserver 8181\n\n"
                      "\tOnly Supports:", VERSION);
        for (i = 0; extensions[i].ext != 0; i++)
            (void) printf(" %s", extensions[i].ext);
        (void) printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                      "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
                      "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
        exit(0);
    }
    fs = fs_lead("fs", "fs_dt");
    puts("fs start done");
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        logger(ERROR, "system call", "socket", 0);
    port = atoi(argv[1]);
    if (port < 0 || port > 60000)
        logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        logger(ERROR, "system call", "bind", 0);
    if (listen(listenfd, 64) < 0)
        logger(ERROR, "system call", "listen", 0);
    signal(SIGINT, del_sig);
    pthread_mutex_init(&rs,NULL);
    pthread_mutex_init(&wl,NULL);
    deal_pool = thpool_init(50, "read_sock");
    data_pool = thpool_init(50, "read_html");
    post_pool = thpool_init(200, "post_data");
    for (hit = 1;; ++hit) {
        length = sizeof(cli_addr);
        if ((socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &length)) < 0) {
            logger(ERROR, "system call", "accept", 0);
            continue;
        }
        ++total;
        MALLOC(param, webparam, 1);
        param->fd = socketfd;
        param->hit = hit;
        thpool_add_work(deal_pool, deal, (void*)param);
    }
}
```

### 实验过程

- 调用webtest.py进行测试：”run 20 1000“ (即，调用20个线程进行总共1000次随机访问)

### 运行结果与分析

- 服务器端：

```shell
[2017011344@cupcs_ha2 webserver]$ run -br 9168
fs start done
^C

Total requests:	1000
read socket:	  0.13ms/time
read web:	      0.00ms/time
post data:     	691.94ms/time
write log:	    0.07ms/time
```

- 测试端：

```shell
➜ webtest master $ run 20 1000
http status:	 {200: 1000}
ms/time:     	 {200: 697.641}
Total use time: 697641.0984 ms
```

- 分析：

  相较于Linux文件系统慢了很多，在网络通畅且服务器性能平稳时，平均每次请求需要耗费300ms，相比于Linux文件系统要慢很多。