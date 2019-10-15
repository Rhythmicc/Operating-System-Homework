#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <monitor.h>
#include <pthread.h>

const int MAX_ITER=1e5;
pthread_mutex_t mutex;
unsigned in_cir = 0, total = 0;

int is_in_circle(double x, double y) {
    return 1.0 - x * x - y * y >= 0;
}

double get_val() {
    double val = rand() / (RAND_MAX * 1.0);
    return rand() & 1 ? -val : val;
}

void *build_montecario(void*args) {
    for (int i = 0; i < MAX_ITER; ++i) {
        double x = get_val(), y = get_val();
        pthread_mutex_lock(&mutex);
        ++total;
        in_cir += is_in_circle(x, y) ? 1 : 0;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(void) {
    srand(time(NULL));
    pthread_mutex_init(&mutex,NULL);
    pthread_t ls[10];
    for (int i = 0; i < 10; ++i)pthread_create(ls + i, NULL, build_montecario, NULL);
    for (int i = 0; i < 10; ++i)pthread_join(*(ls + i), NULL);
    sleep(1);
    double PI = 4.0 * in_cir / total;
    printf("in cricle:%u total:%u PI = %lf\n", in_cir, total, PI);
    return 0;
}
