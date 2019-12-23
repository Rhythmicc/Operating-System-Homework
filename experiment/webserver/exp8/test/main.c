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
