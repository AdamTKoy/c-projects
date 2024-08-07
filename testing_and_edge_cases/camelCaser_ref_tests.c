/**
 * extreme_edge_cases
 * CS 341 - Spring 2024
 */
#include <stdio.h>

#include "camelCaser_ref_utils.h"
#include "camelCaser.c"


int main() {
    // Enter the string you want to test with the reference here.
    // char *input = "hello. welcome to cs241";
    // print_camelCaser(input);
    // char *correct[] = {"hello", NULL};
    // char ** c0 = camel_caser(input);
    // printf("check_output test 0: %d\n", check_output(input, correct));
    // printf("check_output test 0b: %d\n", check_output(input, c0));

    char *input1 = "the end is near. and so on. go Cubs!";
    //print_camelCaser(input1);
    char * correct1[] = {"theEndIsNear", "andSoOn", "goCubs", NULL};
    char ** c1 = camel_caser(input1);
    printf("check_output test 1a: %d\n", check_output(input1, correct1));
    printf("check_output test 1b: %d\n", check_output(input1, c1));

    char *input2 = "...only....the.....once?";
    //print_camelCaser(input2);
    char * correct2[] = {"", "", "", "only", "", "", "", "the", "", "", "", "", "once", NULL};
    char ** c2 = camel_caser(input2);
    printf("check_output test 2a: %d\n", check_output(input2, correct2));
    printf("check_output test 2b: %d\n", check_output(input2, c2));

    char *input3 = "..@@@?@!!!!!!_>>>";
    //print_camelCaser(input3);
    char * correct3[] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", NULL};
    char ** c3 = camel_caser(input3);
    printf("check_output test 3a: %d\n", check_output(input3, correct3));
    printf("check_output test 3b: %d\n", check_output(input3, c3));

    char *input4 = "This is a non-printing symbol: \x05.";
    //print_camelCaser(input4);
    char * correct4[] = {"thisIsANon", "printingSymbol", "\x05", NULL};
    char ** c4 = camel_caser(input4);
    printf("check_output test 4a: %d\n", check_output(input4, correct4));
    printf("check_output test 4b: %d\n", check_output(input4, c4));

    //print_camelCaser(NULL);
    char ** correct5 = NULL;
    const char * c5a = NULL;
    char ** c5 = camel_caser(c5a);
    printf("check_output test 5a: %d\n", check_output(NULL, correct5));
    printf("check_output test 5b: %d\n", check_output(NULL, c5));

    char *input6 = "";
    //print_camelCaser(input6);
    char * correct6a[] = {NULL};
    char ** c6a = correct6a;
    char ** c6 = camel_caser(input6);
    printf("check_output test 6a: %d\n", check_output(input6, correct6a));
    printf("check_output test 6b: %d\n", check_output(input6, c6a));
    printf("check_output test 6c: %d\n", check_output(input6, c6));

    char *input7 = "This              is         a         SPACIOUS        test string , howdy?.";
    // print_camelCaser(input7);
    char * correct7[] = {"thisIsASpaciousTestString", "howdy", "", NULL};
    char ** c7 = camel_caser(input7);
    printf("check_output test 7a: %d\n", check_output(input7, correct7));
    printf("check_output test 7b: %d\n", check_output(input7, c7));

    char *input8 = "S t o p y e l l i n g!";
    // print_camelCaser(input8);
    char * correct8[] = {"sTOPYELLING", NULL};
    char ** c8 = camel_caser(input8);
    printf("check_output test 8a: %d\n", check_output(input8, correct8));
    printf("check_output test 8b: %d\n", check_output(input8, c8));

    char *input9 = "                                                              ";
    // print_camelCaser(input9);
    char * correct9[] = {NULL};
    char ** c9 = camel_caser(input9);
    printf("check_output test 9a: %d\n", check_output(input9, correct9));
    printf("check_output test 9b: %d\n", check_output(input9, c9));

    char *input10 = "L_O_W_E_R_C_A_S_E!";
    // print_camelCaser(input10);
    char * correct10[] = {"l", "o", "w", "e", "r", "c", "a", "s", "e", NULL};
    char ** c10 = camel_caser(input10);
    printf("check_output test 10a: %d\n", check_output(input10, correct10));
    printf("check_output test 10b: %d\n", check_output(input10, c10));

    char *input11 = "\x08. \x11, \x12 \x13 \x02?";
    // print_camelCaser(input11);
    char * correct11[] = {"\x08", "\x11", "\x12\x13\x02", NULL};
    char ** c11 = camel_caser(input11);
    printf("check_output test 11a: %d\n", check_output(input11, correct11));
    printf("check_output test 11b: %d\n", check_output(input11, c11));

    char *input12 = "This could be a very long sentence but will any of it remain or does it all get thrown away";
    // print_camelCaser(input12);
    char * correct12[] = {NULL};
    char ** c12a = correct12;
    char ** c12b = camel_caser(input12);
    printf("check_output test 12a: %d\n", check_output(input12, c12a));
    printf("check_output test 12b: %d\n", check_output(input12, c12b));

    char *input13 = "WEEKENDS ARE THE BEEESSSSTTTT! Monday no thank you";
    // print_camelCaser(input13);
    char * correct13[] = {"weekendsAreTheBeeesssstttt", NULL};
    char ** c13 = camel_caser(input13);
    printf("check_output test 13a: %d\n", check_output(input13, correct13));
    printf("check_output test 13b: %d\n", check_output(input13, c13));

    char *input14 = "whatDoYouMeanITalkTooMuch whoWasEvenAskingYou itsFeelingABitCrampedInHere!";
    // print_camelCaser(input14);
    char * correct14[] = {"whatdoyoumeanitalktoomuchWhowasevenaskingyouItsfeelingabitcrampedinhere",  NULL};
    char ** c14 = camel_caser(input14);
    printf("check_output test 14a: %d\n", check_output(input14, correct14));
    printf("check_output test 14b: %d\n", check_output(input14, c14));
    

    return 0;
}
