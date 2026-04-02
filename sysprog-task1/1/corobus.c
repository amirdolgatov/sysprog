#include "corobus.h"

#include "libcoro.h"
#include "rlist.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct data_vector {
	unsigned *data;
	size_t size;
	size_t capacity;
};

#if 1 /* Uncomment this if want to use */

/** Append @a count messages in @a data to the end of the vector. */
static void
data_vector_append_many(struct data_vector *vector,
	const unsigned *data, size_t count)
{
	if (vector->size + count > vector->capacity)
	{
		if (vector->capacity == 0)
			vector->capacity = 4;
		else
			vector->capacity *= 2;
		if (vector->capacity < vector->size + count)
		{
			vector->capacity = vector->size + count;
		}
		vector->data = realloc(vector->data,sizeof(vector->data[0]) * vector->capacity);
	}
	memcpy(&vector->data[vector->size], data, sizeof(data[0]) * count);
	vector->size += count;
}

/** Append a single message to the vector. */
static void
data_vector_append(struct data_vector *vector, unsigned data)
{
	data_vector_append_many(vector, &data, 1);
}

/** Pop @a count of messages into @a data from the head of the vector. */
static void
data_vector_pop_first_many(struct data_vector *vector, unsigned *data, size_t count)
{
	assert(count <= vector->size);
	memcpy(data, vector->data, sizeof(data[0]) * count);
	vector->size -= count;
	memmove(vector->data, &vector->data[count], vector->size * sizeof(vector->data[0]));
}

/** Pop a single message from the head of the vector. */
static unsigned
data_vector_pop_first(struct data_vector *vector)
{
	unsigned data = 0;
	data_vector_pop_first_many(vector, &data, 1);
	return data;
}

#endif

/**
 * One coroutine waiting to be woken up in a list of other
 * suspended coros.
 */
struct wakeup_entry
{
	struct rlist base;
	struct coro *coro;
};

/** A queue of suspended coros waiting to be woken up. */
struct wakeup_queue
{
	struct rlist coros;
};

#if 1 /* Uncomment this if want to use */

/** Suspend the current coroutine until it is woken up. */
static void
wakeup_queue_suspend_this(struct wakeup_queue *queue)
{
	struct wakeup_entry entry;
	entry.coro = coro_this();
	rlist_add_tail_entry(&queue->coros, &entry, base);
	// printf("I am coro %p, suspended now\n", entry.coro);
	coro_suspend();
	// printf("I am coro %p, I am back \n", entry.coro);
	rlist_del_entry(&entry, base);
}

/** Wakeup the first coroutine in the queue. */
static void
wakeup_queue_wakeup_first(struct wakeup_queue *queue)
{
	if (rlist_empty(&queue->coros))
		return;

	// todo удалить цикл
	struct wakeup_entry *item;
	rlist_foreach_entry(item, &queue->coros, base)
	{
		// printf("coro address: %p\n", item->coro);
		coro_wakeup(item->coro);
	}

	struct wakeup_entry *entry = rlist_first_entry(&queue->coros,
		struct wakeup_entry, base);
	coro_wakeup(entry->coro);
}

#endif

struct coro_bus_channel
{
	/** Channel max capacity. */
	size_t size_limit;
	/** Coroutines waiting until the channel is not full. */
	struct wakeup_queue send_queue;
	/** Coroutines waiting until the channel is not empty. */
	struct wakeup_queue recv_queue;
	/** Message queue. */
	struct data_vector data;
};

struct coro_bus
{
	struct coro_bus_channel **channels;
	int channel_count;
};

static enum coro_bus_error_code global_error = CORO_BUS_ERR_NONE;

enum coro_bus_error_code
coro_bus_errno(void)
{
	return global_error;
}

void
coro_bus_errno_set(enum coro_bus_error_code err)
{
	global_error = err;
}

#define MAX_CHANNEL_COUNT 15

struct coro_bus *
coro_bus_new(void)
{
	struct coro_bus * coro_new = malloc(sizeof(struct coro_bus));
	coro_new->channels = (struct coro_bus_channel **) malloc(sizeof(struct coro_bus_channel *) * MAX_CHANNEL_COUNT);
	memset(coro_new->channels, 0, sizeof(struct coro_bus_channel *) * MAX_CHANNEL_COUNT);
	coro_new->channel_count = 0;
	coro_bus_errno_set(CORO_BUS_ERR_NONE);
	return coro_new;
}

