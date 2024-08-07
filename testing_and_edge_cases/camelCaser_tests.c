/**
 * extreme_edge_cases
 * CS 341 - Spring 2024
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camelCaser.h"
#include "camelCaser_tests.h"


int test_camelCaser(char **(*camelCaser)(const char *),
                    void (*destroy)(char **)) {
    
    int i = 0;
    
    // #1 (normal string)
    const char * s1 = "the end is near. and so on. go Cubs!";
    char * correct1[] = {"theEndIsNear", "andSoOn", "goCubs", NULL};
    char ** r1 = camelCaser(s1);

    if (!r1) return 0;
    
    while (correct1[i]) {
        if (!r1[i]) return 0;
        if (strcmp(r1[i], correct1[i]) != 0) return 0;
        i++;
    }

    destroy(r1);

    i = 0;

    // #2 (excessive punctuation mixed with letters/words)
    const char * s2 = "...only....the.....once?";
    char * correct2[] = {"", "", "", "only", "", "", "", "the", "", "", "", "", "once", NULL};
    char ** r2 = camelCaser(s2);

    if (!r2) return 0;

    while (correct2[i]) {
        if (!r2[i]) return 0;
        if (strcmp(r2[i], correct2[i]) != 0) return 0;
        i++;
    }
	
    destroy(r2);

    i = 0;

    // #3 (ALL punctuation)
    const char * s3 = "..@@@?@!!!!!!_>>>";
    char * correct3[] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", NULL};
    char ** r3 = camelCaser(s3);

    if (!r3) return 0;

    while (correct3[i]) {
        if (!r3[i]) return 0;
        if (strcmp(r3[i], correct3[i]) != 0) return 0;
        i++;
    }

    destroy(r3);

    i = 0;

    // #4 (includes non-printing ASCII characters)
    const char * s4 = "This is a non-printing symbol: \x05.";
    char * correct4[] = {"thisIsANon", "printingSymbol", "\x05", NULL};
    char ** r4 = camelCaser(s4);

    if (!r4) return 0;

    while (correct4[i]) {
        if (!r4[i]) return 0;
        if (strcmp(r4[i], correct4[i]) != 0) return 0;
        i++;
    }

    destroy(r4);


    // #5 (NULL)
    const char * s5 = NULL;
    char ** r5 = camelCaser(s5);
    if (r5) return 0;


    // #6 (empty string)
    const char * s6 = "";
    char ** r6 = camelCaser(s6);
    if (r6[0]) return 0;
    destroy(r6);

    i = 0;

    // #7 (excessive whitespace, capitalization that needs fixing, double punctuation)
    const char * s7 = "This              is         a         SPACIOUS        test string , howdy?.";
    char * correct7[] = {"thisIsASpaciousTestString", "howdy", "", NULL};
    char ** r7 = camelCaser(s7);

    if (!r7) return 0;

    while (correct7[i]) {
        if (!r7[i]) return 0;
        if (strcmp(r7[i], correct7[i]) != 0) return 0;
        i++;
    }

    destroy(r7);

    i = 0;

    // #8 (output should be entirely capitalized except first letter)
    const char * s8 = "S t o p y e l l i n g!";
    char * correct8[] = {"sTOPYELLING", NULL};
    char ** r8 = camelCaser(s8);

    if (!r8) return 0;

    while (correct8[i]) {
        if (!r8[i]) return 0;
        if (strcmp(r8[i], correct8[i]) != 0) return 0;
        i++;
    }

    destroy(r8);

    // #9 (all space)
    const char * s9 = "                                                              ";
    char ** r9 = camelCaser(s9);
    if (r9[0]) return 0;
    destroy(r9);

    i = 0;

    // #10 (output should be entirely lower case)
    const char * s10 = "L_O_W_E_R_C_A_S_E!";
    char * correct10[] = {"l", "o", "w", "e", "r", "c", "a", "s", "e", NULL};
    char ** r10 = camelCaser(s10);

    if (!r10) return 0;

    while (correct10[i]) {
        if (!r10[i]) return 0;
        if (strcmp(r10[i], correct10[i]) != 0) return 0;
        i++;
    }

    destroy(r10);

    i = 0;

    // #11 (all non-printing characters)
    const char * s11 = "\x07. \x11, \x12 \x13 \x02?";
    char * correct11[] = {"\x07", "\x11", "\x12\x13\x02", NULL};
    char ** r11 = camelCaser(s11);

    if (!r11) return 0;

    while (correct11[i]) {
        if (!r11[i]) return 0;
        if (strcmp(r11[i], correct11[i]) != 0) return 0;
        i++;
    }

    destroy(r11);

    // #12 (NO punctuation)
    const char * s12 = "This could be a very long sentence but will any of it remain or does it all get thrown away";
    char ** r12 = camelCaser(s12);
    if (r12[0]) return 0;
    destroy(r12);

    i = 0;

    // #13 (1 complete and 1 non-complete)
    const char * s13 = "WEEKENDS ARE THE BEEESSSSTTTT! Monday no thank you";
    char * correct13[] = {"weekendsAreTheBeeesssstttt", NULL};
    char ** r13 = camelCaser(s13);

    if (!r13) return 0;

    while (correct13[i]) {
        if (!r13[i]) return 0;
        if (strcmp(r13[i], correct13[i]) != 0) return 0;
        i++;
    }

    destroy(r13);

    i = 0;

    // #14 (runon)
    const char * s14 = "whatDoYouMeanITalkTooMuch whoWasEvenAskingYou itsFeelingABitCrampedInHere!";
    char * correct14[] = {"whatdoyoumeanitalktoomuchWhowasevenaskingyouItsfeelingabitcrampedinhere",  NULL};
    char ** r14 = camelCaser(s14);

    if (!r14) return 0;

    while (correct14[i]) {
        if (!r14[i]) return 0;
        if (strcmp(r14[i], correct14[i]) != 0) return 0;
        i++;
    }

    destroy(r14);

    return 1;
}
