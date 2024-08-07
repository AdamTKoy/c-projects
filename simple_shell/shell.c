/**
 * shell
 * CS 341 - Spring 2024
 */
#include "format.h"
#include "shell.h"
#include "vector.h"
#include "string.h"
#include "sstring.h"
#include "callbacks.h"

// for waitpid
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>

#include <unistd.h>
#include <stdlib.h> // for exit()

#include <ulimit.h> // for setting process limit
// For each new *terminal*, set max # processes to prevent forkbomb:
// run command: ulimit -u # (they recommended 100-200)

typedef struct process
{
    char *command;
    pid_t pid;
} process;

static pid_t current_child = (pid_t)-1;

// modular functions:
void ext_cmd(char *line, vector *stringsplit, vector *process_list);
void change_directory(char *path, char *buffer);
void handler();
void process_logical(char *lineptr, char *buffer, vector *process_list);
void process_logical_file(FILE *file, char *lineptr, char *buffer, vector *process_list);
void process_redirection_output(vector *output_cmd, char *redir_file, vector *process_list);
void process_redirection_append(vector *append_cmd, char *redir_file, vector *process_list);
void process_prefix(char *lineptr, vector *history, char *buffer, vector *process_list);
vector *string_split(char *input, const char *delim);
void write_history(vector *v, char *hist_filename);

