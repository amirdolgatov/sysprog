#include "parser.h"

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include<sys/types.h>
#include <sys/wait.h>

#include "list.h"
#include "rlist.h"

struct exec_command
{
	const struct command * cmd;
	bool pipe_input;
	bool pipe_output;
	bool is_last;
	int32_t out[2];      // 1 - write STDOUT, 0 - for read from this process
	int32_t in;		     // STDIN for this process
	enum output_type out_type;
	/** Valid if the out type is FILE. */
	char *out_file;
};

char **argv = NULL;
int argc = 0;
int fork_count = 0;
int g_last_exit_code = 0;


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

// static void
// execute_command_line(const struct command_line *line)
// {
// 	/* REPLACE THIS CODE WITH ACTUAL COMMAND EXECUTION */
//
// 	assert(line != NULL);
// 	printf("================================\n");
// 	printf("Command line:\n");
// 	printf("Is background: %d\n", (int)line->is_background);
// 	printf("Output: ");
// 	if (line->out_type == OUTPUT_TYPE_STDOUT)
// 	{
// 		printf("stdout\n");
// 	}
// 	else if (line->out_type == OUTPUT_TYPE_FILE_NEW)
// 	{
// 		printf("new file - \"%s\"\n", line->out_file);
// 	}
// 	else if (line->out_type == OUTPUT_TYPE_FILE_APPEND)
// 	{
// 		printf("append file - \"%s\"\n", line->out_file);
// 	}
// 	else
// 	{
// 		assert(false);
// 	}
// 	printf("Expressions:\n");
// 	const struct expr *e = line->head;
// 	while (e != NULL)
// 	{
// 		if (e->type == EXPR_TYPE_COMMAND)
// 		{
// 			printf("\tCommand: %s\n", e->cmd.exe);
// 			printf("arg_count: %d ", e->cmd.arg_count);
// 			for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
// 				printf("arg: %s ", e->cmd.args[i]);
// 			printf("\n");
// 		}
// 		else if (e->type == EXPR_TYPE_PIPE)
// 		{
// 			printf("\tPIPE\n");
// 		}
// 		else if (e->type == EXPR_TYPE_AND)
// 		{
// 			printf("\tAND\n");
// 		}
// 		else if (e->type == EXPR_TYPE_OR)
// 		{
// 			printf("\tOR\n");
// 		}
// 		else
// 		{
// 			assert(false);
// 		}
// 		e = e->next;
// 	}
// }

