/**
 * parallel_make
 * CS 341 - Spring 2024
 */

#include "format.h"
#include "graph.h"
#include "parmake.h"
#include "parser.h"

// my additions
#include "../includes/queue.h"
#include "../includes/set.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// static variables for graph
static graph *g;
static pthread_mutex_t graph_lock;

// static queue for rules that need their commands executed
static queue *tasks;

// when you need to modify a rule's 'state' member
static pthread_mutex_t task_lock;

static int done = 0;
static pthread_mutex_t done_lock;

// reference: https://www.geeksforgeeks.org/detect-cycle-in-a-graph/
static int check_cycle(void *n, set *visited, set *to_check);

// function to push task w/ no remaining dependencies to queue
// (and decrement ancestors' remaining number of dependencies)
static void push_to_queue(rule_t *rule);

// set to keep track of rules that fail (or should fail)
static set *fails = NULL;

// when you need to read from or modify the 'fails' set
static pthread_mutex_t fail_set;

// thread function
static void *func()
{

    rule_t *rule = NULL;
    int should_fail;
    int dep_failed;

    while (1)
    {
        // try to pull from task queue (thread-safe)

        // prevents race condition on 'done' global variable
        pthread_mutex_lock(&done_lock);

        if (!done)
        {
            rule = (rule_t *)queue_pull(tasks);

            if (!rule)
            {
                done = 1;
            }
            else
            {
                pthread_mutex_lock(&task_lock);
                rule->state = 1; // 1: processing rule
                pthread_mutex_unlock(&task_lock);
            }
        }

        pthread_mutex_unlock(&done_lock);

        if (done)
        {
            break;
        }

        // check if newly-pulled rule is in failure set
        should_fail = 0;

        pthread_mutex_lock(&fail_set);
        vector *fs = set_elements(fails);
        pthread_mutex_unlock(&fail_set);

        for (size_t i = 0; i < vector_size(fs); i++)
        {
            void *e = vector_get(fs, i);

            if (strcmp((char *)e, rule->target) == 0)
            {
                should_fail = 1;
            }
        }

        vector_destroy(fs);
        fs = NULL;

        if (should_fail)
        {
            // add this rule's ancestors to 'fails' set if not already there
            pthread_mutex_lock(&graph_lock);
            vector *ancestors = graph_antineighbors(g, (void *)rule->target);
            pthread_mutex_unlock(&graph_lock);

            void *a = NULL;

            for (size_t i = 0; i < vector_size(ancestors); i++)
            {
                a = vector_get(ancestors, i);

                pthread_mutex_lock(&fail_set);
                set_add(fails, a);
                pthread_mutex_unlock(&fail_set);
            }

            vector_destroy(ancestors);
            ancestors = NULL;

            // check if any ancestors waiting on this dependency to complete
            pthread_mutex_lock(&task_lock);
            rule->state = 3;
            pthread_cond_broadcast(rule->data);
            pthread_mutex_unlock(&task_lock);

            continue; // try for another rule
        }

        // grab vector of dependencies from graph
        pthread_mutex_lock(&graph_lock);
        vector *deps = graph_neighbors(g, (void *)rule->target);
        pthread_mutex_unlock(&graph_lock);

        // check if rule has unsatisfied dependencies
        // (dependencies would have already been pulled by threads,
        // but maybe are still being worked on)
        rule_t *d = NULL;
        dep_failed = 0;

        for (size_t i = 0; i < vector_size(deps); i++)
        {

            pthread_mutex_lock(&graph_lock);
            d = (rule_t *)graph_get_vertex_value(g, vector_get(deps, i));
            pthread_mutex_unlock(&graph_lock);

            pthread_mutex_lock(&task_lock);

            if (d->state == 1)
            {
                // dependency still processing; need to wait
                while (pthread_cond_wait(d->data, &task_lock) != 0)
                    ;

                // now check state again
                // if it's 2, keep going
                // if it's 3, set this rule's state to '3' and add it and all its ancestors to fail set
                if (d->state != 2)
                {
                    dep_failed = 1;

                    pthread_mutex_lock(&fail_set);
                    set_add(fails, (void *)rule->target);
                    pthread_mutex_unlock(&fail_set);

                    pthread_mutex_lock(&graph_lock);
                    vector *ancestors = graph_antineighbors(g, (void *)rule->target);
                    pthread_mutex_unlock(&graph_lock);

                    void *a = NULL;

                    for (size_t i = 0; i < vector_size(ancestors); i++)
                    {
                        a = vector_get(ancestors, i);

                        pthread_mutex_lock(&fail_set);
                        set_add(fails, a);
                        pthread_mutex_unlock(&fail_set);
                    }

                    vector_destroy(ancestors);
                    ancestors = NULL;

                    rule->state = 3;
                    // if any thread was waiting on this rule to finish, notify it
                    pthread_cond_broadcast(rule->data);
                }
            }

            pthread_mutex_unlock(&task_lock);
        }

        // if a dependency failed, skip to next task in queue
        if (dep_failed)
        {
            continue;
        }

        int found = 0;
        size_t num_cmds = vector_size(rule->commands);

        // if rule->target is the name of file on disk (contains a '/')
        if (strchr(rule->target, '/') != NULL)
        {
            // does rule depend on another rule that is NOT a file on disk?
            for (size_t i = 0; i < vector_size(deps); i++)
            {

                void *d = vector_get(deps, i);

                pthread_mutex_lock(&graph_lock);
                rule_t *dep = (rule_t *)graph_get_vertex_value(g, d);
                pthread_mutex_unlock(&graph_lock);

                if (strchr(dep->target, '/') == NULL)
                { // found non-file-on-disk command
                    found = 1;
                    int result = 0;
                    char *cmd = NULL;
                    // if yes, run commands
                    for (size_t j = 0; j < num_cmds; j++)
                    {
                        cmd = (char *)vector_get(rule->commands, j);
                        result = system(cmd);
                        if (result != 0)
                        { // command failed

                            // get ancestors

                            pthread_mutex_lock(&graph_lock);
                            vector *ancestors = graph_antineighbors(g, (void *)rule->target);
                            pthread_mutex_unlock(&graph_lock);

                            // add rule and all its ancestors to global 'fails' set
                            void *a;

                            pthread_mutex_lock(&fail_set);
                            set_add(fails, (void *)rule->target);
                            pthread_mutex_unlock(&fail_set);

                            for (size_t k = 0; k < vector_size(ancestors); k++)
                            {
                                a = vector_get(ancestors, k);

                                pthread_mutex_lock(&fail_set);
                                set_add(fails, a);
                                pthread_mutex_unlock(&fail_set);
                            }

                            vector_destroy(ancestors);
                            ancestors = NULL;

                            pthread_mutex_lock(&task_lock);
                            rule->state = 3;
                            pthread_cond_broadcast(rule->data);
                            pthread_mutex_unlock(&task_lock);

                            break;
                        }
                    }

                    break;
                }
            }

            // if we get here and 'found' is still 0
            // then we found NO dependencies that were NOT files on disk
            if (!found)
            {
                // do any dependencies have newer modification time than rule's modification time?
                struct stat rule_stats;
                stat(rule->target, &rule_stats);

                if (stat(rule->target, &rule_stats) == -1)
                {
                    // note: file may not be created yet and then it's expected for stat to return -1
                    char *cmd = NULL;
                    int result = 0;
                    for (size_t j = 0; j < num_cmds; j++)
                    {
                        cmd = (char *)vector_get(rule->commands, j);
                        result = system(cmd);
                        if (result != 0)
                        { // command failed

                            // get ancestors
                            pthread_mutex_lock(&graph_lock);
                            vector *ancestors = graph_antineighbors(g, (void *)rule->target);
                            pthread_mutex_unlock(&graph_lock);

                            // add rule and all its ancestors to global 'fails' set
                            pthread_mutex_lock(&fail_set);
                            set_add(fails, (void *)rule->target);
                            pthread_mutex_unlock(&fail_set);

                            void *a;

                            for (size_t k = 0; k < vector_size(ancestors); k++)
                            {
                                a = vector_get(ancestors, k);

                                pthread_mutex_lock(&fail_set);
                                set_add(fails, a);
                                pthread_mutex_unlock(&fail_set);
                            }

                            vector_destroy(ancestors);
                            ancestors = NULL;

                            pthread_mutex_lock(&task_lock);
                            rule->state = 3;
                            pthread_cond_broadcast(rule->data);
                            pthread_mutex_unlock(&task_lock);

                            break;
                        }
                    }
                }

                else
                {
                    void *d = NULL;
                    rule_t *dep = NULL;

                    for (size_t i = 0; i < vector_size(deps); i++)
                    {

                        d = vector_get(deps, i);

                        pthread_mutex_lock(&graph_lock);
                        dep = (rule_t *)graph_get_vertex_value(g, d);
                        pthread_mutex_unlock(&graph_lock);

                        struct stat dep_stats;
                        stat(dep->target, &dep_stats);

                        // (is child/dependency newer than parent by more than 1 second?)
                        if (difftime(dep_stats.st_mtime, rule_stats.st_mtime) >= 1)
                        {
                            // if YES: run rule's commands one by one
                            char *cmd = NULL;
                            int result = 0;
                            for (size_t j = 0; j < num_cmds; j++)
                            {
                                cmd = (char *)vector_get(rule->commands, j);
                                result = system(cmd);
                                if (result != 0)
                                { // command failed

                                    // pthread_mutex_lock(&task_lock);
                                    rule->state = 3;
                                    // pthread_mutex_unlock(&task_lock);

                                    // get ancestors
                                    pthread_mutex_lock(&graph_lock);
                                    vector *ancestors = graph_antineighbors(g, (void *)rule->target);
                                    pthread_mutex_unlock(&graph_lock);

                                    // add rule and all its ancestors to global 'fails' set
                                    pthread_mutex_lock(&fail_set);
                                    set_add(fails, (void *)rule->target);
                                    pthread_mutex_unlock(&fail_set);

                                    void *a;

                                    for (size_t k = 0; k < vector_size(ancestors); k++)
                                    {
                                        a = vector_get(ancestors, k);

                                        pthread_mutex_lock(&fail_set);
                                        set_add(fails, a);
                                        pthread_mutex_unlock(&fail_set);
                                    }

                                    vector_destroy(ancestors);
                                    ancestors = NULL;

                                    break;
                                }
                            }
                            // do not need to keep checking dependencies
                            break;
                        }
                    }
                }
            }

            // if we get here, then we're done processing rule
            // if 'state' is still 1, then it was satisfied and we can mark as '2'

            pthread_mutex_lock(&task_lock);
            if (rule->state == 1)
            {
                rule->state = 2; // 2: rule satisfied

                // broadcast cv in case any ancestors were waiting on this dependency to finish
                pthread_cond_broadcast(rule->data);
            }
            pthread_mutex_unlock(&task_lock);
        }

        else
        { // target is not file on disk, so run commands one by one
            int result = 0;
            char *cmd = NULL;
            int failure = 0;

            for (size_t i = 0; i < num_cmds; i++)
            {
                cmd = (char *)vector_get(rule->commands, i);

                result = system(cmd);

                if (result != 0)
                { // command failed
                    failure = 1;

                    pthread_mutex_lock(&task_lock);
                    rule->state = 3; // 3: failure
                    pthread_cond_broadcast(rule->data);
                    pthread_mutex_unlock(&task_lock);

                    // get ancestors
                    pthread_mutex_lock(&graph_lock);
                    vector *ancestors = graph_antineighbors(g, (void *)rule->target);
                    pthread_mutex_unlock(&graph_lock);

                    void *a;

                    pthread_mutex_lock(&fail_set);
                    set_add(fails, (void *)rule->target);
                    pthread_mutex_unlock(&fail_set);

                    for (size_t k = 0; k < vector_size(ancestors); k++)
                    {
                        a = vector_get(ancestors, k);

                        pthread_mutex_lock(&fail_set);
                        set_add(fails, a);
                        pthread_mutex_unlock(&fail_set);
                    }

                    vector_destroy(ancestors);
                    ancestors = NULL;

                    break;
                }
            }

            if (!failure)
            { // if 'failure' still 0 then all commands finished successfully

                pthread_mutex_lock(&task_lock);
                rule->state = 2;
                pthread_cond_broadcast(rule->data);
                pthread_mutex_unlock(&task_lock);
            }
        }

        if (deps)
        {
            vector_destroy(deps);
            deps = NULL;
        }
    }

    return NULL;
}

