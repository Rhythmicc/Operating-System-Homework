#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#define NUM_CUSTOMERS 5
#define NUM_RESOURCES 3
int curr[NUM_CUSTOMERS][NUM_RESOURCES];
int max_need[NUM_CUSTOMERS][NUM_RESOURCES];
int finish[NUM_CUSTOMERS];
int max_res[NUM_RESOURCES];
int avail[NUM_RESOURCES];
pthread_mutex_t mutex;

void cur_state(){
    puts("\n---------------------------------");
    printf("Current available resources: ");
    for (int i=0;i<NUM_RESOURCES;++i) printf("%d%c", avail[i], i==NUM_RESOURCES-1?'\n':' ');
    puts("\nCurrent max_need table:");
    for (int i=0;i<NUM_CUSTOMERS;++i)for (int j=0;j<NUM_RESOURCES;++j)printf("%d%c", max_need[i][j],j==NUM_RESOURCES-1?'\n':'\t');
    puts("\nCurrent allocation table:");
    for (int i=0;i<NUM_CUSTOMERS;++i){
        printf("p%d\t", i+1);
        for (int j=0;j<NUM_RESOURCES;++j)printf("%d%c", curr[i][j], j==NUM_RESOURCES-1?'\n':'\t');
    }
    puts("\nCurrent need table:");
    for (int i=0;i<NUM_CUSTOMERS;++i){
        printf("p%d\t", i+1);
        for (int j=0;j<NUM_RESOURCES;++j)printf("%d%c", max_need[i][j] - curr[i][j], j==NUM_RESOURCES-1?'\n':'\t');
    }
}

void* Banker(void* Pid){
    int pid = (int)Pid;
    bool wait = true;
    while (finish[pid] == 0){
        while (wait){
            wait = false;
            for (int i=0; i < NUM_RESOURCES; ++i){
                int need = max_need[pid][i] - curr[pid][i];
                if (need > avail[i])wait = true;
            }
        }
        pthread_mutex_lock(&mutex);
        printf("\nProcess p%d is allocated with: ", pid+1);
        for (int i=0; i < NUM_RESOURCES; ++i){
            int need = max_need[pid][i] - curr[pid][i];
            printf("%d ", need);
            curr[pid][i] += need;
            avail[i] -= need;
        }
        finish[pid] = 1;
        for (int i=0; i < NUM_RESOURCES; ++i)if (max_need[pid][i] != curr[pid][i])finish[pid] = 0;
        if (finish[pid] == 1){
            printf("\np%d has done!\n", pid+1);
            for (int i=0; i < NUM_RESOURCES; ++i){
                avail[i] += curr[pid][i];
                curr[pid][i] = 0;
                max_need[pid][i] = 0;
            }
        }
        cur_state();
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(0);
}

int main(){
    pthread_t tid[NUM_CUSTOMERS];
    pthread_attr_t attr;
    puts("Give the max number of each resources:");
    for (int i=0;i<NUM_RESOURCES;++i)scanf("%d", max_res+i);
    puts("Give the number of available resources:");
    for (int i=0;i<NUM_RESOURCES;++i)scanf("%d", avail+i);
    puts("Give the current allocation of each process:");
    for (int i=0;i<NUM_CUSTOMERS;++i)for (int j=0;j<NUM_RESOURCES;++j)scanf("%d", &curr[i][j]);
    for (int j=0;j<NUM_RESOURCES;++j){
        int sum=0;
        for (int i=0;i<NUM_CUSTOMERS;++i)sum += curr[i][j];
        if (avail[j] + sum != max_res[j]){
            perror("Wrong setting\n");
            return 0;
        }
    }
    puts("Give the max number of resources each process may need:");
    for (int i=0;i<NUM_CUSTOMERS;++i)for (int j=0;j<NUM_RESOURCES;++j)scanf("%d", &max_need[i][j]);
    for (int i=0;i<NUM_CUSTOMERS;++i){
        for (int j=0;j<NUM_RESOURCES;++j)
            if (max_need[i][j] > max_res[j]){
                perror("Wrong setting\n");
                return 0;
            }
    }
    cur_state();
    puts("\nStart the Banker's algorithm");
    pthread_attr_init(&attr);
    pthread_mutex_init(&mutex, NULL);
    for (int i=0;i<NUM_CUSTOMERS;++i)pthread_create(&tid[i], &attr, Banker, (void*)i);
    for (int i=0;i<NUM_CUSTOMERS;++i)pthread_join(tid[i], NULL);
    puts("\nAll processes have finished");
    return 0;
}
