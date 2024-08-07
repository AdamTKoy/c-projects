/**
 * vector
 * CS 341 - Spring 2024
 */
#include "vector.h"

// I added:
#include <stdio.h>
#include <string.h>

int main() {
    // Write your test cases here

    vector * t1 = vector_create(&int_copy_constructor, &int_destructor, &int_default_constructor);

    printf("Starting address of t1 is %p\n", vector_begin(t1));
    printf("Ending address of t1 is %p\n", vector_end(t1));

    printf("Current size of t1 is %zu\n", vector_size(t1));
    printf("Current capacity of t1 is %zu\n", vector_capacity(t1));

    printf("Is t1 empty? %s\n", vector_empty(t1) ? "true" : "false");

    int i1 = 10;
    int i2 = 14;
    int i3 = 7;
    int i4 = 4;
    vector_push_back(t1, &i1);
    vector_push_back(t1, &i2);
    vector_push_back(t1, &i3);
    vector_push_back(t1, &i4);

    printf("Current size of t1 is now %zu\n", vector_size(t1));

    vector_reserve(t1, 12);

    printf("Current capacity of t1 is now %zu\n", vector_capacity(t1));
    printf("Current size of t1 is now %zu\n", vector_size(t1));

    // should 'get' and 'at' return the SAME or DIFFERENT?
    int * g1 = vector_get(t1, 0);
    printf("g1 is: %p\n", g1);
    printf("element at position 0 is: %d\n", *g1);


    printf("Is t1 empty now? %s\n", vector_empty(t1) ? "true" : "false");

    vector_resize(t1, 18);
    printf("Current capacity of t1 is now %zu\n", vector_capacity(t1));
    printf("Current size of t1 is now %zu\n", vector_size(t1));

    // this works
    // vector_clear(t1);


    vector_erase(t1, 3);

    printf("Current size of t1 is now %zu\n", vector_size(t1));

    int * g2 = vector_get(t1, 6);
    printf("element at position 6 is: %d\n", *g2);

    int i5 = 10;
    int i6 = 256;

    vector_set(t1, 6, &i6);
    int * g4 = vector_get(t1, 6);
    printf("element at position 6 is now: %d\n", *g4);

    vector_insert(t1, 6, &i5);
    int * g3 = vector_get(t1, 6);
    printf("element at position 6 is now: %d\n", *g3);
    int * e7 = vector_get(t1, 7);
    printf("(element at position 7 is): %d\n", *e7);
    printf("Current size of t1 (after insert) is now %zu\n", vector_size(t1));

    vector_erase(t1, 6);
    int * e1 = vector_get(t1, 6);
    printf("element at position 6 (after erase) is now: %d\n", *e1);
    int * e2 = vector_get(t1, 7);
    printf("(element at position 7 is now): %d\n", *e2);

    printf("Current size of t1 (after erase) is now %zu\n", vector_size(t1));

    vector_pop_back(t1);

    printf("Current size of t1 (after pop_back) is now %zu\n", vector_size(t1));

    vector_resize(t1, 128);
    printf("Current size of t1 is now %zu\n", vector_size(t1));
    printf("Current capacity of t1 is now %zu\n", vector_capacity(t1));

    int i128 = 438;
    vector_insert(t1, 128, &i128);
    printf("Current size of t1 is now %zu\n", vector_size(t1));
    printf("Current capacity of t1 is now %zu\n", vector_capacity(t1));

    int * g128 = vector_get(t1, 128);
    printf("element at position 128 is now: %d\n", *g128);

    size_t idx;
    for (idx = 50; idx < 75; idx++) {
        vector_set(t1, idx, &i2);
    }

    int * g5 = vector_get(t1, 67);
    printf("element at position 67 is now: %d\n", *g5);

    for (idx = 0; idx < 1000; idx++) {
        vector_push_back(t1, &i2);
    }

    printf("Current size of t1 is now %zu\n", vector_size(t1));
    printf("Current capacity of t1 is now %zu\n", vector_capacity(t1));

    vector_resize(t1, 8);
    printf("Current size of t1 is now %zu\n", vector_size(t1));
    printf("Current capacity of t1 is now %zu\n", vector_capacity(t1));

    vector_destroy(t1);

    printf("Everything's OK!\n");

    return 0;
}
