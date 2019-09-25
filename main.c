/**
 * Simple shell interface program.
 *
 * Operating System Concepts - Ninth Edition
 * Copyright John Wiley & Sons - 2013
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include<sys/types.h>
#include <sys/wait.h>

#define MAX_LINE		80 /* 80 chars per line, per command */
typedef unsigned long long ULL;

/**
 * Basic data structure: from line 26 to line 90
 * Process manager: from line 94 to line 125
 * Main process: from line 126 to end
 */

typedef struct { /// string stream
    char *line;
    char *start;
    unsigned max_len, len;
}sstream;

int blank(char x) {
    return isblank(x) || x == '\n' ? 1 : 0;
}

int seof(sstream*stream) {
    return stream->start - stream->line == stream->len ? 1 : 0;
}

void stream_getline(sstream*stream) {
    fgets(stream->line, (int) stream->max_len, stdin);
    stream->start = stream->line;
    stream->len = strlen(stream->line);
}

sstream*newSstream(unsigned len) {
    sstream *ret = (sstream *) malloc(sizeof(sstream));
    ret->line = (char *) malloc(sizeof(char) * len);
    ret->start = ret->line;
    ret->max_len = len;
    ret->len = 0;
    return ret;
}

void chrcpy(char*dest,char*start,const char*end) {
    for (char *i = start; i != end; ++i) {
        *dest = *i;
        ++dest;
    }
    *dest = '\0';
}

int nextString(sstream*stream, char*dist) {
    while (*(stream->start) && blank(*(stream->start)))++(stream->start);
    if (seof(stream))return 0;
    char *pre_start = stream->start;
    while (*(stream->start) && !blank(*(stream->start)))++(stream->start);
    chrcpy(dist, pre_start, stream->start);
    return 1;
}

typedef struct { /// string vector
    char **v;
    int tail, max_tail;
}vector;

vector ls;

char*get_element(unsigned n) {
    if (n < ls.tail)return ls.v[n];
    return NULL;
}

void push_back(char*cmd) {
    if (ls.tail == ls.max_tail) {
        ls.max_tail += 10;
        ls.v = realloc(ls.v, sizeof(char *) * ls.max_tail);
    }
    ls.v[ls.tail] = (char *) malloc(sizeof(char) * (strlen(cmd) + 1));
    strcpy(ls.v[ls.tail], cmd);
    ++ls.tail;
}

int pid_pipe[2],not_have_listener[2];

void listener(void*pro) {
    pid_t *p = (pid_t *)pro;
    pid_t pid;
    while(1){
        if(read(pid_pipe[0], &pid, sizeof(pid_t)) != -1)*p+=pid;
        printf("listener pid: %d, *p: %d\n",pid,*p);
        if(!*p)return;
    }
}

void manage_process() {
    if(fork()==0) {
        int status;
        pid_t process = 0, group = getppid();;
        pthread_t tid;
        pthread_create(&tid, NULL, (void *) listener, (void *) &process);
        pthread_join(tid, NULL); /// create thread to listen pid
        printf("compress pid: %d\n",process);
        while (process) {
            pid_t pid = waitpid(-group,&status,WNOHANG);
            printf("get pid: %d\n",pid);
            if(pid>0)process-=pid; /// catch process done
        }
        /// all son process is done
        status = 1;
        write(not_have_listener[1], &status, sizeof(int));
        exit(0);
    }
}

int execute(char*args[], int background) {
    pid_t pid = fork();
    if (pid == -1) {
        puts("Can't fork!");
        return -1;
    } else if (!pid) {
        int res = execvp(args[0], args);
        if (res)puts("Execute failed");
        exit(res);
    } else {
        if (!background) {
            wait(NULL);
            return 0;
        } else {
            int status;
            write(pid_pipe[1], &pid, sizeof(pid_t)); /// add pid to manager
            if (read(not_have_listener[0], &status, sizeof(int)) != -1)
                manage_process(); /// check whether exist listener and create listener.
        }
    }
    return 0;
}

int main(void) {
    /// Init
    char *args[MAX_LINE / 2 + 1];    /* command line (of 80) has max of 40 arguments */
    int has_alloc[MAX_LINE / 2 + 1];
    memset(has_alloc, 0, sizeof(has_alloc));
    sstream *stream = newSstream(1005);
    char argv[105];
    int should_run = 1;
    int i, upper=1;
    ls.max_tail = 10;
    ls.tail = 0;
    ls.v = (char **) malloc(sizeof(char *) * ls.max_tail);
    pipe(pid_pipe),pipe(not_have_listener);
    write(not_have_listener[1], &upper, sizeof(int));
    /// TODO: mainloop
    while (should_run) {
        printf("osh>");
        stream_getline(stream);
        int empty = 1;
        for (size_t k = 0; k < stream->len; ++k)
            if (!blank(stream->line[k])) {
                empty = 0;
                break;
            }
        if (empty)continue; /// get and check command
        int tail = 0;
        while (nextString(stream, argv)) {
            if (has_alloc[tail])args[tail] = (char *) realloc(args[tail], sizeof(char) * strlen(argv));
            else {
                args[tail] = (char *) malloc(sizeof(char) * strlen(argv));
                has_alloc[tail] = 1;
            }
            strcpy(args[tail], argv);
            ++tail;
        } /// split command
        if (!strcmp(args[0], "exit"))should_run = 0; /// exit
        else if (!strcmp(argv, "!!")) { /// view all history
            if (ls.tail)for (i = 0; i < ls.tail; ++i)printf("%d %s", i + 1, ls.v[i]);
            else puts("No commands in history!");
        } else if (args[0][0] == '!') { /// view indx-th history
            if (tail == 2) {
                unsigned indx = 0;
                size_t p, len = strlen(args[1]);
                for (p = 0; p < len; ++p)indx = indx * 10 + (args[1][p] - '0');
                if (indx) {
                    char *res = get_element(indx - 1);
                    if (res)printf("%d %s", indx, res);
                    else puts("No enough commands!");
                } else puts("Search index based 1!");
            } else puts("Error: No number after \"!\"");
        } else { /// execute command
            int back = args[tail - 1][0] == '&' ? 1 : 0;
            if (back) {
                free(args[tail - 1]);
                args[tail - 1] = NULL;
                has_alloc[tail - 1] = 0;
            }
            if (!execute(args, back))push_back(stream->line); /// add command to string vector
        }
    }
    /// free space
    for (i = 0; i < MAX_LINE / 2 + 1; ++i)if (has_alloc[i])free(args[i]);
    for (i = 0; i < ls.tail; ++i)free(ls.v[i]);
    free(ls.v);
    free(stream);
    return 0;
}
