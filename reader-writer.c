#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define TICK_HALF usleep(250000)
#define TICK usleep(500000)
#define MAX_WORKERS 10

typedef struct _rwlock_t
{
    sem_t writelock;
    sem_t mutex;
    int AR; // number of Active Readers
} rwlock_t;

typedef struct _ptlock_t
{
    sem_t mutex;
    char buffer[MAX_WORKERS][50];
    bool done;
} ptlock_t;

typedef struct
{
    int thread_id;
    int job_type; // 0: reader, 1: writer
    int arrival_delay;
    int running_time;
} arg_t;

rwlock_t rwlock;
ptlock_t ptlock;
int num_workers;
int cur_time;
int DB;

void rwlock_init(rwlock_t *rw)
{
    rw->AR = 0;
    sem_init(&rw->mutex, 0, 1);
    sem_init(&rw->writelock, 0, 1);
}

void ptlock_init(ptlock_t *pt)
{
    pt->done = false;
    memset(pt->buffer, '\0', sizeof(pt->buffer));
    sem_init(&pt->mutex, 0, 1);
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

void *printer(void *arg)
{
    TICK_HALF;
    while (!ptlock.done)
    {
        TICK;
        printf("%d\t%d\tsomething\t", cur_time, rwlock.AR);
        for (int i = 0; i < num_workers; i++)
        {
            printf("%s\t", *ptlock.buffer[i] != '\0' ? ptlock.buffer[i] : "\t");
        }
        printf("\n");
        memset(ptlock.buffer, '\0', sizeof(ptlock.buffer));
        cur_time++;
    }
    return NULL;
}

void *reader(void *arg)
{
    arg_t *args = (arg_t *)arg;

    TICK;
    sem_wait(&ptlock.mutex);
    sprintf(ptlock.buffer[args->thread_id], "acquire\t");
    sem_post(&ptlock.mutex);
    rwlock_acquire_readlock(&rwlock);
    // start reading
    int i;
    for (i = 1; i <= args->running_time; i++)
    {
        TICK;
        sem_wait(&ptlock.mutex);
        sprintf(ptlock.buffer[args->thread_id], "reading %d/%d", i, args->running_time);
        sem_post(&ptlock.mutex);
    }
    // end reading
    TICK;
    rwlock_release_readlock(&rwlock);
    sem_wait(&ptlock.mutex);
    sprintf(ptlock.buffer[args->thread_id], rwlock.AR ? "release\t" : "rel/wake\t");
    sem_post(&ptlock.mutex);
    return NULL;
}

void *writer(void *arg)
{
    bool flag = false;
    arg_t *args = (arg_t *)arg;

    TICK;
    sem_wait(&ptlock.mutex);
    sprintf(ptlock.buffer[args->thread_id], rwlock.AR ? "acq/sleep" : "acquire");
    if (rwlock.AR) flag = true;
    sem_post(&ptlock.mutex);
    rwlock_acquire_writelock(&rwlock);
    if (flag)
    {
        sem_wait(&ptlock.mutex);
        sprintf(ptlock.buffer[args->thread_id], "ready\t");
        if (rwlock.AR) flag = true;
        sem_post(&ptlock.mutex);
    }
    // start writing
    int i;
    for (i = 1; i <= args->running_time; i++)
    {
        TICK;
        sem_wait(&ptlock.mutex);
        if (i == args->running_time) DB++;
        sprintf(ptlock.buffer[args->thread_id], "writing %d/%d", i, args->running_time);
        sem_post(&ptlock.mutex);
    }

    // end writing
    TICK;
    rwlock_release_writelock(&rwlock);
    sem_wait(&ptlock.mutex);
    sprintf(ptlock.buffer[args->thread_id], "release");
    sem_post(&ptlock.mutex);

    return NULL;
}

void *worker(void *arg)
{
    arg_t *args = (arg_t *)arg;
    int i;
    for (i = 1; i <= args->arrival_delay; i++)
    {
        TICK;
        sem_wait(&ptlock.mutex);
        sprintf(ptlock.buffer[args->thread_id], "delay %d/%d", i, args->arrival_delay);
        sem_post(&ptlock.mutex);
    }
    if (args->job_type == 0)
        reader(arg);
    else if (args->job_type == 1)
        writer(arg);
    else
    {
        TICK;
        sem_wait(&ptlock.mutex);
        sprintf(ptlock.buffer[args->thread_id], "unknown");
        sem_post(&ptlock.mutex);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t worker_thread[MAX_WORKERS];
    pthread_t printer_thread;
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
            if (num_workers > 10)
            {
                printf("The num_worker variable must be less than 10.");
                return -1;
            }
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

    rwlock_init(&rwlock);
    ptlock_init(&ptlock);

    printf("begin\n");
    printf(" ... heading  ...  \n"); // a[]의 정보를 반영해서 헤딩 라인을 출력
    printf("Time\tAR\twritelock->Q\t");
    for (int i = 0; i < num_workers; i++)
    {
        printf("%s%d(%d:%d)\t\t", a[i].job_type ? "W" : "R", a[i].thread_id, a[i].arrival_delay, a[i].running_time);
    }
    printf("\n");

    for (int i = 0; i < num_workers; i++)
    {
        pthread_create(&worker_thread[i], NULL, worker, &a[i]);
    }
    pthread_create(&printer_thread, NULL, printer, NULL);

    for (int i = 0; i < num_workers; i++)
    {
        pthread_join(worker_thread[i], NULL);
    }
    ptlock.done = true;
    pthread_join(printer_thread, NULL);

    printf("end: DB %d\n", DB);

    return 0;
}
