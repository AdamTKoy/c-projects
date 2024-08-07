/**
 * password_cracker
 * CS 341 - Spring 2024
 */
#include "cracker1.h"
#include "format.h"
#include "utils.h"

#include "../includes/queue.h"
#include <string.h>
#include <unistd.h>
#include <crypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static int numRecovered = 0;
static int numFailed = 0;

static int done = 0;
static int found = 0;

// static/global queue so all threads can access
static queue *tasks;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

typedef struct task_t
{
    char username[9]; // at most 8 char + \0
    char pw_hash[14]; // always 13 char + \0
    char pw_known[9]; // at most 8 char + \0
} task_t;

static void *func(void *thread_id)
{
    double start_time = 0;
    double elapsed = 0;
    int max_guesses = 0;
    int num_unknowns = 0;
    char *name = NULL;
    char *known = NULL;
    int prefix_len = 0;
    int hash_count = 0;

    task_t *task = NULL;

    size_t id_temp = (size_t)thread_id;
    int id = (int)id_temp;

    while (1)
    {
        pthread_mutex_lock(&mtx); // to prevent race condition for 'done' variable

        if (!done)
        { // if done is 0, keep pulling
            task = queue_pull(tasks);
            if (!task)
            {
                done = 1;
            }
        }

        pthread_mutex_unlock(&mtx);

        if (done)
            break;

        start_time = getTime();
        elapsed = 0;

        // PRINT START
        name = task->username;
        v1_print_thread_start(id, name);

        // PROCESS TASK
        known = task->pw_known;
        const char *known_pw = known;
        prefix_len = getPrefixLength(known_pw);
        hash_count = 0;

        if (prefix_len == (int)strlen(known_pw))
        { // if NO unknown characters, already have password
            numRecovered++;
            elapsed = getTime() - start_time;
            v1_print_thread_result(id, name, known, hash_count, elapsed, 0);
            continue;
        }

        // if I get here, I must have at least ONE unknown

        max_guesses = 26;
        num_unknowns = strlen(known_pw) - prefix_len;

        for (int i = 1; i < num_unknowns; i++)
        {
            max_guesses *= 26;
        }

        char *guess = malloc(strlen(known_pw) + 1);
        guess[strlen(known_pw)] = '\0';

        // copy over known characters to 'guess'
        for (int i = 0; i < prefix_len; i++)
        {
            guess[i] = known_pw[i];
        }

        // initialize all unknown characters in 'guess' to 'a'
        for (int j = prefix_len; j < (int)strlen(known_pw); j++)
        {
            guess[j] = 'a';
        }

        const char *hash = task->pw_hash;

        struct crypt_data cdata;

        while (hash_count < max_guesses)
        {
            // hashing guess and comparing w/ given hash:
            cdata.initialized = 0;

            const char *hashed = crypt_r(guess, "xx", &cdata);

            if (strcmp(hashed, hash) == 0)
            {
                // match!
                numRecovered++;
                found = 1;
                elapsed = getTime() - start_time;
                v1_print_thread_result(id, name, guess, hash_count + 1, elapsed, 0);
                break;
            }
            else
            {
                // incremement string and start loop again
                incrementString(guess);
                hash_count++;
            }
        }

        if (hash_count == max_guesses && !found)
        {
            numFailed++;
            elapsed = getTime() - start_time;

            // NOTE: 'guess' variable is ignored if last argument (result) is 1 (failure)
            v1_print_thread_result(id, name, guess, max_guesses, elapsed, 1);
        }
        else
        {
            found = 0;
        }

        if (guess)
        {
            free(guess);
            guess = NULL;
        }

        // free task pulled from queue
        if (task)
        {
            free(task);
            task = NULL;
        }
    }

    // thread is DONE, returning to main
    return NULL;
}

int start(size_t thread_count)
{
    const char spc[2] = " ";

    tasks = queue_create(-1); // limitless queue

    // make threads
    pthread_t threads[thread_count];

    // NOTE: initially all threads will be blocked since queue size is 0

    for (size_t i = 0; i < thread_count; i++)
    {
        pthread_create(&threads[i], NULL, &func, (void *)(i + 1));
    }

    // main thread reads lines of input from STDIN
    // convert line to task (parse into 3 parts) and convert into a new struct
    // push to queue

    char *line = NULL;
    size_t bytes_read = 0;
    char *newline;

    // populate queue:
    while (getline(&line, &bytes_read, stdin) != -1)
    {

        // make a new task_t struct:
        task_t *new_task = malloc(sizeof(task_t));

        // remove newline character:
        newline = strchr(line, '\n');
        if (newline)
            *newline = '\0';

        // split (tokenize) line by space delimiter

        // first token: username
        char *token = strtok(line, spc);
        strcpy(new_task->username, token);
        new_task->username[strlen(token)] = '\0';

        // next token: hashed password
        token = strtok(NULL, spc);
        strcpy(new_task->pw_hash, token);
        new_task->pw_hash[13] = '\0';

        // last token: currently known portion of user password
        // assign token to known password in struct
        token = strtok(NULL, spc);
        strcpy(new_task->pw_known, token);
        new_task->pw_known[strlen(token)] = '\0';

        queue_push(tasks, (void *)new_task);
    }

    queue_push(tasks, (void *)(NULL));

    // free line
    if (line)
    {
        free(line);
    }

    // wait for all threads to finish
    for (size_t i = 0; i < thread_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // final summary (function grabs times on its own)
    v1_print_summary(numRecovered, numFailed);

    queue_destroy(tasks);
    tasks = NULL;

    return 0; // DO NOT change the return code since AG uses it to check if your
              // program exited normally
}
