#include "thread_pool.h"
#include <pthread.h>
#include <rlist.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

enum TASK_STATE
{
	NONE,
	WAITING,
	RUNNING,
	END
};

enum WORKER_STATE
{
	CHECK_TASK,
	EXECUTE,
	WAITING,
	TERMINATE
};

struct thread_task
{
	thread_task_f function;
	void *arg;
	int32_t status;
	struct rlist rlist_node;
	pthread_mutex_t status_mutex;
	/* PUT HERE OTHER MEMBERS */
};

struct thread_pool
{
	pthread_t *threads;
	uint32_t thread_count;					// сколько сейчас потоков
	uint32_t idle_threads;					// сколько сейчас потоков в работе
	uint32_t thread_limit;
	
	pthread_cond_t new_task_condvar;   		// оповещение потоков о новой задаче
	struct rlist head;
	int size;
	pthread_mutex_t mutex;					// защита очереди от одновременного доступа
	/* PUT HERE OTHER MEMBERS */
};

int get_task_status(struct thread_task *task)
{
	pthread_mutex_lock(&task->status_mutex);
	int state = task->status;
	pthread_mutex_unlock(&task->status_mutex);
	return state;
}

void set_task_status(struct thread_task *task, int status)
{
	pthread_mutex_lock(&task->status_mutex);
	task->status = status;
	pthread_mutex_unlock(&task->status_mutex);
}

int
task_queue_init(struct task_queue *queue)
{
	queue->size = 0;
	rlist_create(&queue->head);
	return pthread_mutex_init(&queue->mutex, NULL);
}

int 
task_queue_push(struct thread_pool *pool, struct thread_task *task)
{
	pthread_mutex_lock(&pool->mutex);
	if (pool->size >= TPOOL_MAX_TASKS)
	{
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	
	rlist_add_tail_entry(&pool->head, task, rlist_node);
	pool->size++;
	
	pthread_mutex_unlock(&pool->mutex);
	return 0;
}

//! the main function, performs the work cycle of the thread
void* worker(void *arg)
{
	struct thread_pool *pool = (struct thread_pool *) arg;
	// firstly increment
	// second check task in queue
	struct thread_task *task = NULL;
	struct task_queue *task_queue = &pool->head;

	while (1)
	{
		pthread_mutex_lock(&task_queue->mutex);
		while (rlist_empty(&task_queue->head)) 
		{
            pthread_cond_wait(&pool->new_task_condvar, &pool->mutex);
		}

		task = rlist_shift_entry(task_queue, struct thread_task, rlist_node);
		task_queue->size--;
		pool->idle_threads--;
		pthread_mutex_unlock(&pool->mutex);

		set_task_status(task, RUNNING);
		task->function(arg);
		set_task_status(task, END);
	}
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (max_thread_count > TPOOL_MAX_THREADS || max_thread_count <= 0)
	{
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	struct thread_pool *pool_ptr;
	pool_ptr = malloc(sizeof(struct thread_pool));
	pool_ptr->threads = malloc(max_thread_count * sizeof(pthread_t));
	pool_ptr->thread_limit = max_thread_count;
	pool_ptr->thread_count = 0;
	pool_ptr->idle_threads = 0;

	pthread_cond_init(&pool_ptr->new_task_condvar, NULL);
	pthread_mutex_init(&pool_ptr->mutex, NULL);

	*pool = pool_ptr;
	return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	return pool->thread_count;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)pool;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	// 1. Не превышено ли количество задач ?
	pthread_mutex_lock(&pool->mutex);
	if (pool->size >= TPOOL_MAX_TASKS)
	{
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	else        // как реализовать ленивое создание потока ?
	{
		// 2. Есть ли свободные потоки ?
		if (pool->thread_count < pool->thread_limit)
		{
			if (pool->idle_threads == 0)
			{
				int ret = pthread_create(&pool->threads[pool->thread_count++], NULL, worker, pool);
				return ret;
			} 
		}
		// just send signal
		pthread_cond_broadcast(&pool->new_task_condvar);
	}
	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)function;
	(void)arg;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return false;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return false;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#if NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#if NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
