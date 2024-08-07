/**
 * nonstop_networking
 * CS 341 - Spring 2024
 */

// Reference for much of my epoll code: https://eklitzke.org/blocking-io-nonblocking-io-and-epoll

#include "common.h"
#include "compare.h"
#include "dictionary.h"
#include "format.h"
#include "set.h"
#include "vector.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLIENTS 128
#define MAX_SIZE 1024 * 16

static volatile int endSession = 0;

static dictionary *client_dict = NULL;
static set *temp_files = NULL;

static char *temp_dir_name = NULL;

static int epfd = -1;
static int list_length = 0;
static int serverSocket;

typedef struct client_t
{

    int fd;
    int state;
    int remote_fd;

    verb req_verb;

    char buf[MAX_SIZE];

    size_t buf_offset;
    size_t size;

    char *file_name;
    char *mapped_file;
    char *req_copy;

} client_t;

static void close_server() { endSession = 1; }

static void early_shutdown()
{
    shutdown(serverSocket, SHUT_RDWR);
    close(serverSocket);
    exit(EXIT_FAILURE);
}

// Adapted from https://github.com/eklitzke/epollet/blob/master/poll.c
static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl()");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl()");
        return -1;
    }
    return 0;
}

// helper functions
void calculate_size(client_t *client);
void change_data_direction(client_t *client);
void cleanup();
void client_cleanup(client_t *client);
void delete_file(client_t *client);
void error_response(client_t *client);
void list_to_buffer(client_t *client);
void ok_response(client_t *client);
void process_client(int client_sock);
void process_request(client_t *client);
void read_file(client_t *client);
void read_request(client_t *client);
void read_size(client_t *client);
void write_file(client_t *client);
void write_list(client_t *client);
void write_size(client_t *client);

int main(int argc, char **argv)
{

    // USAGE: ./server <port>
    if (argc != 2)
    {
        print_server_usage();
        exit(EXIT_FAILURE);
    }

    struct sigaction act;
    memset(&act, '\0', sizeof(act));
    act.sa_handler = close_server;
    if (sigaction(SIGINT, &act, NULL) < 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE, SIG_IGN);

    // start server:
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // argv[1]: port
    int s = getaddrinfo(NULL, argv[1], &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (serverSocket < 0)
    {
        perror("serverSocket");
        exit(EXIT_FAILURE);
    }

    // reusing addresses
    int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1)
    {
        perror("SO_REUSEADDR");
        early_shutdown();
    }

    if (bind(serverSocket, result->ai_addr, result->ai_addrlen) != 0)
    {
        perror("bind");
        early_shutdown();
    }

    freeaddrinfo(result);

    if (set_nonblocking(serverSocket) == -1)
    {
        perror("set_nonblocking(server)");
        early_shutdown();
    }

    if (listen(serverSocket, MAX_CLIENTS) != 0)
    {
        perror("listen");
        early_shutdown();
    }

    // temporary directory
    char templt[] = "XXXXXX";
    temp_dir_name = mkdtemp(templt);
    if (!temp_dir_name)
    {
        perror("temp directory");
        early_shutdown();
    }

    print_temp_directory(temp_dir_name);

    if (chdir(temp_dir_name) != 0)
    {
        perror("chdir");
        early_shutdown();
    }

    // set of strings for server temp file names
    temp_files = string_set_create();

    // EPOLL
    epfd = epoll_create1(0);
    if (epfd == -1)
    {
        perror("epoll_create1");
        cleanup();
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev, events[MAX_CLIENTS];
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = serverSocket;
    ev.events = EPOLLIN | EPOLLET; // ET: edge-triggered

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, serverSocket, &ev) == -1)
    {
        perror("epoll_ctl_add: serverSocket");
        cleanup();
        exit(EXIT_FAILURE);
    }

    // client fds returned from 'acccept' are keys
    client_dict = dictionary_create(int_hash_function, int_compare, int_copy_constructor,
                                    int_destructor, NULL, NULL);

    while (1)
    {
        if (endSession)
        {
            break;
        } // loop only ends on error or SIGINT

        int nfds = epoll_wait(epfd, events, MAX_CLIENTS, -1);
        if (nfds == -1)
        {
            perror("epoll_wait");
            cleanup();
            exit(EXIT_FAILURE);
        }

        for (int n = 0; n < nfds; n++)
        {

            if (events[n].data.fd == serverSocket)
            {

                while (1)
                {
                    int client_socket = accept(serverSocket, NULL, NULL);

                    if (client_socket == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        else
                        {
                            perror("accept");
                            cleanup();
                            exit(EXIT_FAILURE);
                        }
                    }
                    else
                    {
                        if (set_nonblocking(client_socket) == -1)
                        {
                            perror("set_nonblocking(client)");
                            cleanup();
                            exit(EXIT_FAILURE);
                        }

                        ev.data.fd = client_socket;
                        ev.events = EPOLLIN | EPOLLET;

                        if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_socket, &ev) == -1)
                        {
                            perror("epoll_ctl_add(client)");
                            cleanup();
                            exit(EXIT_FAILURE);
                        }

                        client_t *new_client = calloc(1, sizeof(client_t));

                        new_client->fd = client_socket;
                        new_client->state = 0; // processing header
                        new_client->buf_offset = 0;
                        new_client->req_copy = NULL;
                        new_client->file_name = NULL;
                        new_client->mapped_file = NULL;
                        new_client->remote_fd = -1;

                        dictionary_set(client_dict, (void *)&client_socket, (void *)new_client);
                    }
                }
            }

            else
            {
                process_client(events[n].data.fd);
            }
        }
    }

    cleanup();
    exit(EXIT_SUCCESS);
}

