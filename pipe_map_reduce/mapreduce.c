/**
 * mapreduce
 * CS 341 - Spring 2024
 */
#include "utils.h"

// I added
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

static pid_t *children;
static int child_idx = 0;

int main(int argc, char **argv)
{

    if (argc != 6)
    {
        void print_usage();
        exit(-1);
    }

    // argv[0] : ./mapreduce
    // argv[1] : input file
    // argv[2] : output file
    // argv[3] : mapper executable
    // argv[4] : reducer executable
    // argv[5] : mapper count

    // last arg is number of mappers
    int mapper_count = atoi(argv[argc - 1]);

    children = malloc(sizeof(pid_t) * mapper_count * 2);

    // Create pipes to go between each splitter and associated mapper
    // [0][1], [2][3], [4][5], etc.
    // reference: https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
    int mapper_pipes[2 * mapper_count];

    for (int i = 0; i < mapper_count; i++)
    {
        if (pipe2(mapper_pipes + i * 2, 0) < 0)
        {
            perror("mapper pipe failed\n");
        }
    }

    // Create one input pipe for the reducer.
    // *input* pipe is reducer_pipe[0]
    // reducer_pipe[1] (output pipe) is sent to output file (argv[2])
    int reducer_pipe[2];

    if (pipe2(reducer_pipe, 0) < 0)
    {
        perror("reducer pipe failed\n");
    }

    // Open the output file.
    // output file should be argv[2]
    int output_file = open(argv[2], O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
    if (output_file == -1)
    {
        perror("failed to open output file.\n");
    }

    // Start a splitter process for each mapper.

    // create <mapper_count> child processes to SPLIT
    pid_t splitter_child;

    for (int i = 0; i < mapper_count; i++)
    {

        splitter_child = fork();

        if (splitter_child == -1)
        {
            perror("fork failed for splitter child\n");
        }

        if (splitter_child == 0)
        { // child

            // dup2
            // sending output from splitter process to READ pipe of associated mapper process
            dup2(mapper_pipes[i * 2 + 1], 1);
            close(mapper_pipes[i * 2 + 1]); // already dup'd
            close(mapper_pipes[i * 2]);     // not using this end

            // FORMAT: ./splitter myfile 4 0 > myfile.chunk0
            char convert[6];
            sprintf(convert, "%d", i);

            execl("./splitter", "./splitter", argv[1], argv[5], convert, (char *)NULL);
            perror("splitter failed to exec.\n");
            return 1;
        }

        else
        { // parent process
            children[child_idx] = splitter_child;
            child_idx++;

            // parent should not wait
            int status;
            waitpid(splitter_child, &status, WNOHANG);

            if (status / 255 != 0)
            {
                print_nonzero_exit_status("./splitter", WIFEXITED(status));
            }
        }
    }

    pid_t mapper_child;

    // Start all the mapper processes.
    for (int j = 0; j < mapper_count; j++)
    {

        close(mapper_pipes[j * 2 + 1]);

        mapper_child = fork();

        if (mapper_child == -1)
        {
            perror("fork failed for mapper child\n");
        }

        if (mapper_child == 0)
        { // child

            dup2(mapper_pipes[j * 2], 0);
            close(mapper_pipes[j * 2]); // already dup'd

            // sending output from child process to input pipe of associated mapper process
            dup2(reducer_pipe[1], 1);
            close(reducer_pipe[1]); // already dup'd

            execl(argv[3], argv[3], (char *)NULL);
            perror("mapper failed to exec.\n");
            exit(-1);
        }

        else
        { // parent
            children[child_idx] = mapper_child;
            child_idx++;

            int status;
            waitpid(mapper_child, &status, WNOHANG);

            if (status / 255 != 0)
            {
                print_nonzero_exit_status(argv[3], WIFEXITED(status));
            }
        }
    }

    // Start the reducer process.

    pid_t reducer_child = fork();

    if (reducer_child == -1)
    {
        perror("fork failed for reducer child\n");
    }

    if (reducer_child == 0)
    { // reducer

        dup2(reducer_pipe[0], 0);
        close(reducer_pipe[0]); // already dup'd

        // sending output from reducer process to provided output file
        dup2(output_file, 1);
        close(reducer_pipe[1]); // already dup'd

        execl(argv[4], argv[4], (char *)NULL);
        perror("reducer failed to exec.\n");
        return 1;
    }

    else
    { // parent

        for (int k = 0; k < child_idx; k++)
        {
            int status;
            waitpid(children[k], &status, 0);

            if (status / 255 != 0)
            {
                print_nonzero_exit_status("fix me!", WIFEXITED(status));
            }
        }

        for (int j = 0; j < mapper_count; j++)
        {
            close(mapper_pipes[j * 2]);
        }

        // Wait for the reducer to finish.
        close(reducer_pipe[0]);
        close(reducer_pipe[1]);

        int status;
        waitpid(reducer_child, &status, 0);

        if (status / 255 != 0)
        {
            print_nonzero_exit_status(argv[4], WIFEXITED(status));
        }

        close(output_file);
    }

    if (children)
    {
        free(children);
        children = NULL;
    }

    // Count the number of lines in the output file.
    print_num_lines(argv[2]);

    return 0;
}
