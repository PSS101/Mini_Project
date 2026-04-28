/*
 * client.c  —  Person C
 *
 * TCP CLI client for Mini Redis.
 * Reads commands from stdin, sends to server, prints response.
 *
 * Usage:  ./client <host> <port>
 *
 * Protocol framing: server terminates each response with "\r\nEND\n".
 * The client accumulates recv() chunks until it sees that sentinel,
 * then strips it and prints the message.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SEND_BUF  1024
#define RECV_BUF  8192   /* large enough for SMEMBERS / HGETALL on big sets */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int         port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid host address: %s\n", host);
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("Connected to Mini Redis at %s:%d\n", host, port);
    printf("Type EXIT to quit.\n\n");

    char  send_buf[SEND_BUF];
    char  recv_buf[RECV_BUF];
    int   total;

    while (1) {
        printf("mini-redis> ");
        fflush(stdout);

        if (!fgets(send_buf, sizeof(send_buf), stdin)) break;

        /* local exit — don't send to server */
        if (strncmp(send_buf, "EXIT", 4) == 0) break;

        /* make sure the command ends with \n for server parsing */
        if (send_buf[strlen(send_buf) - 1] != '\n') {
            size_t len = strlen(send_buf);
            if (len < sizeof(send_buf) - 1) {
                send_buf[len]     = '\n';
                send_buf[len + 1] = '\0';
            }
        }

        if (send(sock, send_buf, strlen(send_buf), 0) < 0) {
            perror("send");
            break;
        }

        /* Accumulate response until we see "END\n" sentinel */
        memset(recv_buf, 0, sizeof(recv_buf));
        total = 0;

        while (1) {
            int n = recv(sock, recv_buf + total,
                         sizeof(recv_buf) - total - 1, 0);
            if (n <= 0) goto done;
            total += n;
            recv_buf[total] = '\0';
            if (strstr(recv_buf, "END\n")) break;
        }

        /* Strip the "\r\nEND\n" sentinel before printing */
        char *end_marker = strstr(recv_buf, "\r\nEND");
        if (end_marker) *end_marker = '\0';

        printf("%s\n", recv_buf);
    }

done:
    close(sock);
    printf("Disconnected.\n");
    return 0;
}
