//
// Created by amir on 03.04.26.
//

#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

struct list
{
	void * data;
	uint32_t size;
};

struct list * create_list(uint32_t size, uint32_t type_size);

struct list * resize_list(struct list * list_ptr, uint32_t new_size);

void list_free(struct list * list_ptr);

#endif //LIST_H