int parmake(char *makefile, size_t num_threads, char **targets)
{

    // Note: They will try to artificially create spurious wakeups

    // The parser will return a graph whose VERTICES represent all the RULES contained in the makefile (incl. those that do NOT need to be run)
    // as well as a SENTINEL vertex with an empty target ("") whose NEIGHBORS are the GOALS specified by the `targets` argument
    // The graph's vertex set is effectively a DICTIONARY that maps targets to `rule_t` STRUCTS
    g = parser_parse_makefile(makefile, targets);

    // vector of all goals (represented as strings)
    // NOTE: If `targets` is empty (eg. targets == NULL), the FIRST target in the file will be used as the only goals
    vector *goals = graph_neighbors(g, "");

    // Check each goal for a cycle
    int result;
    set *visited_set;
    set *check_set;
    set *to_remove = set_create(pointer_hash_function, shallow_compare, NULL, NULL);

    for (size_t i = 0; i < vector_size(goals); i++)
    {
        visited_set = set_create(pointer_hash_function, shallow_compare, NULL, NULL);
        check_set = set_create(pointer_hash_function, shallow_compare, NULL, NULL);
        void *vtx = vector_get(goals, i);

        result = check_cycle(vtx, visited_set, check_set);

        if (result)
        { // cycle detected
            print_cycle_failure((char *)vtx);
            set_add(to_remove, vtx);
        }

        set_destroy(visited_set);
        visited_set = NULL;
        set_destroy(check_set);
        check_set = NULL;
    }

    // get vector of all members of set: vector *set_elements(set *this);
    vector *removing = set_elements(to_remove);

    void *v = NULL;

    // remove all goals w/ cycles from graph
    for (size_t k = 0; k < vector_size(removing); k++)
    {
        v = vector_get(removing, k);

        // Removing a vertex will destroy the associated string key and `rule_t` struct (i.e. destroy the target)
        graph_remove_vertex(g, v);
    }

    set_destroy(to_remove);
    to_remove = NULL;
    vector_destroy(removing);
    removing = NULL;
    vector_destroy(goals);
    goals = NULL;

    // limitless queue
    tasks = queue_create(-1);
    fails = set_create(pointer_hash_function, shallow_compare, NULL, NULL);

    // grab goals again (since you've potentially removed vertices from graph)
    goals = graph_neighbors(g, "");

    // mutexes for graph and global variables
    pthread_mutex_init(&done_lock, NULL);
    pthread_mutex_init(&graph_lock, NULL);
    pthread_mutex_init(&task_lock, NULL);
    pthread_mutex_init(&fail_set, NULL);

    // initialize threads
    pthread_t threads[num_threads];

    for (size_t idx = 0; idx < num_threads; idx++)
    {
        pthread_create(&threads[idx], NULL, &func, NULL);
    }

    // populate queue

    // convert strings to rule_t structs before pushing to queue
    // `rule_t * rule = (rule_t *)graph_get_vertex_value(dep_graph, "tgt")`

    set *v_seen = set_create(pointer_hash_function, shallow_compare, NULL, NULL);
    set *processed = set_create(pointer_hash_function, shallow_compare, NULL, NULL);

    vector *to_process = NULL;
    vector *to_review = NULL;

    // threads are already active, so be sure to add mutex locks where appropriate

    // cycle through EVERY vertex and set its 'state' member to be # of dependencies
    // (is there a way to make this so that we only update vertices that are or are descendants of goals?)
    vector *all_vertices = graph_vertices(g);
    size_t num_rules = vector_size(all_vertices);

    // one condition variable for each rule
    // that will be stored in 'data' member of rule_t struct
    pthread_cond_t cvs[num_rules];

    void *vertx = NULL;

    for (size_t idx = 0; idx < num_rules; idx++)
    {
        vertx = vector_get(all_vertices, idx);
        rule_t *r = (rule_t *)graph_get_vertex_value(g, vertx);

        r->state = (int)graph_vertex_degree(g, vertx);

        // condition variable for this specific rule
        pthread_cond_init(&cvs[idx], NULL);
        r->data = (void *)&cvs[idx];
    }
    vector_destroy(all_vertices);
    all_vertices = NULL;

    for (size_t idx = 0; idx < vector_size(goals); idx++)
    {
        to_process = vector_create(&string_copy_constructor, &string_destructor, &string_default_constructor);
        to_review = vector_create(&string_copy_constructor, &string_destructor, &string_default_constructor);

        vector_push_back(to_process, vector_get(goals, idx));

        set_add(v_seen, vector_get(goals, idx));

        void *v = NULL;

        while (!vector_empty(to_process))
        {

            // always grab the first element
            v = vector_get(to_process, 0);

            pthread_mutex_lock(&graph_lock);
            rule_t *rule = (rule_t *)graph_get_vertex_value(g, v);
            pthread_mutex_unlock(&graph_lock);

            // if we've already processed the target, skip ahead to next target in to_process
            if (set_contains(processed, v))
            {
                vector_erase(to_process, 0);
                continue;
            }

            // pull children
            pthread_mutex_lock(&graph_lock);
            vector *children = graph_neighbors(g, v);
            pthread_mutex_unlock(&graph_lock);

            if (vector_size(children) > 0)
            { // if target has dependencies, check if they also have dependencies

                void *child;

                for (size_t i = 0; i < vector_size(children); i++)
                {
                    child = vector_get(children, i);

                    // if we've already seen the target, skip ahead to next child/target
                    if (set_contains(processed, child))
                    {
                        continue;
                    }

                    pthread_mutex_lock(&graph_lock);
                    size_t num_dep = graph_vertex_degree(g, child);
                    pthread_mutex_unlock(&graph_lock);

                    if (num_dep > 0)
                    { // if dep/child also has children/dependencies

                        if (!set_contains(v_seen, child))
                        {
                            set_add(v_seen, child);
                            vector_push_back(to_process, child);
                        }
                    }
                    else
                    { // no children, can push to task queue
                        pthread_mutex_lock(&graph_lock);
                        rule_t *c = (rule_t *)graph_get_vertex_value(g, child);
                        pthread_mutex_unlock(&graph_lock);
                        set_add(processed, child);
                        push_to_queue(c);
                    }
                }

                if (rule->state == 0)
                { // if ALL children/dependencies of this rule were pushed to task queue
                    set_add(processed, v);
                    push_to_queue(rule);
                    vector_erase(to_process, 0);
                }
                else
                { // otherwise rule still has remaining dependencies and we'll come back to it later
                    vector_insert(to_review, 0, (void *)rule->target);
                    vector_erase(to_process, 0);
                }
            }
            else
            { // no children, can push to task queue right away
                set_add(processed, v);
                push_to_queue(rule);
                vector_erase(to_process, 0);
            }

            vector_destroy(children);
            children = NULL;
        }

        // now to_process vector is empty, so go back to to_review
        vector_destroy(to_process);
        to_process = NULL;

        rule_t *r1 = NULL;

        for (size_t i = 0; i < vector_size(to_review); i++)
        {
            v = vector_get(to_review, i);

            pthread_mutex_lock(&graph_lock);
            r1 = (rule_t *)graph_get_vertex_value(g, v);
            pthread_mutex_unlock(&graph_lock);

            if (r1->state == 0)
            {
                set_add(processed, v);
                push_to_queue(r1);
            }
        }

        vector_destroy(to_review);
        to_review = NULL;
    }

    // once we get here, we're done adding tasks, so add NULL terminator
    queue_push(tasks, (void *)NULL);

    // wait for all threads to finish
    for (size_t i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    for (size_t j = 0; j < num_rules; j++)
    {
        pthread_cond_destroy(&cvs[j]);
    }

    vector_destroy(goals);
    goals = NULL;

    graph_destroy(g);
    g = NULL;

    set_destroy(v_seen);
    v_seen = NULL;
    set_destroy(processed);
    processed = NULL;
    set_destroy(fails);
    fails = NULL;

    queue_destroy(tasks);
    tasks = NULL;

    pthread_mutex_destroy(&graph_lock);
    pthread_mutex_destroy(&task_lock);
    pthread_mutex_destroy(&done_lock);
    pthread_mutex_destroy(&fail_set);

    return 0;
}

// run this on each 'goal'
// and then it will recursively check each goal's dependencies

static int check_cycle(void *n, set *visited, set *to_check)
{

    if (set_contains(to_check, n))
    {
        return 1;
    }

    if (set_contains(visited, n))
    {
        return 0;
    }

    set_add(visited, n);
    set_add(to_check, n);

    pthread_mutex_lock(&graph_lock);
    vector *neighbors = graph_neighbors(g, n);
    pthread_mutex_unlock(&graph_lock);

    int result = 0;

    for (size_t i = 0; i < vector_size(neighbors); i++)
    {
        result = check_cycle(vector_get(neighbors, i), visited, to_check);
        if (result)
        { // cycle detected
            vector_destroy(neighbors);
            neighbors = NULL;
            return 1;
        }
    }

    // if we get here, then no cycles detected in n's children
    // n has been fully checked and can be removed from to_check set
    set_remove(to_check, n);

    vector_destroy(neighbors);
    neighbors = NULL;
    return 0;
}

static void push_to_queue(rule_t *rule)
{

    pthread_mutex_lock(&graph_lock);
    vector *ancestors = graph_antineighbors(g, (void *)rule->target);
    pthread_mutex_unlock(&graph_lock);

    if (vector_size(ancestors) > 0)
    {
        void *a = NULL;
        rule_t *r = NULL;
        for (size_t j = 0; j < vector_size(ancestors); j++)
        {
            a = vector_get(ancestors, j);

            pthread_mutex_lock(&graph_lock);
            r = (rule_t *)graph_get_vertex_value(g, a);
            pthread_mutex_unlock(&graph_lock);

            // pthread_mutex_lock(&task_lock);
            r->state--;
            // pthread_mutex_unlock(&task_lock);
        }
    }

    queue_push(tasks, (void *)rule);

    vector_destroy(ancestors);
    ancestors = NULL;
}