/**
 * extreme_edge_cases
 * CS 341 - Spring 2024
 */
#include "camelCaser.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

char **camel_caser(const char *input_str)
{

    char **output_s;

    // if input is NULL, return NULL
    if (!input_str)
    {
        output_s = NULL;
        return output_s;
    }

    char *input_s = malloc(strlen(input_str) + 1);
    strcpy(input_s, input_str);

    // determine how many pointers we need to allocate
    int num_ptrs = 1;

    while (*input_s)
    { // while pointer does not point to NULL
        if (ispunct(*input_s))
        {
            num_ptrs++;
        }
        input_s++;
    }

    // allocates space for desired # of char pointers
    output_s = malloc(num_ptrs * sizeof(char *));

    // reset input
    input_s -= strlen(input_str);

    // calculate necessary size of each char array
    // 1: count, 2: allocate space, 3: copy over each valid char
    int num_keep = 1;
    int idx = 0;
    int s_idx = 0;

    while ((*input_s) && idx < num_ptrs - 1)
    {
        if ((isalpha(*input_s) || (!isspace(*input_s) && !isalpha(*input_s) && !ispunct(*input_s))))
        {
            num_keep++;
        }

        if (ispunct(*input_s))
        {
            output_s[idx] = malloc(num_keep);
            output_s[idx][num_keep - 1] = '\0';
            num_keep = 1;
            idx++;
        }
        input_s++;
        s_idx++;
    }

    // reset input again
    input_s -= s_idx;

    // Now we actually copy over the desired characters:
    int idx1 = 0;
    int idx2 = 0;
    int capital_flag = 0;
    s_idx = 0;

    while (*input_s && (idx1 < num_ptrs - 1))
    {
        // If something that we keep, copy it to the new char arrays
        if ((isalpha(*input_s) || (!isspace(*input_s) && !isalpha(*input_s) && !ispunct(*input_s))))
        {
            output_s[idx1][idx2] = *input_s;

            // new (non-first) word? capitalize it.
            if ((capital_flag) && isalpha(*input_s))
            {
                output_s[idx1][idx2] = toupper(*input_s);
                capital_flag = 0;
            }
            else
            {
                if (isupper(output_s[idx1][idx2]) && isalpha(*input_s))
                {
                    output_s[idx1][idx2] = tolower(output_s[idx1][idx2]);
                }
            }

            idx2++;
        }

        if (isspace(*input_s))
        {
            capital_flag = 1;
        }

        if (ispunct(*input_s))
        {
            idx1++;
            idx2 = 0;
        }

        input_s++;
        s_idx++;
    }

    input_s -= s_idx;
    free(input_s);
    input_s = NULL;

    // ONLY the first word is different (all lower case) --> do this at the END
    // if the first character of any string is uppercase alpha
    // then convert to lower
    int i;
    for (i = 0; i < num_ptrs - 1; i++)
    {
        if (isalpha(output_s[i][0]) && isupper(output_s[i][0]))
        {
            output_s[i][0] = tolower(output_s[i][0]);
        }
    }

    output_s[num_ptrs - 1] = NULL;
    return output_s;
}

void destroy(char **result)
{

    // only have to free if result is *not* NULL
    if (result)
    {

        int idx = 0;

        while (result[idx])
        {
            free(result[idx]);
            result[idx] = NULL;
            idx++;
        }

        free(result);
    }

    return;
}
