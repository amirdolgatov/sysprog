#include "userfs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define FILE_NAME_SIZE 32

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block 
{
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file 
{
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
	bool is_deleted;
	uint32_t file_size;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc 
{
	struct file *file;
	struct block *current_block;
	
	uint32_t block_offset;
	uint32_t offset;
	/* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

struct block*
create_memory_block()
{
	struct block *block = malloc(sizeof(struct block));
	if (NULL == block)
	{
		return NULL;
	}

	block->memory = malloc(BLOCK_SIZE);
	if (NULL == block->memory)
	{
		free(block);
		return NULL;
	}

	block->occupied = 0;
	block->next = NULL;
	block->prev = NULL;
	return block;
}

struct block*
add_block_after(struct block *last_block)
{
	if (NULL == last_block)
	{
		return NULL;
	}

	struct block* block = create_memory_block();

	block->prev = last_block;
	last_block->next = block;
	return block;
}

void
delete_last_block(struct block *last_block)
{
	(void)last_block;
}

// always add after
void 
add_file_to_list(struct file *file)
{
	if (NULL == file_list)
	{
		file_list = file;
		file_list->next = NULL;
		file_list->prev = NULL;
	}
	else
	{
		file->next = file_list->next;
		file->prev = file_list;
		if (NULL != file->next)
		{
			file->next->prev = file;
		}
		file_list->next = file;
	}
}

// deleting O(1)
void 
delete_file_from_list(struct file *file)
{
    // Шаг 1: Исправляем указатель у того, кто стоит ДО удаляемого файла
    if (file == file_list) 
    {
        file_list = file->next; // Если удаляем голову, новой головой становится следующий
    } 
    else 
    {
        file->prev->next = file->next; // Иначе прошлый элемент теперь смотрит на следующий
    }

    // Шаг 2: Исправляем указатель у того, кто стоит ПОСЛЕ удаляемого файла
    if (NULL != file->next) 
    {
        file->next->prev = file->prev; // Следующий элемент теперь смотрит на прошлый
    }
}


struct block*
add_memory_block(struct file *file)
{
	struct block *new_block = add_block_after(file->last_block);
	if (NULL == new_block)
	{
		return NULL;
	}

	file->last_block = new_block;
	return new_block;
}

void 
free_blocks(struct block *block_list)
{
	struct block *block = block_list;
	while (block != NULL)
	{
		struct block *next = block->next;
		free(block->memory);
		free(block);
		block = next;
	}
}

void 
free_file(struct file *file)
{
	free(file->name);
	free_blocks(file->block_list);
}

int 
insert_descriptor(struct filedesc *descriptor)
{
	if (file_descriptor_count == file_descriptor_capacity)    // handle lack of memory
	{
		if (file_descriptor_capacity == 0)
		{
			file_descriptor_capacity = 4;
		}
		else
		{
			file_descriptor_capacity *= 2;
		}
		file_descriptors = realloc(file_descriptors,sizeof(struct filedesc *) * file_descriptor_capacity);
	}

	int index = 0;
	for ( ; index < file_descriptor_capacity; index++)
	{
		if (NULL == file_descriptors[index])          // find first free space
		{
			file_descriptors[index] = descriptor;
			file_descriptor_count++;
			break;
		}
	}

	// printf("insert_descriptor: file_descriptor_count = %d, file_descriptor_capacity = %d\n", 
	// 	file_descriptor_count, file_descriptor_capacity);
	return index;
}

struct file *
find_file(const char *filename)
{
	struct file *find = NULL;
	for (struct file *file = file_list; file != NULL; file = file->next)
	{
		if (strcmp(filename, file->name) == 0)
		{
			find = file;
			break;
		}
	}
	return find;
}

struct file *
create_file(const char *filename)
{
	struct file *file = malloc(sizeof(struct file));

	if (NULL == file)
	{
		return file;
	}

	file->block_list = NULL;
	file->last_block = NULL;
	file->refs = 0;
	file->next = NULL;
	file->prev = NULL;
	file->is_deleted = 0;
	file->file_size = 0;
	
	// printf("create_file: block_list at %p\n", file->block_list);	

	file->name = malloc(FILE_NAME_SIZE);
	strcpy(file->name, filename);

	// add file to list
	add_file_to_list(file);

	return file;
}

void
dump_block(struct block *block)
{
	printf("\ndump_block: occupied = %d\n", block->occupied);
	printf("----------------------------------------------------------------------------------\n");
	int i = 0;
	int j = 0;
	while (i < BLOCK_SIZE)
	{
		j = 0;
		while (j < 32)
		{
			printf("%c", block->memory[i + j]);
			++j;
		}
		i += j;
		printf("\n");
	}
	printf("----------------------------------------------------------------------------------\n");
}

void
dump_file(int fd)
{
	if (fd < 0 || fd >= file_descriptor_capacity)
	{
		printf("dump_file: fd is invalid = %d\n", fd);
		return;
	}

	if (NULL == file_descriptors[fd])          // not file
	{
		printf("dump_file: fd is invalid = %d\n", fd);
		return;	
	}

	// size is not null - we can read data
	struct filedesc *descriptor = file_descriptors[fd];
	struct block *block_list = descriptor->file->block_list;

	for (struct block *block = block_list; block != NULL; block = block->next)
	{
		dump_block(block);	
	}
}

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

int
ufs_open(const char *filename, int flags)
{
	/* check file in userFS */
	// printf("ufs_open: file_list = %p\n", file_list);
	bool need_create = flags & UFS_CREATE;
	struct file *file = find_file(filename);
	
	if (NULL == file && !need_create)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;         // no file
	}

	if (NULL == file)
	{
		file = create_file(filename);
	}

	if (NULL == file)
	{
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}

	// Absolutely right, the file is in the file system.
	// нужно увеличить количество ссылок и создать struct filedesc
	file->refs++;

	struct filedesc *descriptor = malloc(sizeof(struct filedesc));

	if (NULL == descriptor)
	{
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}

	descriptor->block_offset = 0;
	descriptor->offset = 0;
	descriptor->file = file;
	descriptor->current_block = NULL;

	int index = insert_descriptor(descriptor);

	ufs_error_code = UFS_ERR_NO_ERR;
	return index;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	if (fd < 0 || fd >= file_descriptor_capacity)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (NULL == file_descriptors[fd])          // not file
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;	
	}

	if (0 == size)
	{
		ufs_error_code = UFS_ERR_NO_ERR;
		return 0;
	}

	// size is not null - we can read data
	struct filedesc *descriptor = file_descriptors[fd];
	struct file *file = descriptor->file;

	// first data
	if (0 == file->file_size)
	{
		file->block_list = create_memory_block();
		if (NULL == file->block_list)
		{
			ufs_error_code = UFS_ERR_NO_MEM;
			return -1;
		}
		file->last_block = file->block_list;
		descriptor->current_block = file->block_list;
	}

	if (0 == descriptor->offset)
	{
		descriptor->current_block = file->block_list;
	}

	struct block *current_block = descriptor->current_block;
	uint32_t buf_offset = 0;
	ufs_error_code = UFS_ERR_NO_ERR;

	// uint32_t s = size;
	while (size > 0)
	{
		if (descriptor->block_offset == BLOCK_SIZE)
		{
			// file is too big
			if (file->file_size == MAX_FILE_SIZE)
			{
				ufs_error_code = UFS_ERR_NO_MEM;
				return -1;
			}

			current_block = current_block->next;
			if (NULL == current_block)
			{
				current_block = add_memory_block(file);	
			}
			if (NULL == current_block)
			{
				ufs_error_code = UFS_ERR_NO_MEM;
				break;
			}
			descriptor->block_offset = 0;
		}

		uint32_t free_space = BLOCK_SIZE - descriptor->block_offset;
		uint32_t bytes = MIN(free_space, size);
		char *data_destination = current_block->memory + descriptor->block_offset;
		char const *data_source = buf + buf_offset;
		memcpy(data_destination, data_source, bytes);

		buf_offset += bytes;
		size -= bytes;

		descriptor->block_offset += bytes;
		descriptor->offset += bytes;

		if (descriptor->offset > file->file_size)
		{ 	
			// shift the border
			current_block->occupied = descriptor->block_offset;
			file->file_size = descriptor->offset;
		}
		// printf("ufs_write: current_block at %p\n", current_block);
		// printf("ufs_write: bytes = %d, occupied = %d\n", bytes, current_block->occupied);
	}
	// printf("ufs_write: buf_offset = %d, s = %u\n", buf_offset, s);
	descriptor->current_block = current_block;
	return buf_offset;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	if (fd < 0 || fd >= file_descriptor_capacity)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (NULL == file_descriptors[fd])          // not file
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;	
	}

	if (0 == size)
	{
		ufs_error_code = UFS_ERR_NO_ERR;
		return 0;
	}

	struct filedesc *descriptor = file_descriptors[fd];
	struct file *file = descriptor->file;

	// empty file
	if (0 == file->file_size)
	{
		return 0;
	}

	if (0 == descriptor->offset)
	{
		descriptor->current_block = file->block_list;   // is not null
	}
	
	struct block *current_block = descriptor->current_block;   

	uint32_t buf_offset = 0;
	// size_t s = size;
	while (size > 0)
	{
		if (descriptor->offset == file->file_size)
		{
			break;   // end of file
		}

		if (descriptor->block_offset == BLOCK_SIZE)
		{
			current_block = current_block->next;
			descriptor->block_offset = 0;
		}

		uint32_t read_space = current_block->occupied - descriptor->block_offset;
		uint32_t bytes = MIN(read_space, size);

		char *data_destination = buf + buf_offset;
		char *data_source = current_block->memory + descriptor->block_offset;
		memcpy(data_destination, data_source, bytes);

		buf_offset += bytes;
		size -= bytes;

		descriptor->block_offset += bytes;
		descriptor->offset += bytes;

		// printf("ufs_read: current_block at %p\n", current_block);
		// printf("ufs_read: bytes = %d, buf_offset = %d\n", bytes, buf_offset);  
	}

	// printf("ufs_read: size = %ld\n", s);

	ufs_error_code = UFS_ERR_NO_ERR;
	descriptor->current_block = current_block;
	return buf_offset;
}

