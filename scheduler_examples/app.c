#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/errno.h>

#include "debug.h"

#include "msg.h"

/*
 * Run like: ./app <name> <time_s>
 */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <name> <time_s>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse arguments
    const char *app_name = argv[1];
    char *endptr;
    errno = 0;
    long val = strtol(argv[2], &endptr, 10);
    if (errno != 0) {
        perror("strtol");  // conversion error (overflow, etc.)
        return 1;
    }
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid number: %s\n", argv[2]);
        return 1;
    }
    if (val < 0 || val > INT_MAX) {  // optional range check
        fprintf(stderr, "Value out of range: %ld\n", val);
        return 1;
    }
    int32_t time_s = (int32_t) val;

    // Setup socket for communication
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // All in place to start simulating the app
    printf("Application %s started, will need the CPU for %d seconds\n", app_name, time_s);

    // Send RUN request
    pid_t pid = getpid();
    msg_t msg = {
        .pid = pid,
        .request = PROCESS_REQUEST_RUN,
        .time_ms = time_s * 1000
    };
    if (write(sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
        perror("write");
        close(sockfd);
        return EXIT_FAILURE;
    }
    DBG("Application %s (PID %d) sent RUN request for %d ms",
           app_name, pid, msg.time_ms);
    // Wait for ACK and the internal simulation time
    if (read(sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
        perror("read");
        close(sockfd);
        return EXIT_FAILURE;
    }
    if (msg.request != PROCESS_REQUEST_ACK) {
        printf("Received invalid request. Expected ACK\n");
        return EXIT_FAILURE;
    }

    // Received ACK
    uint32_t start_time_ms = msg.time_ms;
//    printf("Application %s (PID %d) started running at time %d ms\n", app_name, pid, start_time_ms);

    // Wait for the EXIT message
    if (read(sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
        perror("read");
        close(sockfd);
        return EXIT_FAILURE;
    }
    if (msg.request != PROCESS_REQUEST_DONE) {
        printf("Received invalid request. Expected EXIT\n");
    }

    // Received EXIT, print stats
    double real = (msg.time_ms - start_time_ms)/1000.0;
    double user = (double)time_s;
    double sys = real - time_s;

    printf("Application %s (PID %d) finished at time %d ms, Elapsed: %.03f seconds, CPU: %.03f seconds\n",
           app_name, pid, msg.time_ms, real, user);

    close(sockfd);
    return EXIT_SUCCESS;
}