void
coro_bus_delete(struct coro_bus *bus)
{
	if (bus == NULL)
	{
		return;
	}
	if (0 != bus->channel_count)
	{
		for (int i = 0; i < bus->channel_count; i++)
		{
			if (NULL != bus->channels[i])
			{
				free(bus->channels[i]);
				bus->channels[i] = NULL;                           // в каналах могут быть данные, добавить позднее
			}
		}
	}
	free(bus);
}


int
coro_bus_channel_open(struct coro_bus *bus, size_t size_limit)
{
	int descriptor = -1;
	// ищем место новому каналу
	for (int index = 0; index < MAX_CHANNEL_COUNT; index++)
	{
		if (NULL == bus->channels[index])
		{
			descriptor = index;
			break;
		}
	}

	if (descriptor < 0)
	{
		coro_bus_errno_set(CORO_BUS_ERR_NOT_IMPLEMENTED);
		return -1;
	}

	struct coro_bus_channel * channel = malloc(sizeof(struct coro_bus_channel));
	rlist_create(&channel->send_queue.coros);
	rlist_create(&channel->recv_queue.coros);
	channel->size_limit = size_limit;
	channel->data.size = 0;
	channel->data.capacity = 0;

	bus->channels[descriptor] = channel;
	bus->channel_count++;

	coro_bus_errno_set(CORO_BUS_ERR_NONE);
	return descriptor;
}

void
coro_bus_channel_close(struct coro_bus *bus, int channel)
{
	coro_bus_errno_set(CORO_BUS_ERR_NO_CHANNEL);
	if (channel >= MAX_CHANNEL_COUNT)
	{
		return;
	}

	struct coro_bus_channel* channel_ptr = bus->channels[channel];
	if (NULL != channel_ptr)
	{
		// у канала могут быть очереди на чтение и запись
		// если канал закрывается необходимо освободить очереди

		struct wakeup_entry *item;
		rlist_foreach_entry(item, &channel_ptr->send_queue.coros, base)
		{
			//printf("coro address: %p\n", item->coro);
			coro_wakeup(item->coro);
		}
		rlist_foreach_entry(item, &channel_ptr->recv_queue.coros, base)
		{
			//printf("coro address: %p\n", item->coro);
			coro_wakeup(item->coro);
		}

		free(channel_ptr);				// пока без перемещения данных
		bus->channels[channel] = NULL;
		bus->channel_count--;
		coro_bus_errno_set(CORO_BUS_ERR_NONE);
	}
}

int
coro_bus_send(struct coro_bus *bus, int channel, unsigned data)
{
	/*
	 * Try sending in a loop, until success. If error, then
	 * check which one is that. If 'wouldblock', then suspend
	 * this coroutine and try again when woken up.
	 *
	 * If see the channel has space, then wakeup the first
	 * coro in the send-queue. That is needed so when there is
	 * enough space for many messages, and many coroutines are
	 * waiting, they would then wake each other up one by one
	 * as lone as there is still space.
	 */
	struct coro_bus_channel * bus_channel = bus->channels[channel];
	while(1)
	{
		coro_bus_try_send(bus, channel, data);
		if (CORO_BUS_ERR_NONE == coro_bus_errno())
		{
			if (bus_channel->data.size < bus_channel->size_limit)
			{
				// printf("I am coro %p, just now send data, then I try to weak up next coro in send_queue\n", coro_this());
				wakeup_queue_wakeup_first(&bus_channel->send_queue);
			}
			return 0;
		}
		if (CORO_BUS_ERR_WOULD_BLOCK == coro_bus_errno())
		{
			wakeup_queue_suspend_this(&bus_channel->send_queue);	// go sleep
			continue;
		}
		if (CORO_BUS_ERR_NO_CHANNEL == coro_bus_errno())
		{
			return -1;
		}
	}
}

int
coro_bus_try_send(struct coro_bus *bus, int channel, unsigned data)
{
	/*
	 * Append data if has space. Otherwise 'wouldblock' error.
	 * Wakeup the first coro in the recv-queue! To let it know
	 * there is data.
	 */
	// есть ли такой канал ?
	if (channel >= MAX_CHANNEL_COUNT)
	{
		coro_bus_errno_set(CORO_BUS_ERR_NO_CHANNEL);
		return -1;
	}
	if (NULL == bus->channels)
	{
		coro_bus_errno_set(CORO_BUS_ERR_NO_CHANNEL);
		return -1;
	}
	struct coro_bus_channel * bus_channel = bus->channels[channel];
	if (NULL == bus_channel)
	{
		coro_bus_errno_set(CORO_BUS_ERR_NO_CHANNEL);
		return -1;
	}

	int success = -1;
	if (bus_channel->data.size == bus_channel->size_limit)	// пытаемся записать данные
	{
		coro_bus_errno_set(CORO_BUS_ERR_WOULD_BLOCK);
	}
	else
	{
		data_vector_append(&bus_channel->data, data);
		coro_bus_errno_set(CORO_BUS_ERR_NONE);
		success = 0;
	}

	wakeup_queue_wakeup_first(&bus_channel->recv_queue);	// wakeup first to recv
	return success;
}