int
ufs_close(int fd)
{
	if (fd < 0)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	// close filedescriptor
	if (fd >= file_descriptor_capacity)
	{
		// printf("ufs_close: fd = %d, file_descriptor_capacity = %d\n", fd, file_descriptor_capacity);
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (NULL == file_descriptors[fd])
	{
		// printf("ufs_close: File is not opened\n");
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct filedesc *filedesc = file_descriptors[fd];

	if (filedesc->file->refs > 0)
	{
		filedesc->file->refs--;        
	}

	if (filedesc->file->refs == 0 && filedesc->file->is_deleted)
	{
		free_file(filedesc->file);
		free(filedesc->file);
	}

	free(filedesc);
	file_descriptors[fd] = NULL;
	file_descriptor_count--;

	ufs_error_code = UFS_ERR_NO_ERR;
	return 0;
}

int
ufs_delete(const char *filename)
{
	struct file *file = find_file(filename);
	
	if (NULL == file)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;         // no file
	}

	// if file is closed, delete it immediatly
	if (0 == file->refs)
	{
		free_file(file);
		delete_file_from_list(file);
		free(file);

		ufs_error_code = UFS_ERR_NO_ERR;
		return 0;         
	}


	// file is opened
	delete_file_from_list(file);
	file->is_deleted = 1;

	ufs_error_code = UFS_ERR_NO_ERR;
	return 0;         
}

#if NEED_RESIZE

int
ufs_resize(int fd, size_t new_size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)new_size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

#endif

void
ufs_destroy(void)
{
}
