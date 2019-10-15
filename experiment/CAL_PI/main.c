#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <monitor.h>
#include <pthread.h>

const int MAX_ITER=1000;
pthread_mutex_t mutex;
unsigned long in_cir = 0, total = 0;

int is_in_circle(double x, double y){
    return 1.0 - x*x - y*y>=0;
}

double get_val(){
    double val = rand()%1000/1000.0;
    return rand()&1?-val:val;
}

void *build_montecario(void*args){
    for(int i =0;i<MAX_ITER;++i){
        double x=get_val(), y=get_val();
        pthread_mutex_lock(&mutex);
        ++total;
        in_cir+=is_in_circle(x,y)?1:0;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(void) {
    srand(time(NULL));
    for (int i = 0; i < 10; ++i) {
        pthread_t pthread_id;
        pthread_create(&pthread_id, NULL, build_montecario, NULL);
        pthread_join(pthread_id, NULL);
    }
    sleep(1);
    double PI = in_cir * 1.0 / total * 4;
    printf("in cricle:%lu total:%lu PI = %lf\n", in_cir, total, PI);
    FILE*f = fopen("result.txt","a");
    fprintf(f, "%lf\n", PI);
    return 0;
}
