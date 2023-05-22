#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct _rwlock_t
{
    sem_t writelock;
    sem_t mutex;
    int AR; // number of Active Readers
} rwlock_t;

void rwlock_init(rwlock_t *rw)
{
    rw->AR = 0;
    sem_init(&rw->mutex, 0, 1);
    sem_init(&rw->writelock, 0, 1);
}

void rwlock_acquire_readlock(rwlock_t *rw)
{
    sem_wait(&rw->mutex);
    rw->AR++;
    if (rw->AR == 1)
        sem_wait(&rw->writelock);
    sem_post(&rw->mutex);
}

void rwlock_release_readlock(rwlock_t *rw)
{
    sem_wait(&rw->mutex);
    rw->AR--;
    if (rw->AR == 0)
        sem_post(&rw->writelock);
    sem_post(&rw->mutex);
}

void rwlock_acquire_writelock(rwlock_t *rw)
{
    sem_wait(&rw->writelock);
}

void rwlock_release_writelock(rwlock_t *rw)
{
    sem_post(&rw->writelock);
}

//
// Don't change the code below (just use it!) But fix it if bugs are found!
//

int loops;
int DB = 0;

typedef struct
{
    int thread_id;
    int job_type; // 0: reader, 1: writer
    int arrival_delay;
    int running_time;
} arg_t;

sem_t print_lock;

#define TAB 10
void space(int s)
{
    sem_wait(&print_lock);
    int i;
    for (i = 0; i < s * TAB; i++)
        printf(" ");
}

void space_end()
{
    sem_post(&print_lock);
}

#define TICK sleep(1) // 1/100초 단위로 하고 싶으면 usleep(10000)
rwlock_t rwlock;

void *reader(void *arg)
{
    arg_t *args = (arg_t *)arg;

    TICK;
    rwlock_acquire_readlock(&rwlock);
    // start reading
    int i;
    for (i = 0; i < args->running_time - 1; i++)
    {
        TICK;
        space(args->thread_id);
        printf("reading %d of %d\n", i, args->running_time);
        space_end();
    }
    TICK;
    space(args->thread_id);
    printf("reading %d of %d, DB is %d\n", i, args->running_time, DB);
    space_end();
    // end reading
    TICK;
    rwlock_release_readlock(&rwlock);
    return NULL;
}

void *writer(void *arg)
{
    arg_t *args = (arg_t *)arg;

    TICK;
    rwlock_acquire_writelock(&rwlock);
    // start writing
    int i;
    for (i = 0; i < args->running_time - 1; i++)
    {
        TICK;
        space(args->thread_id);
        printf("writing %d of %d\n", i, args->running_time);
        space_end();
    }
    TICK;
    DB++;
    space(args->thread_id);
    printf("writing %d of %d, DB is %d\n", i, args->running_time, DB);
    space_end();
    // end writing
    TICK;
    rwlock_release_writelock(&rwlock);

    return NULL;
}

void *worker(void *arg)
{
    arg_t *args = (arg_t *)arg;
    int i;
    for (i = 0; i < args->arrival_delay; i++)
    {
        TICK;
        space(args->thread_id);
        printf("arrival delay %d of %d\n", i, args->arrival_delay);
        space_end();
    }
    if (args->job_type == 0)
        reader(arg);
    else if (args->job_type == 1)
        writer(arg);
    else
    {
        space(args->thread_id);
        printf("Unknown job %d\n", args->thread_id);
        space_end();
    }
    return NULL;
}

#define MAX_WORKERS 10

int main(int argc, char *argv[])
{
    int num_workers;
    pthread_t p[MAX_WORKERS];
    arg_t a[MAX_WORKERS];

    int opt;
    bool is_n = false, is_a = false;
    extern char *optarg;

    while ((opt = getopt(argc, argv, "n:a:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            is_n = true;
            num_workers = atoi(optarg);
            break;
        case 'a':
            is_a = true;
            int idx = 0;
            char *next_ptr;
            char *ptr = strtok_r(optarg, ",", &next_ptr);
            while (ptr != NULL)
            {
                int sub_idx = 0;
                char *sub_next_ptr;
                char *sub_ptr = strtok_r(ptr, ":", &sub_next_ptr);
                a[idx].thread_id = idx;
                while (sub_ptr != NULL)
                {
                    switch (sub_idx)
                    {
                    case 0:
                        a[idx].job_type = atoi(sub_ptr);
                        break;
                    case 1:
                        a[idx].arrival_delay = atoi(sub_ptr);
                        break;
                    case 2:
                        a[idx].running_time = atoi(sub_ptr);
                        break;
                    }
                    sub_ptr = strtok_r(NULL, ":", &sub_next_ptr);
                    sub_idx++;
                }
                ptr = strtok_r(NULL, ",", &next_ptr);
                idx++;
            }
            break;
        }
    }

    if (!(is_n & is_a))
    {
        printf("You must specify the n and a options.");
        return -1;
    }

    // a[i]가 잘 적용됬는지 확인
    // for (int i = 0; i < num_workers; i++)
    // {
    //     printf("%d %d %d %d\n", a[i].thread_id, a[i].job_type, a[i].arrival_delay, a[i].running_time);
    // }

    rwlock_init(&rwlock);

    printf("begin\n");
    printf(" ... heading  ...  \n"); // a[]의 정보를 반영해서 헤딩 라인을 출력

    for (int i = 0; i < num_workers; i++)
        pthread_create(&p[i], NULL, worker, &a[i]);

    for (int i = 0; i < num_workers; i++)
        pthread_join(p[i], NULL);

    // printf("end: DB %d\n", value);

    return 0;
}
