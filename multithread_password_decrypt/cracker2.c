/**
 * password_cracker
 * CS 341 - Spring 2024
 */
#include "cracker2.h"
#include "format.h"
#include "utils.h"

#include "../includes/queue.h"
#include <string.h>
#include <unistd.h>
#include <crypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static queue *tasks;

static size_t t_count = 0;

static int cancel_flag = 0;
static int done = 0;
static int global_found = 0;

static double start_time = 0;
static double start_cpu_time = 0;
static double total_cpu_time = 0;
static double elapsed = 0;

static pthread_barrier_t barrier;
static pthread_barrier_t barrier2;

static pthread_mutex_t mtx;

typedef struct task2_t
{
    char username[9];     // at most 8 char + \0
    char pw_hash[14];     // always 13 char + \0
    char pw_known[9];     // at most 8 char + \0
    int unknowns;         // number of periods in known password
    int total_hash_count; // total num hashes across all threads
    char *found_pw;
} task2_t;

static task2_t *current = NULL;

static void *func(void *thread_id)
{

    int prefix_len = 0;
    int hash_count = 0;
    long *start = NULL;
    long *max_guesses = NULL;

    int found = 0;

    size_t id_temp = (size_t)thread_id;
    int id = (int)id_temp;

    while (1)
    {

        // make sure all worker threads assembled and main thread has new task before starting
        pthread_barrier_wait(&barrier);

        if (done)
            break;

        start = (long *)malloc(sizeof(long));
        max_guesses = (long *)malloc(sizeof(long));
        getSubrange(current->unknowns, t_count, id, start, max_guesses);

        char *guess = malloc(strlen(current->pw_known) + 1);
        strcpy(guess, current->pw_known);
        setStringPosition(guess, *start);

        // copy over known password characters
        prefix_len = strlen(guess) - current->unknowns;

        for (int i = 0; i < prefix_len; i++)
        {
            guess[i] = current->pw_known[i];
        }

        v2_print_thread_start(id, current->username, *start, guess);
        hash_count = 0;

        const char *hash = current->pw_hash;

        struct crypt_data cdata;
        const char *hashed = NULL;

        while (hash_count < (int)*max_guesses)
        {

            pthread_mutex_lock(&mtx);
            if (cancel_flag)
            { // Another thread found pw -> CANCEL
                current->total_hash_count += hash_count;
                pthread_mutex_unlock(&mtx);

                v2_print_thread_result(id, hash_count, 1);

                break;
            }
            pthread_mutex_unlock(&mtx);

            cdata.initialized = 0;

            hashed = crypt_r(guess, "xx", &cdata);

            if (strcmp(hashed, hash) == 0)
            { // pw FOUND
                hash_count++;
                found = 1;

                pthread_mutex_lock(&mtx);

                global_found = 1;

                current->total_hash_count += hash_count;
                current->found_pw = guess;

                cancel_flag = 1;

                pthread_mutex_unlock(&mtx);

                v2_print_thread_result(id, hash_count, 0);

                break;
            }
            else
            { // Not Found; Try Again
                incrementString(guess);
                hash_count++;
            }

            if (hash_count == (int)*max_guesses)
            { // Out of guesses

                pthread_mutex_lock(&mtx);
                current->total_hash_count += (int)*max_guesses;
                pthread_mutex_unlock(&mtx);

                v2_print_thread_result(id, hash_count, 2);
            }
        }

        // freeing any local dynamically allocated memory
        // (have to keep the 'found' password until main thread can print summary)
        if (!found && guess)
        {
            free(guess);
            guess = NULL;
        }

        if (start)
        {
            free(start);
            start = NULL;
        }
        if (max_guesses)
        {
            free(max_guesses);
            max_guesses = NULL;
        }

        // when this releases, main thread knows that all threads finished and can perform closing actions
        pthread_barrier_wait(&barrier2);
    }

    return NULL;
}

int start(size_t thread_count)
{
    const char spc[2] = " ";

    t_count = thread_count;

    pthread_barrier_init(&barrier, NULL, thread_count + 1);
    pthread_barrier_init(&barrier2, NULL, thread_count + 1);
    pthread_mutex_init(&mtx, NULL);

    tasks = queue_create(-1);

    char *line = NULL;
    size_t bytes_read = 0;
    char *newline = NULL;

    while (getline(&line, &bytes_read, stdin) != -1)
    {

        task2_t *new_task = malloc(sizeof(task2_t));

        // remove newline character:
        newline = strchr(line, '\n');
        if (newline)
            *newline = '\0';

        // first token: username
        char *token = strtok(line, spc);
        strcpy(new_task->username, token);
        new_task->username[strlen(token)] = '\0';

        // second token: hashed password
        token = strtok(NULL, spc);
        strcpy(new_task->pw_hash, token);
        new_task->pw_hash[13] = '\0';

        // last token: currently known portion of user password
        token = strtok(NULL, spc);
        strcpy(new_task->pw_known, token);
        new_task->pw_known[strlen(token)] = '\0';

        new_task->unknowns = strlen(new_task->pw_known) - getPrefixLength(new_task->pw_known);
        new_task->total_hash_count = 0;
        new_task->found_pw = NULL;

        queue_push(tasks, (void *)new_task);
    }

    queue_push(tasks, (void *)(NULL));

    if (line)
    {
        free(line);
    }

    pthread_t threads[thread_count];

    for (size_t i = 0; i < thread_count; i++)
    {
        pthread_create(&threads[i], NULL, &func, (void *)(i + 1));
    }

    while (1)
    {
        // pull new task from queue
        pthread_mutex_lock(&mtx);
        current = queue_pull(tasks);
        pthread_mutex_unlock(&mtx);

        // check for empty queue
        if (!current)
        {

            done = 1;

            // wait for all worker threads to assemble before breaking out
            pthread_barrier_wait(&barrier);

            break;
        }

        pthread_mutex_lock(&mtx);
        v2_print_start_user(current->username);
        pthread_mutex_unlock(&mtx);

        // wait for all worker threads to assemble before breaking out
        pthread_barrier_wait(&barrier);

        start_time = getTime();
        start_cpu_time = getCPUTime();

        // wait for all worker threads to finish w/ current task
        pthread_barrier_wait(&barrier2);

        elapsed = getTime() - start_time;
        total_cpu_time = getCPUTime() - start_cpu_time;

        pthread_mutex_lock(&mtx); // print summary and reset global variables

        v2_print_summary(current->username, current->found_pw, current->total_hash_count,
                         elapsed, total_cpu_time, !global_found);

        // reset

        if (cancel_flag)
            cancel_flag = 0;
        if (global_found)
            global_found = 0;

        if (current->found_pw)
        {
            free(current->found_pw);
            current->found_pw = NULL;
        }

        if (current)
        {
            free(current);
            current = NULL;
        }

        pthread_mutex_unlock(&mtx);
    }

    for (size_t i = 0; i < thread_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    queue_destroy(tasks);
    tasks = NULL;

    pthread_barrier_destroy(&barrier);
    pthread_barrier_destroy(&barrier2);
    pthread_mutex_destroy(&mtx);

    return 0; // DO NOT change the return code since AG uses it to check if your
              // program exited normally
}
