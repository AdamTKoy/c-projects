/**
 * vector
 * CS 341 - Spring 2024
 */
#include "sstring.h"
#include "vector.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <string.h>

struct sstring {
    // Anything you want
    char * content;
    size_t len;
} ;


sstring *cstr_to_sstring(const char *input) {
    // your code goes here
    assert(input);

    sstring * new_sstring = malloc(sizeof(sstring));
    new_sstring->content = malloc(strlen(input) + 1);
    strcpy(new_sstring->content, input);
    new_sstring->len = strlen(input);

    return new_sstring;
}

char *sstring_to_cstr(sstring *input) {
    // your code goes here
    char * cstr = malloc(strlen(input->content) + 1);
    strcpy(cstr, input->content);
    return cstr;
}

int sstring_append(sstring *this, sstring *addition) {
    // your code goes here
    size_t sz = this->len + addition->len;
    this->content = realloc(this->content, sz + 1);
    strcat(this->content, addition->content);

    this->len = sz;

    int x = sz;
    return x;
}

vector *sstring_split(sstring *this, char delimiter) {
    // your code goes here
    vector * split_strings = vector_create(&string_copy_constructor, &string_destructor, &string_default_constructor);

    char * token = strsep(&this->content, &delimiter);

    while (token) {
        vector_push_back(split_strings, token);
        token = strsep(&this->content, &delimiter);
    }
    
    return split_strings;
}

int sstring_substitute(sstring *this, size_t offset, char *target,
                       char *substitution) {
    // your code goes here
    char *found = strstr((this->content + offset), target);

    if (found) {
        // first, make a char array big enough for old + new
        size_t new_len = this->len + strlen(substitution) - strlen(target) + 1;

        
        char * sub = malloc(new_len);

        // copy everything original over
        strcpy(sub, this->content);

        // find first occurence (after offset) in new char array
        char * f1 = strstr(sub + offset, target);

        // set that char to be the terminating char
        *f1 = '\0';
        
        // concatenate the substitution
        strcat(sub, substitution);

        // concatenate any remaining characters after the target
        char * f3 = found + strlen(target);
        strcat(sub, f3);
        
        this->content = realloc(this->content, new_len);
        strcpy(this->content, sub);
        this->len = new_len - 1;
        
        return 0;
    }

    else return -1;
}

char *sstring_slice(sstring *this, int start, int end) {
    // your code goes here

    // malloc a new char array of length (end - start + 1)
    char * slice = malloc(end - start + 1);
    int i;
    
    for (i = 0; i < (end - start); i++) {
        slice[i] = this->content[i + start];
    }

    return slice;
}

void sstring_destroy(sstring *this) {
    // your code goes here

    // free char array 'content'
    if (this->content) {
        free(this->content);
        this->content = NULL;
    }

    free(this);
    // this = NULL;?
}