void calculate_size(client_t *client)
{

    if (client->req_verb == GET)
    {

        if (set_contains(temp_files, (void *)client->file_name))
        {
            struct stat st;
            if (fstat(client->remote_fd, &st) != 0)
            {
                perror("GET: calculate_size: fstat");
                client_cleanup(client);
                return;
            }

            client->size = (size_t)st.st_size;

            client->state = 7;
            client->buf_offset = 0;
            change_data_direction(client);
            ok_response(client);
        }

        else
        {
            sprintf(client->buf, "ERROR\n%s", (char *)err_no_such_file);
            client->size = strlen((char *)err_no_such_file) + 6;

            client->state = 5;
            change_data_direction(client);
            error_response(client);
        }
    }

    else if (client->req_verb == LIST)
    {

        if (list_length > 0)
        {
            client->size = list_length - 1;
        }
        else
        {
            client->size = 0;
        }

        client->state = 7;
        client->buf_offset = 0;
        change_data_direction(client);
        ok_response(client);
    }

    else
    {
        client_cleanup(client);
    }
}

void change_data_direction(client_t *client)
{
    struct epoll_event ev;
    ev.events = EPOLLOUT;
    ev.data.fd = client->fd;
    epoll_ctl(epfd, EPOLL_CTL_MOD, client->fd, &ev);
}

void cleanup()
{

    if (epfd != -1)
    {
        close(epfd);
    }

    if (client_dict)
    {

        size_t dict_size = dictionary_size(client_dict);

        if (dict_size > 0)
        {
            vector *clients = dictionary_values(client_dict);

            for (size_t i = 0; i < dict_size; i++)
            {
                client_t *current = (client_t *)vector_get(clients, i);

                if (current)
                {
                    if (current->remote_fd != -1)
                    {
                        close(current->remote_fd);
                    }
                    if (current->req_copy)
                    {
                        free(current->req_copy);
                        current->req_copy = NULL;
                    }
                    close(current->fd);
                    free(current);
                    current = NULL;
                }
            }

            vector_destroy(clients);
        }

        dictionary_destroy(client_dict);
        client_dict = NULL;
    }

    size_t num_files = set_cardinality(temp_files);

    if (num_files > 0)
    {

        char *fn;
        vector *files = set_elements(temp_files);

        for (size_t i = 0; i < num_files; i++)
        {
            fn = (char *)vector_get(files, i);
            unlink(fn);
        }

        vector_destroy(files);
        files = NULL;
    }

    set_destroy(temp_files);
    temp_files = NULL;

    chdir("..");

    // Note: can only use rmdir on *empty*  directory
    if (temp_dir_name)
    {
        if (rmdir(temp_dir_name) == -1)
        {
            perror("rmdir");
        }
    }

    if (shutdown(serverSocket, SHUT_RDWR) != 0)
    {
        perror("server shutdown");
    }
    close(serverSocket);
}

