# 操作系统课程设计报告五

### 连浩丞 计算机二班 2017011344

## 题目1

### 设计方案

- 将web函数流程分为三块，分别进行解析socket，读数据和发送数据。
- 针对每一块设立一个线程池。

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
    int ret, file_fd;
    gettimeofday(&t1, NULL);
    if ((file_fd = open(p->buffer+5, O_RDONLY)) == -1) { /* 打开指定的文件名*/
        logger(NOTFOUND, "failed to open file", p->buffer+5, p->fd);
    } else {
        logger(LOG, "SEND", p->buffer + 5, p->hit);
        long len = (long) lseek(file_fd, (off_t) 0, SEEK_END); /* 通过 lseek 获取文件长度*/
        (void) lseek(file_fd, (off_t) 0, SEEK_SET); /* 将文件指针移到文件首位置*/
        (void) sprintf(p->buffer,
                       "HTTP/1.1 200 OK\nServer:nweb/%d.0\nContent-Length:%ld\nConnection:close\nContent-Type: %s\n\n",
                       VERSION, len, p->fstr); /* Header + a blank line */
        logger(LOG, "Header", p->buffer, p->hit);
        (void) write(p->fd, p->buffer, strlen(p->buffer));
        while ((ret = read(file_fd, p->buffer, BUFSIZE)) > 0) {
            (void) write(p->fd, p->buffer, ret);
        }
        usleep(1000);
        close(file_fd);
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
    if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
        (void) printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
                      "\tnweb is a small and very safe mini web server\n"
                      "\tnweb only servers out file/web pages with extensions named below\n"
                      "\t and only from the named directory or its sub-directories.\n"
                      "\tThere is no fancy features = safe and secure.\n\n"
                      "\tExample:webserver 8181 /home/nwebdir &\n\n"
                      "\tOnly Supports:", VERSION);
        for (i = 0; extensions[i].ext != 0; i++)
            (void) printf(" %s", extensions[i].ext);
        (void) printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                      "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
                      "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
        exit(0);
    }
    if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5) ||
        !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5) ||
        !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5) ||
        !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6)) {
        (void) printf("ERROR: Bad top directory %s, see nweb -?\n", argv[2]);
        exit(3);
    }
    if (chdir(argv[2]) == -1) {
        (void) printf("ERROR: Can't Change to directory %s\n", argv[2]);
        exit(4);
    }

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

- 服务器修改代码并运行：`run -br 9168 /usr/shiyou1/Crawler4jDate`
- 在本地使用`webtest.py`进行测试：`run 100 1000`

### 运行结果与分析

- 服务器输出：

```text
[2017011344@cupcs_ha2 webserver]$ run -br 9168 /usr/shiyou1/Crawler4jDate
^C

Total requests: 1000
read socket:    0.61ms/time
read web:       0.00ms/time
post data:      32.58ms/time
write log:      0.32ms/time
```

- 测试端输出：

```text
http status:     {200: 1000}
ms/time:         {200: 95.758}
```



## 题目2

### 设计方案

- 修改线程池代码，增加两个监视器。
- 监视器1：每隔0.5秒获取活跃进程数统计最值和平均值。
- 监视器2：线程池中的线程做完一个任务后向监视器汇报用时。

### 源代码

- 监视器1：

```c
void*thpool_monitor(void*param){
    thpool_*pool = (thpool_*)param;

    while(threads_keepalive) {
        usleep(500000);
        int wnum = pool->num_threads_working;
        pool->aver_work += wnum;
        ++pool->tol;
        pool->min_work = pool->min_work > wnum ? wnum : pool->min_work;
        pool->max_work = pool->max_work < wnum ? wnum : pool->max_work;
        printf("%s:\tworking thread: %d\n", pool->name, wnum);
    }
    return NULL;
}
```

- 监视器2:

```c
void*thpool_time_monitor(void*param){
    thpool_*pool = (thpool_*)param;
    char tv[15];
    double t, sum=0;
    unsigned tol = 0;
    while(threads_keepalive){
        read(pool->time_fd[0], tv, 15);
        sscanf(tv, "%lf", &t);
        if(t<0)break;
        ++tol;
        sum += t;
    }
    printf("%s\tworking time:\t%.3lf\n", pool->name ,sum / tol);
    return NULL;
}
```

### 实验过程

- 修改thpool.c并编译运行
- 利用webtest测试

### 运行结果与分析

- 服务器：

```tex
[2017011344@cupcs_ha2 webserver]$ run -br 9168 /usr/shiyou1/Crawler4jDate
...
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 0
read_sock:	working thread: 1
read_html:	working thread: 0
post_data:	working thread: 15
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 42
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 50
read_sock:	working thread: 2
read_html:	working thread: 0
post_data:	working thread: 42
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 39
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 36
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 43
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 46
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 34
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 35
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 34
read_sock:	working thread: 1
read_html:	working thread: 0
post_data:	working thread: 25
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 23
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 10
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 6
read_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 0
...
^Cread_sock:	working thread: 0
read_html:	working thread: 0
post_data:	working thread: 0
read_sock:	max work: 2	min work: 0	aver work: 0
read_sock	working time:	0.582
read_html:	max work: 0	min work: 0	aver work: 0
read_html	working time:	0.017
post_data:	max work: 50	min work: 0	aver work: 10
post_data	working time:	248.552


Total requests: 1000
read socket:    2.22ms/time
read web:       0.00ms/time
post data:      34.07ms/time
write log:      0.91ms/time
```

- 测试端输出：

```tex
http status:     {200: 1000}
ms/time:         {200: 86.978}
```



## 题目3

### 分析

- 观察各部分用时，发现数据发送线程池工作时间普遍较长。导致任务队列大量堆积，可以通过增加发送线程池中的线程数目来提升性能。与此同时，其余两个线程池存在较多闲置线程，可以考虑减少数目以降低非活跃线程对服务器资源的占用。
- 考虑线程池资源分配如下：

```c
deal_pool = thpool_init(50, "read_sock");
data_pool = thpool_init(50, "read_html");
post_pool = thpool_init(200, "post_data");
```

### 运行结果

- 服务器：

```tex
Total requests:	1000
read socket:	  0.04ms/time
read web:	      0.00ms/time
post data:	    68.09ms/time
write log:	    0.03ms/time
```

- 测试端：

```tex
http status:     {200: 1000}
ms/time:         {200: 0.14}
```

#### 服务性能显著提升

