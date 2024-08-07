/**
 * perilous_pointers
 * CS 341 - Spring 2024
 */
#include "part2-functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * (Edit this function to print out the "Illinois" lines in
 * part2-functions.c in order.)
 */
int main()
{
    first_step(81);

    int x = 132;
    int *x_add = &x;
    second_step(x_add);
    x_add = NULL;

    int arr[2];
    arr[0] = 8942;
    int *a1 = arr;
    int **a2 = &a1;
    double_step(a2);

    char strange_x[] = "\x01\x00\x00\x05\x01\x0F";
    char *strange_ptr = strange_x;
    strange_step(strange_ptr);
    strange_ptr = NULL;

    char es_x[] = "abc";
    void *ptr = es_x;
    empty_step(ptr);
    ptr = NULL;

    char y[] = "abcu";
    void *twos_p1 = y;
    char *twos_p2 = y;
    two_step(twos_p1, twos_p2);
    twos_p1 = twos_p2 = NULL;

    char z[] = "abcdefg";
    char *threes_p1 = z;
    char *threes_p2 = z + 2;
    char *threes_p3 = threes_p2 + 2;
    three_step(threes_p1, threes_p2, threes_p3);
    threes_p1 = threes_p2 = threes_p3 = NULL;

    char s[] = "abjrefghijklm";
    char *p1 = s;
    char *p2 = p1;
    char *p3 = p2;
    step_step_step(p1, p2, p3);
    p1 = p2 = p3 = NULL;

    char x1[] = "a";
    char *arg1 = (char *)x1;
    int arg2 = 97;
    it_may_be_odd(arg1, arg2);
    arg1 = NULL;

    char x2[] = "Hello,CS341";
    char *toks_arg = x2;
    tok_step(toks_arg);
    toks_arg = NULL;

    char end_x[] = "\x01\x02";
    void *end1 = &end_x;
    void *end2 = &end_x;
    the_end(end1, end2);
    end1 = end2 = NULL;

    return 0;
}
