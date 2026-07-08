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

struct task_queue
{
	struct rlist head;
	int size;
	pthread_mutex_t mutex;			// защита очереди от одновременного доступа
}

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
	uint32_t thread_count;
	uint32_t thread_limit;
	pthread_cond_t new_task_condvar;   		// оповещение потоков о новой задаче
	struct task_queue *queue;
	int active_workers;
	pthread_mutex_t active_workers_mutex;
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
task_queue_push(struct task_queue *queue, struct thread_task *task)
{
	pthread_mutex_lock(&queue->mutex);
	if (queue->size >= TPOOL_MAX_TASKS)
	{
		pthread_mutex_unlock(&queue->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	
	rlist_add_tail_entry(&queue->head, task, rlist_node);
	queue->size++;
	
	pthread_mutex_unlock(&queue->mutex);
	return 0;
}

struct thread_task*
task_queue_pop(struct task_queue *queue)
{
	pthread_mutex_lock(&queue->mutex);
	if (queue->size == 0)
	{
		pthread_mutex_unlock(&mutex);
		return NULL;
	}
	struct thread_task *task = rlist_shift_entry(&queue->head, struct thread_task, rlist_node)ж
	queue->size--;
	
	pthread_mutex_unlock(&queue->mutex);
	return task;
}

int task_queue_empty(struct task_queue *queue)
{
	pthread_mutex_lock(&queue->mutex);
	int ret = rlist_empty(&queue->head);
	pthread_mutex_unlock(&queue->mutex);
	return ret;
}

int 
create_thread(pthread_t *thread_id, thread_task_f function, void *arg)
{
	int result = pthread_create(thread_id, NULL, thread_function, arg);
	if (0 == result) 
	{
        // эта функция не нужна ????
    }
    else
    {
    	fprintf(stderr, "Error creating thread\n");
        return 1;
    }
}


//! the main function, performs the work cycle of the thread
void worker(struct thread_pool *pool)
{
	// firstly increment
	increment_active_workers(pool);
	// second check task in queue
	int run = 1;            			// how to terminate thread ?
	int state = CHECK_TASK;
	struct thread_task *task = NULL;
	struct task_queue *task_queue = &pool->task_queue_head;

	while (run)
	{
		pthread_mutex_lock(&task_queue->mutex);
		while (rlist_empty(&task_queue->head)) 
		{
            pthread_cond_wait(&condvar, &task_queue->mutex);
		}

		task = rlist_shift_entry(&task_queue->head, struct thread_task, rlist_node);
		task_queue->size--;

		pthread_mutex_unlock(&task_queue->mutex);

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
	pool_ptr->active_workers = 0;

	pthread_cond_init(&pool_ptr->task_condvar, NULL);
	pthread_mutex_init(&pool_ptr->active_workers_mutex, NULL);

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

	rlist_add_tail_entry(&pool->task_queue_head, &task->rlist_node, rlist_node);
	return TPOOL_ERR_NOT_IMPLEMENTED;
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
