#include "parser.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include<sys/types.h>
#include <sys/wait.h>

#include "rlist.h"

struct exec_command
{
	const struct command * cmd;
	bool input;
	bool output;
};

char **argv = NULL;
int argc = 0;

int to_parent[2];
int to_child[2];

char** create_argv(const struct command *cmd)
{
	argc = cmd->arg_count + 2;
	argv = (char **)realloc(argv, argc * sizeof(char *));
	if (NULL == argv)
	{
		argc = 0;
		return NULL;
	}

	uint32_t i = 0;
	uint32_t j = 0;
	argv[i++] = cmd->exe;
	while (i <= cmd->arg_count)
	{
		// printf("cmd->args[j] = %s", cmd->args[j]);
		argv[i++] = cmd->args[j++];
	}

	argv[i] = NULL;
	return argv;
}

static void
execute_command_line(const struct command_line *line)
{
	/* REPLACE THIS CODE WITH ACTUAL COMMAND EXECUTION */

	assert(line != NULL);
	printf("================================\n");
	printf("Command line:\n");
	printf("Is background: %d\n", (int)line->is_background);
	printf("Output: ");
	if (line->out_type == OUTPUT_TYPE_STDOUT)
	{
		printf("stdout\n");
	}
	else if (line->out_type == OUTPUT_TYPE_FILE_NEW)
	{
		printf("new file - \"%s\"\n", line->out_file);
	}
	else if (line->out_type == OUTPUT_TYPE_FILE_APPEND)
	{
		printf("append file - \"%s\"\n", line->out_file);
	}
	else
	{
		assert(false);
	}
	printf("Expressions:\n");
	const struct expr *e = line->head;
	while (e != NULL)
	{
		if (e->type == EXPR_TYPE_COMMAND)
		{
			printf("\tCommand: %s\n", e->cmd.exe);
			printf("arg_count: %d ", e->cmd.arg_count);
			for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
				printf("arg: %s ", e->cmd.args[i]);
			printf("\n");
		}
		else if (e->type == EXPR_TYPE_PIPE)
		{
			printf("\tPIPE\n");
		}
		else if (e->type == EXPR_TYPE_AND)
		{
			printf("\tAND\n");
		}
		else if (e->type == EXPR_TYPE_OR)
		{
			printf("\tOR\n");
		}
		else
		{
			assert(false);
		}
		e = e->next;
	}
}

void set_io(struct exec_command * cmd)
{
	if (cmd->input)
	{
		dup2(to_parent[0], STDIN_FILENO);
	}
	if (cmd->output)
	{
		dup2(to_parent[1], STDOUT_FILENO);  // default direction of stdout (to parent, may be to file)
	}

	close(to_parent[1]);
	close(to_child[0]);
	close(to_parent[0]);
	close(to_child[1]);
}

void exec_wrapper(struct exec_command * command)
{
	char** args = create_argv(command->cmd);
	if (NULL != args)
	{
		if (fork() == 0)
		{
			set_io(command);
			int ret = execvp(command->cmd->exe, argv);
			if (ret == -1)
			{
				perror ("execv");
				exit (EXIT_FAILURE);
			}
		}
	}

	// ждём завершения обоих потомков, чтобы не оставлять зомби
	wait(NULL);
	wait(NULL);
}

struct rlist * read_command(const struct command_line *line)
{
	const struct expr *expr = line->head;
	int size = 0;
	while (expr != NULL)                 // calculate size
	{
		++size;
		expr = expr->next;
	}

	expr = line->head;
	if (expr == line->tail)         // один элемент в списке
	{
		struct exec_command * command = malloc(sizeof(struct exec_command));
		rlist_create(&command->list);
		command->cmd = &expr->cmd;
		command->input = 0;
		rlist_add_tail(&head, &command->list);
		return &head;
	}

	while (expr != NULL)
	{
		struct exec_command * command;
		if (expr->type == EXPR_TYPE_COMMAND)
		{
			command = malloc(sizeof(struct exec_command));
			rlist_create(&command->list);
			command->cmd = &expr->cmd;
			rlist_add_tail(&head, &command->list);

			cmd_list[i].input = cmd_list[i - 1].output;
		}
		else if (expr->type == EXPR_TYPE_PIPE)
		{
			cmd_list[i].output = 1;
		}
		expr = expr->next;
	}
	return cmd_list;
}

static void
execute_command_line(const struct command_line *line)
{
	assert(line != NULL);

	const struct expr *expr = line->head;
	int size = 0;
	while (expr != NULL)                 // calculate size
	{
		++size;
		expr = expr->next;
	}

	struct exec_command * cmd_list = malloc(size * sizeof(struct exec_command));



}

int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser *p = parser_new();
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0)
	{
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true)
		{
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE)
			{
				printf("Error: %d\n", (int)err);
				continue;
			}
			execute_command_line(line);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}
