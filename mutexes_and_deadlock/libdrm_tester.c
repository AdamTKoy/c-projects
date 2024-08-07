/**
 * deadlock_demolition
 * CS 341 - Spring 2024
 */
#include "libdrm.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// pthread_t   tids[2];
// drm_t      *drms[2];
// int         vals[2];

drm_t * A;
drm_t * B;
drm_t * C;
drm_t * D;

pthread_t p1;
pthread_t p2;
pthread_t p3;
pthread_t p4;

/*
void *basic_deadlock_task(void *idx) {
    size_t i = *(size_t *)(idx);
    pthread_t *tid = tids + i;
    if (drm_wait(drms[i], tid)) {
        printf("%lu is here\n", i);
        ++vals[i];
        if (!i) {
            printf("%lu is sleeping\n", i);
            sleep(1);
        }
            
    }
    else {
        printf("Deadlock 0 in task %lu!\n", i);
    }
    if (drm_wait(drms[i ^ 1], tid)) {
        printf("%lu is there\n", i);
        --vals[i ^ 1];
        if (i) {
            printf("%lu is sleeping\n", i);
            sleep(1);
        }
    }
    else {
        printf("Deadlock 1 in task %lu\n", i);
    }
    if (!drm_post(drms[i], tid)) {
        //printf("tid was %p\n", tid);
        //printf("drms[%lu] was %p\n", i, drms[i]);
        printf("Error unlocking drms in task %lu\n", i);
        //abort();
    }
    if (!drm_post(drms[i ^ 1], tid)) {
        //printf("tid was %p\n", tid);
        //printf("drms[%lu] was %p\n", i, drms[i]);
        printf("Error unlocking drms in task %lu\n", i);
        //abort();
    }
    printf("Final values in task %lu: %i, %i\n", i, vals[i], vals[i ^ 1]);
    return NULL;
}
*/


void * p1_task() {
    int a = drm_wait(A, (void*)pthread_self());
    if (!a) {
        printf("deadlock: rejecting p1's attempt to lock A.\n");
    }
    else {
        printf("p1 locked A successfully.\n");
    }

    int b = drm_wait(B, (void*)pthread_self());
    if (!b) {
        printf("deadlock: rejecting p1's attempt to lock B.\n");
    }
    else {
        printf("p1 locked B successfully.\n");
    }
    

    int ap = drm_post(A, (void*)pthread_self());
    if (!ap) {
        printf("rejecting p1's attempt to UN_lock A.\n");
    }
    else {
        printf("p1 UN_locked A successfully.\n");
    }

    int bp = drm_post(B, (void*)pthread_self());
    if (!bp) {
        printf("rejecting p1's attempt to UN_lock B.\n");
    }
    else {
        printf("p1 UN_locked B successfully.\n");
    }

    int c = drm_wait(C, (void*)pthread_self());
    if (!c) {
        printf("deadlock: rejecting p1's attempt to lock C.\n");
    }
    else {
        printf("p1 locked C successfully.\n");
    }

    
    int cp = drm_post(C, (void*)pthread_self());
    if (!cp) {
        printf("rejecting p1's attempt to UN_lock C.\n");
    }
    else {
        printf("p1 UNlocked C successfully.\n");
    }
    

    return NULL;
}

