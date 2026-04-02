#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include<sys/types.h>
#include <sys/wait.h>

void exec_wrapper()
{
	int to_parent[2];
	int to_child[2];
	int ret;

	pipe(to_parent);
	pipe(to_child);

	if (fork() == 0)
	{
		close(to_parent[0]);  	// это читающий конец родителя
		close(to_child[1]);		// нам не нужно писать ребенку (это мы)
		close(to_child[0]);		// родитель ничего не отправляет
		dup2(to_parent[1], STDOUT_FILENO);  // default direction of stdout (to parent, may be to file)
		close(to_parent[1]);
		// можно закрыть to_child[0], читать нечего
		ret = execlp("ls", "ls", NULL);
		if (ret == -1)
		{
			perror ("execv");
			exit (EXIT_FAILURE);
		}
	}

	if (fork() == 0)
	{
		dup2(to_parent[0], STDIN_FILENO);
		close(to_parent[0]);  	// это читающий конец родителя
		close(to_child[1]);		// нам не нужно писать ребенку (это мы) pwd | tail -c 8
		close(to_child[0]);		// родитель ничего не отправляет
		close(to_parent[1]);  	// это читающий конец родителя
		char buf[32];

		// scanf("%s", buf);
		// printf("\nbuf: %s\n", buf);
		ret = execlp("grep", "grep", "task", NULL);
		if (ret == -1)
		{
			perror ("execv");
			exit (EXIT_FAILURE);
		}
	}

	close(to_parent[1]);
	close(to_child[0]);
	close(to_parent[0]);
	close(to_child[1]);

	// ждём завершения обоих потомков, чтобы не оставлять зомби
    wait(NULL);
    wait(NULL);
	return;
}

int main()
{
	exec_wrapper();
	return 0;
}