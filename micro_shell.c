#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>

void put_error(char *str)
{
    write(2, str, strlen(str));
}

int my_own_cd(char **args, int arg_num)
{
    if (arg_num != 2)
    {
        put_error("error: cd: bad arguments\n");
        return 1;
    }
    if (chdir(args[1]) != 0)
    {
        put_error("error: cd: cannot change directory to ");
        put_error(args[1]);
        put_error("\n");
        return 1;
    }
    return 0;
}

int execute_command(char **argv, int arg_count, char **envp, int input_fd)
{
    pid_t pid;
    int status;

    if (arg_count == 0)
        return 0;

    if (strcmp(argv[0], "cd") == 0)
        return my_own_cd(argv, arg_count);

    pid = fork();
    if (pid == -1)
    {
        put_error("error: fatal\n");
        return 1;
    }

    if (pid == 0)
    {
        if (input_fd != STDIN_FILENO)
        {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        argv[arg_count] = NULL;
        execve(argv[0], argv, envp);
        put_error("error: cannot execute ");
        put_error(argv[0]);
        put_error("\n");
        exit(1);
    }
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

int run_pipeline(char **argv, int count, char **envp)
{
    int i = 0;
    int prev_pipe = STDIN_FILENO;
    int cmd_start = 0;
    int pipe_fd[2];
    int status = 0;

    while (i <= count)
    {
        if (i == count || strcmp(argv[i], "|") == 0 || strcmp(argv[i], ";") == 0)
        {
            char is_pipe = (i < count && strcmp(argv[i], "|") == 0);
            
            if (is_pipe)
            {
                if (pipe(pipe_fd) == -1)
                {
                    put_error("error: fatal\n");
                    return 1;
                }
            }
            int save_stdout = -1;
            if (is_pipe)
            {
                save_stdout = dup(STDOUT_FILENO);
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
            }   
            status = execute_command(argv + cmd_start, i - cmd_start, envp, prev_pipe);
            if (prev_pipe != STDIN_FILENO)
                close(prev_pipe);
            if (is_pipe)
            {
                dup2(save_stdout, STDOUT_FILENO);
                close(save_stdout);
                prev_pipe = pipe_fd[0];
            }
            else
                prev_pipe = STDIN_FILENO;

            cmd_start = i + 1;
        }
        i++;
    }
    return status;
}

int main(int argc, char **argv, char **envp)
{
    if (argc < 2)
        return 0;
    
    return run_pipeline(argv + 1, argc - 1, envp);
}
