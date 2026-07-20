#include "thread_pool.h"
#include <pthread.h>
#include <rlist.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

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
	bool shutdown;
	
	pthread_cond_t new_task_condvar;   		// оповещение потоков о новой задаче
	struct rlist task_queue_head;
	int tasks_in_pool;
	pthread_mutex_t mutex;					// защита очереди от одновременного доступа
	/* PUT HERE OTHER MEMBERS */
};

//! the main function, performs the work cycle of the thread
void* worker(void *arg)
{
	struct thread_pool *pool = (struct thread_pool *) arg;
	struct thread_task *task = NULL;
	struct rlist *head = &pool->task_queue_head;

	// pthread_t id = pthread_self();

	while (1)
	{
		pthread_mutex_lock(&pool->mutex);   								// доступ к очереди и счетчику idle_threads в одной секции
		pool->idle_threads++;
		while (rlist_empty(head) && !pool->shutdown) 
		{
            		pthread_cond_wait(&pool->new_task_condvar, &pool->mutex);		// ждем пока не появится новая задача
		}

		// may be this signal for exit ?
		if (pool->shutdown)
		{
			// printf("worker [%lu]: shutdown\n", id);
			pthread_mutex_unlock(&pool->mutex);
			break;
		}

		task = rlist_shift_entry(head, struct thread_task, rlist_node);
		pool->idle_threads--;
		pthread_mutex_unlock(&pool->mutex);
		pthread_mutex_lock(&task->status_mutex);
		task->status = RUNNING;
		pthread_mutex_unlock(&task->status_mutex);

		// printf("worker [%lu]: I am starting task %p\n", id, task);
		task->result = task->function(task->arg);

		pthread_mutex_lock(&task->status_mutex);
		task->status = END;
		pthread_mutex_unlock(&task->status_mutex);

		pthread_mutex_lock(&pool->mutex);
		pool->tasks_in_pool--;
		pthread_mutex_unlock(&pool->mutex);

		pthread_cond_broadcast(&task->is_finished);
	}
	return NULL;
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
	pool_ptr->shutdown = 0;
	pool_ptr->tasks_in_pool = 0;

	pthread_cond_init(&pool_ptr->new_task_condvar, NULL);
	pthread_mutex_init(&pool_ptr->mutex, NULL);
	rlist_create(&pool_ptr->task_queue_head);

	*pool = pool_ptr;
	return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	// printf("thread_count = %d\n", pool->thread_count);
	return pool->thread_count;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	// printf("test: thread_pool_delete\n");
	/* IMPLEMENT THIS FUNCTION */   // вначале простое удаление, удаление памяти, останов потоков
	pthread_mutex_lock(&pool->mutex);
	if (pool->tasks_in_pool > 0)
	{
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_HAS_TASKS;
	}
	// printf("test: set pool->shutdown = 1\n");
	pool->shutdown = 1;
	// printf("test: send signal\n");
	pthread_cond_broadcast(&pool->new_task_condvar);      // all threads should to finish
	pthread_mutex_unlock(&pool->mutex);

	for (uint32_t i = 0; i < pool->thread_limit; ++i)
	{
		pthread_join(pool->threads[i], NULL);
	}

	// теперь, я сразу разрушаю мютекс ?
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
	if (pool->tasks_in_pool >= TPOOL_MAX_TASKS)
	{
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	else        // как реализовать ленивое создание потока ?
	{
		// add task to queue
		rlist_add_tail_entry(&pool->task_queue_head, task, rlist_node);
		pool->tasks_in_pool++;
		task->status = WAITING;

		// 2. Do I need to create a new thread?
		if (pool->thread_count < pool->thread_limit)
		{
			if (pool->idle_threads == 0)
			{
				pthread_create(&pool->threads[pool->thread_count], NULL, worker, pool);
				pool->thread_count++;
			} 
		}
		// just send signal
		pthread_cond_broadcast(&pool->new_task_condvar);
		pthread_mutex_unlock(&pool->mutex);
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
	else if (END == task->status)
	{
		// printf("Test thread: Oh! the task is already completed\n");
		*result = task->result;
		pthread_mutex_unlock(&task->status_mutex);
	}
	else
	{
		while (END != task->status)
		{
			// printf("Test thread: I should wait result\n");
			pthread_cond_wait(&task->is_finished, &task->status_mutex);		
			*result = task->result;
			// printf("Test thread: Task is completed, result = %d\n", *((int *)task->result));
		}
		pthread_mutex_unlock(&task->status_mutex);
	}
	return 0;
}

#if NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	pthread_mutex_lock(&task->status_mutex);
	if (NONE == task->status)
	{
		pthread_mutex_unlock(&task->status_mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	if (END == task->status)
	{
		// printf("Test thread: Oh! the task is already completed\n");
		*result = task->result;
		pthread_mutex_unlock(&task->status_mutex);
		return 0;
	}

	if (timeout == 0.0)
	{
		pthread_mutex_unlock(&task->status_mutex);
		return TPOOL_ERR_TIMEOUT;
	}

	// Вычисляем абсолютное время дедлайна
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout;
    ts.tv_nsec += (long)((timeout - (time_t)timeout) * 1e9);
    if (ts.tv_nsec >= 1000000000L) 
    {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    int ret = 0;
	while (END != task->status)
	{
		// printf("Test thread: I should wait result\n");
		int rc = pthread_cond_timedwait(&task->is_finished, &task->status_mutex, &ts);		
		// printf("Test thread: Task is completed, result = %d\n", *((int *)task->result));
		
		if (rc == ETIMEDOUT) 
		{
            // Таймаут истёк, а задача так и не завершилась
            ret = TPOOL_ERR_TIMEOUT;
            break;
        }
	}

	if (ret == 0)
	{
		*result = task->result;
	}

	pthread_mutex_unlock(&task->status_mutex);
	return ret;
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
		// printf("thread_task_delete : task->status = %d\n", state);
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