int
coro_bus_recv(struct coro_bus *bus, int channel, unsigned *data)
{
	struct coro_bus_channel * bus_channel = bus->channels[channel];
	while(1)
	{
		coro_bus_try_recv(bus, channel, data);
		if (CORO_BUS_ERR_NONE == coro_bus_errno())
		{
			if (0 != bus_channel->data.size)
			{
				// printf("I am coro %p, just now read data, then I try to weak up next coro in recv_queue\n", coro_this());
				wakeup_queue_wakeup_first(&bus_channel->recv_queue);
			}
			return 0;
		}
		if (CORO_BUS_ERR_WOULD_BLOCK == coro_bus_errno())
		{
			wakeup_queue_suspend_this(&bus_channel->recv_queue);	// go sleep
			continue;
		}
		if (CORO_BUS_ERR_NO_CHANNEL == coro_bus_errno())
		{
			return -1;
		}
	}
}

int
coro_bus_try_recv(struct coro_bus *bus, int channel, unsigned *data)
{
	// есть ли такой канал ?
	if (channel >= MAX_CHANNEL_COUNT)
	{
		coro_bus_errno_set(CORO_BUS_ERR_NO_CHANNEL);
		return -1;
	}
	if (NULL == bus->channels)
	{
		coro_bus_errno_set(CORO_BUS_ERR_NO_CHANNEL);
		return -1;
	}
	struct coro_bus_channel * bus_channel = bus->channels[channel];
	if (NULL == bus_channel)
	{
		coro_bus_errno_set(CORO_BUS_ERR_NO_CHANNEL);
		return -1;
	}

	int success = -1;
	if (0 == bus_channel->data.size)	// пытаемся читать данные
	{
		coro_bus_errno_set(CORO_BUS_ERR_WOULD_BLOCK);
	}
	else
	{
		*data = data_vector_pop_first(&bus_channel->data);
		coro_bus_errno_set(CORO_BUS_ERR_NONE);
		success = 0;
	}

	wakeup_queue_wakeup_first(&bus_channel->send_queue);	// wakeup first to write
	return success;
}

#if NEED_BROADCAST

int
coro_bus_broadcast(struct coro_bus *bus, unsigned data)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)bus;
	(void)data;
	coro_bus_errno_set(CORO_BUS_ERR_NOT_IMPLEMENTED);
	return -1;
}

int
coro_bus_try_broadcast(struct coro_bus *bus, unsigned data)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)bus;
	(void)data;
	coro_bus_errno_set(CORO_BUS_ERR_NOT_IMPLEMENTED);
	return -1;
}

#endif

#if NEED_BATCH

int
coro_bus_send_v(struct coro_bus *bus, int channel, const unsigned *data, unsigned count)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)bus;
	(void)channel;
	(void)data;
	(void)count;
	coro_bus_errno_set(CORO_BUS_ERR_NOT_IMPLEMENTED);
	return -1;
}

int
coro_bus_try_send_v(struct coro_bus *bus, int channel, const unsigned *data, unsigned count)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)bus;
	(void)channel;
	(void)data;
	(void)count;
	coro_bus_errno_set(CORO_BUS_ERR_NOT_IMPLEMENTED);
	return -1;
}

int
coro_bus_recv_v(struct coro_bus *bus, int channel, unsigned *data, unsigned capacity)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)bus;
	(void)channel;
	(void)data;
	(void)capacity;
	coro_bus_errno_set(CORO_BUS_ERR_NOT_IMPLEMENTED);
	return -1;
}

int
coro_bus_try_recv_v(struct coro_bus *bus, int channel, unsigned *data, unsigned capacity)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)bus;
	(void)channel;
	(void)data;
	(void)capacity;
	coro_bus_errno_set(CORO_BUS_ERR_NOT_IMPLEMENTED);
	return -1;
}

#endif
