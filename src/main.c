#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include "../include/HTTP_Server.h"

#define BUFSIZE 4080

typedef struct Response {
    int code;
    char *msg;
} Response;


size_t my_strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

void simple_println(const char *str) {
    write(STDOUT_FILENO, str, my_strlen(str));
}


char *simple_strdup(const char *src, char *buf, size_t bufsize) {
    size_t i = 0;
    while (src[i] && i < bufsize - 1) {
        buf[i] = src[i];
        i++;
    }
    buf[i] = '\0';
    return buf;
}

Response *Get(char *path) {
    static char msg_buf[BUFSIZE];
    Response *response = (Response *)malloc(sizeof(Response));

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        response->code = 404;
        simple_strdup("File not found\n", msg_buf, BUFSIZE);
        response->msg = msg_buf;
        return response;
    }

    ssize_t len = read(fd, msg_buf, BUFSIZE - 1);
    if (len < 0) len = 0;
    msg_buf[len] = '\0';
    close(fd);

    response->code = 200;
    response->msg = msg_buf;
    return response;
}

Response *Post(char *path, char *data) {
    static char msg_buf[BUFSIZE];
    Response *response = (Response *)malloc(sizeof(Response));

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        response->code = 500;
        simple_strdup("Failed to open file for writing\n", msg_buf, BUFSIZE);
        simple_println("Failed to open file for writing\n");
        response->msg = msg_buf;
        return response;
    }

    size_t len = my_strlen(data);
    ssize_t written = write(fd, data, len);

    if (written != len) {
        close(fd);
        response->code = 500;
        simple_strdup("Failed to write data to file\n", msg_buf, BUFSIZE);
        response->msg = msg_buf;
        return response;
    }

    close(fd);
    response->code = 200;
    simple_strdup("Data written successfully\n", msg_buf, BUFSIZE);
    response->msg = msg_buf;
    return response;
}

Response *Delete(char *path) {
    static char msg_buf[BUFSIZE];
    Response *response = (Response *)malloc(sizeof(Response));

    int ret = unlink(path);
    if (ret != 0) {
        response->code = 404;
        simple_strdup("Failed to delete file\n", msg_buf, BUFSIZE);
        response->msg = msg_buf;
        return response;
    }

    response->code = 200;
    simple_strdup("File deleted successfully\n", msg_buf, BUFSIZE);
    response->msg = msg_buf;
    return response;
}

Response *ManageUserInput(char *method, char *path, char *data) {
    if (strcmp(method, "GET") == 0) {
        return Get(path);
    } else if (strcmp(method, "POST") == 0) {
        return Post(path, data);
    } else if (strcmp(method, "DELETE") == 0) {
        return Delete(path);
    }
    return NULL;
}

int main() {
    HTTP_Server http_server;
    init_server(&http_server, 6969);

    while (1) {
        char client_msg[BUFSIZE] = "";
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket = accept(http_server.socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) continue;

        simple_println("I got a connection!\n");

        ssize_t received = read(client_socket, client_msg, BUFSIZE - 1);
        client_msg[received] = '\0';
        write(STDOUT_FILENO, client_msg, strlen(client_msg));
        write(STDOUT_FILENO, "\n", 1);

        char clone[BUFSIZE];
        memcpy(clone, client_msg, received + 1);

        char *method = strtok(clone, " ");
        char *path = strtok(NULL, " ");

        int content_length = 0;
        char *content_length_header = strstr(client_msg, "Content-Length: ");
        if (content_length_header)
            content_length = atoi(content_length_header + 16);

        char *headers_end = strstr(client_msg, "\r\n\r\n");
        char *body_start = headers_end ? headers_end + 4 : NULL;
        char *data = NULL;
        if (body_start && content_length > 0) {
            data = (char *)malloc(content_length + 1);
            memcpy(data, body_start, content_length);
            data[content_length] = '\0';
        }

        if (path[0] == '/')
            path++;

        if (fork() == 0) {
            Response *response = ManageUserInput(method, path, data);

            char http_header[4096];
            int header_len = snprintf(http_header, sizeof(http_header),
                                      "HTTP/1.1 %d OK\r\nContent-Length: %lu\r\n\r\n%s\r\n\r\n",
                                      response->code, strlen(response->msg), response->msg);

            send(client_socket, http_header, header_len, 0);
            close(client_socket);
            free(response);
            if (data) free(data);
            _exit(0);
        }
    }

    return 0;
}