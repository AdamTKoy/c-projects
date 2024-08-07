/**
 * vector
 * CS 341 - Spring 2024
 */
#include "sstring.h"
#include "vector.h"

// I added:
#include <stdio.h>
#include <string.h>

int main() {
    // TODO create some tests

    char sample1[] = "Hello World!";
    const char * s1 = sample1;

    sstring * ss1 = cstr_to_sstring(s1);

    char * r1 = sstring_to_cstr(ss1);
    printf("cstring from ss1 is: %s\n", r1);
    printf("length of cstring from ss1 is: %lu\n", strlen(r1));

    char sample2[] = " ...and goodbye!";
    const char * s2 = sample2;

    sstring * ss2 = cstr_to_sstring(s2);
    char * r2 = sstring_to_cstr(ss2);
    printf("length of cstring from ss2 is: %lu\n", strlen(r2));

    int ap1 = sstring_append(ss1, ss2);
    printf("length of appended string is: %d\n", ap1);

    char * r3 = sstring_to_cstr(ss1);
    printf("cstring from ss1 is now: %s\n", r3);
    

    vector * v1 = sstring_split(ss1, 'o');
    size_t i;
    char * cc;
    for (i = 0; i < vector_size(v1); i++) {
        cc = vector_get(v1, i);
        printf("Element %zu: %s\n", i, cc);
    }

    char sample3[] = "Hello, my name is Adam.";
    const char * s3 = sample3;
    sstring * ss3 = cstr_to_sstring(s3);
    char * slice1 = sstring_slice(ss3, 4, 9);
    printf("slice1 is %s\n", slice1);

    char * targ = "name";
    char * subs = "vehicle";
    int x1 = sstring_substitute(ss3, 3, targ, subs);
    printf("x1 is: %d\n", x1);

    char * targ2 = "xoxo";
    int x2 = sstring_substitute(ss3, 0, targ2, subs);
    printf("x2 is: %d\n", x2);

    char * r4 = sstring_to_cstr(ss3);
    printf("cstring from ss3 is now: %s\n", r4);


    sstring_destroy(ss1);
    sstring_destroy(ss2);
    sstring_destroy(ss3);

    printf("All tests passed!\n");

    return 0;
}