void client_cleanup(client_t *client)
{

    int cfd = client->fd;

    if (client->remote_fd != -1)
    {
        close(client->remote_fd);
    }
    if (client->mapped_file)
    {
        munmap(client->mapped_file, client->size);
        client->mapped_file = NULL;
    }
    if (client->req_copy)
    {
        free(client->req_copy);
        client->req_copy = NULL;
    }

    shutdown(client->fd, SHUT_RDWR);
    close(client->fd);

    free(client);
    client = NULL;

    dictionary_remove(client_dict, (void *)&cfd);
}

void delete_file(client_t *client)
{

    if (set_contains(temp_files, (void *)client->file_name))
    {
        unlink(client->file_name);
        set_remove(temp_files, (void *)client->file_name);
        list_length -= strlen(client->file_name) + 1;

        client->state = 7;
        change_data_direction(client);
        ok_response(client);
    }

    else
    {
        sprintf(client->buf, "ERROR\n%s", (char *)err_no_such_file);
        client->size = strlen((char *)err_no_such_file) + 6;

        client->state = 5;
        change_data_direction(client);
        error_response(client);
    }
}

void error_response(client_t *client)
{ // print ERROR + '\n' + message to client socket

    ssize_t write_result;

    while (client->buf_offset < client->size)
    {

        write_result = write(client->fd, client->buf + client->buf_offset, client->size - client->buf_offset);

        if (write_result > 0)
        {
            client->buf_offset += write_result;
        }

        else if (write_result == 0)
        {
            break;
        }

        else if (write_result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("error_response:write(-1)");
                client_cleanup(client);
                return;
            }
        }
    }

    client_cleanup(client);
}

void list_to_buffer(client_t *client)
{

    char *current_file;

    size_t num_files = set_cardinality(temp_files);
    vector *files = set_elements(temp_files);

    for (size_t i = 0; i < num_files; i++)
    {

        current_file = vector_get(files, i);

        if (i == (num_files - 1))
        {
            sprintf(client->buf + client->buf_offset, "%s", current_file);
            client->buf_offset += strlen(current_file);
        }

        else
        {
            sprintf(client->buf + client->buf_offset, "%s\n", current_file);
            client->buf_offset += strlen(current_file) + 1;
        }
    }

    if (client->buf_offset != client->size)
    {
        perror("list_to_buffer: size mismatch");
        client_cleanup(client);
        return;
    }

    client->buf_offset = 0;
    client->state = 6;
    write_list(client);
}

void ok_response(client_t *client)
{ // OK + '\n' (+ optionals)

    sprintf(client->buf, "OK\n");

    ssize_t result;

    while (client->buf_offset < 3)
    {
        result = write(client->fd, client->buf, 3);

        if (result > 0)
        {
            client->buf_offset += result;
        }

        else if (result == 0)
        {
            break;
        }

        else if (result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("ok_response:write");
                client_cleanup(client);
                return;
            }
        }
    }

    if (client->buf_offset != 3)
    {
        perror("ok_response:write");
        client_cleanup(client);
        return;
    }

    if (client->req_verb == GET || client->req_verb == LIST)
    {

        client->buf_offset = 0;
        client->state = 10;
        write_size(client);
    }

    else
    {
        client_cleanup(client);
    }
}

void process_client(int client_sock)
{

    // undefined behavior if key (client_sock) does not exist in dictionary
    client_t *current = (client_t *)dictionary_get(client_dict, &client_sock);

    switch (current->state)
    {
    case 0:
        read_request(current);
        break;
    case 1:
        process_request(current);
        break;
    case 2:
        calculate_size(current);
        break;
    case 3:
        read_size(current);
        break;
    case 4:
        delete_file(current);
        break;
    case 5:
        error_response(current);
        break;
    case 6:
        write_list(current);
        break;
    case 7:
        ok_response(current);
        break;
    case 8:
        write_file(current);
        break;
    case 9:
        list_to_buffer(current);
        break;
    case 10:
        write_size(current);
        break;
    case 11:
        read_file(current);
        break;
    default:
        // perror("process_client: unmatched state");
        client_cleanup(current);
    }
}

