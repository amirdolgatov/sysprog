//
// Created by amir on 03.04.26.
//
#include "list.h"

struct list * create_list(uint32_t size, uint32_t type_size)
{
	struct list * list_ptr = NULL;
	list_ptr = malloc(sizeof(struct list));
	if (list_ptr == NULL)
	{
		return NULL;
	}

	list_ptr->data = calloc(size, type_size);
	if (list_ptr->data == NULL)
	{
		free(list_ptr);
		return NULL;
	}

	list_ptr->size = size;
	return list_ptr;
}

struct list * resize_list(struct list * list_ptr, uint32_t new_size)
{
	void * new_ptr = realloc(list_ptr->data, new_size);
	if (new_ptr == NULL)
	{
		return NULL;
	}

	list_ptr->size = new_size;
	return list_ptr;
}

void list_free(struct list * list_ptr)
{
	free(list_ptr->data);
	list_ptr->size = 0;
}