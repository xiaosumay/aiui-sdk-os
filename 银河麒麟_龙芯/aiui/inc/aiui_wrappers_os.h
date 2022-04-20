#ifndef __AIUI_WRAPPERS_OS_H__
#define __AIUI_WRAPPERS_OS_H__

/*线程属性，如有额外参数，在此处添加*/
typedef struct aiui_thread_param_s
{
    int deatch;       /*是否分离*/
    int priority;     /*initial thread priority */
    int stack_size;   /* stack size requirements in bytes; 0 is default stack size */
    const char *name; /* thread name. */
} aiui_thread_param_t;

void *aiui_hal_malloc(int size);

void aiui_hal_free(void *ptr);

void aiui_print_mem_info();

void aiui_hal_sleep(int time);    // 毫秒

int aiui_hal_thread_create(void **thread_handle,
                           aiui_thread_param_t *param,
                           void *(*start_routine)(void *),
                           void *arg);

void aiui_hal_thread_destroy(void *thread_handle);

void *aiui_hal_sem_create(void);

void aiui_hal_sem_destroy(void *sem);

void aiui_hal_sem_post(void *sem);

int aiui_hal_sem_wait(void *sem, unsigned int timeout_ms);

void aiui_hal_sem_clear(void *sem);

void *aiui_hal_mutex_create(void);

void aiui_hal_mutex_destroy(void *mutex);

int aiui_hal_mutex_lock(void *mutex);

int aiui_hal_mutex_unlock(void *mutex);

#endif    // __AIUI_WRAPPERS_OS_H__
