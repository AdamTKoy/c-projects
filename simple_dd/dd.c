/**
 * deepfried_dd
 * CS 341 - Spring 2024
 */
#include "format.h"
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

static size_t full_in = 0;
static size_t partial_in = 0;
static size_t full_out = 0;
static size_t partial_out = 0;
static size_t total = 0;

static struct timespec start, finish;

void handler();

int main(int argc, char **argv)
{

    clock_gettime(CLOCK_MONOTONIC, &start);

    int opt;

    // default values
    FILE *input_file = stdin;
    FILE *output_file = stdout;

    ssize_t blocks_to_copy = -1; // -1 means copy entire file
    size_t block_size = 512;
    size_t blocks_to_skip_input = 0;
    size_t blocks_to_skip_output = 0;

    while ((opt = getopt(argc, argv, "i:o:b:c:p:k:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            input_file = fopen(optarg, "r");
            if (input_file == NULL)
            {
                print_invalid_input(optarg);
                exit(1);
            }
            break;

        case 'o':
            // w+ opens or creates file for both reading and writing
            output_file = fopen(optarg, "w+");
            if (output_file == NULL)
            {
                print_invalid_output(optarg);
                exit(1);
            }
            break;

        case 'b': // in bytes per block
            block_size = (size_t)(atoi(optarg));
            break;

        case 'c':
            blocks_to_copy = (size_t)(atoi(optarg));
            break;

        case 'p':
            blocks_to_skip_input = (size_t)(atoi(optarg));
            break;

        case 'k':
            blocks_to_skip_output = (size_t)(atoi(optarg));
            break;

        case '?': // unrecognized argument, getopt auto-prints error message
            exit(1);
        }
    }

    signal(SIGUSR1, handler);

    if (input_file != stdin && blocks_to_skip_input != 0)
    {
        fseek(input_file, blocks_to_skip_input * block_size, SEEK_SET);
    }

    if (output_file != stdout && blocks_to_skip_output != 0)
    {
        fseek(output_file, blocks_to_skip_output * block_size, SEEK_SET);
    }

    size_t read_bytes;
    size_t write_bytes;
    char buf[block_size];

    // while loop for 'stdin' version where loop terminates when feof() catches Ctrl-D
    if (input_file == stdin)
    {
        while (feof(input_file) == 0)
        { // feof returns non-zero once it tries to read beyond the end of a file

            // check for blocks_to_copy limit
            if (blocks_to_copy != -1 && (full_out + partial_out >= (size_t)blocks_to_copy))
            {
                break;
            }

            // reads 'block_size' items, each of size 1 byte
            // and returns number of items (number of bytes) read
            read_bytes = fread(buf, 1, block_size, input_file);

            if (read_bytes < block_size)
            {
                partial_in++;
            }
            else
            {
                full_in++;
            }

            write_bytes = fwrite(buf, 1, read_bytes, output_file);

            if (write_bytes < block_size)
            {
                partial_out++;
            }
            else
            {
                full_out++;
            }

            total += write_bytes;
        }
    }

    // while loop for non-stdin version where loop terminates when no more bytes to read
    else
    {
        read_bytes = fread(buf, 1, block_size, input_file);

        while (read_bytes > 0)
        {

            // does the blocks_to_copy argument include partial blocks?
            if (blocks_to_copy != -1 && (full_out + partial_out >= (size_t)blocks_to_copy))
            {
                break;
            }

            if (read_bytes < block_size)
            {
                partial_in++;
            }
            else
            {
                full_in++;
            }

            write_bytes = fwrite(buf, 1, read_bytes, output_file);

            if (write_bytes < block_size)
            {
                partial_out++;
            }
            else
            {
                full_out++;
            }

            total += write_bytes;

            read_bytes = fread(buf, 1, block_size, input_file);
        }
    }

    if (input_file != stdin)
    {
        fclose(input_file);
    }
    if (output_file != stdout)
    {
        fclose(output_file);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish); // END TIME
    double duration1 = (finish.tv_sec - start.tv_sec);
    double duration2 = (finish.tv_nsec - start.tv_nsec) / (1000000000.0);
    double duration = duration1 + duration2;

    print_status_report(full_in, partial_in, full_out, partial_out, total, duration);

    return 0;
}

void handler()
{
    clock_gettime(CLOCK_MONOTONIC, &finish); // END TIME
    double duration1 = (finish.tv_sec - start.tv_sec);
    double duration2 = (finish.tv_nsec - start.tv_nsec) / (1000000000.0);
    double duration = duration1 + duration2;

    print_status_report(full_in, partial_in, full_out, partial_out, total, duration);
}