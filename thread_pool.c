#include "thread_pool.h"
#include "pthread.h"
#include <stdlib.h>
#include <stdio.h>

// 表示任务
typedef struct task{
  handler_t func;
  void *arg;
}task_t;

// 任务队列
typedef struct task_queue
{
  int head;// 队头
  int last;// 队尾
  int count;// 任务数
  task_t * task;//指向一个任务数组
}task_queue_t;

// 线程池的manager
struct thread_pool_t
{
  pthread_mutex_t mutex; // 互斥锁 线程对任务的互斥访问
  pthread_cond_t cond; // 条件变量 线程同步
  pthread_t * threads; // 指向一个线程
  task_queue_t task_queue; // 保存线程队列

  int close; // 线程池是否停止
  int thread_prosses_count;// 线程池运行线程的数量
  int thread_count;// 线程池线程数量
  int task_queue_size;// 工作队列的大小
};

void *worker_func(void * arg)
{
  thread_pool_t * pool = (thread_pool_t*)arg;
  task_queue_t * task_que;
  task_t task;
  while(1)
  {
    pthread_mutex_lock(&pool->mutex);
    task_que = &pool->task_queue;
    while (task_que->count==0 && pool->close==0)
    {
      pthread_cond_wait(&pool->cond,&pool->mutex);
    }
    
    if(pool->close == 1)
    {
      break;;
    }

    task = task_que->task[task_que->head]; // 取出一个任务
    task_que->head = (task_que->head + 1) % pool->task_queue_size;
    task_que->count--;
    pthread_mutex_unlock(&pool->mutex);
    (*(task.func))(task.arg); // 调用任务处理函数
  }

  --(pool->thread_prosses_count);
  pthread_mutex_unlock(&pool->mutex);
  pthread_exit(NULL);
}

thread_pool_t * thread_pool_create(int thread_count,int queue_size)
{
  thread_pool_t * pool; 
  if(thread_count <= 0 || queue_size <= 0)
  {
    return NULL;
  }
  
  pool =(thread_pool_t*)malloc(sizeof(thread_pool_t)); // 创建一个线程manager
  if(pool == NULL)
  {
    return NULL;
  }

  pool->close = pool->thread_prosses_count = pool->thread_count = 0; // 初始化
  pool->task_queue_size = queue_size; 
  pool->task_queue.head = pool->task_queue.last = pool->task_queue.count = 0;
  pool->task_queue.task = (task_t*)malloc(sizeof(task_t)*queue_size); // 初始化任务数组

  if(pool->task_queue.task == NULL)
  {
    free(pool);
    return NULL;
  }

  pool->threads = (pthread_t *) malloc(sizeof(pthread_t)* thread_count);// 初始化线程
  if(pool->threads == NULL)
  {
    free(pool->task_queue.task);
    free(pool);
    return NULL;
  }

  for(int i = 0 ; i < thread_count ; i++)
  {
    // 创建一个线程
    int ret = pthread_create(&(pool->threads[i]),NULL,worker_func,(void*)pool);
    ++(pool->thread_prosses_count);
    ++(pool->thread_count);
  }
    // 初始化互斥锁
    pthread_mutex_init(&pool->mutex,NULL);
    // 初始化条件变量
    pthread_cond_init(&pool->cond,NULL);

    return pool;
}

int thread_pool_destroy(thread_pool_t * pool)
{
  if(pool == NULL)
  {
    return -1;
  }
  int ret = pthread_mutex_lock(&pool->mutex); // 上锁 防止线程继续执行新的工作
  if(ret == 0)
  {
    return -2;
  }

  if(pool->close == 1)
  {
    return -3;
  }

  pool->close = 1;

  ret = pthread_cond_broadcast(&pool->cond); // 唤醒所有睡眠进程
  if(ret == 0)
  {
    return -2;
  }

  ret = pthread_mutex_unlock(&pool->mutex); // 解锁
  if(ret == 0)
  {
    return -2;
  }

  // 让所有线程正常退出
  wait_all_task_done(pool);
  // 释放空间
  free_thread_cap(pool);

}

int wait_all_task_done(thread_pool_t *pool)
{
  for(int i = 0 ; i < pool->thread_count ; i++)
  {
    int ret = pthread_join(pool->threads[i],NULL);
    if(ret == 0)
    {
      return 0;
    }
  }
  return 1;
}

int free_thread_cap(thread_pool_t *pool)
{
  if(pool == NULL)
  {
    return 0;
  }

  if(pool->threads)
  {
    free(pool->threads);
    pool->threads = NULL;
    pthread_mutex_lock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
  }

  if(pool->task_queue.task)
  {
    free(pool->task_queue.task);
    pool->task_queue.task = NULL;
  }

  free(pool);
}

int thread_pool_post(thread_pool_t*pool,handler_t func ,void *arg)
{
  if(pool == NULL || func == NULL)
  {
    return 0;
  }
  // 上锁
  pthread_mutex_lock(&pool->mutex);

  if(pool->close == 1)
  {
    pthread_mutex_unlock(&pool->mutex);
    return -1;
  }

  if(pool->task_queue_size == pool->task_queue.count)
  {
    pthread_mutex_unlock(&pool->mutex);
    return -1;
  }

  task_queue_t * task_queue = &pool->task_queue;
  // 找到队列中插入任务的位置
  task_t * task = &task_queue->task[task_queue->last];
  task->func = func;
  task->arg = arg;
  task_queue->last = (task_queue->last + 1) % pool->task_queue_size;
  ++(task_queue->count);
  pthread_cond_signal(&pool->cond);
  pthread_mutex_unlock(&pool->mutex);
  
  return 0;

}