int shell(int argc, char *argv[])
{
    // FIRST: process any arguments included after invoking ./shell

    signal(SIGINT, handler);

    int opt;

    int is_hist = 0;
    char *hist_file;

    int is_file = 0;
    char *read_file;

    // ref: https://www.geeksforgeeks.org/getopt-function-in-c-to-parse-command-line-arguments/
    while ((opt = getopt(argc, argv, ":h:f:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            is_hist = 1;
            hist_file = optarg;
            break;
        case 'f':
            is_file = 1;
            read_file = optarg;
            break;
        case ':': // missing value
            print_usage();
            break;
        case '?': // unrecognized argument
            print_usage();
            break;
        }
    }

    // SECOND: two routes
    // file --> read, execute, (maybe write to history), quit
    // non-file --> keep prompting until user enters 'exit'

    // vector to store commands
    vector *v_hist = vector_create(&string_copy_constructor, &string_destructor, &string_default_constructor);

    // vector to store processes (SHALLOW)
    vector *proc_list = vector_create(NULL, NULL, NULL);

    process_info *parent_info = malloc(sizeof(process_info));
    parent_info->pid = getpid();
    parent_info->command = "./shell";
    vector_push_back(proc_list, parent_info);

    char *buf = (char *)malloc(100 * sizeof(char));
    getcwd(buf, 100); // current working directory

    const char *space = " ";
    const char *logic_and = "&&";
    const char *logic_or = "||";
    const char *logic_both = ";";
    const char *redir_out = ">";
    const char *redir_app = ">>";
    const char *redir_in = "<";

    char *line = NULL;
    char *line2 = NULL;
    size_t capacity = 0;
    ssize_t bytesread = 0;
    vector *ss = NULL;
    vector *ss2 = NULL;

    if (is_file)
    { // -f

        FILE *file = fopen(read_file, "r");
        if (!file)
        {
            print_script_file_error();
            exit(1);
        }

        while (1)
        {

            bytesread = getline(&line, &capacity, file);

            if (bytesread == -1)
                break; // EOF breaks loop

            if (bytesread > 0 && line[bytesread - 1] == '\n')
            {
                line[bytesread - 1] = '\0';
            }

            print_prompt(buf, getpid());

            // check for logical operator
            if (strstr(line, logic_and) || strstr(line, logic_or) || strstr(line, logic_both))
            {
                vector_push_back(v_hist, line);
                print_command(line);
                process_logical_file(file, line, buf, proc_list);
                fflush(stdout);
            }

            else
            { // no logical operator

                char *line_dup = malloc(strlen(line) + 1);
                strcpy(line_dup, line);

                vector *split_cmd = string_split(line, space);
                char *cmd = vector_get(split_cmd, 0);

                print_command(line_dup);
                vector_push_back(v_hist, line_dup);

                if (strcmp(cmd, "cd") == 0)
                {
                    char *pth = vector_get(split_cmd, 1);
                    change_directory(pth, buf);
                }

                else
                { // process external command in child process
                    ext_cmd(line_dup, split_cmd, proc_list);
                }

                free(line_dup);
                if (split_cmd)
                {
                    vector_destroy(split_cmd);
                }
            }
        } // end while loop (-f)

        // free/close at the end
        fclose(file);
        if (is_hist)
        {
            write_history(v_hist, hist_file);
        }

    } // end -f 'if'

    else
    { // not a file

        while (1)
        {
            print_prompt(buf, getpid());

            bytesread = getline(&line, &capacity, stdin);

            // EOF/Ctrl-D
            if (bytesread == -1)
            {
                break;
            }

            if (strlen(line) != 1)
            { // if blank line, will automatically prompt again

                if (bytesread > 0 && line[bytesread - 1] == '\n')
                {
                    line[bytesread - 1] = '\0';
                }

                if (strncmp(line, "exit", 4) == 0)
                {
                    break; // stopping condition
                }

                if (strncmp(line, "kill", 4) == 0)
                {
                    vector_push_back(v_hist, line);

                    char *kill_line = malloc(strlen(line) + 1);
                    strcpy(kill_line, line);

                    vector *split_cmd = string_split(line, space);

                    char *cpid = NULL;
                    process_info *pi = NULL;
                    int cpidi;
                    int found;
                    size_t i;

                    if (vector_size(split_cmd) < 2)
                    { // missing pid
                        print_invalid_command(kill_line);
                    }

                    else
                    {
                        print_command_executed(getpid());
                        cpid = vector_get(split_cmd, 1);
                        cpidi = atoi(cpid);
                        found = 0;

                        for (i = 0; i < vector_size(proc_list); i++)
                        {
                            pi = vector_get(proc_list, i);
                            if (pi->pid == cpidi)
                            {
                                found = 1;
                                break;
                            }
                        }

                        if (found)
                        {
                            print_killed_process(cpidi, pi->command);
                            kill(cpidi, SIGKILL);
                            vector_erase(proc_list, i);
                        }
                        else
                        {
                            print_no_process_found(cpidi);
                        }
                    }

                    if (split_cmd)
                    {
                        vector_destroy(split_cmd);
                        split_cmd = NULL;
                    }

                    if (kill_line)
                    {
                        free(kill_line);
                        kill_line = NULL;
                    }
                }

                else if (strncmp(line, "stop", 4) == 0)
                {
                    vector_push_back(v_hist, line);

                    char *stop_line = malloc(strlen(line) + 1);
                    strcpy(stop_line, line);

                    vector *split_cmd = string_split(line, space);

                    char *cpid = NULL;
                    process_info *pi = NULL;
                    int cpidi;
                    int found;
                    size_t i;

                    if (vector_size(split_cmd) < 2)
                    { // missing pid
                        print_invalid_command(stop_line);
                    }

                    else
                    {
                        print_command_executed(getpid());
                        cpid = vector_get(split_cmd, 1);
                        cpidi = atoi(cpid);
                        found = 0;

                        for (i = 0; i < vector_size(proc_list); i++)
                        {
                            pi = vector_get(proc_list, i);
                            if (pi->pid == cpidi)
                            {
                                found = 1;
                                break;
                            }
                        }

                        if (found)
                        {
                            kill(cpidi, SIGSTOP);
                            print_stopped_process(cpidi, pi->command);
                        }
                        else
                        {
                            print_no_process_found(cpidi);
                        }
                    }

                    if (split_cmd)
                    {
                        vector_destroy(split_cmd);
                        split_cmd = NULL;
                    }

                    if (stop_line)
                    {
                        free(stop_line);
                        stop_line = NULL;
                    }
                }

                else if (strncmp(line, "cont", 4) == 0)
                {
                    vector_push_back(v_hist, line);

                    char *cont_line = malloc(strlen(line) + 1);
                    strcpy(cont_line, line);

                    vector *split_cmd = string_split(line, space);

                    char *cpid = NULL;
                    process_info *pi = NULL;
                    int cpidi;
                    int found;
                    size_t i;

                    if (vector_size(split_cmd) < 2)
                    { // missing pid
                        print_invalid_command(cont_line);
                    }

                    else
                    {
                        print_command_executed(getpid());
                        cpid = vector_get(split_cmd, 1);
                        cpidi = atoi(cpid);
                        found = 0;

                        for (i = 0; i < vector_size(proc_list); i++)
                        {
                            pi = vector_get(proc_list, i);
                            if (pi->pid == cpidi)
                            {
                                found = 1;
                                break;
                            }
                        }

                        if (found)
                        {
                            kill(cpidi, SIGCONT);
                            print_continued_process(cpidi, pi->command);
                        }
                        else
                        {
                            print_no_process_found(cpidi);
                        }
                    }

                    if (split_cmd)
                    {
                        vector_destroy(split_cmd);
                        split_cmd = NULL;
                    }

                    if (cont_line)
                    {
                        free(cont_line);
                        cont_line = NULL;
                    }
                }

                // PS
                // reference: https://stackoverflow.com/questions/34575285/read-proc-stat-information
                else if (strncmp(line, "ps", 2) == 0)
                {
                    vector_push_back(v_hist, line);
                    print_process_info_header();
                    size_t i;
                    FILE *fp;

                    for (i = 0; i < vector_size(proc_list); i++)
                    {

                        process_info *x = vector_get(proc_list, i);
                        char buf1[50];
                        snprintf(buf1, sizeof(buf1), "/proc/%d/stat", x->pid);

                        fp = fopen(buf1, "r");

                        // script file error?
                        if (fp == NULL)
                        {
                            print_script_file_error();
                        }

                        else
                        {
                            int pidx;
                            char statex;
                            unsigned long int utimex;
                            unsigned long int stimex;
                            long int num_thdsx;
                            unsigned long int vsizex;
                            unsigned long long starttimex;
                            int dum;
                            unsigned dum1;
                            unsigned long int dum2;
                            long int dum3;

                            char buf2[255];
                            char buf3[255];
                            size_t buf_len = 255;
                            char cmdx[50];

                            fscanf(fp, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu %lu",
                                   &pidx, cmdx, &statex, &dum, &dum, &dum, &dum, &dum, &dum1, &dum2, &dum2, &dum2, &dum2, &utimex,
                                   &stimex, &dum3, &dum3, &dum3, &dum3, &num_thdsx, &dum3, &starttimex, &vsizex);

                            x->nthreads = num_thdsx;
                            x->vsize = vsizex / 1024;
                            x->state = statex;

                            // divide starttime (measured in clock ticks) by sysconf(_SC_CLK_TCK) to get SECONDS
                            size_t exe_time_min = ((utimex + stimex) / sysconf(_SC_CLK_TCK)) / 60;
                            size_t exe_time_sec = ((utimex + stimex) / sysconf(_SC_CLK_TCK)) % 60;

                            execution_time_to_string(buf2, buf_len, exe_time_min, exe_time_sec);
                            x->time_str = buf2;

                            time_t rawtime;
                            struct tm *info;
                            time(&rawtime);
                            info = localtime(&rawtime);

                            time_struct_to_string(buf3, buf_len, info);
                            x->start_str = buf3;

                            print_process_info(x);
                        }

                        fclose(fp);
                    }
                }

                // check for LOGICAL
                else if (strstr(line, "&&") || strstr(line, "||") || strstr(line, ";"))
                {
                    vector_push_back(v_hist, line);
                    process_logical(line, buf, proc_list);
                }

                else if (strstr(line, "&"))
                { // background process (always external cmd)
                    vector_push_back(v_hist, line);

                    char *bg_cmd = malloc(strlen(line) - 1);
                    strncpy(bg_cmd, line, strlen(line) - 2);

                    char *line_copy = malloc(strlen(line) + 1);
                    strcpy(line_copy, line);

                    vector *split_bg_cmd = string_split(bg_cmd, space);
                    char **args = (char **)vector_at(split_bg_cmd, 0);

                    fflush(stdout);
                    pid_t child = fork();
                    if (child == -1)
                    {
                        print_fork_failed();
                    }

                    int pgs = setpgid(child, child);
                    if (pgs == -1)
                    {
                        print_setpgid_failed();
                    }

                    if (child == 0)
                    {
                        execvp(args[0], args);
                        print_exec_failed(args[0]);
                        exit(1);
                    }

                    else
                    {
                        print_command_executed(child);

                        process_info *ch = malloc(sizeof(process_info));
                        ch->pid = child;
                        ch->command = line_copy;
                        vector_push_back(proc_list, ch);

                        int status = 0;
                        waitpid(child, &status, WNOHANG);

                        if (WIFEXITED(status) == 0)
                        {
                            print_wait_failed();
                        }
                    }
                    if (bg_cmd)
                        free(bg_cmd);
                    if (split_bg_cmd)
                    {
                        vector_destroy(split_bg_cmd);
                        split_bg_cmd = NULL;
                    }
                }

                // REDIRECTION - APPEND
                else if (strstr(line, redir_app))
                {

                    vector_push_back(v_hist, line);

                    ss = string_split(line, redir_app);
                    char *fd = vector_get(ss, 1) + 1; // filename to append to
                    line2 = vector_get(ss, 0);        // cmd to execute to file
                    ss2 = string_split(line2, space);

                    process_redirection_append(ss2, fd, proc_list);
                }

                // REDIRECTION - OUTPUT ">"
                else if (strstr(line, redir_out))
                {

                    vector_push_back(v_hist, line);

                    ss = string_split(line, redir_out);
                    char *fd = vector_get(ss, 1) + 1; // filename to write to
                    line2 = vector_get(ss, 0);
                    ss2 = string_split(line2, space);

                    process_redirection_output(ss2, fd, proc_list);
                }

                // REDIRECTION - INPUT "<"
                else if (strstr(line, redir_in))
                {
                    vector_push_back(v_hist, line);
                    ss = string_split(line, redir_in);
                    char *cmd2 = vector_get(ss, 0); // command

                    if (strlen(cmd2) > 0 && cmd2[strlen(cmd2) - 1] == ' ')
                    {
                        cmd2[strlen(cmd2) - 1] = '\0';
                    }

                    char *fd = vector_get(ss, 1) + 1; // filename to read in from

                    int fd_in = open(fd, O_RDWR, S_IRUSR);
                    if (fd_in == -1)
                    {
                        print_redirection_file_error();
                    }

                    int saved_stdin = dup(0);

                    ss2 = string_split(line, space);
                    vector_insert(ss2, 0, cmd2);

                    close(0);
                    dup2(fd_in, 0);

                    fflush(stdout);

                    pid_t child = fork();
                    if (child == -1)
                    {
                        print_fork_failed();
                    }

                    int pgs = setpgid(child, child);
                    if (pgs == -1)
                    {
                        print_setpgid_failed();
                    }

                    process_info *ch = malloc(sizeof(process_info));
                    ch->pid = child;
                    vector_push_back(proc_list, ch);

                    if (child == 0)
                    {
                        print_command_executed(getpid());
                        execlp(cmd2, cmd2, NULL);
                        print_exec_failed(cmd2);
                        exit(1);
                    }

                    else
                    {
                        current_child = child;

                        int status = 0;
                        waitpid(child, &status, 0);

                        if (WIFSIGNALED(status))
                        { // child killed by signal
                            for (size_t i = 0; i < vector_size(proc_list); i++)
                            {
                                process_info *pi = vector_get(proc_list, i);
                                if (pi->pid == child)
                                {
                                    vector_erase(proc_list, i);
                                    break;
                                }
                            }
                        }
                        else if (WIFEXITED(status) == 0)
                        { // parent failed to wait
                            print_wait_failed();
                            for (size_t i = 0; i < vector_size(proc_list); i++)
                            {
                                process_info *pi = vector_get(proc_list, i);
                                if (pi->pid == child)
                                {
                                    vector_erase(proc_list, i);
                                    break;
                                }
                            }
                        }
                        else if (WIFEXITED(status))
                        { // child terminated normally
                            for (size_t i = 0; i < vector_size(proc_list); i++)
                            {
                                process_info *pi = vector_get(proc_list, i);
                                if (pi->pid == child)
                                {
                                    vector_erase(proc_list, i);
                                    break;
                                }
                            }
                        }
                        current_child = -1;
                    }

                    dup2(saved_stdin, 0);
                    close(saved_stdin);
                    fflush(stdin);
                    fflush(stdout);
                }

                else
                { // No logical or redirection operators

                    char *line_dup = malloc(strlen(line) + 1);
                    strcpy(line_dup, line);

                    ss = string_split(line, space);

                    char *cmd = vector_get(ss, 0);
                    size_t vhist_sz = vector_size(v_hist);

                    if (strcmp(cmd, "cd") == 0)
                    {
                        vector_push_back(v_hist, line_dup);
                        char *pth = vector_get(ss, 1);
                        change_directory(pth, buf);
                    }

                    else if (strcmp(cmd, "!history") == 0)
                    {
                        size_t i;
                        for (i = 0; i < vhist_sz; i++)
                        {
                            print_history_line(i, vector_get(v_hist, i));
                        }
                    }

                    else if (cmd[0] == '!')
                    {
                        process_prefix(line_dup, v_hist, buf, proc_list);
                    }

                    else if (cmd[0] == '#')
                    {
                        char *num = cmd + 1;
                        size_t k = atol(num);

                        if (k < vhist_sz)
                        {
                            char *redo = vector_get(v_hist, k);

                            char *rc = malloc(strlen(redo) + 1);
                            strcpy(rc, redo);
                            vector_push_back(v_hist, rc);
                            print_command(rc);

                            // check for logical
                            if (strstr(rc, "&&") || strstr(rc, "||") || strstr(rc, ";"))
                            {
                                process_logical(rc, buf, proc_list);
                            }
                            else
                            {
                                // split redo and determine cd or external
                                vector *rd = string_split(rc, space);
                                char *new_cmd = vector_get(rd, 0);

                                if (strncmp(new_cmd, "cd", 2) == 0)
                                {
                                    char *pth = vector_get(rd, 1);

                                    change_directory(pth, buf);
                                }
                                else
                                {
                                    ext_cmd(redo, rd, proc_list);
                                }
                                if (rd)
                                {
                                    vector_destroy(rd);
                                    rd = NULL;
                                }
                            }

                            if (rc)
                            {
                                free(rc);
                                rc = NULL;
                            }
                        }
                        else
                        {
                            print_invalid_index();
                        }
                    }

                    else
                    { // external command
                        vector_push_back(v_hist, line_dup);
                        ext_cmd(line_dup, ss, proc_list);
                    }

                    free(line_dup);
                    line_dup = NULL;
                } // end else (non logical or redirection)

            } // end if loop (non-empty getline)

        } // end 'while' loop (non-file)
    } // end 'else' (non-file)

    if (line)
    {
        free(line);
        line = NULL;
    }

    if (line2)
    {
        free(line2);
        line2 = NULL;
    }

    if (is_hist)
    {
        write_history(v_hist, hist_file);
    }

    if (v_hist)
    {
        vector_destroy(v_hist);
        v_hist = NULL;
    }

    if (ss)
    {
        vector_destroy(ss);
        ss = NULL;
    }

    if (ss2)
    {
        vector_destroy(ss2);
        ss2 = NULL;
    }

    if (proc_list)
    {
        vector_destroy(proc_list);
        proc_list = NULL;
    }

    if (buf)
    {
        free(buf);
        buf = NULL;
    }

    // successful termination!
    exit(0);
}

void change_directory(char *path, char *buffer)
{

    int cdir = chdir(path);
    if (cdir != 0)
    {
        print_no_directory(path);
    }
    else
    {
        getcwd(buffer, 100);
    }
}

void ext_cmd(char *line, vector *stringsplit, vector *process_list)
{
    char *line_dup = malloc(strlen(line) + 1);
    strcpy(line_dup, line);

    char **args = (char **)vector_at(stringsplit, 0);

    fflush(stdout);

    pid_t child = fork();
    if (child == -1)
    {
        print_fork_failed();
    }

    int pgs = setpgid(child, child);
    if (pgs == -1)
    {
        print_setpgid_failed();
    }

    if (child == 0)
    {
        print_command_executed(getpid());
        execvp(args[0], args);
        print_exec_failed(args[0]);
        exit(1);
    }

    // parent:
    else
    {
        current_child = child;

        process_info *ch = malloc(sizeof(process_info));
        ch->pid = child;
        ch->command = line_dup;
        vector_push_back(process_list, ch);

        int status = 0;
        waitpid(child, &status, 0);

        if (WIFSIGNALED(status))
        { // child killed by signal
            for (size_t i = 0; i < vector_size(process_list); i++)
            {
                process_info *pi = vector_get(process_list, i);
                if (pi->pid == child)
                {
                    vector_erase(process_list, i);
                    break;
                }
            }
        }

        else if (WIFEXITED(status) == 0)
        { // parent failed to wait
            print_wait_failed();
            for (size_t i = 0; i < vector_size(process_list); i++)
            {
                process_info *pi = vector_get(process_list, i);
                if (pi->pid == child)
                {
                    vector_erase(process_list, i);
                    break;
                }
            }
        }

        else if (WIFEXITED(status))
        { // child terminated normally
            for (size_t i = 0; i < vector_size(process_list); i++)
            {
                process_info *pi = vector_get(process_list, i);
                if (pi->pid == child)
                {
                    vector_erase(process_list, i);
                    break;
                }
            }
        }

        current_child = -1;
    }
}

// Signal handler reference:
// https://stackoverflow.com/questions/5113545/enable-a-signal-handler-using-sigaction-in-c
void handler()
{
    if (current_child != -1)
    {
        kill(current_child, SIGINT);
    }
}

void process_logical(char *lineptr, char *buffer, vector *process_list)
{

    int chose_and = 0;
    int chose_or = 0;
    int idx = 0;

    const char *l_and = "&&";
    const char *l_or = "||";
    const char *l_both = ";";
    const char *spc = " ";

    vector *logic_split = NULL;

    if (strstr(lineptr, l_and))
    { // found 'AND'
        logic_split = string_split(lineptr, l_and);
        chose_and = 1;
    }
    else if (strstr(lineptr, l_or))
    { // found 'OR'
        logic_split = string_split(lineptr, l_or);
        chose_or = 1;
    }
    else
    { // 'BOTH'
        logic_split = string_split(lineptr, l_both);
    }

    for (idx = 0; idx < 2; idx++)
    {

        char *part = vector_get(logic_split, idx);
        vector *split_section = string_split(part, spc);
        char *cmd = vector_get(split_section, 0);

        if (strncmp(cmd, "cd", 2) == 0)
        {
            char *path = vector_get(split_section, 1);
            int cdir = chdir(path);

            if (cdir != 0)
            {
                print_no_directory(path);
                if (chose_and && idx == 0)
                {
                    break;
                }
            }
            else
            {
                getcwd(buffer, 100);
                if (chose_or && idx == 0)
                {
                    break;
                }
            }
        }
        else
        {
            char **args = (char **)vector_at(split_section, 0);

            char *cmd_copy = malloc(strlen(args[0]) + 1);
            strcpy(cmd_copy, args[0]);

            fflush(stdout);

            pid_t child = fork();
            if (child == -1)
            {
                print_fork_failed();
                if (chose_and)
                    break;
            }

            int pgs = setpgid(child, child);
            if (pgs == -1)
            {
                print_setpgid_failed();
            }

            if (child == 0)
            {
                print_command_executed(getpid());
                execvp(args[0], args);
                print_exec_failed(args[0]);
                exit(13);
            }
            else
            { // parent
                current_child = child;

                process_info *ch = malloc(sizeof(process_info));
                ch->pid = child;
                ch->command = cmd_copy;
                vector_push_back(process_list, ch);

                int status = 0;
                waitpid(child, &status, 0);

                if (WIFSIGNALED(status))
                { // child killed via SIGINT
                    for (size_t i = 0; i < vector_size(process_list); i++)
                    {
                        process_info *pi = vector_get(process_list, i);
                        if (pi->pid == child)
                        {
                            vector_erase(process_list, i);
                            current_child = -1;
                            break;
                        }
                    }
                }

                else if (WIFEXITED(status) == 0)
                {
                    print_wait_failed();
                    for (size_t i = 0; i < vector_size(process_list); i++)
                    {
                        process_info *pi = vector_get(process_list, i);
                        if (pi->pid == child)
                        {
                            vector_erase(process_list, i);
                            current_child = -1;
                            break;
                        }
                    }
                    if (chose_and && idx == 0)
                    {
                        current_child = -1;
                        break;
                    }
                }

                else if (status / 255 == 13)
                {
                    for (size_t i = 0; i < vector_size(process_list); i++)
                    {
                        process_info *pi = vector_get(process_list, i);
                        if (pi->pid == child)
                        {
                            vector_erase(process_list, i);
                            current_child = -1;
                            break;
                        }
                    }
                    if (chose_and && idx == 0)
                    {
                        current_child = -1;
                        break;
                    }
                }

                else if (WIFEXITED(status))
                { // successful child termination
                    for (size_t i = 0; i < vector_size(process_list); i++)
                    {
                        process_info *pi = vector_get(process_list, i);
                        if (pi->pid == child)
                        {
                            vector_erase(process_list, i);
                            current_child = -1;
                            break;
                        }
                    }
                    if (chose_or && idx == 0)
                    {
                        current_child = -1;
                        break;
                    }
                }
                current_child = -1;
            }
        }
    }
}

void process_logical_file(FILE *file, char *lineptr, char *buffer, vector *process_list)
{

    int chose_and = 0;
    int chose_or = 0;
    int idx = 0;

    const char *l_and = "&&";
    const char *l_or = "||";
    const char *l_both = ";";
    const char *spc = " ";

    vector *logic_split = NULL;

    if (strstr(lineptr, l_and))
    { // found 'AND'
        logic_split = string_split(lineptr, l_and);
        chose_and = 1;
    }
    else if (strstr(lineptr, l_or))
    { // found 'OR'
        logic_split = string_split(lineptr, l_or);
        chose_or = 1;
    }
    else
    { // 'BOTH'
        logic_split = string_split(lineptr, l_both);
    }

    for (idx = 0; idx < 2; idx++)
    {

        char *part = vector_get(logic_split, idx);
        vector *split_section = string_split(part, spc);
        char *cmd = vector_get(split_section, 0);

        if (strncmp(cmd, "cd", 2) == 0)
        {
            char *path = vector_get(split_section, 1);
            int cdir = chdir(path);

            if (cdir != 0)
            {
                print_no_directory(path);
                if (chose_and && idx == 0)
                {
                    break;
                }
            }
            else
            {
                getcwd(buffer, 100);
                if (chose_or && idx == 0)
                {
                    break;
                }
            }
        }
        else
        {
            char **args = (char **)vector_at(split_section, 0);

            char *cmd_copy = malloc(strlen(args[0]) + 1);
            strcpy(cmd_copy, args[0]);

            fflush(file);

            pid_t child = fork();
            if (child == -1)
            {
                print_fork_failed();
                if (chose_and)
                    break;
            }

            int pgs = setpgid(child, child);
            if (pgs == -1)
            {
                print_setpgid_failed();
            }

            if (child == 0)
            {
                print_command_executed(getpid());
                execvp(args[0], args);
                print_exec_failed(args[0]);
                exit(13);
            }
            else
            { // parent
                current_child = child;

                process_info *ch = malloc(sizeof(process_info));
                ch->pid = child;
                ch->command = cmd_copy;
                vector_push_back(process_list, ch);

                int status = 0;
                waitpid(child, &status, 0);

                if (WIFSIGNALED(status))
                { // child killed via SIGINT
                    for (size_t i = 0; i < vector_size(process_list); i++)
                    {
                        process_info *pi = vector_get(process_list, i);
                        if (pi->pid == child)
                        {
                            vector_erase(process_list, i);
                            current_child = -1;
                            break;
                        }
                    }
                }

                else if (WIFEXITED(status) == 0)
                {
                    print_wait_failed();
                    for (size_t i = 0; i < vector_size(process_list); i++)
                    {
                        process_info *pi = vector_get(process_list, i);
                        if (pi->pid == child)
                        {
                            vector_erase(process_list, i);
                            current_child = -1;
                            break;
                        }
                    }
                    if (chose_and && idx == 0)
                    {
                        current_child = -1;
                        break;
                    }
                }

                else if (status / 255 == 13)
                {
                    for (size_t i = 0; i < vector_size(process_list); i++)
                    {
                        process_info *pi = vector_get(process_list, i);
                        if (pi->pid == child)
                        {
                            vector_erase(process_list, i);
                            current_child = -1;
                            break;
                        }
                    }
                    if (chose_and && idx == 0)
                    {
                        current_child = -1;
                        break;
                    }
                }

                else if (WIFEXITED(status))
                { // successful child termination
                    for (size_t i = 0; i < vector_size(process_list); i++)
                    {
                        process_info *pi = vector_get(process_list, i);
                        if (pi->pid == child)
                        {
                            vector_erase(process_list, i);
                            current_child = -1;
                            break;
                        }
                    }
                    if (chose_or && idx == 0)
                    {
                        current_child = -1;
                        break;
                    }
                }
                current_child = -1;
            }
        }
    }
}

void process_redirection_output(vector *output_cmd, char *redir_file, vector *process_list)
{
    int fd = open(redir_file, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        print_redirection_file_error();
        return;
    }

    fflush(stdout);
    pid_t child = fork();
    if (child == -1)
    {
        print_fork_failed();
        close(fd);
        return;
    }

    int pgs = setpgid(child, child);
    if (pgs == -1)
    {
        print_setpgid_failed();
    }

    if (child == 0)
    {
        print_command_executed(getpid());

        close(1);
        dup(fd);

        char **args = (char **)vector_at(output_cmd, 0);
        execvp(args[0], args);
        print_exec_failed(args[0]);
        exit(1);
    }
    else
    { // parent
        current_child = child;

        process_info *ch = malloc(sizeof(process_info));
        ch->pid = child;
        vector_push_back(process_list, ch);

        int status = 0;
        waitpid(child, &status, 0);
        if (WIFSIGNALED(status))
        { // if child killed via SIGINT
            for (size_t i = 0; i < vector_size(process_list); i++)
            {
                process_info *pi = vector_get(process_list, i);
                if (pi->pid == child)
                {
                    vector_erase(process_list, i);
                    current_child = -1;
                    break;
                }
            }
        }

        else if (WIFEXITED(status) == 0)
        {
            print_wait_failed();
            current_child = -1;
        }
        else if (WIFEXITED(status))
        { // successful child termination
            for (size_t i = 0; i < vector_size(process_list); i++)
            {
                process_info *pi = vector_get(process_list, i);
                if (pi->pid == child)
                {
                    vector_erase(process_list, i);
                    current_child = -1;
                    break;
                }
            }
        }
    }

    close(fd);
}

void process_redirection_append(vector *output_cmd, char *redir_file, vector *process_list)
{
    int fd = open(redir_file, O_CREAT | O_RDWR | O_APPEND, S_IRWXU);
    if (fd == -1)
    {
        print_redirection_file_error();
        return;
    }

    fflush(stdout);
    pid_t child = fork();
    if (child == -1)
    {
        print_fork_failed();
        close(fd);
        return;
    }

    int pgs = setpgid(child, child);
    if (pgs == -1)
    {
        print_setpgid_failed();
    }

    if (child == 0)
    {
        print_command_executed(getpid());

        close(1);
        dup(fd);

        char **args = (char **)vector_at(output_cmd, 0);
        execvp(args[0], args);
        print_exec_failed(args[0]);
        exit(1);
    }
    else
    { // parent
        current_child = child;

        process_info *ch = malloc(sizeof(process_info));
        ch->pid = child;
        vector_push_back(process_list, ch);

        int status = 0;
        waitpid(child, &status, 0);

        if (WIFSIGNALED(status))
        { // child killed via SIGINT
            for (size_t i = 0; i < vector_size(process_list); i++)
            {
                process_info *pi = vector_get(process_list, i);
                if (pi->pid == child)
                {
                    vector_erase(process_list, i);
                    current_child = -1;
                    break;
                }
            }
        }

        else if (WIFEXITED(status) == 0)
        {
            print_wait_failed();
            current_child = -1;
        }

        else if (WIFEXITED(status))
        { // successful child termination
            for (size_t i = 0; i < vector_size(process_list); i++)
            {
                process_info *pi = vector_get(process_list, i);
                if (pi->pid == child)
                {
                    vector_erase(process_list, i);
                    current_child = -1;
                    break;
                }
            }
        }
    }

    close(fd);
}

void process_prefix(char *lineptr, vector *history, char *buffer, vector *process_list)
{

    char *comp = lineptr + 1;
    char *vi;
    size_t comp_len = strlen(comp);
    size_t idx;
    size_t found_idx = 0;
    size_t found = 0;
    size_t hist_sz = vector_size(history);
    const char *spc = " ";

    for (idx = 0; idx < hist_sz; idx++)
    {
        vi = vector_get(history, idx);
        if (strncmp(comp, vi, comp_len) == 0)
        {
            found = 1;
            found_idx = idx;
        }
    }
    if (found)
    {
        char *redo = vector_get(history, found_idx);
        vector_push_back(history, redo);

        char *copy = malloc(strlen(redo) + 1);
        strcpy(copy, redo);
        print_command(redo);

        // split redo and determine cd or external
        vector *rd = string_split(copy, spc);
        char *new_cmd = vector_get(rd, 0);

        if (strncmp(new_cmd, "cd", 2) == 0)
        {
            char *pth = vector_get(rd, 1);
            change_directory(pth, buffer);
        }
        else
        {
            ext_cmd(redo, rd, process_list);
        }
        free(copy);
    }
    else
    {
        print_no_history_match();
    }
}

vector *string_split(char *input, const char *delim)
{
    vector *container = vector_create(&string_copy_constructor, &string_destructor, &string_default_constructor);
    char *token = strtok(input, delim);
    while (token)
    {
        vector_push_back(container, token);
        token = strtok(NULL, delim);
    }
    return container;
}

// File command reference: https://www.decompile.com/cpp/faq/fopen_write_append.htm
void write_history(vector *v, char *hist_filename)
{

    FILE *hf = fopen(hist_filename, "a");
    if (!hf)
    {
        hf = fopen(hist_filename, "w");
        if (!hf)
        {
            print_history_file_error();
            return;
        }
    }

    size_t idx = 0;
    char *current = vector_get(v, idx);

    while (current)
    {
        fprintf(hf, "%s\n", current);
        idx++;
        current = vector_get(v, idx);
    }
    fclose(hf);
}