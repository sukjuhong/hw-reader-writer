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

#define TICK usleep(500000) // 1/100초 단위로 하고 싶으면 usleep(10000)
rwlock_t rwlock;
sem_t print_lock;
int num_workers;
int DB;
char comment[24];

typedef struct
{
    int thread_id;
    int job_type; // 0: reader, 1: writer
    int arrival_delay;
    int running_time;
} arg_t;

void print_timeline(int thread_id, char *msg)
{
    char line[100] = "| %d\t | something\t |";
    for (int i = 0; i < num_workers; i++)
    {
        if (i == thread_id)
            strcat(line, " %s\t |");
        else
            strcat(line, " \t\t |");
    }
    strcat(line, "\n");
    printf(line, rwlock.AR, msg);
}

void *reader(void *arg)
{
    arg_t *args = (arg_t *)arg;

    TICK;
    sem_wait(&print_lock);
    sprintf(comment, "acquire");
    print_timeline(args->thread_id, comment);
    sem_post(&print_lock);
    rwlock_acquire_readlock(&rwlock);
    // start reading
    int i;
    for (i = 1; i <= args->running_time - 1; i++)
    {

        TICK;
        sem_wait(&print_lock);
        sprintf(comment, "reading %d/%d", i, args->running_time);
        print_timeline(args->thread_id, comment);
        sem_post(&print_lock);
    }
    TICK;
    sem_wait(&print_lock);
    sprintf(comment, "reading %d/%d", i, args->running_time);
    print_timeline(args->thread_id, comment);
    sem_post(&print_lock);

    // end reading
    TICK;
    rwlock_release_readlock(&rwlock);
    sem_wait(&print_lock);
    sprintf(comment, rwlock.AR == 0 ? "rel/wake" : "release");
    print_timeline(args->thread_id, comment);
    sem_post(&print_lock);
    return NULL;
}

void *writer(void *arg)
{
    arg_t *args = (arg_t *)arg;

    TICK;
    sem_wait(&print_lock);
    sprintf(comment, rwlock.AR ? "acq/sleep" : "acquire");
    print_timeline(args->thread_id, comment);
    sem_post(&print_lock);
    rwlock_acquire_writelock(&rwlock);
    // start writing
    int i;
    for (i = 1; i <= args->running_time - 1; i++)
    {
        TICK;
        sem_wait(&print_lock);
        sprintf(comment, "writing %d/%d", i, args->running_time);
        print_timeline(args->thread_id, comment);
        sem_post(&print_lock);
    }
    TICK;
    DB++;
    sem_wait(&print_lock);
    sprintf(comment, "writing %d/%d", i, args->running_time);
    print_timeline(args->thread_id, comment);
    sem_post(&print_lock);

    // end writing
    TICK;
    sem_wait(&print_lock);
    sprintf(comment, "release");
    print_timeline(args->thread_id, comment);
    sem_post(&print_lock);
    rwlock_release_writelock(&rwlock);

    return NULL;
}

void *worker(void *arg)
{
    arg_t *args = (arg_t *)arg;
    int i;
    for (i = 1; i <= args->arrival_delay; i++)
    {
        TICK;
    }
    if (args->job_type == 0)
        reader(arg);
    else if (args->job_type == 1)
        writer(arg);
    else
    {
        sem_wait(&print_lock);
        print_timeline(args->thread_id, "Unknown Job");
        sem_post(&print_lock);
    }
    return NULL;
}

#define MAX_WORKERS 10

int main(int argc, char *argv[])
{
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
    sem_init(&print_lock, 0, 1);

    printf("begin\n");
    printf(" ... heading  ...  \n"); // a[]의 정보를 반영해서 헤딩 라인을 출력
    char heading[200] = "| AR\t | writelock->Q\t |";
    for (int i = 0; i < num_workers; i++)
    {
        char heading_thread[24];
        sprintf(heading_thread, " %c%d\t\t |", a[i].job_type ? 'W' : 'R', a[i].thread_id);
        strcat(heading, heading_thread);
    }
    printf("%s\n", heading);

    for (int i = 0; i < num_workers; i++)
    {
        pthread_create(&p[i], NULL, worker, &a[i]);
    }

    for (int i = 0; i < num_workers; i++)
    {
        pthread_join(p[i], NULL);
    }

    printf("end: DB %d\n", DB);

    return 0;
}
