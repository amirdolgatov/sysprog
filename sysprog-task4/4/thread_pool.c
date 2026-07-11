#include "thread_pool.h"
#include <pthread.h>
#include <rlist.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#include <stdio.h>

enum TASK_STATE
{
	NONE,
	WAITING,
	RUNNING,
	END
};

struct thread_task
{
	thread_task_f function;
	void *arg;
	int32_t status;
	struct rlist rlist_node;
	pthread_mutex_t status_mutex;
	pthread_cond_t is_finished;   		// оповещение потоков о новой задаче
	void *result;
};

struct thread_pool
{
	pthread_t *threads;
	uint32_t thread_count;					// сколько сейчас потоков
	uint32_t idle_threads;					// сколько сейчас потоков в работе
	uint32_t thread_limit;
	
	pthread_cond_t new_task_condvar;   		// оповещение потоков о новой задаче
	struct rlist task_queue_head;
	int task_queue_size;
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

//! the main function, performs the work cycle of the thread
void* worker(void *arg)
{
	struct thread_pool *pool = (struct thread_pool *) arg;
	struct thread_task *task = NULL;
	struct rlist *head = &pool->task_queue_head;

	while (1)
	{
		pthread_mutex_lock(&pool->mutex);   							// доступ к очереди и счетчику idle_threads в одной секции
		while (rlist_empty(head)) 
		{
            pthread_cond_wait(&pool->new_task_condvar, &pool->mutex);		// ждем пока не появится новая задача
		}

		task = rlist_shift_entry(head, struct thread_task, rlist_node);
		pool->task_queue_size--;
		pool->idle_threads--;
		pthread_mutex_unlock(&pool->mutex);

		pthread_mutex_lock(&task->status_mutex);
		task->status = RUNNING;
		pthread_mutex_unlock(&task->status_mutex);

		task->result = task->function(arg);

		pthread_mutex_lock(&task->status_mutex);
		task->status = END;
		pthread_mutex_unlock(&task->status_mutex);
		pthread_cond_broadcast(&task->is_finished);
	}
}

void stop_threads(struct thread_pool *pool)
{
	(void)pool;
	return;
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
	rlist_create(&pool_ptr->task_queue_head);
	pool_ptr->task_queue_size = 0;

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
	/* IMPLEMENT THIS FUNCTION */   // вначале простое удаление, удаление памяти, останов потоков
	stop_threads(pool);

	// handle memory returning
	pthread_mutex_destroy(&pool->mutex);
	pthread_cond_destroy(&pool->new_task_condvar);

	free(pool->threads);
	free(pool);

	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	// 1. Не превышено ли количество задач ?
	pthread_mutex_lock(&pool->mutex);
	if (pool->task_queue_size >= TPOOL_MAX_TASKS)
	{
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	else        // как реализовать ленивое создание потока ?
	{
		// add task to queue
		rlist_add_tail_entry(&pool->task_queue_head, task, rlist_node);
		pool->task_queue_size++;
		task->status = WAITING;

		// 2. Do I need to create a new thread?
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
	struct thread_task *task_ptr = malloc(sizeof(struct thread_task));
	task_ptr->function = function;
	task_ptr->arg = arg;
	task_ptr->status = NONE;
	pthread_cond_init(&task_ptr->is_finished, NULL);
	pthread_mutex_init(&task_ptr->status_mutex, NULL);

	*task = task_ptr;
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	pthread_mutex_lock((pthread_mutex_t *)&task->status_mutex);
	int state = task->status;
	pthread_mutex_unlock((pthread_mutex_t *)&task->status_mutex);
	return state == END;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	pthread_mutex_lock((pthread_mutex_t *)&task->status_mutex);
	int state = task->status;
	pthread_mutex_unlock((pthread_mutex_t *)&task->status_mutex);
	return state == RUNNING;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	pthread_mutex_lock(&task->status_mutex);
	if (NONE == task->status)
	{
		pthread_mutex_unlock(&task->status_mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	else
	{
		while (END != task->status)
		{
			pthread_cond_wait(&task->is_finished, &task->status_mutex);		
			*result = task->result;
			pthread_mutex_unlock(&task->status_mutex);
		}
	}
	return 0;
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
	pthread_mutex_lock(&task->status_mutex);
	int state = task->status;
	pthread_mutex_unlock(&task->status_mutex);

	if ((WAITING == state) || (RUNNING == state))
	{
		return TPOOL_ERR_TASK_IN_POOL;
	}
	else
	{
		printf("thread_task_delete : task->status = %d\n", state);
		// handle memory returning
		pthread_mutex_destroy(&task->status_mutex);
		pthread_cond_destroy(&task->is_finished);
		free(task);
	}
	return 0;
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
