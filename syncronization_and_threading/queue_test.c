/**
 * critical_concurrency
 * CS 341 - Spring 2024
 */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <time.h>

#include "queue.h"

#define NUM_THREADS 5
#define MAX 10

void * push_fun(void * args);
void * pull_fun(void * args);


int main() {

	//int ret;
	time_t t;
	srand((unsigned) time(&t));

	ssize_t max = 15;
    
    queue * q1 = queue_create(max);

	pthread_t pusher[NUM_THREADS];
	pthread_t puller[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&pusher[i], NULL, push_fun, q1);
		pthread_create(&puller[i], NULL, pull_fun, q1);
	}

	/*
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&puller[i], NULL, pull_fun, q1);
	}
	*/


	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(pusher[i], NULL);
	}

	printf("pushes have finished.\n");

	
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(puller[i], NULL);
	}

	printf("pulls have finished.\n");

    queue_destroy(q1);

	printf("success!\n");

    return 0;
}


void * push_fun(void * args) {
	
	for (int i = 0; i < 10000; i++) {
		int a = rand() % 100;
		void * av = &a;
		void * d1 = malloc(sizeof(int));
		memcpy(d1,av,sizeof(int));
		queue_push(args, d1);
	}

	return NULL;
}

void * pull_fun(void * args) {
	
	for (int j = 0; j < 10000; j++) {
		void * d2 = queue_pull(args);
		free(d2);
	}

	return NULL;
}