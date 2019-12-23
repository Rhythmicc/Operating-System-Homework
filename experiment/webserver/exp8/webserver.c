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
