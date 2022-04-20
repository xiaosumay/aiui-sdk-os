#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include "aiui_log.h"
#include "aiui_wrappers_os.h"

#define AIUI_TAG "aiui_os"
static int malloc_count = 0;
static int free_count = 0;

void *aiui_hal_malloc(int size)
{
    malloc_count++;
    return malloc(size);
}

void aiui_hal_free(void *ptr)
{
    free_count++;
    free(ptr);
}

void aiui_print_mem_info()
{
    LOGD(AIUI_TAG, "malloc = %d, free = %d", malloc_count, free_count);
}

void aiui_hal_sleep(int time)
{
    usleep(1000 * time);
}

int aiui_hal_thread_create(void **thread_handle,
                           aiui_thread_param_t *param,
                           void *(*start_routine)(void *),
                           void *arg)
{
    //if (param != NULL) LOGE(AIUI_TAG, "thread name = %s", param->name);
    int ret = pthread_create((pthread_t *)thread_handle, NULL, start_routine, arg);
    if (ret != 0) {
        LOGD(AIUI_TAG, "pthread_create error: %d\n", (int)ret);
        return -1;
    }
    if (param == NULL || param->deatch == 1) {
        pthread_detach((pthread_t)*thread_handle);
        //LOGD(AIUI_TAG, "attach thread");
    }
    return 0;
}

void aiui_hal_thread_destroy(void *thread_handle)
{
    if (thread_handle != NULL) {
        pthread_join((pthread_t)thread_handle, NULL);
    }
}

void *aiui_hal_sem_create(void)
{
    sem_t *sem = (sem_t *)malloc(sizeof(sem_t));
    if (NULL == sem) {
        return NULL;
    }

    if (0 != sem_init(sem, 0, 0)) {
        free(sem);
        return NULL;
    }

    return sem;
}

void aiui_hal_sem_destroy(void *sem)
{
    sem_destroy((sem_t *)sem);
    free(sem);
}

void aiui_hal_sem_post(void *sem)
{
    sem_post((sem_t *)sem);
}

int aiui_hal_sem_wait(void *sem, unsigned int timeout_ms)
{
    if (timeout_ms <= 0) {
        sem_wait((sem_t *)sem);
        return 0;
    } else {
        struct timespec ts;
        int s;
        do {
            if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
                return -1;
            }

            s = 0;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_nsec -= 1000000000;
                s = 1;
            }

            ts.tv_sec += timeout_ms / 1000 + s;

        } while (((s = sem_timedwait((sem_t *)sem, &ts)) != 0) && errno == EINTR);

        return (s == 0) ? 0 : -1;
    }
}

void aiui_hal_sem_clear(void *sem)
{
    int value = 0;
    sem_getvalue((sem_t *)sem, &value);
    //LOGE(AIUI_TAG, "value = %d", value);
    while (value-- != 0) {
        aiui_hal_sem_wait(sem, -1);    // 清空信号量
    }
}

void *aiui_hal_mutex_create(void)
{
    int err_num;
    pthread_mutex_t *mutex = (pthread_mutex_t *)aiui_hal_malloc(sizeof(pthread_mutex_t));
    if (NULL == mutex) {
        return NULL;
    }

    if (0 != (err_num = pthread_mutex_init(mutex, NULL))) {
        LOGD(AIUI_TAG, "create mutex failed");
        aiui_hal_free(mutex);
        return NULL;
    }

    return mutex;
}

void aiui_hal_mutex_destroy(void *mutex)
{
    pthread_mutex_destroy((pthread_mutex_t *)mutex);

    aiui_hal_free(mutex);
}

int aiui_hal_mutex_lock(void *mutex)
{
    return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

int aiui_hal_mutex_unlock(void *mutex)
{
    return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}