void set_output(struct exec_command * cmd)
{
	if (cmd->pipe_input)
	{
		dup2(cmd->in, STDIN_FILENO);
		close(cmd->in);
	}
	if (cmd->pipe_output)
	{
		dup2(cmd->out[1], STDOUT_FILENO);  // default direction of stdout (to parent, may be to file)
		close(cmd->out[0]);
		close(cmd->out[1]);
	}
	else
	{
		if (!cmd->is_last)
		{
			return;
		}

		if (cmd->out_file == NULL)
		{
			return;
		}

		switch(cmd->out_type)
		{
			case OUTPUT_TYPE_FILE_NEW: // Аналог ">"
			{
				// Добавляем O_TRUNC
				cmd->out[1] = open(cmd->out_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
				if (cmd->out[1] != -1)
				{
					dup2(cmd->out[1], STDOUT_FILENO);
					close(cmd->out[1]); // Важно закрыть после копирования
				}
			}
			break;

			case OUTPUT_TYPE_FILE_APPEND: // Аналог ">>"
			{
				// Используем O_APPEND
				cmd->out[1] = open(cmd->out_file, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
				if (cmd->out[1] != -1)
				{
					dup2(cmd->out[1], STDOUT_FILENO);
					close(cmd->out[1]);
				}
			}
			break;
			default:
			{

			}
		}
	}

}

pid_t exec_wrapper(struct exec_command * command)
{
	char** args = create_argv(command->cmd);
	if (NULL != args)
	{
		if (strcmp(command->cmd->exe, "cd") == 0)          // смена директории - обязателно в текущем процессе, а не в дочернем
		{
			if (command->cmd->arg_count > 0)
			{
				chdir(command->cmd->args[0]);
			}
			return 0;
		}
		// 2. EXIT как ОДИНОЧНАЯ команда (завершает шелл)
		if (strcmp(command->cmd->exe, "exit") == 0 && !command->pipe_input && !command->pipe_output)
		{
			int code = 0;
			if (command->cmd->arg_count > 0)
			{
				code = atoi(command->cmd->args[0]);
			}
			exit(code); // Прикончит основной процесс
		}

		fork_count++;
		pid_t pid = fork();
		if (pid == 0)
		{
			set_output(command);

			if (strcmp(command->cmd->exe, "exit") == 0)          // выход из процесса
			{
				if (command->cmd->arg_count > 0)
				{
					int code = atoi(command->cmd->args[0]);
					exit(code);
				}
			}
			int ret = execvp(command->cmd->exe, args);
			if (ret == -1)
			{
				perror ("execv");
				exit (EXIT_FAILURE);
			}
		}
		return pid;
	}
	return 0;
}

struct list * read_command(const struct command_line *line)
{
	const struct expr *expr = line->head;
	int size = 0;
	while (expr != NULL)                 // calculate size
	{
		if (expr->type == EXPR_TYPE_COMMAND)
		{
			++size;
		}
		expr = expr->next;
	}

	if (size == 0)
	{
		return NULL;
	}

	struct list * command_list = create_list(size, sizeof(struct exec_command));
	assert(command_list);
	struct exec_command * cmd_buffer = command_list->data;

	int i = 0;
	expr = line->head;
	cmd_buffer[i].cmd = &expr->cmd;
	cmd_buffer[i].pipe_input = 0;
	cmd_buffer[i].pipe_output = 0;

	if (expr == line->tail)         // один элемент в списке
	{
		cmd_buffer[i].is_last = 1;   // last command
		cmd_buffer[i].out_type = line->out_type;
		cmd_buffer[i].out_file = line->out_file;
		return command_list;
	}

	expr = expr->next;
	while (expr != NULL)
	{
		if (expr->type == EXPR_TYPE_COMMAND)
		{
			++i;
			cmd_buffer[i].cmd = &expr->cmd;
			cmd_buffer[i].pipe_input = cmd_buffer[i - 1].pipe_output;
			cmd_buffer[i].pipe_output = 0;
		}
		else if (expr->type == EXPR_TYPE_PIPE)
		{
			cmd_buffer[i].pipe_output = 1;
		}
		expr = expr->next;
	}

	cmd_buffer[i].is_last = 1;   // last command
	cmd_buffer[i].out_type = line->out_type;
	cmd_buffer[i].out_file = line->out_file;
	return command_list;
}

static void
execute_command_line(const struct command_line *line)
{
	struct list * cmd_list = read_command(line);
	struct exec_command * cmd_buffer = cmd_list->data;
	pid_t last_pid = 0;
	fork_count = 0;

	// обрабатываем первую команду

	if (cmd_list->size == 0)
	{
		return;							// нет команд, выход
	}

	uint32_t i = 0;
	if (cmd_buffer[i].pipe_output)
	{
		pipe(cmd_buffer[i].out);
	}
	last_pid = exec_wrapper(&cmd_buffer[i]);
	++i;

	for ( ; i < cmd_list->size; i++)
	{
		// пройти по списку
		if (cmd_buffer[i].pipe_output)
		{
			pipe(cmd_buffer[i].out);
		}
		if (cmd_buffer[i].pipe_input)
		{
			cmd_buffer[i].in = cmd_buffer[i - 1].out[0];
		}

		close(cmd_buffer[i - 1].out[1]);
		last_pid = exec_wrapper(&cmd_buffer[i]);
		close(cmd_buffer[i - 1].out[0]);
	}

	// Вместо безусловного close в конце execute_command_line:
	if (cmd_buffer[cmd_list->size - 1].pipe_output)
	{
		close(cmd_buffer[cmd_list->size - 1].out[0]);
		close(cmd_buffer[cmd_list->size - 1].out[1]);
	}

	int status;
	for (int i = 0; i < fork_count; i++)
	{
		int current_pid = wait(&status);
		if (current_pid == last_pid)
		{
			// Нас интересует только статус последнего в конвейере
			if (WIFEXITED(status))
			{
				g_last_exit_code = WEXITSTATUS(status);
			}
		}
	}

	list_free(cmd_list);
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
	return g_last_exit_code;
}