void * p2_task() {
    
    // LOCKS
    int c = drm_wait(C, (void*)pthread_self());
    if (!c) {
        printf("deadlock: rejecting p2's attempt to lock C.\n");
    }
    else {
        printf("p2 locked C successfully.\n");
    }

    
    int d = drm_wait(D, (void*)pthread_self());
    if (!d) {
        printf("deadlock: rejecting p2's attempt to lock D.\n");
    }
    else {
        printf("p2 locked D successfully.\n");
    }
    

    int b = drm_wait(B, (void*)pthread_self());
    if (!b) {
        printf("deadlock: rejecting p2's attempt to lock B.\n");
    }
    else {
        printf("p2 locked B successfully.\n");
    }

    // UNLOCKS
    int cp = drm_post(C, (void*)pthread_self());
    if (!cp) {
        printf("rejecting p2's attempt to UN_lock C.\n");
    }
    else {
        printf("p2 UN_locked C successfully.\n");
    }

    
    int dp = drm_post(D, (void*)pthread_self());
    if (!dp) {
        printf("rejecting p2's attempt to UN_lock D.\n");
    }
    else {
        printf("p2 UN_locked D successfully.\n");
    }
    

    int bp = drm_post(B, (void*)pthread_self());
    if (!bp) {
        printf("rejecting p2's attempt to UN_lock B.\n");
    }
    else {
        printf("p2 UN_locked B successfully.\n");
    }

    return NULL;
}

void * p3_task() {
    
    int b = drm_wait(B, (void*)pthread_self());
    if (!b) {
        printf("deadlock: rejecting p3's attempt to lock B.\n");
    }
    else {
        printf("p3 locked B successfully.\n");
    }

    int bp = drm_post(B, (void*)pthread_self());
    if (!bp) {
        printf("rejecting p3's attempt to UN_lock B.\n");
    }
    else {
        printf("p3 UN_locked B successfully.\n");
    }

    return NULL;
}

void * p4_task() {

    /*
    int c = drm_wait(C, (void*)pthread_self());
    if (!c) {
        printf("deadlock: rejecting p4's attempt to lock C.\n");
    }
    else {
        printf("p4 locked C successfully.\n");
    }

    int b = drm_wait(B, (void*)pthread_self());
    if (!b) {
        printf("deadlock: rejecting p4's attempt to lock B.\n");
    }
    else {
        printf("p4 locked B successfully.\n");
    }

    int cp = drm_post(C, (void*)pthread_self());
    if (!cp) {
        printf("rejecting p4's attempt to UN_lock C.\n");
    }
    else {
        printf("p4 UN_locked C successfully.\n");
    }

    int bp = drm_post(B, (void*)pthread_self());
    if (!bp) {
        printf("rejecting p4's attempt to UN_lock B.\n");
    }
    else {
        printf("p4 UN_locked B successfully.\n");
    }
    */
    
    int d = drm_wait(D, (void*)pthread_self());
    if (!d) {
        printf("deadlock: rejecting p4's attempt to lock D.\n");
    }
    else {
        printf("p4 locked D successfully.\n");
    }

    int dp = drm_post(D, (void*)pthread_self());
    if (!dp) {
        printf("rejecting p4's attempt to UN_lock D.\n");
    }
    else {
        printf("p4 UN_locked D successfully.\n");
    }
    

    return NULL;
}


int main() {
    // Initialize stuff
    // drms[0] = drm_init();
    // drms[1] = drm_init();
    // memset(vals, 0, sizeof(vals));
    
    // Test 1
    // size_t idx0 = 0;
    // size_t idx1 = 1;
    // pthread_create(tids + 0, NULL, basic_deadlock_task, &idx0);
    // pthread_create(tids + 1, NULL, basic_deadlock_task, &idx1);
    // pthread_join(tids[0], NULL);
    // pthread_join(tids[1], NULL);

    // Free stuff
    // drm_destroy(drms[0]);
    // drm_destroy(drms[1]);

    A = drm_init();
    B = drm_init();
    C = drm_init();
    D = drm_init();

    pthread_create(&p1, NULL, p1_task, NULL);
    pthread_create(&p2, NULL, p2_task, NULL);
    pthread_create(&p3, NULL, p3_task, NULL);
    pthread_create(&p4, NULL, p4_task, NULL);

    pthread_join(p1, NULL);
    pthread_join(p2, NULL);
    pthread_join(p3, NULL);
    pthread_join(p4, NULL);

    drm_destroy(A);
    drm_destroy(B);
    drm_destroy(C);
    drm_destroy(D);

    printf("successfully completed test!\n");

    return 0;
}
