#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>


#include "debug.h"

#include "msg.h"
#include "burst_queue.h"

/**
 * Extracts the basename of a file without its extension.
 * The basename is the last part of the path after the last '/'.
 * If there is a '.' in the basename, it will be removed.
 *
 * @param path The full path to the file.
 * @return Newly allocated string containing the basename without extension.
 */
char *get_basename_no_ext(const char* path) {
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;

    const char* dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    char *result = malloc(len + 1);
    strncpy(result, base, len);
    result[len] = '\0';
    return result;
}

typedef enum {
    process_error = 0,
    process_success,
    process_terminated
} process_status_en;

process_status_en handle_process_requests(int sockfd, const pid_t pid, const char *app_name, burst_t *burst, process_request_t request, uint32_t *sim_start_time_ms, uint32_t *sim_clock_ms) {
    msg_t msg = {
        .pid = pid,
        .request = request,
        .time_ms = (request == PROCESS_REQUEST_RUN)?burst->burst_time_ms:burst->block_time_ms
    };
    // Send request
    if (write(sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
        perror("write");
        close(sockfd);
        return process_error;
    }
    DBG("Application %s (PID %d) sent %s request for %u ms",
           app_name, pid, PROCESS_REQUEST_STRINGS[request], msg.time_ms);
    // Wait for ACK and the internal simulation time
    if (read(sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
        perror("read");
        close(sockfd);
        return process_error;
    }
    if (msg.request != PROCESS_REQUEST_ACK) {
        printf("Received invalid request. Expected ACK, received %s\n", PROCESS_REQUEST_STRINGS[msg.request]);
        return process_error;
    }
    *sim_clock_ms = msg.time_ms;
    if (*sim_start_time_ms == 0) *sim_start_time_ms = *sim_clock_ms; // First burst, set the start time
    DBG("Received %s from scheduler for application %s (PID %d) at time %u ms\n",
           PROCESS_REQUEST_STRINGS[msg.request], app_name, pid, *sim_clock_ms);

    // Wait for DONE and the internal simulation time
    if (read(sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
        perror("read");
        close(sockfd);
        return process_error;
    }

    if (msg.request != PROCESS_REQUEST_DONE) {
        printf("Received invalid request. Expected DONE, received %s\n", PROCESS_REQUEST_STRINGS[msg.request]);
        return process_error;
    }
    *sim_clock_ms = msg.time_ms;
    DBG("Received %s from scheduler for application %s (PID %d) at time %u ms\n",
           PROCESS_REQUEST_STRINGS[msg.request], app_name, pid, *sim_clock_ms);

    return process_success;
}

/*
 * Run like: ./app-pre <burst-file.csv>
 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <burst-file.csv>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse arguments
    const char *burstfile_name = argv[1];
    char *app_name = get_basename_no_ext(burstfile_name);

    burst_queue_t bursts = {.head = NULL, .tail = NULL};

    if (read_queue_from_file(&bursts, burstfile_name) <= 0) {
        fprintf(stderr, "Failed to read burst file %s\n", burstfile_name);
        return EXIT_FAILURE;
    }

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

    pid_t pid = getpid();
    uint32_t sim_clock_ms = 0;              // Clock of the scheduler

    uint32_t start_time_ms = 0;             // Start time of the app
    uint32_t cpu_duration_ms = 0;           // duration of the app (bursts and blocks)
    uint32_t block_duration_ms = 0;         // duration of the app in blocked state

    burst_t *active_burst;

    while ((active_burst = dequeue_burst(&bursts)) != NULL) {
        if (handle_process_requests(sockfd, pid, app_name, active_burst, PROCESS_REQUEST_RUN, &start_time_ms, &sim_clock_ms) == process_error)
            break;
        cpu_duration_ms += active_burst->burst_time_ms;

        if (active_burst->block_time_ms > 0) {
            if (handle_process_requests(sockfd, pid, app_name, active_burst, PROCESS_REQUEST_BLOCK, &start_time_ms, &sim_clock_ms) == process_error)
                break;
            block_duration_ms += active_burst->block_time_ms;
        }
    }

    // Received EXIT, print stats
    double real = (sim_clock_ms - start_time_ms)/1000.0;
    double user = (double)cpu_duration_ms/1000.0;
    double sys = (double)block_duration_ms/1000.0;

    printf("Application %s (PID %d) finished at time %d ms, Elapsed: %.03f seconds, CPU: %.03f seconds, BLOCKED: %.03f seconds\n",
           app_name, pid, sim_clock_ms, real, user, sys);

    close(sockfd);
    free(app_name);
    return EXIT_SUCCESS;
}