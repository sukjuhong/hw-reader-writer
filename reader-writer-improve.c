#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct _rwlock_t
{
    int AR, AW, WR, WW;
    pthread_mutex_t m;
    pthread_cond_t okToRead, okToWrite;
} rwlock_t;

rwlock_t rwlock;

void rwlock_init(rwlock_t *rw)
{
    rw->AR = 0;
    rw->AW = 0;
    rw->WR = 0;
    rw->WW = 0;
    pthread_cond_init(&(rw->m), NULL);
    pthread_cond_init(&(rw->okToRead), NULL);
    pthread_cond_init(&(rw->okToWrite), NULL);
}

void rwlock_acquire_readlock(rwlock_t *rw)
{
    pthread_mutex_lock(&rw->m);
    while (rw->AW > 0)
    { // Bad
        rw->WR++;
        Cond_wait(&rw->okToRead, &rw->m);
        rw->WR--;
    }
    rw->AR++;
    pthread_mutex_unlock(&rw->m);
}

void rwlock_release_readlock(rwlock_t *rw)
{
    pthread_mutex_unlock(&rw->m);
    rw->AR--;
    if (rw->AR == 0 && rw->WW > 0)
        pthread_cond_signal(&rw->okToWrite);
    pthread_mutex_unlock(&rw->m);
}

void rwlock_acquire_writelock(rwlock_t *rw)
{
    pthread_mutex_unlock(&rw->m);
    while ((rw->AW + rw->AR) > 0)
    {
        rw->WW++;
        pthread_cond_broadcast(&rw->okToWrite);
        rw->WW--;
    }
    rw->AW++;
    pthread_mutex_unlock(&rw->m);
}

void rwlock_release_writelock(rwlock_t *rw)
{
    pthread_mutex_unlock(&rw->m);
    rw->AW--;
    if (rw->WW > 0)
    {
        Cond_signal(&rw->okToWrite);
    }
    else if (rw->WR > 0)
    {
        Cond_broadcast(&rw->okToRead);
    }
    pthread_mutex_unlock(&rw->m);
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

    // command line argument로 공급 받거나
    // 예: -n 6 -a 0:0:5,0:1:8,1:3:4,0:5:7,1:6:2,0:7:4    또는   -n 6 -a r:0:5,r:1:8,w:3:4,r:5:7,w:6:2,r:7:4
    // 아래 코드에서 for-loop을 풀고 배열 a에 직접 쓰는 방법으로 worker 세트를 구성한다.

    int num_workers;
    pthread_t p[MAX_WORKERS];
    arg_t a[MAX_WORKERS];

    for (int i = 0; i < argc; i++)
    {
        printf("%s", argv[i]);
    }

    rwlock_init(&rwlock);

    int i;
    // for (i = 0; i < num_workers; i++)
    // {
    //     a[i].thread_id = i;
    //     a[i].job_type = ...;
    //     a[i].arrival_delay = ...;
    //     a[i].running_time = ...;
    // }

    printf("begin\n");
    printf(" ... heading  ...  \n"); // a[]의 정보를 반영해서 헤딩 라인을 출력

    for (i = 0; i < num_workers; i++)
        pthread_create(&p[i], NULL, worker, &a[i]);

    for (i = 0; i < num_workers; i++)
        pthread_join(p[i], NULL);

    // printf("end: value %d\n", value);

    return 0;
}