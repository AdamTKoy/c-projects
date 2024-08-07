/**
 * deadlock_demolition
 * CS 341 - Spring 2024
 */
#include "graph.h"
#include "libdrm.h"
#include "set.h"
#include <pthread.h>

// I added:
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// note: only libdrm.c is graded; cannot change .h file
typedef struct drm_t
{
    pthread_mutex_t m;
} drm_t;

static graph *sg;
static pthread_mutex_t graph_lock = PTHREAD_MUTEX_INITIALIZER;

// reference: Ananth from CS341 discord
static int check_cycle(graph *g, void *n, set *seen);

drm_t *drm_init()
{

    // utilizing graph_lock for making inits thread safe
    pthread_mutex_lock(&graph_lock);

    if (sg == NULL)
    {
        // lazy initialization
        sg = graph_create(pointer_hash_function, shallow_compare, NULL, NULL, NULL, NULL, NULL, NULL);
    }

    drm_t *mutx = malloc(sizeof(drm_t));

    pthread_mutex_init(&mutx->m, NULL);

    // add drm/lock to graph
    graph_add_vertex(sg, (void *)mutx);

    pthread_mutex_unlock(&graph_lock);

    // return pointer to allocated heap memory
    return mutx;
}

int drm_post(drm_t *drm, pthread_t *thread_id)
{ // post equivalent to UNlock
    pthread_mutex_lock(&graph_lock);

    if (graph_contains_vertex(sg, (void *)thread_id))
    { // thread already in graph

        // does thread own drm?
        if (graph_adjacent(sg, (void *)drm, (void *)thread_id))
        {

            // YES: remove edge and unlock drm_t
            graph_remove_edge(sg, (void *)drm, (void *)thread_id);

            pthread_mutex_unlock(&drm->m);

            pthread_mutex_unlock(&graph_lock);
            return 1;
        }
    }

    pthread_mutex_unlock(&graph_lock);

    // o.w. no unlock possible
    return 0;
}

int drm_wait(drm_t *drm, pthread_t *thread_id)
{ // wait equivalent to LOCK
    pthread_mutex_lock(&graph_lock);

    // (1) make sure thread is in graph

    if (graph_contains_vertex(sg, (void *)thread_id) == 0)
    {
        graph_add_vertex(sg, (void *)thread_id);
    }

    // (2) check if current thread already owns drm (ATTEMPTING DOUBLE LOCK)

    // Is the drm already owned?
    if (graph_vertex_degree(sg, (void *)drm) > 0)
    {

        // is current thread already the owner?
        if (graph_adjacent(sg, (void *)drm, (void *)thread_id))
        {
            // reject attempt to double lock
            pthread_mutex_unlock(&graph_lock);
            return 0;
        }
    }

    // (3) otherwise add REQUEST edge from thread to drm
    graph_add_edge(sg, (void *)thread_id, (void *)drm);

    // check for deadlock condition

    // cycle?
    // YES: reject, remove edge, and return w/o locking (return 0)

    set *new_set = set_create(pointer_hash_function, shallow_compare, NULL, NULL);

    if (check_cycle(sg, (void *)thread_id, new_set))
    { // cycle detected!

        graph_remove_edge(sg, (void *)thread_id, (void *)drm);

        set_destroy(new_set);
        new_set = NULL;

        pthread_mutex_unlock(&graph_lock);

        return 0;
    }
    else
    { // no cycle detected

        // BUT: lock is currently owned, so leave 'request' edge for now

        set_destroy(new_set);
        new_set = NULL;

        pthread_mutex_unlock(&graph_lock);

        // add to queue for locking this drm
        pthread_mutex_lock(&drm->m);

        // if here, it got the lock, so reverse edge
        pthread_mutex_lock(&graph_lock);

        graph_remove_edge(sg, (void *)thread_id, (void *)drm);
        graph_add_edge(sg, (void *)drm, (void *)thread_id);

        pthread_mutex_unlock(&graph_lock);

        return 1;
    }
}

void drm_destroy(drm_t *drm)
{
    // Destroy any resources of the given drm.
    pthread_mutex_lock(&graph_lock);

    // Remove the vertex representing this drm from the Resource Allocation Graph.
    // NOTE: this also removes incoming/outgoing edges from drm vertex
    graph_remove_vertex(sg, (void *)drm);

    pthread_mutex_unlock(&graph_lock);

    // free the malloc'd memory
    free(drm);

    return;
}

static int check_cycle(graph *g, void *n, set *seen)
{

    if (set_contains(seen, n))
    {
        return 1;
    }
    set_add(seen, n);
    vector *neighbors = graph_neighbors(g, n);
    for (size_t i = 0; i < vector_size(neighbors); i++)
    {
        if (check_cycle(g, vector_get(neighbors, i), seen))
        {
            vector_destroy(neighbors);
            neighbors = NULL;
            return 1;
        }
    }

    vector_destroy(neighbors);
    neighbors = NULL;
    return 0;
}