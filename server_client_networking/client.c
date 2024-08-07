/**
 * nonstop_networking
 * CS 341 - Spring 2024
 */

#include "common.h"
#include "format.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define GET_BUF_SIZE 1024 * 16
#define ERR_BUF_SIZE 128
#define LIST_BUF_SIZE 1024 * 4

static volatile int serverSocket;

int connect_to_server(const char *host, const char *port);
ssize_t write_bytes(int dest, const char *buffer, size_t count);
ssize_t read_bytes(int source, char *buffer, size_t count);

// provided functions
char **parse_args(int argc, char **argv);
verb check_args(char **args);

// my helper functions
void print_error_func(int server_fd);
char *get_server_response(int server_fd);

// from CC lab
void close_server_connection()
{
    if (shutdown(serverSocket, SHUT_RDWR) != 0)
    {
        perror("server socket shutdown");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    close(serverSocket);
}

int main(int argc, char **argv)
{

    // validate arguments (indices 2 up to 4), also get 'verb' enum
    // on error, function handles error messages and exiting program
    verb v = check_args(argv);

    char **args = parse_args(argc, argv); // needed to split host and port string

    // args[0]: host
    // args[1]: port
    serverSocket = connect_to_server(args[0], args[1]);

    free(args);
    args = NULL;

    if (v == GET)
    { // Format: "GET "<remote file><\n>

        char *req_buf = calloc(1, 5 + strlen(argv[3]));
        sprintf(req_buf, "%s %s\n", argv[2], argv[3]);
        size_t req_len = strlen(req_buf);

        ssize_t req_bytes_sent = write_bytes(serverSocket, req_buf, req_len);

        if (req_bytes_sent != (ssize_t)req_len)
        {
            perror("GET: write_bytes (request)");
            free(req_buf);
            req_buf = NULL;
            close_server_connection();
            exit(EXIT_FAILURE);
        }

        free(req_buf);
        req_buf = NULL;

        shutdown(serverSocket, SHUT_WR);

        char *response_buf = get_server_response(serverSocket);

        if (strcmp(response_buf, "OK") == 0)
        {

            free(response_buf);
            response_buf = NULL;

            // read next 'size_t' bytes from server indicating size of file
            size_t data_size;

            ssize_t size_bytes_read = read_bytes(serverSocket, (char *)&data_size, sizeof(size_t));

            if (size_bytes_read != (ssize_t)sizeof(size_t))
            {
                perror("GET: read_bytes (size)");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            // 777 permission provides read/write/edit access to everyone
            int local = open(argv[4], O_RDWR | O_CREAT | O_TRUNC, 00777);

            if (local == -1)
            {
                perror("GET: open local file");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            size_t num_read = 0;
            ssize_t read_result, write_result;

            char *buf = malloc(GET_BUF_SIZE);

            while (1)
            {

                read_result = read(serverSocket, buf, GET_BUF_SIZE);

                if (read_result > 0)
                {

                    write_result = write(local, buf, read_result);

                    if (write_result != read_result)
                    {
                        perror("client:GET:write");
                        free(buf);
                        buf = NULL;
                        close(serverSocket);
                        exit(EXIT_FAILURE);
                    }

                    num_read += read_result;

                    if (num_read > data_size)
                    {
                        break;
                    }
                }

                else if (read_result == 0)
                {
                    break;
                }

                else if (read_result == -1)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        perror("client:GET:read");
                        free(buf);
                        buf = NULL;
                        close(serverSocket);
                        exit(EXIT_FAILURE);
                    }
                }
            }

            free(buf);
            buf = NULL;

            if (num_read > data_size)
            {
                print_received_too_much_data();
            }

            else if (num_read < data_size)
            {
                print_too_little_data();
            }

            close(local);
        }

        else if (strcmp(response_buf, "ERROR") == 0)
        {
            free(response_buf);
            response_buf = NULL;
            print_error_func(serverSocket);
        }

        else
        {
            print_invalid_response();
        }

        if (response_buf)
        {
            free(response_buf);
            response_buf = NULL;
        }
        close(serverSocket);
    }

    else if (v == PUT)
    { // Format: "PUT "<local file><\n><write size><binary data from file>

        int local = open(argv[4], O_RDONLY);
        if (local == -1)
        {
            perror("PUT: open local file");
            close_server_connection();
            exit(EXIT_FAILURE);
        }

        struct stat st;
        int f_stats = fstat(local, &st);
        if (f_stats != 0)
        {
            perror("PUT: fstat");
            close(local);
            close_server_connection();
            exit(EXIT_FAILURE);
        }
        size_t data_size = (size_t)st.st_size;

        char *buf = calloc(1, strlen(argv[3]) + 5 + 1);

        sprintf(buf, "%s %s\n", argv[2], argv[3]);
        size_t req_len = strlen(argv[3]) + 5;
        ssize_t result = write_bytes(serverSocket, buf, req_len);

        if (result != (ssize_t)req_len)
        {
            perror("PUT: write_bytes (request)");
            free(buf);
            buf = NULL;
            close(local);
            close_server_connection();
            exit(EXIT_FAILURE);
        }

        free(buf);
        buf = NULL;

        // COMMENT OUT --> for testing too much/little data (artificially breaking program)
        // data_size += 10;

        result = write_bytes(serverSocket, (char *)&data_size, sizeof(size_t));

        // COMMENT OUT --> for testing too much/little data
        // data_size -= 10;

        if (result != sizeof(size_t))
        {
            perror("PUT: write_bytes (size)");
            close(local);
            close_server_connection();
            exit(EXIT_FAILURE);
        }

        // MMAPing local file to send data (that may exceed memory size)
        char *local_map = mmap(0, data_size, PROT_READ, MAP_SHARED, local, 0);
        if (local_map == MAP_FAILED)
        {
            perror("PUT: creating mmap");
            close(local);
            close_server_connection();
            exit(EXIT_FAILURE);
        }

        size_t num_written = 0;

        while (num_written < data_size)
        {

            result = write(serverSocket, local_map + num_written, data_size - num_written);

            if (result > 0)
            {
                num_written += result;
            }

            else if (result == 0)
            {
                break;
            }

            else if (result == -1 && errno == EINTR)
            {
                continue;
            }

            else
            {
                perror("PUT: write mmap to server");
                close(local);
                close_server_connection();
                munmap(local_map, data_size);
                exit(EXIT_FAILURE);
            }
        }

        munmap(local_map, data_size);
        close(local);

        shutdown(serverSocket, SHUT_WR);

        char *response_buf = get_server_response(serverSocket);

        if (strcmp(response_buf, "OK") == 0)
        {
            print_success();
        }
        else if (strcmp(response_buf, "ERROR") == 0)
        {
            free(response_buf);
            response_buf = NULL;
            print_error_func(serverSocket);
        }
        else
        {
            print_invalid_response();
        }

        if (response_buf)
        {
            free(response_buf);
            response_buf = NULL;
        }
    }

    else if (v == DELETE)
    { // Format: "DELETE "<file><\n>

        char *buf = calloc(1, strlen(argv[3]) + 8);
        sprintf(buf, "%s %s\n", argv[2], argv[3]);

        size_t req_len = strlen(buf);
        ssize_t result = write_bytes(serverSocket, buf, req_len);

        if (result != (ssize_t)req_len)
        {
            perror("DELETE: write_bytes(request)");
            free(buf);
            buf = NULL;
            close_server_connection();
            exit(EXIT_FAILURE);
        }

        free(buf);
        buf = NULL;

        shutdown(serverSocket, SHUT_WR);

        char *response_buf = get_server_response(serverSocket);

        if (strcmp(response_buf, "OK") == 0)
        {
            print_success();
        }

        else if (strcmp(response_buf, "ERROR") == 0)
        {
            free(response_buf);
            response_buf = NULL;
            print_error_func(serverSocket);
        }

        else
        {
            print_invalid_response();
        }

        if (response_buf)
        {
            free(response_buf);
            response_buf = NULL;
        }
    }

    // v == LIST (args already validated)
    else
    { // Format: "LIST"<\n>

        char *buf = calloc(1, 5);
        sprintf(buf, "%s\n", argv[2]);

        size_t req_len = strlen(buf);

        ssize_t write_result = write_bytes(serverSocket, buf, req_len);

        if (write_result != (ssize_t)req_len)
        {
            perror("LIST: write_bytes(request)");
            free(buf);
            buf = NULL;
            close_server_connection();
            exit(EXIT_FAILURE);
        }

        free(buf);
        buf = NULL;

        shutdown(serverSocket, SHUT_WR);

        char *response_buf = get_server_response(serverSocket);

        if (strcmp(response_buf, "OK") == 0)
        {

            free(response_buf);
            response_buf = NULL;

            size_t data_size;

            ssize_t size_bytes_read = read_bytes(serverSocket, (char *)&data_size, sizeof(size_t));

            if (size_bytes_read != (ssize_t)sizeof(size_t))
            {
                perror("LIST: read_bytes(size)");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            size_t num_read = 0;
            ssize_t read_result;

            buf = malloc(LIST_BUF_SIZE);

            while (1)
            {
                read_result = read(serverSocket, buf, LIST_BUF_SIZE);

                if (read_result > 0)
                {
                    num_read += read_result;

                    if (num_read > data_size)
                    {
                        break;
                    }

                    write_result = write(fileno(stdout), buf, read_result);

                    if (write_result != read_result)
                    {
                        perror("LIST: write");
                        free(buf);
                        buf = NULL;
                        close(serverSocket);
                        exit(EXIT_FAILURE);
                    }
                }

                else if (read_result == 0)
                {
                    break;
                }

                else if (read_result == -1 && errno == EINTR)
                {
                    continue;
                }

                else if (read_result == -1)
                {
                    perror("LIST: read");
                    free(buf);
                    buf = NULL;
                    close(serverSocket);
                    exit(EXIT_FAILURE);
                }
            }

            if (num_read > data_size)
            {
                print_received_too_much_data();
            }

            else if (num_read < data_size)
            {
                print_too_little_data();
            }

            free(buf);
            buf = NULL;
        }

        else if (strcmp(response_buf, "ERROR") == 0)
        {
            free(response_buf);
            response_buf = NULL;
            print_error_func(serverSocket);
        }

        else
        {
            print_invalid_response();
        }

        if (response_buf)
        {
            free(response_buf);
            response_buf = NULL;
        }
    }

    close(serverSocket);
    exit(EXIT_SUCCESS);
}

/**
 * Given commandline argc and argv, parses argv.
 *
 * argc argc from main()
 * argv argv from main()
 *
 * Returns char* array in form of {host, port, method, remote, local, NULL}
 * where `method` is ALL CAPS
 */
char **parse_args(int argc, char **argv)
{
    if (argc < 3)
    {
        return NULL;
    }

    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (port == NULL)
    {
        return NULL;
    }

    char **args = calloc(1, 6 * sizeof(char *));
    args[0] = host;
    args[1] = port;
    args[2] = argv[2];
    char *temp = args[2];
    while (*temp)
    {
        *temp = toupper((unsigned char)*temp);
        temp++;
    }
    if (argc > 3)
    {
        args[3] = argv[3];
    }
    if (argc > 4)
    {
        args[4] = argv[4];
    }

    return args;
}

/**
 * Validates args to program.  If `args` are not valid, help information for the
 * program is printed.
 *
 * args     arguments to parse
 *
 * Returns a verb which corresponds to the request method
 */
verb check_args(char **args)
{
    if (args == NULL)
    {
        print_client_usage();
        exit(1);
    }

    char *command = args[2];

    if (strcmp(command, "LIST") == 0)
    {
        return LIST;
    }

    if (strcmp(command, "GET") == 0)
    {
        if (args[3] != NULL && args[4] != NULL)
        {
            return GET;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "DELETE") == 0)
    {
        if (args[3] != NULL)
        {
            return DELETE;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "PUT") == 0)
    {
        if (args[3] == NULL || args[4] == NULL)
        {
            print_client_help();
            exit(1);
        }
        return PUT;
    }

    // Not a valid Method
    print_client_help();
    exit(1);
}

int connect_to_server(const char *host, const char *port)
{

    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(host, port, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(1);
    }

    int sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock_fd == -1)
    {
        perror(NULL);
        freeaddrinfo(result);
        exit(1);
    }

    int ok = connect(sock_fd, result->ai_addr, result->ai_addrlen);
    if (ok == -1)
    {
        perror(NULL);
        freeaddrinfo(result);
        exit(1);
    }

    freeaddrinfo(result);

    return sock_fd;
}

void print_error_func(int server_fd)
{
    char *err_buf = calloc(1, ERR_BUF_SIZE);
    size_t num_read = 0;
    ssize_t read_result;

    while (num_read < ERR_BUF_SIZE)
    {

        read_result = read(server_fd, err_buf, ERR_BUF_SIZE);

        if (read_result > 0)
        {
            num_read += read_result;
        }

        else if (read_result == 0)
        {
            break;
        }

        else if (read_result == -1 && EINTR == errno)
        {
            continue;
        }

        else if (read_result == -1)
        {
            free(err_buf);
            err_buf = NULL;
            perror("print_error_func");
            close(serverSocket);
            exit(EXIT_FAILURE);
        }
    }

    print_error_message(err_buf);

    free(err_buf);
    err_buf = NULL;
}

ssize_t read_bytes(int source, char *buffer, size_t count)
{

    size_t num_read = 0;
    ssize_t result;

    while (num_read < count)
    {
        result = read(source, buffer + num_read, count - num_read);

        if (result > 0)
        {
            num_read += result;
        }
        else if (result == 0)
        {
            return num_read;
        }
        else if (result == -1 && errno == EINTR)
        {
            continue;
        }
        else
        {
            perror("read_bytes: read");
            return -1;
        }
    }

    return num_read;
}

ssize_t write_bytes(int dest, const char *buffer, size_t count)
{

    size_t num_written = 0;
    ssize_t result;

    while (num_written < count)
    {
        result = write(dest, buffer + num_written, count - num_written);

        if (result > 0)
        {
            num_written += result;
        }
        else if (result == 0)
        {
            return num_written;
        }
        else if (result == -1 && errno == EINTR)
        {
            continue;
        }
        else if (result == -1)
        {
            perror("write_bytes: write");
            return -1;
        }
    }

    return num_written;
}

char *get_server_response(int server_fd)
{

    char *buffer = calloc(1, 7);
    size_t num_read = 0;
    ssize_t read_result;

    while (1)
    {
        read_result = read(server_fd, buffer + num_read, 1);

        if (read_result > 0)
        {
            if (buffer[num_read] == '\n')
            {
                buffer[num_read] = '\0'; // terminate string for strcmp
                return buffer;
            }

            num_read += read_result;

            if (num_read > 5)
            {
                return buffer;
            } // invalid response
        }

        else if (read_result == 0)
        {
            return buffer;
        }

        else if (read_result == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                free(buffer);
                buffer = NULL;
                perror("get_server_response: read");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }
        }
    }
}