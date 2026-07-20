#include "userfs.h"
#include <stddef.h>
#include <string.h>

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
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc 
{
	struct file *file;
	uint32_t pos;
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

// always add after
void add_file_to_list(struct file *file)
{
	if (NULL == file_list)
	{
		file_list = malloc(sizeof(struct file));
		file_list->next = file_list;   // инициализация
		file_list->prev = file_list;
	}

	file->prev = file_list;
	file->next = file_list->next;
	file_list->next->prev = file;
	file_list->next = file;
}

// deleting O(1)
void delete_file_from_list(struct file *file)
{
	file->next->prev = file->prev;
	file->prev->next = file->next;
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
	for ( ; index < file_descriptor_count; index++)
	{
		if (NULL == file_descriptors[index])          // find first free space
		{
			file_descriptors[index] = descriptor;
			file_descriptor_count++;
		}
	}

	return index;
}

struct file *
find_file(const char *filename)
{
	struct file *find = NULL;
	for (struct file *file = file_list->next; file != file_list; file = file->next)
	{
		if (strcmp(filename, file->name) == 0)
		{
			find = file;
			break;
		}
	}
	return find;
}

struct file *create_file(const char *filename)
{
	struct file *new_file = malloc(sizeof(struct file));

	if (NULL == file)
	{
		return file;
	}

	file->block_list = NULL;
	file->last_block = NULL;
	file->refs = 0;
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

	descriptor->pos = 0;
	descriptor->file = file;

	int index = insert_descriptor(descriptor);

	ufs_error_code = UFS_ERR_NO_ERR;
	return index;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)buf;
	(void)size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)buf;
	(void)size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

int
ufs_close(int fd)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

int
ufs_delete(const char *filename)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)filename;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
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