void process_request(client_t *client)
{

    client->req_copy = malloc(strlen(client->buf) + 1);
    strcpy(client->req_copy, client->buf);

    char *token_verb = strtok(client->req_copy, " ");
    char *token_filename = strtok(NULL, " "); // will be NULL for LIST
    char *token_null = strtok(NULL, " ");

    if (!token_verb || token_null ||
        (strcmp(token_verb, "LIST") == 0 && token_filename) ||
        (strcmp(token_verb, "LIST") != 0 && !token_filename))
    {

        client->buf_offset = 0;
        sprintf(client->buf, "ERROR\n%s", (char *)err_bad_request);
        client->size = strlen((char *)err_bad_request) + 6;

        client->state = 5;
        change_data_direction(client);
        error_response(client);

        return;
    }

    // check verb
    if (strcmp(token_verb, "GET") == 0)
    {
        client->req_verb = GET;
        client->file_name = token_filename;

        client->remote_fd = open(client->file_name, O_RDONLY);

        if (client->remote_fd == -1)
        {

            sprintf(client->buf, "ERROR\n%s", (char *)err_no_such_file);
            client->size = strlen((char *)err_no_such_file) + 6;

            client->buf_offset = 0;
            client->state = 5;
            change_data_direction(client);
            error_response(client);
            return;
        }

        client->state = 2;
        calculate_size(client);
    }

    else if (strcmp(token_verb, "PUT") == 0)
    {

        client->req_verb = PUT;
        client->file_name = token_filename;

        client->remote_fd = open(client->file_name, O_RDWR | O_CREAT | O_TRUNC, 00777);

        if (client->remote_fd == -1)
        {
            perror("PUT:open");
            client_cleanup(client);
            return;
        }

        client->state = 3;
        client->buf_offset = 0;
        read_size(client);
    }

    else if (strcmp(token_verb, "LIST") == 0)
    {
        client->req_verb = LIST;
        client->state = 2;
        calculate_size(client);
    }

    else if (strcmp(token_verb, "DELETE") == 0)
    {
        client->req_verb = DELETE;
        client->file_name = token_filename;
        client->state = 4;
        delete_file(client);
    }

    else
    { // invalid verb
        client->buf_offset = 0;
        sprintf(client->buf, "ERROR\n%s", (char *)err_bad_request);
        client->size = strlen((char *)err_bad_request) + 6;

        client->state = 5;
        change_data_direction(client);
        error_response(client);
    }
}

void read_file(client_t *client)
{ // PUT

    ssize_t read_result;
    ssize_t write_result;

    while (1)
    {

        read_result = read(client->fd, client->buf, MAX_SIZE);

        if (read_result > 0)
        {
            write_result = write(client->remote_fd, client->buf, read_result);

            if (write_result != read_result)
            {
                perror("read_file: write");
                unlink(client->file_name);
                client_cleanup(client);
                return;
            }

            client->buf_offset += read_result;
        }

        else if (read_result == 0)
        {
            break;
        }

        else if (read_result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("read_file: read");
                unlink(client->file_name);
                client_cleanup(client);
                return;
            }
        }
    }

    close(client->remote_fd);

    if (client->buf_offset != client->size)
    {

        unlink(client->file_name);

        sprintf(client->buf, "ERROR\n%s", (char *)err_bad_file_size);
        client->size = strlen((char *)err_bad_file_size) + 6;

        client->buf_offset = 0;
        client->state = 5;
        change_data_direction(client);
        error_response(client);
    }

    else
    {
        set_add(temp_files, (void *)client->file_name);
        list_length += strlen(client->file_name) + 1;
        client->buf_offset = 0;
        client->state = 7;
        change_data_direction(client);
        ok_response(client);
    }
}

