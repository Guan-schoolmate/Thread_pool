#ifndef _PTHRAD_POOL_
#define _PTHRAD_POOL_


typedef struct thread_pool_t thread_pool_t;
typedef void (*handler_t) (void*) ;

thread_pool_t * thread_pool_create(int thread_count,int queue_size);

int thread_pool_destroy(thread_pool_t * pool);

int thread_pool_post(thread_pool_t*pool,handler_t func ,void *arg);

int wait_all_task_done(thread_pool_t *pool);
int free_thread_cap(thread_pool_t *pool);
#endif