void read_request(client_t *client)
{

    ssize_t read_result;

    while (1)
    {
        read_result = read(client->fd, client->buf + client->buf_offset, 1);

        if (read_result > 0)
        {
            if (client->buf[client->buf_offset] == '\n')
            {
                client->buf[client->buf_offset] = '\0'; // terminate for strcmp
                client->state = 1;
                client->buf_offset = 0;
                break;
            }

            client->buf_offset += read_result;

            // max request from client:
            // "DELETE" (6) + ' ' (1) + [filename] (max 255 bytes) + '\n' (1) = 263 bytes
            if (client->buf_offset > 263)
            {

                client->buf_offset = 0;
                sprintf(client->buf, "ERROR\n%s", (char *)err_bad_request);
                client->size = strlen((char *)err_bad_request) + 6;

                client->state = 5;
                change_data_direction(client);
                error_response(client);
                return;
            }
        }

        else if (read_result == 0)
        {
            break;
        }

        else if (read_result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("read_request");
                client_cleanup(client);
                return;
            }
        }
    }

    client->state = 1;
    process_request(client);
}

void read_size(client_t *client)
{ // PUT

    size_t data_size;
    ssize_t result;

    while (client->buf_offset < sizeof(size_t))
    {
        result = read(client->fd, (char *)&data_size + client->buf_offset, sizeof(size_t) - client->buf_offset);

        if (result > 0)
        {
            client->buf_offset += result;
        }

        else if (result == 0)
        {
            break;
        }

        else if (result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("ok_response:write");
                client_cleanup(client);
                return;
            }
        }
    }

    if (client->buf_offset != (sizeof(size_t)))
    {
        perror("PUT:read_size");
        unlink(client->file_name);
        client_cleanup(client);
        return;
    }

    client->size = data_size;
    client->buf_offset = 0;
    client->state = 11;

    read_file(client);
}

void write_file(client_t *client)
{ // GET

    ssize_t result;

    while (client->buf_offset < client->size)
    {

        result = write(client->fd, client->mapped_file + client->buf_offset, client->size - client->buf_offset);

        if (result > 0)
        {
            client->buf_offset += result;
        }

        else if (result == 0)
        {
            break;
        }

        else if (result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("write_file");
                client_cleanup(client);
                return;
            }
        }
    }

    client_cleanup(client); // GET complete
}

void write_list(client_t *client)
{

    ssize_t result;

    while (client->buf_offset < client->size)
    {

        result = write(client->fd, client->buf + client->buf_offset, client->size - client->buf_offset);

        if (result > 0)
        {
            client->buf_offset += result;
        }

        else if (result == 0)
        {
            break;
        }

        else if (result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("write_list: fwrite");
                client_cleanup(client);
                return;
            }
        }
    }

    client_cleanup(client); // LIST complete
}

void write_size(client_t *client)
{ // GET or LIST

    ssize_t result;

    // COMMENT OUT --> for testing too much/little data in client
    // client->size -= 10;

    while (client->buf_offset < sizeof(size_t))
    {
        result = write(client->fd, (char *)&client->size, sizeof(size_t));

        if (result > 0)
        {
            client->buf_offset += result;
        }

        else if (result == 0)
        {
            break;
        }

        else if (result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            if (errno == EINTR)
            {
                continue;
            }

            perror("ok_response:write");
            client_cleanup(client);
            return;
        }
    }

    // COMMENT OUT --> for testing too much/little data in client
    // client->size += 10;

    if (client->buf_offset != sizeof(size_t))
    {
        perror("write_size: size mismatch");
        client_cleanup(client);
        return;
    }

    if (client->req_verb == GET)
    {

        client->mapped_file = mmap(0, client->size, PROT_READ, MAP_SHARED, client->remote_fd, 0);
        if (client->mapped_file == MAP_FAILED)
        {
            perror("write_size: mmap");
            client_cleanup(client);
            return;
        }

        client->buf_offset = 0;
        client->state = 8;
        write_file(client);
    }

    else if (client->req_verb == LIST)
    {
        client->buf_offset = 0;
        client->state = 9;
        list_to_buffer(client);
    }

    else
    {
        perror("invalid verb in write_size");
        client_cleanup(client);
    }
}